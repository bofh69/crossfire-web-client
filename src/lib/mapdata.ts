/**
 * Map data management for the Crossfire web client.
 * Converted from old/common/mapdata.c and old/common/mapdata.h.
 *
 * Manages a virtual fog-of-war map with a scrollable view area.  Big (multi-
 * tile) faces are tracked both inside the view area (in the cells array) and
 * outside (in the bigfaces array).
 */

import {
    MAXLAYERS,
    MAX_VIEW,
    MAXANIM,
    ANIM_MASK,
    ANIM_FLAGS_MASK,
    ANIM_RANDOM,
    ANIM_SYNC,
    MapCellState,
    CONFIG_MAPWIDTH,
    CONFIG_MAPHEIGHT,
    CONFIG_MAPSCROLL,
} from "./protocol";

import type {
    MapCell,
    MapCellLayer,
    MapCellTailLayer,
    MapLabel,
    Animation,
    PlayerPosition,
} from "./protocol";

import { animations } from "./item";
import { useConfig, wantConfig } from "./init";

// Re-export types for consumers that import from mapdata.
export type { MapCell, MapCellLayer, MapCellTailLayer, MapLabel };

// ──────────────────────────────────────────────────────────────────────────────
// Internal constants
// ──────────────────────────────────────────────────────────────────────────────

/** Size of the virtual fog-of-war map. */
const FOG_MAP_SIZE = 512;

/** After shifting: minimum distance of the view area to the map border. */
const FOG_BORDER_MIN = 128;

/** Maximum size of a big face image in tiles. */
const MAX_FACE_SIZE = 16;

/** Maximum view size used for animation iteration. */
const CURRENT_MAX_VIEW = 33;

// ──────────────────────────────────────────────────────────────────────────────
// BigCell – tracks multi-tile faces outside the view area
// ──────────────────────────────────────────────────────────────────────────────

interface BigCell {
    head: MapCellLayer;
    tail: MapCellTailLayer;
    x: number;
    y: number;
    layer: number;
}

// ──────────────────────────────────────────────────────────────────────────────
// Module state
// ──────────────────────────────────────────────────────────────────────────────

/** Internal player position on the virtual map (top-left of view). */
let pl_pos: PlayerPosition = { x: 0, y: 0 };

/** Player position reported after newmap resets. */
let script_pos: PlayerPosition = { x: 0, y: 0 };

/** View dimensions (tiles). */
let viewWidth = 0;
let viewHeight = 0;

/** The virtual map. */
let cells: MapCell[][] = [];
let mapWidth = 0;
let mapHeight = 0;

/** Big-face tracking outside the view area. */
let bigfaces: BigCell[][][] = [];
let activeBigfaces: Set<BigCell> = new Set();

/** Move-to destination. (0, 0) means no destination. */
export let moveToX = 0;
export let moveToY = 0;
export let moveToAttack = false;

/** Global map rendering offsets used for local scroll prediction. */
export let globalOffsetX = 0;
export let globalOffsetY = 0;
export let wantOffsetX = 0;
export let wantOffsetY = 0;

// ──────────────────────────────────────────────────────────────────────────────
// Callbacks for external dependencies not yet in the TypeScript codebase
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Return the size of a face in tiles.  The default returns 1×1 for every
 * face.  Override with {@link setGetMapImageSize} when the image cache is
 * available.
 */
let getMapImageSizeFn: (face: number) => { w: number; h: number } =
    () => ({ w: 1, h: 1 });

/**
 * Try a hardware-accelerated map scroll.  Returns true on success.  The
 * default always returns false (fall back to full redraw).
 */
let displayMapscrollFn: (dx: number, dy: number) => boolean = () => false;

/** Called when `run_move_to` wants to stop movement. */
let stopRunFn: () => void = () => {};

/** Called when `run_move_to` wants to walk in a direction. */
let walkDirFn: (dir: number) => void = () => {};

/** Called when `run_move_to` wants to run in a direction. */
let runDirFn: (dir: number) => void = () => {};

export function setGetMapImageSize(fn: (face: number) => { w: number; h: number }): void {
    getMapImageSizeFn = fn;
}
export function setDisplayMapscroll(fn: (dx: number, dy: number) => boolean): void {
    displayMapscrollFn = fn;
}
export function setStopRun(fn: () => void): void { stopRunFn = fn; }
export function setWalkDir(fn: (dir: number) => void): void { walkDirFn = fn; }
export function setRunDir(fn: (dir: number) => void): void { runDirFn = fn; }

// ──────────────────────────────────────────────────────────────────────────────
// Factory helpers
// ──────────────────────────────────────────────────────────────────────────────

function newLayer(): MapCellLayer {
    return {
        face: 0,
        sizeX: 1,
        sizeY: 1,
        animation: 0,
        animationSpeed: 0,
        animationLeft: 0,
        animationPhase: 0,
    };
}

function newTailLayer(): MapCellTailLayer {
    return { face: 0, sizeX: 0, sizeY: 0 };
}

