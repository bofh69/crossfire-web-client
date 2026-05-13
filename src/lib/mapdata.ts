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
  LogLevel,
  FACE_COLOR_MASK,
  FACE_WALL,
} from "./protocol";

import type {
  MapCell,
  MapCellLayer,
  MapCellTailLayer,
  MapLabel,
  PlayerPosition,
} from "./protocol";

import { animations } from "./item";
import { useConfig, wantConfig } from "./init";
import {
  setMoveToCallbacks,
  run_move_to as runMoveToInternal,
  clear_move_to as clearMoveToInternal,
} from "./mapdata_moveto";
import { LOG } from "./misc";
import { notifyWatchedCell } from "./debug";
import {
  cacheSaveFog,
  cacheGetFog,
  type FogSnapshot,
  type FogCacheCell,
} from "./map_fog_cache";
import {
  MAGIC_MAP_FACE_BASE,
  MAGIC_MAP_WALL_FACE_BASE,
  MAGIC_MAP_WALL_ABOVE,
  MAGIC_MAP_WALL_BELOW,
  MAGIC_MAP_WALL_LEFT,
  MAGIC_MAP_WALL_RIGHT,
} from "./image";

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

/** Move-to destination — re-exported from mapdata_moveto.ts. */
export { moveToX, moveToY, moveToAttack } from "./mapdata_moveto";

/** Global map rendering offsets used for local scroll prediction. */
let globalOffsetX = 0;
let globalOffsetY = 0;
let wantOffsetX = 0;
let wantOffsetY = 0;

/** True in development builds where extra bounds assertions should run. */
const DEV_ASSERTIONS = import.meta.env.DEV;

export function getGlobalOffset(): Readonly<{ x: number; y: number }> {
  return { x: globalOffsetX, y: globalOffsetY };
}

export function getWantOffset(): Readonly<{ x: number; y: number }> {
  return { x: wantOffsetX, y: wantOffsetY };
}

// ──────────────────────────────────────────────────────────────────────────────
// Callbacks for external dependencies not yet in the TypeScript codebase
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Return the size of a face in tiles.  The default returns 1×1 for every
 * face.  Override with {@link setGetMapImageSize} when the image cache is
 * available.
 */
let getMapImageSizeFn: (face: number) => { w: number; h: number } = () => ({
  w: 1,
  h: 1,
});

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

export function setGetMapImageSize(
  fn: (face: number) => { w: number; h: number },
): void {
  getMapImageSizeFn = fn;
}
export function setDisplayMapscroll(
  fn: (dx: number, dy: number) => boolean,
): void {
  displayMapscrollFn = fn;
}
export function setStopRun(fn: () => void): void {
  stopRunFn = fn;
  updateMoveToCallbacks();
}
export function setWalkDir(fn: (dir: number) => void): void {
  walkDirFn = fn;
  updateMoveToCallbacks();
}
export function setRunDir(fn: (dir: number) => void): void {
  runDirFn = fn;
  updateMoveToCallbacks();
}

function updateMoveToCallbacks(): void {
  setMoveToCallbacks({
    plMpos: pl_mpos,
    stopRun: stopRunFn,
    walkDir: walkDirFn,
    runDir: runDirFn,
  });
}

// Initialize move-to callbacks once at module load so set_move_to() can use
// the current player position even before movement handlers are injected.
updateMoveToCallbacks();

// ──────────────────────────────────────────────────────────────────────────────
// Array access helpers
//
// cells and bigfaces are always fully initialised before use (see mapdataAlloc
// and initBigfaces).  The non-null assertions below are safe by construction;
// the helpers centralise them so callers stay readable.
// ──────────────────────────────────────────────────────────────────────────────

/** Get the map cell at absolute coordinates. */
function cellAt(x: number, y: number): MapCell {
  if (DEV_ASSERTIONS) {
    if (x < 0 || y < 0 || x >= mapWidth || y >= mapHeight) {
      throw new RangeError(
        `cellAt: (${x},${y}) out of bounds for map ${mapWidth}x${mapHeight}`,
      );
    }
  }
  return cells[x]![y]!;
}