function newCell(): MapCell {
    const heads: MapCellLayer[] = [];
    const tails: MapCellTailLayer[] = [];
    const smooth: number[] = [];
    for (let i = 0; i < MAXLAYERS; i++) {
        heads.push(newLayer());
        tails.push(newTailLayer());
        smooth.push(0);
    }
    return {
        heads,
        tails,
        labels: [],
        smooth,
        darkness: 0,
        needUpdate: false,
        needResmooth: false,
        state: MapCellState.Empty,
    };
}

function resetCell(cell: MapCell): void {
    for (let i = 0; i < MAXLAYERS; i++) {
        const h = cell.heads[i];
        h.face = 0;
        h.sizeX = 1;
        h.sizeY = 1;
        h.animation = 0;
        h.animationSpeed = 0;
        h.animationLeft = 0;
        h.animationPhase = 0;
        const t = cell.tails[i];
        t.face = 0;
        t.sizeX = 0;
        t.sizeY = 0;
        cell.smooth[i] = 0;
    }
    cell.labels = [];
    cell.darkness = 0;
    cell.needUpdate = false;
    cell.needResmooth = false;
    cell.state = MapCellState.Empty;
}

// ──────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ──────────────────────────────────────────────────────────────────────────────

/** Clear cells[x][y .. y+lenY-1]. */
function clearCells(x: number, y: number, lenY: number): void {
    for (let i = 0; i < lenY; i++) {
        resetCell(cells[x][y + i]);
    }
}

/** Clamp the image size to the valid range. */
function getImageSize(face: number): { w: number; h: number } {
    let { w, h } = getMapImageSizeFn(face);
    w = Math.max(1, Math.min(w, MAX_FACE_SIZE));
    h = Math.max(1, Math.min(h, MAX_FACE_SIZE));
    return { w, h };
}

function markResmooth(x: number, y: number, layer: number): void {
    if (cells[x][y].smooth[layer] > 1) {
        for (let sdx = -1; sdx < 2; sdx++) {
            for (let sdy = -1; sdy < 2; sdy++) {
                if ((sdx || sdy) &&
                    x + sdx > 0 && x + sdx < mapWidth &&
                    y + sdy > 0 && y + sdy < mapHeight) {
                    cells[x + sdx][y + sdy].needResmooth = true;
                }
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Face expansion (inside the view area – the_map.cells)
// ──────────────────────────────────────────────────────────────────────────────

function expandNeedUpdate(x: number, y: number, w: number, h: number): void {
    for (let dx = 0; dx < w; dx++) {
        for (let dy = 0; dy < h; dy++) {
            cells[x - dx][y - dy].needUpdate = true;
        }
    }
}

function expandNeedUpdateFromLayer(x: number, y: number, layer: number): void {
    const head = cells[x][y].heads[layer];
    if (head.face !== 0) {
        expandNeedUpdate(x, y, head.sizeX, head.sizeY);
    }
}

function expandClearFace(x: number, y: number, w: number, h: number, layer: number): void {
    const cell = cells[x][y];
    for (let dx = 0; dx < w; dx++) {
        for (let dy = dx === 0 ? 1 : 0; dy < h; dy++) {
            const tail = cells[x - dx][y - dy].tails[layer];
            if (tail.face === cell.heads[layer].face &&
                tail.sizeX === dx &&
                tail.sizeY === dy) {
                tail.face = 0;
                tail.sizeX = 0;
                tail.sizeY = 0;
                cells[x - dx][y - dy].needUpdate = true;
            }
            markResmooth(x - dx, y - dy, layer);
        }
    }

    const head = cell.heads[layer];
    head.face = 0;
    head.animation = 0;
    head.animationSpeed = 0;
    head.animationLeft = 0;
    head.animationPhase = 0;
    head.sizeX = 1;
    head.sizeY = 1;
    cell.needUpdate = true;
    cell.needResmooth = true;
    markResmooth(x, y, layer);
}

function expandClearFaceFromLayer(x: number, y: number, layer: number): void {
    const head = cells[x][y].heads[layer];
    if (head.face !== 0 && head.sizeX > 0 && head.sizeY > 0) {
        expandClearFace(x, y, head.sizeX, head.sizeY, layer);
    }
}

/**
 * Set a face in cells[][].  If `clear` is true, clear the old face first.
 * Animation updates pass `clear=false` because animations are always the same
 * size and we don't want to clobber animation metadata.
 */
function expandSetFace(x: number, y: number, layer: number, face: number, clear: boolean): void {
    const cell = cells[x][y];

    if (clear) {
        expandClearFaceFromLayer(x, y, layer);
    }

    const { w, h } = getImageSize(face);
    cell.heads[layer].face = face;
    cell.heads[layer].sizeX = w;
    cell.heads[layer].sizeY = h;
    cell.needUpdate = true;
    markResmooth(x, y, layer);

    for (let dx = 0; dx < w; dx++) {
        for (let dy = dx === 0 ? 1 : 0; dy < h; dy++) {
            const tail = cells[x - dx][y - dy].tails[layer];
            tail.face = face;
            tail.sizeX = dx;
            tail.sizeY = dy;
            cells[x - dx][y - dy].needUpdate = true;
            markResmooth(x - dx, y - dy, layer);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Big face expansion (outside the view area – bigfaces[])
// ──────────────────────────────────────────────────────────────────────────────

function expandClearBigface(x: number, y: number, w: number, h: number, layer: number, setNeedUpdate: boolean): void {
    const head = bigfaces[x][y][layer].head;

    for (let dx = 0; dx < w && dx <= x; dx++) {
        for (let dy = dx === 0 ? 1 : 0; dy < h && dy <= y; dy++) {
            const tail = bigfaces[x - dx][y - dy][layer].tail;
            if (tail.face === head.face &&
                tail.sizeX === dx &&
                tail.sizeY === dy) {
                tail.face = 0;
                tail.sizeX = 0;
                tail.sizeY = 0;

                if (x - dx >= 0 && x - dx < viewWidth &&
                    y - dy >= 0 && y - dy < viewHeight) {
                    if (setNeedUpdate) {
                        cells[pl_pos.x + x - dx][pl_pos.y + y - dy].needUpdate = true;
                    }
                }
            }
        }
    }

    head.face = 0;
    head.sizeX = 1;
    head.sizeY = 1;
}

function expandClearBigfaceFromLayer(x: number, y: number, layer: number, setNeedUpdate: boolean): void {
    const headcell = bigfaces[x][y][layer];
    const head = headcell.head;
    if (head.face !== 0) {
        activeBigfaces.delete(headcell);
        expandClearBigface(x, y, head.sizeX, head.sizeY, layer, setNeedUpdate);
    }
}

function expandSetBigface(x: number, y: number, layer: number, face: number, clear: boolean): void {
    const headcell = bigfaces[x][y][layer];
    const head = headcell.head;

    if (clear) {
        expandClearBigfaceFromLayer(x, y, layer, true);
    }

    if (face !== 0) {
        activeBigfaces.add(headcell);
    }

    const { w, h } = getImageSize(face);
    head.face = face;
    head.sizeX = w;
    head.sizeY = h;

    for (let dx = 0; dx < w && dx <= x; dx++) {
        for (let dy = dx === 0 ? 1 : 0; dy < h && dy <= y; dy++) {
            const tail = bigfaces[x - dx][y - dy][layer].tail;
            tail.face = face;
            tail.sizeX = dx;
            tail.sizeY = dy;

            if (x - dx >= 0 && x - dx < viewWidth &&
                y - dy >= 0 && y - dy < viewHeight) {
                cells[pl_pos.x + x - dx][pl_pos.y + y - dy].needUpdate = true;
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Map allocation / initialisation
// ──────────────────────────────────────────────────────────────────────────────

function mapdataAlloc(w: number, h: number): void {
    cells = [];
    mapWidth = w;
    mapHeight = h;
    for (let x = 0; x < w; x++) {
        const col: MapCell[] = [];
        for (let y = 0; y < h; y++) {
            col.push(newCell());
        }
        cells.push(col);
    }
}

function initBigfaces(): void {
    bigfaces = [];
    for (let x = 0; x < MAX_VIEW; x++) {
        bigfaces[x] = [];
        for (let y = 0; y < MAX_VIEW; y++) {
            bigfaces[x][y] = [];
            for (let i = 0; i < MAXLAYERS; i++) {
                bigfaces[x][y][i] = {
                    head: newLayer(),
                    tail: newTailLayer(),
                    x,
                    y,
                    layer: i,
                };
            }
        }
    }
    activeBigfaces = new Set();
}

function mapdataInit(): void {
    mapdataAlloc(FOG_MAP_SIZE, FOG_MAP_SIZE);
    viewWidth = 0;
    viewHeight = 0;
    pl_pos.x = Math.floor(mapWidth / 2);
    pl_pos.y = Math.floor(mapHeight / 2);

    initBigfaces();

    globalOffsetX = 0;
    globalOffsetY = 0;
    wantOffsetX = 0;
    wantOffsetY = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Internal map-clear helper (fog-of-war transition)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Transition a visible cell to fog state.  `x` and `y` are *view-relative*
 * coordinates (NOT absolute map coordinates).
 */
function mapdataClear(x: number, y: number): void {
    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    const cell = cells[px][py];
    if (cell.state === MapCellState.Empty) {
        return;
    }

    if (cell.state === MapCellState.Visible) {
        cell.needUpdate = true;
        for (let i = 0; i < MAXLAYERS; i++) {
            if (cell.heads[i].face) {
                expandNeedUpdateFromLayer(px, py, i);
            }
        }
    }

    cell.state = MapCellState.Fog;
    mapdata_clear_label(px, py);
}

// ──────────────────────────────────────────────────────────────────────────────
// Virtual map recentering
// ──────────────────────────────────────────────────────────────────────────────

function recenterVirtualMapView(diffX: number, diffY: number): void {
    const newX = pl_pos.x + diffX;
    const newY = pl_pos.y + diffY;
    let shiftX: number;
    let shiftY: number;

    if (newX < MAX_FACE_SIZE) {
        shiftX = FOG_BORDER_MIN + MAX_FACE_SIZE - newX;
    } else if (newX + MAX_VIEW > mapWidth) {
        shiftX = mapWidth - FOG_BORDER_MIN - MAX_VIEW - newX;
    } else {
        shiftX = 0;
    }

    if (newY < MAX_FACE_SIZE) {
        shiftY = FOG_BORDER_MIN + MAX_FACE_SIZE - newY;
    } else if (newY + MAX_VIEW > mapHeight) {
        shiftY = mapHeight - FOG_BORDER_MIN - MAX_VIEW - newY;
    } else {
        shiftY = 0;
    }

    if (shiftX === 0 && shiftY === 0) {
        return;
    }

    // Maintain FOG_BORDER_MIN to all directions when shifting.
    if (shiftX === 0) {
        if (newX < FOG_BORDER_MIN + MAX_FACE_SIZE) {
            shiftX = FOG_BORDER_MIN + MAX_FACE_SIZE - newX;
        } else if (newX + MAX_VIEW + FOG_BORDER_MIN > mapWidth) {
            shiftX = mapWidth - FOG_BORDER_MIN - MAX_VIEW - newX;
        }
    }
    if (shiftY === 0) {
        if (newY < FOG_BORDER_MIN + MAX_FACE_SIZE) {
            shiftY = FOG_BORDER_MIN + MAX_FACE_SIZE - newY;
        } else if (newY + MAX_VIEW + FOG_BORDER_MIN > mapHeight) {
            shiftY = mapHeight - FOG_BORDER_MIN - MAX_VIEW - newY;
        }
    }

    // Shift exceeds map size → clear everything and recenter.
    if (shiftX <= -mapWidth || shiftX >= mapWidth ||
        shiftY <= -mapHeight || shiftY >= mapHeight) {
        for (let dx = 0; dx < mapWidth; dx++) {
            clearCells(dx, 0, mapHeight);
        }
        pl_pos.x = Math.floor(mapWidth / 2 - viewWidth / 2);
        pl_pos.y = Math.floor(mapHeight / 2 - viewHeight / 2);
        return;
    }

    pl_pos.x += shiftX;
    pl_pos.y += shiftY;

    // Compute copy region.
    let srcX: number, dstX: number, lenX: number;
    if (shiftX < 0) {
        srcX = -shiftX;
        dstX = 0;
        lenX = mapWidth + shiftX;
    } else {
        srcX = 0;
        dstX = shiftX;
        lenX = mapWidth - shiftX;
    }

    let srcY: number, dstY: number, lenY: number;
    if (shiftY < 0) {
        srcY = -shiftY;
        dstY = 0;
        lenY = mapHeight + shiftY;
    } else {
        srcY = 0;
        dstY = shiftY;
        lenY = mapHeight - shiftY;
    }

    // Shift columns. We must iterate in the right direction to avoid
    // overwriting source data before it is copied.
    if (shiftX < 0) {
        for (let i = 0; i < lenX; i++) {
            const sx = srcX + i;
            const dx = dstX + i;
            for (let j = 0; j < lenY; j++) {
                copyCellData(cells[sx][srcY + j], cells[dx][dstY + j]);
            }
        }
    } else if (shiftX > 0) {
        for (let i = lenX - 1; i >= 0; i--) {
            const sx = srcX + i;
            const dx = dstX + i;
            for (let j = 0; j < lenY; j++) {
                copyCellData(cells[sx][srcY + j], cells[dx][dstY + j]);
            }
        }
    } else {
        // shiftX === 0 but shiftY !== 0
        for (let i = 0; i < lenX; i++) {
            const col = dstX + i;
            if (shiftY < 0) {
                for (let j = 0; j < lenY; j++) {
                    copyCellData(cells[col][srcY + j], cells[col][dstY + j]);
                }
            } else {
                for (let j = lenY - 1; j >= 0; j--) {
                    copyCellData(cells[col][srcY + j], cells[col][dstY + j]);
                }
            }
        }
    }

    // Clear newly opened areas.
    for (let dx = 0; dx < dstX; dx++) {
        clearCells(dx, 0, mapHeight);
    }
    for (let dx = dstX + lenX; dx < mapWidth; dx++) {
        clearCells(dx, 0, mapHeight);
    }
    if (shiftY > 0) {
        for (let dx = 0; dx < lenX; dx++) {
            clearCells(dx + dstX, 0, shiftY);
        }
    } else if (shiftY < 0) {
        for (let dx = 0; dx < lenX; dx++) {
            clearCells(dx + dstX, mapHeight + shiftY, -shiftY);
        }
    }
}

/** Copy all data from one cell to another (shallow copy is fine). */
function copyCellData(src: MapCell, dst: MapCell): void {
    for (let i = 0; i < MAXLAYERS; i++) {
        Object.assign(dst.heads[i], src.heads[i]);
        Object.assign(dst.tails[i], src.tails[i]);
        dst.smooth[i] = src.smooth[i];
    }
    dst.labels = src.labels.slice();
    dst.darkness = src.darkness;
    dst.needUpdate = src.needUpdate;
    dst.needResmooth = src.needResmooth;
    dst.state = src.state;
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────────────────────

/** Get the cell at absolute map coordinates. */
export function mapdata_cell(x: number, y: number): MapCell {
    return cells[x][y];
}

/** Check whether the map contains the given absolute coordinates. */
export function mapdata_contains(x: number, y: number): boolean {
    return x >= 0 && y >= 0 && x < mapWidth && y < mapHeight;
}

/** Return the dimensions of the internal virtual map. */
export function mapdata_size(): { width: number; height: number } {
    return { width: mapWidth, height: mapHeight };
}

/** Check whether a layer can be smoothed at the given absolute coordinates. */
export function mapdata_can_smooth(x: number, y: number, layer: number): boolean {
    return (cells[x][y].heads[layer].face === 0 && layer > 0) ||
        cells[x][y].smooth[layer] !== 0;
}

/**
 * Initialise / reinitialise the map module.  Call this before any other
 * function, and whenever a new display size is negotiated with the server.
 */
export function mapdata_set_size(viewx: number, viewy: number): void {
    mapdata_free();
    mapdataInit();
    viewWidth = viewx;
    viewHeight = viewy;
    pl_pos.x = Math.floor(mapWidth / 2 - viewWidth / 2);
    pl_pos.y = Math.floor(mapHeight / 2 - viewHeight / 2);
}

/** Deallocate all map data. */
export function mapdata_free(): void {
    cells = [];
    mapWidth = 0;
    mapHeight = 0;
    bigfaces = [];
    activeBigfaces = new Set();
}

/** Check whether the given *view-relative* coordinates are within the view. */
export function mapdata_is_inside(x: number, y: number): boolean {
    return x >= 0 && x < viewWidth && y >= 0 && y < viewHeight;
}

/** Return the face at a given view position and layer (head info only). */
export function mapdata_face(x: number, y: number, layer: number): number {
    if (!mapdataHasTile(x, y, layer)) {
        return 0;
    }
    return cells[pl_pos.x + x][pl_pos.y + y].heads[layer].face;
}

/**
 * Return the face at absolute map coordinates and set dx/dy offsets for
 * drawing.  Returns 0 if nothing to draw.
 */
export function mapdata_face_info(
    mx: number, my: number, layer: number,
): { face: number; dx: number; dy: number } {
    const head = cells[mx][my].heads[layer];
    const tail = cells[mx][my].tails[layer];

    if (head.face !== 0) {
        return {
            face: head.face,
            dx: 1 - head.sizeX,
            dy: 1 - head.sizeY,
        };
    } else if (tail.face !== 0) {
        const headPtr = cells[mx + tail.sizeX][my + tail.sizeY].heads[layer];
        return {
            face: tail.face,
            dx: tail.sizeX - headPtr.sizeX + 1,
            dy: tail.sizeY - headPtr.sizeY + 1,
        };
    }
    return { face: 0, dx: 0, dy: 0 };
}

/**
 * Return the big-face ("tail") information at a view position.  Detects
 * and clears obsolete fog-of-war big faces.
 */
export function mapdata_bigface(
    x: number, y: number, layer: number,
): { face: number; ww: number; hh: number } {
    if (!mapdataHasTile(x, y, layer)) {
        return { face: 0, ww: 1, hh: 1 };
    }

    const px = pl_pos.x + x;
    const py = pl_pos.y + y;
    let result = cells[px][py].tails[layer].face;

    if (result !== 0) {
        const dx = cells[px][py].tails[layer].sizeX;
        const dy = cells[px][py].tails[layer].sizeY;
        const w = cells[px + dx][py + dy].heads[layer].sizeX;
        const h = cells[px + dx][py + dy].heads[layer].sizeY;

        let clearBigface: boolean;
        if (cells[px][py].state === MapCellState.Fog) {
            clearBigface = false;
        } else if (x + dx < viewWidth && y + dy < viewHeight) {
            clearBigface = cells[px + dx][py + dy].state === MapCellState.Fog;
        } else {
            clearBigface = bigfaces[x + dx][y + dy][layer].head.face === 0;
        }

        if (!clearBigface) {
            return { face: result, ww: w - 1 - dx, hh: h - 1 - dy };
        }

        expandClearFaceFromLayer(px + dx, py + dy, layer);
    }

    result = bigfaces[x][y][layer].tail.face;
    if (result !== 0) {
        const dx = bigfaces[x][y][layer].tail.sizeX;
        const dy = bigfaces[x][y][layer].tail.sizeY;
        const w = bigfaces[x + dx][y + dy][layer].head.sizeX;
        const h = bigfaces[x + dx][y + dy][layer].head.sizeY;
        return { face: result, ww: w - 1 - dx, hh: h - 1 - dy };
    }

    return { face: 0, ww: 1, hh: 1 };
}

/** Return the big-face head at a view position (used by OpenGL-style renderers). */
export function mapdata_bigface_head(
    x: number, y: number, layer: number,
): { face: number; ww: number; hh: number } {
    if (!mapdataHasTile(x, y, layer)) {
        return { face: 0, ww: 1, hh: 1 };
    }

    const result = bigfaces[x][y][layer].head.face;
    if (result !== 0) {
        return {
            face: result,
            ww: bigfaces[x][y][layer].head.sizeX,
            hh: bigfaces[x][y][layer].head.sizeY,
        };
    }
    return { face: 0, ww: 1, hh: 1 };
}

/** Clear a map space (called from Map2Cmd). View-relative coordinates. */
export function mapdata_clear_space(x: number, y: number): void {
    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    if (x < viewWidth && y < viewHeight) {
        mapdataClear(x, y);
    } else {
        for (let i = 0; i < MAXLAYERS; i++) {
            expandSetBigface(x, y, i, 0, true);
        }
    }
}

/**
 * After Map2Cmd has processed all data for a space, check whether it ended
 * up blank and mark it accordingly.
 */
export function mapdata_set_check_space(x: number, y: number): void {
    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    if (px < 0 || py < 0 || px >= mapWidth || py >= mapHeight) {
        return;
    }

    let isBlank = true;
    const cell = cells[px][py];
    for (let i = 0; i < MAXLAYERS; i++) {
        if (cell.heads[i].face > 0 || cell.tails[i].face > 0) {
            isBlank = false;
            break;
        }
    }
    if (cell.darkness !== 0) {
        isBlank = false;
    }

    if (!isBlank) {
        return;
    }

    if (x < viewWidth && y < viewHeight) {
        mapdataClear(x, y);
    }
}

/** Set darkness for a tile.  View-relative coordinates. */
export function mapdata_set_darkness(x: number, y: number, darkness: number): void {
    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    if (darkness !== -1 && x < viewWidth && y < viewHeight) {
        setDarkness(px, py, 255 - darkness);
    }
}

function setDarkness(x: number, y: number, darkness: number): void {
    if (cells[x][y].darkness === darkness) {
        return;
    }
    cells[x][y].darkness = darkness;
    cells[x][y].needUpdate = true;
}

/** Set smooth value for a layer.  View-relative coordinates. */
export function mapdata_set_smooth(x: number, y: number, smooth: number, layer: number): void {
    const DX = [0, 1, 1, 1, 0, -1, -1, -1];
    const DY = [-1, -1, 0, 1, 1, 1, 0, -1];

    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    if (cells[px][py].smooth[layer] !== smooth) {
        for (let i = 0; i < 8; i++) {
            const rx = px + DX[i];
            const ry = py + DY[i];
            if (rx < 0 || ry < 0 || mapWidth <= rx || mapHeight <= ry) {
                continue;
            }
            cells[rx][ry].needResmooth = true;
        }
        cells[px][py].needResmooth = true;
        cells[px][py].smooth[layer] = smooth;
    }
}

/** Clear all labels at absolute map coordinates. */
export function mapdata_clear_label(px: number, py: number): void {
    cells[px][py].labels = [];
}

/** Add a label at view-relative coordinates. */
export function mapdata_add_label(x: number, y: number, subtype: number, label: string): void {
    if (!(x < viewWidth && y < viewHeight)) {
        return;
    }

    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    if (subtype === 0) {
        mapdata_clear_label(px, py);
        return;
    }

    cells[px][py].labels.push({ subtype, label });
    cells[px][py].needUpdate = true;
}

/**
 * Prepare a cell that may contain fog-of-war data for new visible data.
 * Called when Map2 is about to provide an update for this tile.
 */
export function mapdata_clear_old(x: number, y: number): void {
    if (!(x < viewWidth && y < viewHeight)) {
        return;
    }

    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    if (cells[px][py].state === MapCellState.Fog) {
        cells[px][py].needUpdate = true;
        for (let i = 0; i < MAXLAYERS; i++) {
            expandClearFaceFromLayer(px, py, i);
        }
        cells[px][py].darkness = 0;
    }

    mapdata_clear_label(px, py);
    cells[px][py].state = MapCellState.Visible;
}

/** Set a face on a specific layer.  View-relative coordinates. */
export function mapdata_set_face_layer(x: number, y: number, face: number, layer: number): void {
    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    if (x < viewWidth && y < viewHeight) {
        cells[px][py].needUpdate = true;
        if (face > 0) {
            expandSetFace(px, py, layer, face, true);
        } else {
            expandClearFaceFromLayer(px, py, layer);
        }
    } else {
        expandSetBigface(x, y, layer, face, true);
    }
}

/** Set an animation on a specific layer.  View-relative coordinates. */
export function mapdata_set_anim_layer(
    x: number, y: number, anim: number, animSpeed: number, layer: number,
): void {
    const px = pl_pos.x + x;
    const py = pl_pos.y + y;

    const animation = anim & ANIM_MASK;
    let face = 0;
    let phase = 0;
    let speedLeft = 0;

    if ((anim & ANIM_FLAGS_MASK) === ANIM_RANDOM) {
        const numAnim = animations[animation]?.numAnimations ?? 0;
        if (numAnim === 0) {
            console.warn("mapdata_set_anim_layer: animating object with zero animations");
            return;
        }
        phase = Math.floor(Math.random() * numAnim);
        face = animations[animation].faces[phase];
        speedLeft = Math.floor(Math.random() * animSpeed);
    } else if ((anim & ANIM_FLAGS_MASK) === ANIM_SYNC) {
        if (animations[animation]) {
            animations[animation].speed = animSpeed;
            phase = animations[animation].phase;
            speedLeft = animations[animation].speedLeft;
            face = animations[animation].faces[phase];
        }
    }

    if (x < viewWidth && y < viewHeight) {
        mapdata_clear_old(x, y);
        if (face > 0) {
            expandSetFace(px, py, layer, face, true);
            cells[px][py].heads[layer].animation = animation;
            cells[px][py].heads[layer].animationPhase = phase;
            cells[px][py].heads[layer].animationSpeed = animSpeed;
            cells[px][py].heads[layer].animationLeft = speedLeft;
        } else {
            expandClearFaceFromLayer(px, py, layer);
        }
    } else {
        expandSetBigface(x, y, layer, face, true);
    }
}

/** Scroll the map view.  Call when a map_scroll command is received. */
export function mapdata_scroll(dx: number, dy: number): void {
    script_pos.x += dx;
    script_pos.y += dy;

    recenterVirtualMapView(dx, dy);

    if (wantConfig[CONFIG_MAPSCROLL] && displayMapscrollFn(dx, dy)) {
        // Mark tiles overlapped by big faces from outside the view area.
        for (const bc of activeBigfaces) {
            for (let bx = 0; bx < bc.head.sizeX; bx++) {
                for (let by = bx === 0 ? 1 : 0; by < bc.head.sizeY; by++) {
                    if (bc.x - bx >= 0 && bc.x - bx < viewWidth &&
                        bc.y - by >= 0 && bc.y - by < viewHeight) {
                        cells[pl_pos.x + bc.x - bx][pl_pos.y + bc.y - by].needUpdate = true;
                    }
                }
            }
        }
    } else {
        for (let x = 0; x < viewWidth; x++) {
            for (let y = 0; y < viewHeight; y++) {
                cells[pl_pos.x + x][pl_pos.y + y].needUpdate = true;
            }
        }
    }

    pl_pos.x += dx;
    pl_pos.y += dy;

    // Clear newly visible tiles.
    if (dx > 0) {
        for (let y = 0; y < viewHeight; y++) {
            for (let x = viewWidth - dx; x < viewWidth; x++) {
                mapdataClear(x, y);
            }
        }
    } else {
        for (let y = 0; y < viewHeight; y++) {
            for (let x = 0; x < -dx; x++) {
                mapdataClear(x, y);
            }
        }
    }
    if (dy > 0) {
        for (let x = 0; x < viewWidth; x++) {
            for (let y = viewHeight - dy; y < viewHeight; y++) {
                mapdataClear(x, y);
            }
        }
    } else {
        for (let x = 0; x < viewWidth; x++) {
            for (let y = 0; y < -dy; y++) {
                mapdataClear(x, y);
            }
        }
    }

    // Remove all big faces outside the view area.
    for (const bc of activeBigfaces) {
        expandClearBigfaceFromLayer(bc.x, bc.y, bc.layer, false);
    }

    run_move_to();
}

/** Clear the map for a new map command. */
export function mapdata_newmap(): void {
    script_pos.x = 0;
    script_pos.y = 0;

    globalOffsetX = 0;
    globalOffsetY = 0;
    wantOffsetX = 0;
    wantOffsetY = 0;

    for (let x = 0; x < mapWidth; x++) {
        clearCells(x, 0, mapHeight);
        for (let y = 0; y < mapHeight; y++) {
            cells[x][y].needUpdate = true;
        }
    }

    // Clear bigfaces.
    for (const bc of activeBigfaces) {
        expandClearBigfaceFromLayer(bc.x, bc.y, bc.layer, false);
    }

    clear_move_to();
}

/** Tick all map animations. */
export function mapdata_animation(): void {
    // Update synchronized animations.
    for (let a = 0; a < Math.min(animations.length, MAXANIM); a++) {
        if (animations[a] && animations[a].speed) {
            animations[a].speedLeft++;
            if (animations[a].speedLeft >= animations[a].speed) {
                animations[a].speedLeft = 0;
                animations[a].phase++;
                if (animations[a].phase >= animations[a].numAnimations) {
                    animations[a].phase = 0;
                }
            }
        }
    }

    const maxX = Math.min(viewWidth, CURRENT_MAX_VIEW);
    const maxY = Math.min(viewHeight, CURRENT_MAX_VIEW);

    for (let x = 0; x < maxX; x++) {
        for (let y = 0; y < maxY; y++) {
            const mapSpace = cells[pl_pos.x + x][pl_pos.y + y];

            if (mapSpace.state !== MapCellState.Visible) {
                continue;
            }

            for (let layer = 0; layer < MAXLAYERS; layer++) {
                // Animate cell heads.
                const cell = mapSpace.heads[layer];
                if (cell.animation) {
                    cell.animationLeft++;
                    if (cell.animationLeft >= cell.animationSpeed) {
                        cell.animationLeft = 0;
                        cell.animationPhase++;
                        if (cell.animationPhase >= (animations[cell.animation]?.numAnimations ?? 0)) {
                            cell.animationPhase = 0;
                        }
                        const face = animations[cell.animation]?.faces[cell.animationPhase] ?? 0;
                        if (face > 0) {
                            expandSetFace(pl_pos.x + x, pl_pos.y + y, layer, face, false);
                        } else {
                            expandClearFaceFromLayer(pl_pos.x + x, pl_pos.y + y, layer);
                        }
                    }
                }

                // Animate bigface heads.
                const bfCell = bigfaces[x]?.[y]?.[layer]?.head;
                if (bfCell && bfCell.animation) {
                    bfCell.animationLeft++;
                    if (bfCell.animationLeft >= bfCell.animationSpeed) {
                        bfCell.animationLeft = 0;
                        bfCell.animationPhase++;
                        if (bfCell.animationPhase >= (animations[bfCell.animation]?.numAnimations ?? 0)) {
                            bfCell.animationPhase = 0;
                        }
                        const face = animations[bfCell.animation]?.faces[bfCell.animationPhase] ?? 0;
                        expandSetBigface(x, y, layer, face, false);
                    }
                }
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Player position and movement
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Compute the actual player position (centre of the view) in absolute map
 * coordinates.
 */
export function pl_mpos(): { px: number; py: number } {
    const vw = useConfig[CONFIG_MAPWIDTH] || viewWidth;
    const vh = useConfig[CONFIG_MAPHEIGHT] || viewHeight;
    return {
        px: pl_pos.x + Math.floor(vw / 2),
        py: pl_pos.y + Math.floor(vh / 2),
    };
}

/** Set the move-to destination.  `dx` and `dy` are relative to the player. */
export function set_move_to(dx: number, dy: number): void {
    const oldX = moveToX;
    const oldY = moveToY;

    const pos = pl_mpos();
    moveToX = pos.px + dx;
    moveToY = pos.py + dy;

    // Detect double-click on the same destination.
    if (moveToX === oldX && moveToY === oldY) {
        moveToAttack = true;
    } else {
        moveToAttack = false;
    }
}

/** Clear the current move-to destination. */
export function clear_move_to(): void {
    moveToX = 0;
    moveToY = 0;
    moveToAttack = false;
}

/** Return true if the player is at (or has no) move-to destination. */
export function is_at_moveto(): boolean {
    if (moveToX === 0 && moveToY === 0) {
        return true;
    }
    const pos = pl_mpos();
    return pos.px === moveToX && pos.py === moveToY;
}

/** Take one step towards the move-to destination. */
export function run_move_to(): void {
    if (moveToX === 0 && moveToY === 0) {
        return;
    }

    if (is_at_moveto()) {
        clear_move_to();
        stopRunFn();
        return;
    }

    const pos = pl_mpos();
    const dx = moveToX - pos.px;
    const dy = moveToY - pos.py;
    const dir = relative_direction(dx, dy);

    if (moveToAttack) {
        runDirFn(dir);
    } else {
        walkDirFn(dir);
    }
}

/**
 * Compute the direction (0-8) from a relative dx/dy offset.
 *
 * Returns:
 *   0 = standing still,
 *   1 = north, 2 = northeast, 3 = east, 4 = southeast,
 *   5 = south, 6 = southwest, 7 = west, 8 = northwest
 */
export function relative_direction(dx: number, dy: number): number {
    if (dx === 0 && dy === 0) return 0;
    if (dx === 0 && dy < 0) return 1;
    if (dx > 0 && dy < 0) return 2;
    if (dx > 0 && dy === 0) return 3;
    if (dx > 0 && dy > 0) return 4;
    if (dx === 0 && dy > 0) return 5;
    if (dx < 0 && dy > 0) return 6;
    if (dx < 0 && dy === 0) return 7;
    if (dx < 0 && dy < 0) return 8;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Internal helper
// ──────────────────────────────────────────────────────────────────────────────

function mapdataHasTile(x: number, y: number, layer: number): boolean {
    return x >= 0 && x < viewWidth && y >= 0 && y < viewHeight &&
        layer >= 0 && layer < MAXLAYERS;
}

/** Expose the internal player position (top-left of view). */
export function getPlayerPosition(): PlayerPosition {
    return { x: pl_pos.x, y: pl_pos.y };
}

/** Expose the script position. */
export function getScriptPosition(): PlayerPosition {
    return { x: script_pos.x, y: script_pos.y };
}

/** Return the current view dimensions. */
export function getViewSize(): { width: number; height: number } {
    return { width: viewWidth, height: viewHeight };
}