/** Get the BigCell at view coordinates and layer index. */
function bigfaceAt(x: number, y: number, layer: number): BigCell {
  if (DEV_ASSERTIONS) {
    if (
      x < 0 ||
      y < 0 ||
      x >= MAX_VIEW ||
      y >= MAX_VIEW ||
      layer < 0 ||
      layer >= MAXLAYERS
    ) {
      throw new RangeError(
        `bigfaceAt: (${x},${y},${layer}) out of bounds (${MAX_VIEW}x${MAX_VIEW}, layers=${MAXLAYERS})`,
      );
    }
  }
  return bigfaces[x]![y]![layer]!;
}

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
    const h = cell.heads[i]!;
    h.face = 0;
    h.sizeX = 1;
    h.sizeY = 1;
    h.animation = 0;
    h.animationSpeed = 0;
    h.animationLeft = 0;
    h.animationPhase = 0;
    const t = cell.tails[i]!;
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
    resetCell(cellAt(x, y + i));
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
  if (cellAt(x, y).smooth[layer]! > 1) {
    for (let sdx = -1; sdx < 2; sdx++) {
      for (let sdy = -1; sdy < 2; sdy++) {
        if (
          (sdx || sdy) &&
          x + sdx > 0 &&
          x + sdx < mapWidth &&
          y + sdy > 0 &&
          y + sdy < mapHeight
        ) {
          cellAt(x + sdx, y + sdy).needResmooth = true;
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
      cellAt(x - dx, y - dy).needUpdate = true;
    }
  }
}

function expandNeedUpdateFromLayer(x: number, y: number, layer: number): void {
  const head = cellAt(x, y).heads[layer]!;
  if (head.face !== 0) {
    expandNeedUpdate(x, y, head.sizeX, head.sizeY);
  }
}

function expandClearFace(
  x: number,
  y: number,
  w: number,
  h: number,
  layer: number,
): void {
  const cell = cellAt(x, y);
  for (let dx = 0; dx < w; dx++) {
    for (let dy = dx === 0 ? 1 : 0; dy < h; dy++) {
      const tail = cellAt(x - dx, y - dy).tails[layer]!;
      if (
        tail.face === cell.heads[layer]!.face &&
        tail.sizeX === dx &&
        tail.sizeY === dy
      ) {
        tail.face = 0;
        tail.sizeX = 0;
        tail.sizeY = 0;
        cellAt(x - dx, y - dy).needUpdate = true;
      }
      markResmooth(x - dx, y - dy, layer);
    }
  }

  const head = cell.heads[layer]!;
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
  const head = cellAt(x, y).heads[layer]!;
  if (head.face !== 0 && head.sizeX > 0 && head.sizeY > 0) {
    expandClearFace(x, y, head.sizeX, head.sizeY, layer);
  }
}

/**
 * Set a face in cells[][].  If `clear` is true, clear the old face first.
 * Animation updates pass `clear=false` because animations are always the same
 * size and we don't want to clobber animation metadata.
 */
function expandSetFace(
  x: number,
  y: number,
  layer: number,
  face: number,
  clear: boolean,
): void {
  const cell = cellAt(x, y);

  if (clear) {
    expandClearFaceFromLayer(x, y, layer);
  }

  const { w, h } = getImageSize(face);
  cell.heads[layer]!.face = face;
  cell.heads[layer]!.sizeX = w;
  cell.heads[layer]!.sizeY = h;
  cell.needUpdate = true;
  markResmooth(x, y, layer);
  notifyWatchedCell(x, y, `layer ${layer} face=${face} size=${w}x${h}`);

  for (let dx = 0; dx < w; dx++) {
    for (let dy = dx === 0 ? 1 : 0; dy < h; dy++) {
      const tail = cellAt(x - dx, y - dy).tails[layer]!;
      tail.face = face;
      tail.sizeX = dx;
      tail.sizeY = dy;
      cellAt(x - dx, y - dy).needUpdate = true;
      markResmooth(x - dx, y - dy, layer);
      notifyWatchedCell(
        x - dx,
        y - dy,
        `layer ${layer} tail covered by face=${face} (head at +${dx},+${dy})`,
      );
    }
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// Big face expansion (outside the view area – bigfaces[])
// ──────────────────────────────────────────────────────────────────────────────

function expandClearBigface(
  x: number,
  y: number,
  w: number,
  h: number,
  layer: number,
  setNeedUpdate: boolean,
): void {
  const head = bigfaceAt(x, y, layer).head;

  for (let dx = 0; dx < w && dx <= x; dx++) {
    for (let dy = dx === 0 ? 1 : 0; dy < h && dy <= y; dy++) {
      const tail = bigfaceAt(x - dx, y - dy, layer).tail;
      if (tail.face === head.face && tail.sizeX === dx && tail.sizeY === dy) {
        tail.face = 0;
        tail.sizeX = 0;
        tail.sizeY = 0;

        if (
          x - dx >= 0 &&
          x - dx < viewWidth &&
          y - dy >= 0 &&
          y - dy < viewHeight
        ) {
          if (setNeedUpdate) {
            cellAt(pl_pos.x + x - dx, pl_pos.y + y - dy).needUpdate = true;
          }
        }
      }
    }
  }

  head.face = 0;
  head.sizeX = 1;
  head.sizeY = 1;
}

function expandClearBigfaceFromLayer(
  x: number,
  y: number,
  layer: number,
  setNeedUpdate: boolean,
): void {
  const headcell = bigfaceAt(x, y, layer);
  const head = headcell.head;
  if (head.face !== 0) {
    activeBigfaces.delete(headcell);
    expandClearBigface(x, y, head.sizeX, head.sizeY, layer, setNeedUpdate);
  }
}

function expandSetBigface(
  x: number,
  y: number,
  layer: number,
  face: number,
  clear: boolean,
): void {
  const headcell = bigfaceAt(x, y, layer);
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
  notifyWatchedCell(
    pl_pos.x + x,
    pl_pos.y + y,
    face === 0
      ? `layer ${layer} bigface cleared`
      : `layer ${layer} bigface head face=${face} size=${w}x${h}`,
  );

  for (let dx = 0; dx < w && dx <= x; dx++) {
    for (let dy = dx === 0 ? 1 : 0; dy < h && dy <= y; dy++) {
      const tail = bigfaceAt(x - dx, y - dy, layer).tail;
      tail.face = face;
      tail.sizeX = dx;
      tail.sizeY = dy;
      notifyWatchedCell(
        pl_pos.x + x - dx,
        pl_pos.y + y - dy,
        `layer ${layer} bigface tail covered by face=${face} (head at +${dx},+${dy})`,
      );

      if (
        x - dx >= 0 &&
        x - dx < viewWidth &&
        y - dy >= 0 &&
        y - dy < viewHeight
      ) {
        cellAt(pl_pos.x + x - dx, pl_pos.y + y - dy).needUpdate = true;
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
      bigfaces[x]![y] = [];
      for (let i = 0; i < MAXLAYERS; i++) {
        bigfaces[x]![y]![i] = {
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

  const cell = cellAt(px, py);
  if (cell.state === MapCellState.Empty) {
    return;
  }

  if (cell.state === MapCellState.Visible) {
    cell.needUpdate = true;
    for (let i = 0; i < MAXLAYERS; i++) {
      if (cell.heads[i]!.face) {
        expandNeedUpdateFromLayer(px, py, i);
      }
    }
  }

  cell.state = MapCellState.Fog;
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
  if (
    shiftX <= -mapWidth ||
    shiftX >= mapWidth ||
    shiftY <= -mapHeight ||
    shiftY >= mapHeight
  ) {
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
        copyCellData(cellAt(sx, srcY + j), cellAt(dx, dstY + j));
      }
    }
  } else if (shiftX > 0) {
    for (let i = lenX - 1; i >= 0; i--) {
      const sx = srcX + i;
      const dx = dstX + i;
      for (let j = 0; j < lenY; j++) {
        copyCellData(cellAt(sx, srcY + j), cellAt(dx, dstY + j));
      }
    }
  } else {
    // shiftX === 0 but shiftY !== 0
    for (let i = 0; i < lenX; i++) {
      const col = dstX + i;
      if (shiftY < 0) {
        for (let j = 0; j < lenY; j++) {
          copyCellData(cellAt(col, srcY + j), cellAt(col, dstY + j));
        }
      } else {
        for (let j = lenY - 1; j >= 0; j--) {
          copyCellData(cellAt(col, srcY + j), cellAt(col, dstY + j));
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
    Object.assign(dst.heads[i]!, src.heads[i]!);
    Object.assign(dst.tails[i]!, src.tails[i]!);
    dst.smooth[i] = src.smooth[i]!;
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
  return cellAt(x, y);
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
export function mapdata_can_smooth(
  x: number,
  y: number,
  layer: number,
): boolean {
  return (
    (cellAt(x, y).heads[layer]!.face === 0 && layer > 0) ||
    cellAt(x, y).smooth[layer]! !== 0
  );
}

/**
 * Initialise / reinitialise the map module.  Call this before any other
 * function, and whenever a new display size is negotiated with the server.
 */
export function mapdata_set_size(viewx: number, viewy: number): void {
  if (mapWidth === 0) {
    // First call: allocate the virtual map from scratch.
    mapdataInit();
  } else if (viewWidth > 0 && viewHeight > 0) {
    // Adjust pl_pos so the player centre stays at the same absolute
    // position when the viewport dimensions change.
    pl_pos.x += Math.floor(viewWidth / 2) - Math.floor(viewx / 2);
    pl_pos.y += Math.floor(viewHeight / 2) - Math.floor(viewy / 2);
  }
  viewWidth = viewx;
  viewHeight = viewy;
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
  return cellAt(pl_pos.x + x, pl_pos.y + y).heads[layer]!.face;
}

/**
 * Return the face at absolute map coordinates and set dx/dy offsets for
 * drawing.  Returns 0 if nothing to draw.
 *
 * dx/dy are in tile units relative to the current tile's view position.
 * The caller should compute the draw origin as:
 *   drawX = (vx + dx) * tileSize + tileSize - imageWidth
 *   drawY = (vy + dy) * tileSize + tileSize - imageHeight
 * This bottom-right-aligns the image to the head tile regardless of whether
 * the current tile is the head or a tail.
 *
 * Falls back to bigfaces[] when the face data lives outside the server
 * viewport (bigface head is at view coordinates ≥ viewWidth/viewHeight).
 */
export function mapdata_face_info(
  mx: number,
  my: number,
  layer: number,
): { face: number; dx: number; dy: number } {
  const headInfo = mapdata_head_face_info(mx, my, layer);
  if (headInfo.face !== 0) {
    return headInfo;
  }
  return mapdata_tail_face_info(mx, my, layer);
}

/** Return head-face draw info at absolute map coordinates. */
export function mapdata_head_face_info(
  mx: number,
  my: number,
  layer: number,
): { face: number; dx: number; dy: number } {
  const head = cellAt(mx, my).heads[layer]!;

  if (head.face !== 0) {
    // dx/dy = 0: the head tile IS the head; no shift needed.
    return {
      face: head.face,
      dx: 0,
      dy: 0,
    };
  }

  // Fallback: check bigfaces[] for tiles whose bigface head is outside the
  // server viewport.  expandSetBigface() stores head/tail data only in
  // bigfaces[], not in cells[], so the branches above return nothing for
  // in-viewport tail tiles that belong to an out-of-viewport bigface head.
  const viewX = mx - pl_pos.x;
  const viewY = my - pl_pos.y;
  if (viewX >= 0 && viewX < MAX_VIEW && viewY >= 0 && viewY < MAX_VIEW) {
    // Case A: this tile is the bigface HEAD (outside the server viewport
    // but still within the extended-fog canvas).
    const bigHead = bigfaceAt(viewX, viewY, layer).head;
    if (bigHead.face !== 0) {
      // dx/dy = 0: this tile is the head itself.
      return {
        face: bigHead.face,
        dx: 0,
        dy: 0,
      };
    }
  }

  return { face: 0, dx: 0, dy: 0 };
}

/** Return tail-face draw info at absolute map coordinates. */
export function mapdata_tail_face_info(
  mx: number,
  my: number,
  layer: number,
): { face: number; dx: number; dy: number } {
  const tail = cellAt(mx, my).tails[layer]!;

  if (tail.face !== 0) {
    const hx = mx + tail.sizeX;
    const hy = my + tail.sizeY;
    if (!mapdata_contains(hx, hy)) {
      // Head cell is outside the virtual map — skip to avoid an OOB access.
      return { face: 0, dx: 0, dy: 0 };
    }
    // dx/dy = tail offset: adding these to the current view position gives
    // the head tile's view position, which the renderer needs to bottom-right-
    // align the image.
    return {
      face: tail.face,
      dx: tail.sizeX,
      dy: tail.sizeY,
    };
  }

  // Fallback: check bigfaces[] for tiles whose bigface head is outside the
  // server viewport.
  const viewX = mx - pl_pos.x;
  const viewY = my - pl_pos.y;
  if (viewX >= 0 && viewX < MAX_VIEW && viewY >= 0 && viewY < MAX_VIEW) {
    // Case B: this tile is a bigface TAIL whose head is outside the
    // server viewport.
    const bigTail = bigfaceAt(viewX, viewY, layer).tail;
    if (bigTail.face !== 0) {
      const hdx = bigTail.sizeX;
      const hdy = bigTail.sizeY;
      const headViewX = viewX + hdx;
      const headViewY = viewY + hdy;
      if (
        headViewX >= 0 &&
        headViewX < MAX_VIEW &&
        headViewY >= 0 &&
        headViewY < MAX_VIEW
      ) {
        // dx/dy = offset to the head tile so the renderer can bottom-right-
        // align the image to the head tile's corner.
        return {
          face: bigTail.face,
          dx: hdx,
          dy: hdy,
        };
      }
    }
  }

  return { face: 0, dx: 0, dy: 0 };
}

/**
 * Return the big-face ("tail") information at a view position.  Detects
 * and clears obsolete fog-of-war big faces.
 */
export function mapdata_bigface(
  x: number,
  y: number,
  layer: number,
): { face: number; ww: number; hh: number } {
  if (!mapdataHasTile(x, y, layer)) {
    return { face: 0, ww: 1, hh: 1 };
  }

  const px = pl_pos.x + x;
  const py = pl_pos.y + y;
  let result = cellAt(px, py).tails[layer]!.face;

  if (result !== 0) {
    const dx = cellAt(px, py).tails[layer]!.sizeX;
    const dy = cellAt(px, py).tails[layer]!.sizeY;
    const w = cellAt(px + dx, py + dy).heads[layer]!.sizeX;
    const h = cellAt(px + dx, py + dy).heads[layer]!.sizeY;

    let clearBigface: boolean;
    if (cellAt(px, py).state === MapCellState.Fog) {
      clearBigface = false;
    } else if (x + dx < viewWidth && y + dy < viewHeight) {
      clearBigface = cellAt(px + dx, py + dy).state === MapCellState.Fog;
    } else {
      clearBigface = bigfaceAt(x + dx, y + dy, layer).head.face === 0;
    }

    if (!clearBigface) {
      return { face: result, ww: w - 1 - dx, hh: h - 1 - dy };
    }

    expandClearFaceFromLayer(px + dx, py + dy, layer);
  }

  result = bigfaceAt(x, y, layer).tail.face;
  if (result !== 0) {
    const dx = bigfaceAt(x, y, layer).tail.sizeX;
    const dy = bigfaceAt(x, y, layer).tail.sizeY;
    const w = bigfaceAt(x + dx, y + dy, layer).head.sizeX;
    const h = bigfaceAt(x + dx, y + dy, layer).head.sizeY;
    return { face: result, ww: w - 1 - dx, hh: h - 1 - dy };
  }

  return { face: 0, ww: 1, hh: 1 };
}

/** Return the big-face head at a view position (used by OpenGL-style renderers). */
export function mapdata_bigface_head(
  x: number,
  y: number,
  layer: number,
): { face: number; ww: number; hh: number } {
  if (!mapdataHasTile(x, y, layer)) {
    return { face: 0, ww: 1, hh: 1 };
  }

  const result = bigfaceAt(x, y, layer).head.face;
  if (result !== 0) {
    return {
      face: result,
      ww: bigfaceAt(x, y, layer).head.sizeX,
      hh: bigfaceAt(x, y, layer).head.sizeY,
    };
  }
  return { face: 0, ww: 1, hh: 1 };
}

/** Clear a map space (called from Map2Cmd). View-relative coordinates. */
export function mapdata_clear_space(x: number, y: number): void {
  if (x < viewWidth && y < viewHeight) {
    notifyWatchedCell(
      pl_pos.x + x,
      pl_pos.y + y,
      "space cleared (transitioning to fog)",
    );
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
  const cell = cellAt(px, py);
  for (let i = 0; i < MAXLAYERS; i++) {
    if (cell.heads[i]!.face > 0 || cell.tails[i]!.face > 0) {
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
export function mapdata_set_darkness(
  x: number,
  y: number,
  darkness: number,
): void {
  const px = pl_pos.x + x;
  const py = pl_pos.y + y;

  if (darkness !== -1 && x < viewWidth && y < viewHeight) {
    notifyWatchedCell(px, py, `darkness=${255 - darkness}`);
    setDarkness(px, py, 255 - darkness);
  }
}

function setDarkness(x: number, y: number, darkness: number): void {
  if (cellAt(x, y).darkness === darkness) {
    return;
  }
  cellAt(x, y).darkness = darkness;
  cellAt(x, y).needUpdate = true;
}

/** Set smooth value for a layer.  View-relative coordinates. */
export function mapdata_set_smooth(
  x: number,
  y: number,
  smooth: number,
  layer: number,
): void {
  const DX = [0, 1, 1, 1, 0, -1, -1, -1];
  const DY = [-1, -1, 0, 1, 1, 1, 0, -1];

  const px = pl_pos.x + x;
  const py = pl_pos.y + y;

  if (cellAt(px, py).smooth[layer]! !== smooth) {
    for (let i = 0; i < 8; i++) {
      const rx = px + DX[i]!;
      const ry = py + DY[i]!;
      if (rx < 0 || ry < 0 || mapWidth <= rx || mapHeight <= ry) {
        continue;
      }
      cellAt(rx, ry).needResmooth = true;
    }
    cellAt(px, py).needResmooth = true;
    cellAt(px, py).smooth[layer] = smooth;
  }
}

/** Clear all labels at absolute map coordinates. */
export function mapdata_clear_label(px: number, py: number): void {
  cellAt(px, py).labels = [];
}

/** Clear all labels at view-relative coordinates. */
export function mapdata_clear_label_view(x: number, y: number): void {
  if (!(x < viewWidth && y < viewHeight)) {
    return;
  }
  const px = pl_pos.x + x;
  const py = pl_pos.y + y;
  mapdata_clear_label(px, py);
}

/** Add a label at view-relative coordinates. */
export function mapdata_add_label(
  x: number,
  y: number,
  subtype: number,
  label: string,
): void {
  if (!(x < viewWidth && y < viewHeight)) {
    return;
  }

  const px = pl_pos.x + x;
  const py = pl_pos.y + y;

  if (subtype === 0) {
    notifyWatchedCell(px, py, "labels cleared");
    mapdata_clear_label(px, py);
    return;
  }

  cellAt(px, py).labels.push({ subtype, label });
  cellAt(px, py).needUpdate = true;
  notifyWatchedCell(px, py, `label added: subtype=${subtype} "${label}"`);
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

  if (cellAt(px, py).state === MapCellState.Fog) {
    cellAt(px, py).needUpdate = true;
    for (let i = 0; i < MAXLAYERS; i++) {
      expandClearFaceFromLayer(px, py, i);
    }
    cellAt(px, py).darkness = 0;
  }

  cellAt(px, py).state = MapCellState.Visible;
  notifyWatchedCell(
    px,
    py,
    "server update start (cell entering visible state)",
  );
}

/** Set a face on a specific layer.  View-relative coordinates. */
export function mapdata_set_face_layer(
  x: number,
  y: number,
  face: number,
  layer: number,
): void {
  const px = pl_pos.x + x;
  const py = pl_pos.y + y;

  if (x < viewWidth && y < viewHeight) {
    cellAt(px, py).needUpdate = true;
    if (face > 0) {
      expandSetFace(px, py, layer, face, true);
    } else {
      notifyWatchedCell(px, py, `layer ${layer} face cleared`);
      expandClearFaceFromLayer(px, py, layer);
    }
  } else {
    expandSetBigface(x, y, layer, face, true);
  }
}

/** Set an animation on a specific layer.  View-relative coordinates. */
export function mapdata_set_anim_layer(
  x: number,
  y: number,
  anim: number,
  animSpeed: number,
  layer: number,
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
      LOG(
        LogLevel.Warning,
        "mapdata",
        "mapdata_set_anim_layer: animating object with zero animations",
      );
      return;
    }
    phase = Math.floor(Math.random() * numAnim);
    face = animations[animation]!.faces[phase]!;
    speedLeft = Math.floor(Math.random() * animSpeed);
  } else if ((anim & ANIM_FLAGS_MASK) === ANIM_SYNC) {
    if (animations[animation]) {
      animations[animation]!.speed = animSpeed;
      phase = animations[animation]!.phase;
      speedLeft = animations[animation]!.speedLeft;
      face = animations[animation]!.faces[phase]!;
    }
  }

  if (x < viewWidth && y < viewHeight) {
    mapdata_clear_old(x, y);
    if (face > 0) {
      expandSetFace(px, py, layer, face, true);
      cellAt(px, py).heads[layer]!.animation = animation;
      cellAt(px, py).heads[layer]!.animationPhase = phase;
      cellAt(px, py).heads[layer]!.animationSpeed = animSpeed;
      cellAt(px, py).heads[layer]!.animationLeft = speedLeft;
      notifyWatchedCell(
        px,
        py,
        `layer ${layer} animation=${animation} animSpeed=${animSpeed} phase=${phase} face=${face}`,
      );
    } else {
      notifyWatchedCell(px, py, `layer ${layer} animation face cleared`);
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

  if (wantConfig.mapscroll && displayMapscrollFn(dx, dy)) {
    // Mark tiles overlapped by big faces from outside the view area.
    for (const bc of activeBigfaces) {
      for (let bx = 0; bx < bc.head.sizeX; bx++) {
        for (let by = bx === 0 ? 1 : 0; by < bc.head.sizeY; by++) {
          if (
            bc.x - bx >= 0 &&
            bc.x - bx < viewWidth &&
            bc.y - by >= 0 &&
            bc.y - by < viewHeight
          ) {
            cellAt(pl_pos.x + bc.x - bx, pl_pos.y + bc.y - by).needUpdate =
              true;
          }
        }
      }
    }
  } else {
    for (let x = 0; x < viewWidth; x++) {
      for (let y = 0; y < viewHeight; y++) {
        cellAt(pl_pos.x + x, pl_pos.y + y).needUpdate = true;
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

  runMoveToInternal();
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
      cellAt(x, y).needUpdate = true;
    }
  }

  // Clear bigfaces.
  for (const bc of activeBigfaces) {
    expandClearBigfaceFromLayer(bc.x, bc.y, bc.layer, false);
  }

  clearMoveToInternal();
}

/**
 * Snapshot all fog and visible cells in the virtual map and hand the result
 * to the fog cache under `key`.
 *
 * Coordinates are stored map-relative:
 *   map_x = abs_x − originX  (= view_x + script_pos.x)
 *   map_y = abs_y − originY
 *
 * where `originX = pl_pos.x − script_pos.x` is the scroll-invariant virtual-map
 * origin for this visit (the value pl_pos had when the player first entered the
 * map).  Storing the origin in the snapshot lets the restore logic correctly
 * compute the baseline alignment offset even when the player has scrolled many
 * tiles before leaving.
 */
export function mapdata_save_fog(key: string): void {
  // Skip if the current map path is not yet known (e.g. first login before
  // the server's mapinfo response has been processed).  Nothing useful to cache.
  if (!key) {
    return;
  }

  // Scroll-invariant origin for this map visit.
  const originX = pl_pos.x - script_pos.x;
  const originY = pl_pos.y - script_pos.y;

  const cells_to_save: FogCacheCell[] = [];

  for (let ax = 0; ax < mapWidth; ax++) {
    for (let ay = 0; ay < mapHeight; ay++) {
      const cell = cellAt(ax, ay);
      if (cell.state === MapCellState.Empty) {
        continue;
      }

      // Check if the cell has any drawable content worth caching.
      let hasContent = cell.darkness !== 0;
      if (!hasContent) {
        for (let i = 0; i < MAXLAYERS; i++) {
          if (cell.heads[i]!.face !== 0 || cell.tails[i]!.face !== 0) {
            hasContent = true;
            break;
          }
        }
      }
      if (!hasContent) {
        continue;
      }

      const map_x = ax - originX;
      const map_y = ay - originY;

      // MapCellLayer and MapCellTailLayer contain only number primitives,
      // so the spread operator produces a fully independent copy.
      const heads = cell.heads.map((h) => ({ ...h }));
      const tails = cell.tails.map((t) => ({ ...t }));
      const smooth = cell.smooth.slice();
      const labels = cell.labels.map((l) => ({ ...l }));

      cells_to_save.push({
        x: map_x,
        y: map_y,
        heads,
        tails,
        smooth,
        darkness: cell.darkness,
        labels,
      });
    }
  }

  cacheSaveFog(key, { meta: { originX, originY }, cells: cells_to_save });
  LOG(
    LogLevel.Info,
    "mapdata",
    `Saved ${cells_to_save.length} fog cells for map "${key}"`,
  );
}

// ──────────────────────────────────────────────────────────────────────────────
// Fog-of-war restoration: alignment search
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Search radius (tiles) used when aligning a fog snapshot with the current
 * visible map data.  The player's entry position can differ from the last
 * visit by up to this many tiles in each axis.
 */
const FOG_ALIGN_SEARCH_RADIUS = 5;

/**
 * Minimum number of face-matched cells required to accept an alignment offset.
 * Below this the snap is considered unrelated to the player's current location.
 */
const FOG_ALIGN_MIN_CELLS = 5;

/**
 * Minimum fraction of visible-with-content cells that must correlate with the
 * snapshot (at the best offset) before the fog restore is accepted.
 */
const FOG_ALIGN_MIN_RATIO = 0.75;

/**
 * Number of layers (starting from layer 0) included in the correlation score.
 * Layer 0 typically holds floor tiles; layer 1 holds walls, doors, etc.  Using
 * both avoids over-counting maps whose floor is a single uniform tile.
 */
const FOG_ALIGN_LAYERS = 2;

/**
 * Find the (dx, dy) offset near the expected baseline that best aligns
 * `snapshot` cells with the currently-visible map cells.
 *
 * The baseline is derived from the two scroll-invariant origins:
 *   baseDx = snapshotOriginX − restoreOriginX
 *   baseDy = snapshotOriginY − restoreOriginY
 *
 * The search covers [baseDx − R, baseDx + R] × [baseDy − R, baseDy + R],
 * so the player may have entered up to R tiles away from where they left
 * (in each axis) and the restore still succeeds.
 *
 * Visible cells are referenced by their scroll-stable coordinate
 *   vx_adj = ax − restoreOriginX  (= view_x + script_pos.x)
 * which matches the saved coordinate system regardless of how many
 * map_scroll packets have arrived before this function is called.
 *
 * The correlation metric is the total number of (cell, layer) face matches
 * across layers 0..FOG_ALIGN_LAYERS-1.  Using multiple layers avoids
 * under-discriminating maps where layer 0 is a single uniform floor tile.
 *
 * Returns `null` if the best score is below the minimum threshold (the player
 * has likely entered the map at an unrelated location and the snapshot should
 * not be used).
 */
function findFogRestoreOffset(
  snapshot: FogSnapshot,
  restoreOriginX: number,
  restoreOriginY: number,
): { dx: number; dy: number } | null {
  // Build a lookup from packed (x, y, layer) → face for layers 0..FOG_ALIGN_LAYERS-1.
  // Coordinate range is bounded by the virtual map size (FOG_MAP_SIZE = 512).
  // Pack as ((x + 1024) * 4096 + (y + 1024)) * MAXLAYERS + layer to get
  // unique non-negative keys.
  const snapFaces = new Map<number, number>();
  for (const cell of snapshot.cells) {
    for (let layer = 0; layer < FOG_ALIGN_LAYERS; layer++) {
      const f = cell.heads[layer]?.face ?? 0;
      if (f !== 0) {
        snapFaces.set(
          ((cell.x + 1024) * 4096 + (cell.y + 1024)) * MAXLAYERS + layer,
          f,
        );
      }
    }
  }

  // Collect scroll-stable coords and per-layer faces for all currently Visible
  // cells that have at least one non-zero face in the tracked layers.
  // vx_adj = ax − restoreOriginX matches the save-time coordinate system.
  const visibleCells: Array<{
    vx: number;
    vy: number;
    faces: (number | undefined)[];
  }> = [];
  for (let ax = 0; ax < mapWidth; ax++) {
    for (let ay = 0; ay < mapHeight; ay++) {
      const cell = cellAt(ax, ay);
      if (cell.state !== MapCellState.Visible) continue;
      const faces: (number | undefined)[] = [];
      let hasContent = false;
      for (let layer = 0; layer < FOG_ALIGN_LAYERS; layer++) {
        const f = cell.heads[layer]?.face ?? 0;
        faces.push(f !== 0 ? f : undefined);
        if (f !== 0) hasContent = true;
      }
      if (!hasContent) continue;
      visibleCells.push({
        vx: ax - restoreOriginX,
        vy: ay - restoreOriginY,
        faces,
      });
    }
  }

  if (visibleCells.length === 0 || snapFaces.size === 0) {
    // No data available to determine alignment — skip restore.
    LOG(
      LogLevel.Info,
      "mapdata",
      "Fog alignment: no correlation data; skipping restore",
    );
    return null;
  }

  // The expected offset between the two visits' coordinate systems.
  // Search ±R around this baseline so small entry-point differences are tolerated.
  const baseDx = snapshot.meta.originX - restoreOriginX;
  const baseDy = snapshot.meta.originY - restoreOriginY;

  let bestScore = 0;
  let bestDx = baseDx;
  let bestDy = baseDy;
  const R = FOG_ALIGN_SEARCH_RADIUS;

  for (let odx = baseDx - R; odx <= baseDx + R; odx++) {
    for (let ody = baseDy - R; ody <= baseDy + R; ody++) {
      let score = 0;
      for (const { vx, vy, faces } of visibleCells) {
        // At this offset, the snap coordinate that should underlie this
        // visible cell is (vx − odx, vy − ody).
        const sx = vx - odx;
        const sy = vy - ody;
        const base = ((sx + 1024) * 4096 + (sy + 1024)) * MAXLAYERS;
        for (let layer = 0; layer < FOG_ALIGN_LAYERS; layer++) {
          const f = faces[layer];
          if (f !== undefined && snapFaces.get(base + layer) === f) {
            score++;
          }
        }
      }
      // Prefer higher score; break ties in favour of the offset closest
      // to the baseline — i.e. the player re-entered near the same spot
      // they left, which is more likely than a large displacement.
      const dist = Math.abs(odx - baseDx) + Math.abs(ody - baseDy);
      const bestDist = Math.abs(bestDx - baseDx) + Math.abs(bestDy - baseDy);
      if (score > bestScore || (score === bestScore && dist < bestDist)) {
        bestScore = score;
        bestDx = odx;
        bestDy = ody;
      }
    }
  }

  const ratio = (bestScore / visibleCells.length) * FOG_ALIGN_LAYERS;
  if (bestScore < FOG_ALIGN_MIN_CELLS || ratio < FOG_ALIGN_MIN_RATIO) {
    LOG(
      LogLevel.Info,
      "mapdata",
      `Fog alignment: best score=${bestScore} ratio=${(ratio * 100).toFixed(0)}% ` +
        `— below threshold; skipping restore`,
    );
    return null;
  }

  LOG(
    LogLevel.Info,
    "mapdata",
    `Fog alignment: offset=(${bestDx},${bestDy}) score=${bestScore} ` +
      `ratio=${(ratio * 100).toFixed(0)}%`,
  );
  return { dx: bestDx, dy: bestDy };
}

/**
 * Restore a previously saved fog snapshot for `key` into the virtual map.
 *
 * Must be called after the server's visible-area map2 packets have been
 * processed (so the current Visible cells can be used for alignment).
 *
 * A correlation search over ±FOG_ALIGN_SEARCH_RADIUS tiles around the
 * expected baseline offset finds the best alignment between the snapshot and
 * the current visible data.  If the best correlation is too low the restore
 * is skipped entirely — the player has entered the map at an unrelated
 * location.
 *
 * Each accepted snapshot cell is placed at:
 *   abs_x = restoreOriginX + map_x + dx
 *   abs_y = restoreOriginY + map_y + dy
 * where `restoreOriginX = pl_pos.x − script_pos.x` is the scroll-invariant
 * virtual-map origin for the current visit.
 * only if the target cell is still Empty (not yet written by map2).
 */
export function mapdata_restore_fog(key: string): void {
  const snapshot = cacheGetFog(key);
  if (!snapshot || snapshot.cells.length === 0) {
    return;
  }

  // Scroll-invariant origin for the current map visit.
  const restoreOriginX = pl_pos.x - script_pos.x;
  const restoreOriginY = pl_pos.y - script_pos.y;

  const offset = findFogRestoreOffset(snapshot, restoreOriginX, restoreOriginY);
  if (offset === null) {
    return;
  }
  const { dx: odx, dy: ody } = offset;

  let restored = 0;
  for (const entry of snapshot.cells) {
    const ax = restoreOriginX + entry.x + odx;
    const ay = restoreOriginY + entry.y + ody;

    if (ax < 0 || ay < 0 || ax >= mapWidth || ay >= mapHeight) {
      continue;
    }

    const cell = cellAt(ax, ay);
    if (cell.state !== MapCellState.Empty) {
      // Already written by map2 — don't overwrite live data.
      continue;
    }

    for (let i = 0; i < MAXLAYERS; i++) {
      // MapCellLayer and MapCellTailLayer contain only number primitives;
      // Object.assign produces a fully independent copy.
      Object.assign(cell.heads[i]!, entry.heads[i]!);
      Object.assign(cell.tails[i]!, entry.tails[i]!);
      cell.smooth[i] = entry.smooth[i]!;
    }
    cell.darkness = entry.darkness;
    cell.labels = entry.labels.map((l) => ({ ...l }));
    cell.state = MapCellState.Fog;
    cell.needUpdate = true;
    restored++;
  }

  LOG(
    LogLevel.Info,
    "mapdata",
    `Restored ${restored} fog cells for map "${key}"`,
  );
}

/**
 * Fill empty virtual-map cells with data derived from a server magicmap packet.
 *
 * The magicmap is a `mmapx × mmapy` grid where each byte encodes a tile type
 * (bits 7–4: FACE_WALL / FACE_FLOOR flags; bits 3–0: colour index).  The
 * player's position within the magicmap is `(pmapx, pmapy)`.
 *
 * For each non-void cell (colour ≠ 0) that corresponds to an Empty slot in
 * the virtual map, this function places a client-generated solid-colour face
 * (at layer 0) and marks the cell as Fog so the game map renderer displays
 * it as explored-but-not-currently-visible area.  Cells already in Visible
 * or Fog state (written by map2) are left unchanged.
 *
 * Wall tiles additionally receive a wall-line face on layer 1 chosen from
 * all 16 shapes that encode which of the four cardinal neighbors are also
 * walls (above=bit3, below=bit2, left=bit1, right=bit0).
 *
 * Coordinate mapping:
 *   playerAbsX = pl_pos.x + Math.floor(viewWidth / 2)
 *   ax = playerAbsX + (mx - pmapx)
 */
export function mapdata_apply_magicmap(
  data: Uint8Array,
  mmapx: number,
  mmapy: number,
  pmapx: number,
  pmapy: number,
): void {
  if (mmapx === 0 || mmapy === 0 || data.length < mmapx * mmapy) {
    return;
  }

  /** Return true if the magicmap cell at (mx, my) is a wall. */
  function isWall(mx: number, my: number): boolean {
    if (mx < 0 || my < 0 || mx >= mmapx || my >= mmapy) return false;
    return (data[my * mmapx + mx]! & FACE_WALL) !== 0;
  }

  // Player's centre position on the virtual map.
  const vw = useConfig.mapWidth || viewWidth;
  const vh = useConfig.mapHeight || viewHeight;
  const playerAbsX = pl_pos.x + Math.floor(vw / 2);
  const playerAbsY = pl_pos.y + Math.floor(vh / 2);

  let applied = 0;
  for (let my = 0; my < mmapy; my++) {
    for (let mx = 0; mx < mmapx; mx++) {
      const val = data[my * mmapx + mx]!;
      const colorIdx = val & FACE_COLOR_MASK;
      if (colorIdx === 0) {
        // Index 0 = void/black — leave the cell empty.
        continue;
      }

      const ax = playerAbsX + (mx - pmapx);
      const ay = playerAbsY + (my - pmapy);

      if (ax < 0 || ay < 0 || ax >= mapWidth || ay >= mapHeight) {
        continue;
      }

      const cell = cellAt(ax, ay);
      if (cell.state !== MapCellState.Empty) {
        // Already set by map2 (Visible) or a previous fog restore — leave it.
        continue;
      }

      // Place the synthetic solid-colour face in layer 0.
      const head0 = cell.heads[0]!;
      head0.face = MAGIC_MAP_FACE_BASE | colorIdx;
      head0.sizeX = 1;
      head0.sizeY = 1;

      // Wall tiles: add a wall-line face on layer 1.
      // Build a 4-bit neighbor mask and select the matching wall shape.
      if (val & FACE_WALL) {
        const neighborMask =
          (isWall(mx, my - 1) ? MAGIC_MAP_WALL_ABOVE : 0) |
          (isWall(mx, my + 1) ? MAGIC_MAP_WALL_BELOW : 0) |
          (isWall(mx - 1, my) ? MAGIC_MAP_WALL_LEFT : 0) |
          (isWall(mx + 1, my) ? MAGIC_MAP_WALL_RIGHT : 0);

        const head1 = cell.heads[1]!;
        head1.face = MAGIC_MAP_WALL_FACE_BASE | neighborMask;
        head1.sizeX = 1;
        head1.sizeY = 1;
      }

      cell.state = MapCellState.Fog;
      cell.needUpdate = true;
      applied++;
    }
  }

  LOG(LogLevel.Info, "mapdata", `Applied ${applied} magicmap cells`);
}

export function mapdata_animation(): void {
  // Update synchronized animations.
  for (let a = 0; a < Math.min(animations.length, MAXANIM); a++) {
    const anim = animations[a];
    if (anim && anim.speed) {
      anim.speedLeft++;
      if (anim.speedLeft >= anim.speed) {
        anim.speedLeft = 0;
        anim.phase++;
        if (anim.phase >= anim.numAnimations) {
          anim.phase = 0;
        }
      }
    }
  }

  const maxX = Math.min(viewWidth, CURRENT_MAX_VIEW);
  const maxY = Math.min(viewHeight, CURRENT_MAX_VIEW);

  for (let x = 0; x < maxX; x++) {
    for (let y = 0; y < maxY; y++) {
      const mapSpace = cellAt(pl_pos.x + x, pl_pos.y + y);

      if (mapSpace.state !== MapCellState.Visible) {
        continue;
      }

      for (let layer = 0; layer < MAXLAYERS; layer++) {
        // Animate cell heads.
        const cell = mapSpace.heads[layer]!;
        if (cell.animation) {
          cell.animationLeft++;
          if (cell.animationLeft >= cell.animationSpeed) {
            cell.animationLeft = 0;
            cell.animationPhase++;
            if (
              cell.animationPhase >=
              (animations[cell.animation]?.numAnimations ?? 0)
            ) {
              cell.animationPhase = 0;
            }
            const face =
              animations[cell.animation]?.faces[cell.animationPhase] ?? 0;
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
            if (
              bfCell.animationPhase >=
              (animations[bfCell.animation]?.numAnimations ?? 0)
            ) {
              bfCell.animationPhase = 0;
            }
            const face =
              animations[bfCell.animation]?.faces[bfCell.animationPhase] ?? 0;
            expandSetBigface(x, y, layer, face, false);
          }
        }
      }
    }
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// Player position and movement (delegated to mapdata_moveto.ts)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Compute the actual player position (centre of the view) in absolute map
 * coordinates.
 */
export function pl_mpos(): { px: number; py: number } {
  const vw = useConfig.mapWidth || viewWidth;
  const vh = useConfig.mapHeight || viewHeight;
  return {
    px: pl_pos.x + Math.floor(vw / 2),
    py: pl_pos.y + Math.floor(vh / 2),
  };
}

export {
  set_move_to,
  clear_move_to,
  is_at_moveto,
  run_move_to,
  relative_direction,
} from "./mapdata_moveto";

// ──────────────────────────────────────────────────────────────────────────────
// Internal helper
// ──────────────────────────────────────────────────────────────────────────────

function mapdataHasTile(x: number, y: number, layer: number): boolean {
  return (
    x >= 0 &&
    x < viewWidth &&
    y >= 0 &&
    y < viewHeight &&
    layer >= 0 &&
    layer < MAXLAYERS
  );
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

// ──────────────────────────────────────────────────────────────────────────────
// Debug dump helpers
// ──────────────────────────────────────────────────────────────────────────────

function stateName(s: MapCellState): string {
  switch (s) {
    case MapCellState.Empty:
      return "Empty";
    case MapCellState.Visible:
      return "Visible";
    case MapCellState.Fog:
      return "Fog";
    default:
      return `Unknown(${s})`;
  }
}

function formatLayer(l: MapCellLayer, idx: number): string {
  if (l.face === 0 && l.animation === 0) return "";
  const parts = [`  layer ${idx}: face=${l.face} size=${l.sizeX}x${l.sizeY}`];
  if (l.animation !== 0) {
    parts.push(
      `anim=${l.animation} speed=${l.animationSpeed} left=${l.animationLeft} phase=${l.animationPhase}`,
    );
  }
  return parts.join(" ");
}

function formatTail(t: MapCellTailLayer, idx: number): string {
  if (t.face === 0) return "";
  return `  tail ${idx}: face=${t.face} offset=(${t.sizeX}, ${t.sizeY})`;
}

/**
 * Return a human-readable dump of all cell data at the given absolute map
 * coordinates.  Returns an array of lines.
 */
export function mapdata_debug_tile(ax: number, ay: number): string[] {
  const lines: string[] = [];
  const viewX = ax - pl_pos.x;
  const viewY = ay - pl_pos.y;
  lines.push(
    `Tile at absolute (${ax}, ${ay}), view-relative (${viewX}, ${viewY})`,
  );
  lines.push(
    `  pl_pos=(${pl_pos.x}, ${pl_pos.y}) view=${viewWidth}x${viewHeight}`,
  );

  if (!mapdata_contains(ax, ay)) {
    lines.push("  Outside virtual map bounds.");
    return lines;
  }

  const cell = cellAt(ax, ay);
  lines.push(
    `  state=${stateName(cell.state)} darkness=${cell.darkness} needUpdate=${cell.needUpdate} needResmooth=${cell.needResmooth}`,
  );

  for (let i = 0; i < MAXLAYERS; i++) {
    const hl = formatLayer(cell.heads[i]!, i);
    if (hl) lines.push(hl);
    const tl = formatTail(cell.tails[i]!, i);
    if (tl) lines.push(tl);
    if (cell.smooth[i] !== 0) {
      lines.push(`  smooth ${i}: ${cell.smooth[i]}`);
    }
  }

  if (cell.labels.length > 0) {
    for (const lbl of cell.labels) {
      lines.push(`  label: subtype=${lbl.subtype} "${lbl.label}"`);
    }
  }

  // face_info per layer
  for (let i = 0; i < MAXLAYERS; i++) {
    const info = mapdata_face_info(ax, ay, i);
    if (info.face !== 0) {
      lines.push(
        `  face_info layer ${i}: face=${info.face} dx=${info.dx} dy=${info.dy}`,
      );
    }
  }

  return lines;
}

/**
 * Return a human-readable dump of the player's current position in the
 * virtual map.  Returns an array of lines suitable for logging.
 */
export function mapdata_debug_player_pos(): string[] {
  const lines: string[] = [];
  lines.push(`Player virtual-map position:`);
  lines.push(`  pl_pos=(${pl_pos.x}, ${pl_pos.y})`);
  lines.push(`  script_pos=(${script_pos.x}, ${script_pos.y})`);
  lines.push(`  view=${viewWidth}x${viewHeight}  map=${mapWidth}x${mapHeight}`);
  return lines;
}

/**
 * Return a human-readable dump of all bigface/multitile data at the given
 * absolute map coordinates.  Returns an array of lines.
 */
export function mapdata_debug_bigface(ax: number, ay: number): string[] {
  const lines: string[] = [];
  const viewX = ax - pl_pos.x;
  const viewY = ay - pl_pos.y;
  lines.push(
    `Bigface info at absolute (${ax}, ${ay}), view-relative (${viewX}, ${viewY})`,
  );
  lines.push(
    `  pl_pos=(${pl_pos.x}, ${pl_pos.y}) view=${viewWidth}x${viewHeight}`,
  );

  if (!mapdata_contains(ax, ay)) {
    lines.push("  Outside virtual map bounds.");
    return lines;
  }

  const cell = cellAt(ax, ay);
  lines.push(`  cell state=${stateName(cell.state)} darkness=${cell.darkness}`);

  // Show head and tail info from cells[]
  let hasCellData = false;
  for (let i = 0; i < MAXLAYERS; i++) {
    const h = cell.heads[i]!;
    const t = cell.tails[i]!;
    if (h.face !== 0 && (h.sizeX > 1 || h.sizeY > 1)) {
      lines.push(
        `  cells[] HEAD layer ${i}: face=${h.face} size=${h.sizeX}x${h.sizeY}`,
      );
      hasCellData = true;
    }
    if (t.face !== 0) {
      lines.push(
        `  cells[] TAIL layer ${i}: face=${t.face} offset=(${t.sizeX}, ${t.sizeY})`,
      );
      // Try to describe the head this tail points to
      const hx = ax + t.sizeX;
      const hy = ay + t.sizeY;
      if (mapdata_contains(hx, hy)) {
        const headCell = cellAt(hx, hy);
        const headLayer = headCell.heads[i]!;
        lines.push(
          `    -> head at (${hx}, ${hy}): face=${headLayer.face} size=${headLayer.sizeX}x${headLayer.sizeY} state=${stateName(headCell.state)}`,
        );
      } else {
        lines.push(`    -> head at (${hx}, ${hy}): outside map`);
      }
      hasCellData = true;
    }
  }

  // Show bigfaces[] data
  let hasBigfaceData = false;
  if (viewX >= 0 && viewX < MAX_VIEW && viewY >= 0 && viewY < MAX_VIEW) {
    for (let i = 0; i < MAXLAYERS; i++) {
      const bf = bigfaceAt(viewX, viewY, i);
      if (bf.head.face !== 0) {
        lines.push(
          `  bigfaces[] HEAD layer ${i}: face=${bf.head.face} size=${bf.head.sizeX}x${bf.head.sizeY}`,
        );
        hasBigfaceData = true;
      }
      if (bf.tail.face !== 0) {
        lines.push(
          `  bigfaces[] TAIL layer ${i}: face=${bf.tail.face} offset=(${bf.tail.sizeX}, ${bf.tail.sizeY})`,
        );
        // Try to describe the head this tail points to
        const hdx = bf.tail.sizeX;
        const hdy = bf.tail.sizeY;
        const headViewX = viewX + hdx;
        const headViewY = viewY + hdy;
        if (
          headViewX >= 0 &&
          headViewX < MAX_VIEW &&
          headViewY >= 0 &&
          headViewY < MAX_VIEW
        ) {
          const bigHead = bigfaceAt(headViewX, headViewY, i);
          lines.push(
            `    -> bigface head at view (${headViewX}, ${headViewY}): face=${bigHead.head.face} size=${bigHead.head.sizeX}x${bigHead.head.sizeY}`,
          );
        } else {
          lines.push(
            `    -> bigface head at view (${headViewX}, ${headViewY}): outside MAX_VIEW`,
          );
        }
        hasBigfaceData = true;
      }
    }
  } else {
    lines.push("  view position outside bigfaces[] range (0..MAX_VIEW)");
  }

  // face_info per layer
  for (let i = 0; i < MAXLAYERS; i++) {
    const info = mapdata_face_info(ax, ay, i);
    if (info.face !== 0) {
      lines.push(
        `  face_info layer ${i}: face=${info.face} dx=${info.dx} dy=${info.dy}`,
      );
    }
  }

  if (!hasCellData && !hasBigfaceData) {
    lines.push("  No bigface/multitile data at this position.");
  }

  return lines;
}

/**
 * Return a human-readable dump of all currently active bigface entries.
 * This includes:
 *   - Entries in `activeBigfaces` (multi-tile faces whose head is OUTSIDE
 *     the server viewport on the right/bottom edge, stored in bigfaces[]).
 *   - Multi-tile heads found in cells[] (sizeX > 1 or sizeY > 1) for faces
 *     whose head is within the server viewport or just outside its top/left
 *     edge (up to MAX_FACE_SIZE-1 tiles before the visible area).
 * Returns an array of lines suitable for logging.
 */
export function mapdata_debug_all_bigfaces(): string[] {
  const lines: string[] = [];
  lines.push(
    `  pl_pos=(${pl_pos.x}, ${pl_pos.y}) view=${viewWidth}x${viewHeight}`,
  );

  // --- Section 1: bigfaces stored in cells[] ---
  // Scan from -(MAX_FACE_SIZE-1) to capture heads sitting just outside the
  // top/left viewport edge (negative view-relative coords) as well as those
  // inside the visible area.
  const scanMin = -(MAX_FACE_SIZE - 1);
  const cellsBigfaces: Array<{
    vx: number;
    vy: number;
    layer: number;
    face: number;
    sizeX: number;
    sizeY: number;
  }> = [];
  for (let vx = scanMin; vx < viewWidth; vx++) {
    for (let vy = scanMin; vy < viewHeight; vy++) {
      const ax = pl_pos.x + vx;
      const ay = pl_pos.y + vy;
      if (ax < 0 || ax >= mapWidth || ay < 0 || ay >= mapHeight) {
        continue;
      }
      const cell = cellAt(ax, ay);
      for (let layer = 0; layer < MAXLAYERS; layer++) {
        const h = cell.heads[layer]!;
        if (h.face !== 0 && (h.sizeX > 1 || h.sizeY > 1)) {
          cellsBigfaces.push({
            vx,
            vy,
            layer,
            face: h.face,
            sizeX: h.sizeX,
            sizeY: h.sizeY,
          });
        }
      }
    }
  }

  lines.push(
    `Bigfaces in cells[] (view coords ${scanMin}..${viewWidth - 1}, ${scanMin}..${viewHeight - 1}): ${cellsBigfaces.length} entr${cellsBigfaces.length === 1 ? "y" : "ies"}`,
  );
  if (cellsBigfaces.length === 0) {
    lines.push("  (none)");
  }
  for (let index = 0; index < cellsBigfaces.length; index++) {
    const { vx, vy, layer, face, sizeX, sizeY } = cellsBigfaces[index]!;
    const ax = pl_pos.x + vx;
    const ay = pl_pos.y + vy;
    lines.push(
      `  [${index}] view=(${vx}, ${vy}) abs=(${ax}, ${ay})` +
        ` layer=${layer} face=${face} size=${sizeX}x${sizeY}`,
    );
    for (let dx = 0; dx < sizeX; dx++) {
      for (let dy = dx === 0 ? 1 : 0; dy < sizeY; dy++) {
        const tvx = vx - dx;
        const tvy = vy - dy;
        if (tvx >= 0 && tvx < viewWidth && tvy >= 0 && tvy < viewHeight) {
          lines.push(
            `    tail covers view=(${tvx}, ${tvy}) abs=(${ax - dx}, ${ay - dy}) offset=(${dx}, ${dy})`,
          );
        }
      }
    }
  }

  // --- Section 2: out-of-viewport bigfaces stored in bigfaces[] ---
  lines.push(
    `Out-of-viewport bigfaces (bigfaces[]): ${activeBigfaces.size} entr${activeBigfaces.size === 1 ? "y" : "ies"}`,
  );
  if (activeBigfaces.size === 0) {
    lines.push("  (none)");
  }
  let index = 0;
  for (const bc of activeBigfaces) {
    const ax = pl_pos.x + bc.x;
    const ay = pl_pos.y + bc.y;
    lines.push(
      `  [${index}] view=(${bc.x}, ${bc.y}) abs=(${ax}, ${ay})` +
        ` layer=${bc.layer} face=${bc.head.face} size=${bc.head.sizeX}x${bc.head.sizeY}`,
    );
    // List which in-view cells this bigface covers as tails.
    for (let dx = 0; dx < bc.head.sizeX; dx++) {
      for (let dy = dx === 0 ? 1 : 0; dy < bc.head.sizeY; dy++) {
        const tvx = bc.x - dx;
        const tvy = bc.y - dy;
        if (tvx >= 0 && tvx < viewWidth && tvy >= 0 && tvy < viewHeight) {
          const tax = ax - dx;
          const tay = ay - dy;
          lines.push(
            `    tail covers view=(${tvx}, ${tvy}) abs=(${tax}, ${tay}) offset=(${dx}, ${dy})`,
          );
        }
      }
    }
    index++;
  }

  return lines;
}
