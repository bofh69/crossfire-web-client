/**
 * Map data management for the Crossfire web client.
 * Converted from old/common/mapdata.c and old/common/mapdata.h.
 *
 * Manages a virtual fog-of-war map with a scrollable view area.  Big (multi-
 * tile) faces are tracked both inside the view area (in typed flat arrays)
 * and outside (in the bigfaces array).
 *
 * Memory layout: 13 flat TypedArrays replace the previous per-cell JS-object
 * tree.  At ~153 bytes/cell vs ~1 KiB/cell this cuts static map memory by
 * roughly 6×.  Combined with dynamic map sizing (Phase 2) the working set
 * fits well under 64 MiB.
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

/**
 * Minimum distance (tiles) from the view area to the virtual-map border
 * before the map is recentered.  Reduced from 128 to 64; combined with
 * typed-array storage this still gives ample fog-of-war memory while
 * keeping the map allocation smaller.
 */
const FOG_BORDER_MIN = 64;

/** Maximum size of a big face image in tiles. */
const MAX_FACE_SIZE = 16;

/** Maximum view size used for animation iteration. */
const CURRENT_MAX_VIEW = 33;

/** Minimum virtual-map dimension (tiles) regardless of viewport size. */
const FOG_MAP_MIN_SIZE = 192;

/** Short alias kept in sync with the protocol constant. */
const L = MAXLAYERS;

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

// cellFlags bit encoding
const FLAG_NEED_UPDATE = 0x01;
const FLAG_NEED_RESMOOTH = 0x02;

/** Internal player position on the virtual map (top-left of view). */
let pl_pos: PlayerPosition = { x: 0, y: 0 };

/** Player position reported after newmap resets. */
let script_pos: PlayerPosition = { x: 0, y: 0 };

/** View dimensions (tiles). */
let viewWidth = 0;
let viewHeight = 0;

/** Virtual map dimensions. */
let mapWidth = 0;
let mapHeight = 0;

// ── Scalar per-cell typed arrays ─────────────────────────────────────────────
let cellState = new Uint8Array(0);
let cellDarkness = new Uint8Array(0);
/** bit 0 = needUpdate, bit 1 = needResmooth */
let cellFlags = new Uint8Array(0);

// ── Head layer fields [idx * L + layer] ──────────────────────────────────────
let headFace = new Uint32Array(0);
let headSizeX = new Uint8Array(0);
let headSizeY = new Uint8Array(0);
let headAnim = new Uint16Array(0);
let headAnimSpeed = new Uint8Array(0);
let headAnimLeft = new Uint8Array(0);
let headAnimPhase = new Uint16Array(0);

// ── Tail layer fields [idx * L + layer] ──────────────────────────────────────
let tailFace = new Uint32Array(0);
let tailSizeX = new Uint8Array(0);
let tailSizeY = new Uint8Array(0);

// ── Smooth values [idx * L + layer] ──────────────────────────────────────────
let smooth = new Uint8Array(0);

/** Sparse label storage: keyed by flat cell index. */
let cellLabels = new Map<number, MapLabel[]>();

/** Big-face tracking outside the view area (objects, small fixed size). */
let bigfaces: BigCell[][][] = [];
let activeBigfaces: Set<BigCell> = new Set();

/** Move-to destination – re-exported from mapdata_moveto.ts. */
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
// Flat-index helpers
// ──────────────────────────────────────────────────────────────────────────────

/** Cell index (column-major): ci(x, y) = x * mapHeight + y */
function ci(x: number, y: number): number {
  return x * mapHeight + y;
}

// ──────────────────────────────────────────────────────────────────────────────
// BigCell access helpers (bigfaces[] still uses objects)
// ──────────────────────────────────────────────────────────────────────────────

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
// Internal helpers
// ──────────────────────────────────────────────────────────────────────────────

/** Clamp the image size to the valid range. */
function getImageSize(face: number): { w: number; h: number } {
  let { w, h } = getMapImageSizeFn(face);
  w = Math.max(1, Math.min(w, MAX_FACE_SIZE));
  h = Math.max(1, Math.min(h, MAX_FACE_SIZE));
  return { w, h };
}

/** Clear a contiguous slice of cells in column x, rows y..y+lenY-1. */
function clearCells(x: number, y: number, lenY: number): void {
  const startIdx = ci(x, y);
  const endIdx = startIdx + lenY;

  cellState.fill(MapCellState.Empty, startIdx, endIdx);
  cellDarkness.fill(0, startIdx, endIdx);
  cellFlags.fill(0, startIdx, endIdx);

  for (let i = startIdx; i < endIdx; i++) {
    cellLabels.delete(i);
  }

  const startLi = startIdx * L;
  const endLi = endIdx * L;
  headFace.fill(0, startLi, endLi);
  headSizeX.fill(1, startLi, endLi);
  headSizeY.fill(1, startLi, endLi);
  headAnim.fill(0, startLi, endLi);
  headAnimSpeed.fill(0, startLi, endLi);
  headAnimLeft.fill(0, startLi, endLi);
  headAnimPhase.fill(0, startLi, endLi);
  tailFace.fill(0, startLi, endLi);
  tailSizeX.fill(0, startLi, endLi);
  tailSizeY.fill(0, startLi, endLi);
  smooth.fill(0, startLi, endLi);
}

function markResmooth(x: number, y: number, layer: number): void {
  if (smooth[ci(x, y) * L + layer]! > 1) {
    for (let sdx = -1; sdx < 2; sdx++) {
      for (let sdy = -1; sdy < 2; sdy++) {
        if (
          (sdx || sdy) &&
          x + sdx > 0 &&
          x + sdx < mapWidth &&
          y + sdy > 0 &&
          y + sdy < mapHeight
        ) {
          cellFlags[ci(x + sdx, y + sdy)] =
            cellFlags[ci(x + sdx, y + sdy)]! | FLAG_NEED_RESMOOTH;
        }
      }
    }
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// Face expansion (inside the view area)
// ──────────────────────────────────────────────────────────────────────────────

function expandNeedUpdate(x: number, y: number, w: number, h: number): void {
  for (let dx = 0; dx < w; dx++) {
    for (let dy = 0; dy < h; dy++) {
      cellFlags[ci(x - dx, y - dy)] =
        cellFlags[ci(x - dx, y - dy)]! | FLAG_NEED_UPDATE;
    }
  }
}

function expandNeedUpdateFromLayer(x: number, y: number, layer: number): void {
  const lIdx = ci(x, y) * L + layer;
  if (headFace[lIdx]! !== 0) {
    expandNeedUpdate(x, y, headSizeX[lIdx]!, headSizeY[lIdx]!);
  }
}

function expandClearFace(
  x: number,
  y: number,
  w: number,
  h: number,
  layer: number,
): void {
  const hIdx = ci(x, y);
  const hLIdx = hIdx * L + layer;
  const face = headFace[hLIdx]!;

  for (let dx = 0; dx < w; dx++) {
    for (let dy = dx === 0 ? 1 : 0; dy < h; dy++) {
      const tIdx = ci(x - dx, y - dy);
      const tLIdx = tIdx * L + layer;
      if (
        tailFace[tLIdx]! === face &&
        tailSizeX[tLIdx]! === dx &&
        tailSizeY[tLIdx]! === dy
      ) {
        tailFace[tLIdx] = 0;
        tailSizeX[tLIdx] = 0;
        tailSizeY[tLIdx] = 0;
        cellFlags[tIdx] = cellFlags[tIdx]! | FLAG_NEED_UPDATE;
      }
      markResmooth(x - dx, y - dy, layer);
    }
  }

  headFace[hLIdx] = 0;
  headAnim[hLIdx] = 0;
  headAnimSpeed[hLIdx] = 0;
  headAnimLeft[hLIdx] = 0;
  headAnimPhase[hLIdx] = 0;
  headSizeX[hLIdx] = 1;
  headSizeY[hLIdx] = 1;
  cellFlags[hIdx] = cellFlags[hIdx]! | FLAG_NEED_UPDATE | FLAG_NEED_RESMOOTH;
  markResmooth(x, y, layer);
}

function expandClearFaceFromLayer(x: number, y: number, layer: number): void {
  const lIdx = ci(x, y) * L + layer;
  if (headFace[lIdx]! !== 0 && headSizeX[lIdx]! > 0 && headSizeY[lIdx]! > 0) {
    expandClearFace(x, y, headSizeX[lIdx]!, headSizeY[lIdx]!, layer);
  }
}

function expandSetFace(
  x: number,
  y: number,
  layer: number,
  face: number,
  clear: boolean,
): void {
  const hIdx = ci(x, y);
  const hLIdx = hIdx * L + layer;

  if (clear) {
    expandClearFaceFromLayer(x, y, layer);
  }

  const { w, h } = getImageSize(face);
  headFace[hLIdx] = face;
  headSizeX[hLIdx] = w;
  headSizeY[hLIdx] = h;
  cellFlags[hIdx] = cellFlags[hIdx]! | FLAG_NEED_UPDATE;
  markResmooth(x, y, layer);
  notifyWatchedCell(x, y, `layer ${layer} face=${face} size=${w}x${h}`);

  for (let dx = 0; dx < w; dx++) {
    for (let dy = dx === 0 ? 1 : 0; dy < h; dy++) {
      const tIdx = ci(x - dx, y - dy);
      const tLIdx = tIdx * L + layer;
      tailFace[tLIdx] = face;
      tailSizeX[tLIdx] = dx;
      tailSizeY[tLIdx] = dy;
      cellFlags[tIdx] = cellFlags[tIdx]! | FLAG_NEED_UPDATE;
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
            cellFlags[ci(pl_pos.x + x - dx, pl_pos.y + y - dy)] =
              cellFlags[ci(pl_pos.x + x - dx, pl_pos.y + y - dy)]! |
              FLAG_NEED_UPDATE;
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
        cellFlags[ci(pl_pos.x + x - dx, pl_pos.y + y - dy)] =
          cellFlags[ci(pl_pos.x + x - dx, pl_pos.y + y - dy)]! |
          FLAG_NEED_UPDATE;
      }
    }
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// Dynamic fog-map sizing
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Compute the virtual-map size needed for a given view.
 *
 * Each side requires FOG_BORDER_MIN tiles for recentering room plus
 * MAX_FACE_SIZE tiles for big-face overhangs.  The result is rounded up
 * to the next multiple of 64 for alignment, with a minimum of
 * FOG_MAP_MIN_SIZE.
 */
function computeFogMapSize(viewW: number, viewH: number): number {
  const margin = 2 * (FOG_BORDER_MIN + MAX_FACE_SIZE);
  const needed = Math.max(FOG_MAP_MIN_SIZE, Math.max(viewW, viewH) + margin);
  // Round up to next multiple of 64.
  return Math.ceil(needed / 64) * 64;
}

// ──────────────────────────────────────────────────────────────────────────────
// Map allocation / initialisation
// ──────────────────────────────────────────────────────────────────────────────

function mapdataAlloc(w: number, h: number): void {
  mapWidth = w;
  mapHeight = h;
  const N = w * h;
  const NL = N * L;

  cellState = new Uint8Array(N);
  cellDarkness = new Uint8Array(N);
  cellFlags = new Uint8Array(N);

  headFace = new Uint32Array(NL);
  headSizeX = new Uint8Array(NL);
  headSizeY = new Uint8Array(NL);
  headAnim = new Uint16Array(NL);
  headAnimSpeed = new Uint8Array(NL);
  headAnimLeft = new Uint8Array(NL);
  headAnimPhase = new Uint16Array(NL);

  tailFace = new Uint32Array(NL);
  tailSizeX = new Uint8Array(NL);
  tailSizeY = new Uint8Array(NL);

  smooth = new Uint8Array(NL);

  // headSizeX/Y default to 1 (single tile) so that callers can read them
  // before any face has been placed.
  headSizeX.fill(1);
  headSizeY.fill(1);

  cellLabels = new Map();
}

function initBigfaces(): void {
  bigfaces = [];
  for (let x = 0; x < MAX_VIEW; x++) {
    bigfaces[x] = [];
    for (let y = 0; y < MAX_VIEW; y++) {
      bigfaces[x]![y] = [];
      for (let i = 0; i < MAXLAYERS; i++) {
        bigfaces[x]![y]![i] = {
          head: {
            face: 0,
            sizeX: 1,
            sizeY: 1,
            animation: 0,
            animationSpeed: 0,
            animationLeft: 0,
            animationPhase: 0,
          },
          tail: { face: 0, sizeX: 0, sizeY: 0 },
          x,
          y,
          layer: i,
        };
      }
    }
  }
  activeBigfaces = new Set();
}

function mapdataInit(viewW: number, viewH: number): void {
  const size = computeFogMapSize(viewW, viewH);
  mapdataAlloc(size, size);
  viewWidth = 0;
  viewHeight = 0;
  pl_pos.x = Math.floor(size / 2);
  pl_pos.y = Math.floor(size / 2);

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
 * Transition a visible cell to fog state.  x, y are view-relative.
 */
function mapdataClear(x: number, y: number): void {
  const px = pl_pos.x + x;
  const py = pl_pos.y + y;
  const idx = ci(px, py);

  if (cellState[idx]! === MapCellState.Empty) {
    return;
  }

  if (cellState[idx]! === MapCellState.Visible) {
    cellFlags[idx] = cellFlags[idx]! | FLAG_NEED_UPDATE;
    for (let l = 0; l < L; l++) {
      if (headFace[idx * L + l]! !== 0) {
        expandNeedUpdateFromLayer(px, py, l);
      }
    }
  }

  cellState[idx] = MapCellState.Fog;
}

// ──────────────────────────────────────────────────────────────────────────────
// Virtual map recentering
//
// Uses TypedArray.copyWithin (handles overlapping like memmove) to shift the
// column-major flat arrays.  This replaces the previous per-cell object copy
// loop with a series of native bulk moves.
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
    cellState.fill(MapCellState.Empty);
    cellDarkness.fill(0);
    cellFlags.fill(0);
    headFace.fill(0);
    headSizeX.fill(1);
    headSizeY.fill(1);
    headAnim.fill(0);
    headAnimSpeed.fill(0);
    headAnimLeft.fill(0);
    headAnimPhase.fill(0);
    tailFace.fill(0);
    tailSizeX.fill(0);
    tailSizeY.fill(0);
    smooth.fill(0);
    cellLabels.clear();
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

  // Shift column by column.  TypedArray.copyWithin handles overlapping
  // regions correctly (like memmove) so direction of iteration does not matter.
  for (let i = 0; i < lenX; i++) {
    const srcColBase = (srcX + i) * mapHeight;
    const dstColBase = (dstX + i) * mapHeight;

    // Scalar fields (1 element per cell).
    cellState.copyWithin(
      dstColBase + dstY,
      srcColBase + srcY,
      srcColBase + srcY + lenY,
    );
    cellDarkness.copyWithin(
      dstColBase + dstY,
      srcColBase + srcY,
      srcColBase + srcY + lenY,
    );
    cellFlags.copyWithin(
      dstColBase + dstY,
      srcColBase + srcY,
      srcColBase + srcY + lenY,
    );

    // Layer fields (L elements per cell).
    const srcLiBase = (srcColBase + srcY) * L;
    const dstLiBase = (dstColBase + dstY) * L;
    const copyLenL = lenY * L;

    headFace.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    headSizeX.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    headSizeY.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    headAnim.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    headAnimSpeed.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    headAnimLeft.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    headAnimPhase.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    tailFace.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    tailSizeX.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    tailSizeY.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
    smooth.copyWithin(dstLiBase, srcLiBase, srcLiBase + copyLenL);
  }

  // Shift labels: rebuild the sparse map with updated indices.
  // Only entries within the copy source region survive; their key is offset
  // by (shiftX, shiftY) in cell coordinates.
  if (cellLabels.size > 0) {
    const newLabels = new Map<number, MapLabel[]>();
    for (const [oldIdx, labels] of cellLabels) {
      const oldX = Math.floor(oldIdx / mapHeight);
      const oldY = oldIdx % mapHeight;
      if (
        oldX >= srcX &&
        oldX < srcX + lenX &&
        oldY >= srcY &&
        oldY < srcY + lenY
      ) {
        const newX = oldX - srcX + dstX;
        const newY = oldY - srcY + dstY;
        newLabels.set(newX * mapHeight + newY, labels);
      }
    }
    cellLabels = newLabels;
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

// ──────────────────────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Build a MapCell snapshot from the typed arrays at absolute map coordinates.
 * Used by the fog-cache, debug tools, and any code that needs the full
 * MapCell interface.  Not called in the hot render path (use the scalar
 * accessors below instead).
 */
export function mapdata_cell(mx: number, my: number): MapCell {
  const idx = ci(mx, my);
  const idxL = idx * L;
  const heads: MapCellLayer[] = new Array(L);
  const tails: MapCellTailLayer[] = new Array(L);
  const smoothArr: number[] = new Array(L);
  for (let l = 0; l < L; l++) {
    const lOff = idxL + l;
    heads[l] = {
      face: headFace[lOff]!,
      sizeX: headSizeX[lOff]!,
      sizeY: headSizeY[lOff]!,
      animation: headAnim[lOff]!,
      animationSpeed: headAnimSpeed[lOff]!,
      animationLeft: headAnimLeft[lOff]!,
      animationPhase: headAnimPhase[lOff]!,
    };
    tails[l] = {
      face: tailFace[lOff]!,
      sizeX: tailSizeX[lOff]!,
      sizeY: tailSizeY[lOff]!,
    };
    smoothArr[l] = smooth[lOff]!;
  }
  const f = cellFlags[idx]!;
  return {
    heads,
    tails,
    labels: cellLabels.get(idx) ?? [],
    smooth: smoothArr,
    darkness: cellDarkness[idx]!,
    needUpdate: (f & FLAG_NEED_UPDATE) !== 0,
    needResmooth: (f & FLAG_NEED_RESMOOTH) !== 0,
    state: cellState[idx]! as MapCellState,
  };
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
  const lIdx = ci(x, y) * L + layer;
  return (headFace[lIdx]! === 0 && layer > 0) || smooth[lIdx]! !== 0;
}

/**
 * Initialise / reinitialise the map module.  Call this before any other
 * function, and whenever a new display size is negotiated with the server.
 */
export function mapdata_set_size(viewx: number, viewy: number): void {
  const neededSize = computeFogMapSize(viewx, viewy);

  if (mapWidth === 0) {
    // First call: allocate the virtual map from scratch.
    mapdataInit(viewx, viewy);
  } else {
    if (viewWidth > 0 && viewHeight > 0) {
      // Adjust pl_pos so the player centre stays at the same absolute
      // position when the viewport dimensions change.
      pl_pos.x += Math.floor(viewWidth / 2) - Math.floor(viewx / 2);
      pl_pos.y += Math.floor(viewHeight / 2) - Math.floor(viewy / 2);
    }
    // Grow the map if the new viewport requires more room.
    if (neededSize > mapWidth) {
      mapdataAlloc(neededSize, neededSize);
      pl_pos.x = Math.floor(neededSize / 2) - Math.floor(viewx / 2);
      pl_pos.y = Math.floor(neededSize / 2) - Math.floor(viewy / 2);
    }
  }
  viewWidth = viewx;
  viewHeight = viewy;
}

/** Deallocate all map data. */
export function mapdata_free(): void {
  cellState = new Uint8Array(0);
  cellDarkness = new Uint8Array(0);
  cellFlags = new Uint8Array(0);
  headFace = new Uint32Array(0);
  headSizeX = new Uint8Array(0);
  headSizeY = new Uint8Array(0);
  headAnim = new Uint16Array(0);
  headAnimSpeed = new Uint8Array(0);
  headAnimLeft = new Uint8Array(0);
  headAnimPhase = new Uint16Array(0);
  tailFace = new Uint32Array(0);
  tailSizeX = new Uint8Array(0);
  tailSizeY = new Uint8Array(0);
  smooth = new Uint8Array(0);
  cellLabels = new Map();
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
  return headFace[ci(pl_pos.x + x, pl_pos.y + y) * L + layer]!;
}

/**
 * Return the face at absolute map coordinates and set dx/dy offsets for
 * drawing.  Returns 0 if nothing to draw.
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
  const lIdx = ci(mx, my) * L + layer;
  const face = headFace[lIdx]!;

  if (face !== 0) {
    return { face, dx: 0, dy: 0 };
  }

  // Fallback: check bigfaces[] for tiles whose bigface head is outside the
  // server viewport.
  const viewX = mx - pl_pos.x;
  const viewY = my - pl_pos.y;
  if (viewX >= 0 && viewX < MAX_VIEW && viewY >= 0 && viewY < MAX_VIEW) {
    const bigHead = bigfaceAt(viewX, viewY, layer).head;
    if (bigHead.face !== 0) {
      return { face: bigHead.face, dx: 0, dy: 0 };
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
  const lIdx = ci(mx, my) * L + layer;
  const tFace = tailFace[lIdx]!;

  if (tFace !== 0) {
    const hdx = tailSizeX[lIdx]!;
    const hdy = tailSizeY[lIdx]!;
    const hx = mx + hdx;
    const hy = my + hdy;
    if (!mapdata_contains(hx, hy)) {
      return { face: 0, dx: 0, dy: 0 };
    }
    return { face: tFace, dx: hdx, dy: hdy };
  }

  // Fallback: check bigfaces[].
  const viewX = mx - pl_pos.x;
  const viewY = my - pl_pos.y;
  if (viewX >= 0 && viewX < MAX_VIEW && viewY >= 0 && viewY < MAX_VIEW) {
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
        return { face: bigTail.face, dx: hdx, dy: hdy };
      }
    }
  }

  return { face: 0, dx: 0, dy: 0 };
}

/**
 * Return the big-face ("tail") information at a view position.
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
  const lIdx = ci(px, py) * L + layer;
  let result = tailFace[lIdx]!;

  if (result !== 0) {
    const dx = tailSizeX[lIdx]!;
    const dy = tailSizeY[lIdx]!;
    const headLIdx = ci(px + dx, py + dy) * L + layer;
    const w = headSizeX[headLIdx]!;
    const h = headSizeY[headLIdx]!;

    let clearBigface: boolean;
    if (cellState[ci(px, py)]! === MapCellState.Fog) {
      clearBigface = false;
    } else if (x + dx < viewWidth && y + dy < viewHeight) {
      clearBigface = cellState[ci(px + dx, py + dy)]! === MapCellState.Fog;
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

/** Return the big-face head at a view position. */
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

  const idx = ci(px, py);
  const lBase = idx * L;
  let isBlank = true;
  for (let l = 0; l < L; l++) {
    if (headFace[lBase + l]! !== 0 || tailFace[lBase + l]! !== 0) {
      isBlank = false;
      break;
    }
  }
  if (cellDarkness[idx]! !== 0) {
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
  const idx = ci(x, y);
  if (cellDarkness[idx]! === darkness) {
    return;
  }
  cellDarkness[idx] = darkness;
  cellFlags[idx] = cellFlags[idx]! | FLAG_NEED_UPDATE;
}

/** Set smooth value for a layer.  View-relative coordinates. */
export function mapdata_set_smooth(
  x: number,
  y: number,
  smoothVal: number,
  layer: number,
): void {
  const DX = [0, 1, 1, 1, 0, -1, -1, -1];
  const DY = [-1, -1, 0, 1, 1, 1, 0, -1];

  const px = pl_pos.x + x;
  const py = pl_pos.y + y;
  const lIdx = ci(px, py) * L + layer;

  if (smooth[lIdx]! !== smoothVal) {
    for (let i = 0; i < 8; i++) {
      const rx = px + DX[i]!;
      const ry = py + DY[i]!;
      if (rx < 0 || ry < 0 || mapWidth <= rx || mapHeight <= ry) {
        continue;
      }
      cellFlags[ci(rx, ry)] = cellFlags[ci(rx, ry)]! | FLAG_NEED_RESMOOTH;
    }
    cellFlags[ci(px, py)] = cellFlags[ci(px, py)]! | FLAG_NEED_RESMOOTH;
    smooth[lIdx] = smoothVal;
  }
}

/** Clear all labels at absolute map coordinates. */
export function mapdata_clear_label(px: number, py: number): void {
  cellLabels.delete(ci(px, py));
}

/** Clear all labels at view-relative coordinates. */
export function mapdata_clear_label_view(x: number, y: number): void {
  if (!(x < viewWidth && y < viewHeight)) {
    return;
  }
  mapdata_clear_label(pl_pos.x + x, pl_pos.y + y);
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
  const idx = ci(px, py);

  if (subtype === 0) {
    notifyWatchedCell(px, py, "labels cleared");
    cellLabels.delete(idx);
    return;
  }

  let arr = cellLabels.get(idx);
  if (!arr) {
    arr = [];
    cellLabels.set(idx, arr);
  }
  arr.push({ subtype, label });
  cellFlags[idx] = cellFlags[idx]! | FLAG_NEED_UPDATE;
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
  const idx = ci(px, py);

  if (cellState[idx]! === MapCellState.Fog) {
    cellFlags[idx] = cellFlags[idx]! | FLAG_NEED_UPDATE;
    for (let l = 0; l < L; l++) {
      expandClearFaceFromLayer(px, py, l);
    }
    cellDarkness[idx] = 0;
  }

  cellState[idx] = MapCellState.Visible;
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
    cellFlags[ci(px, py)] = cellFlags[ci(px, py)]! | FLAG_NEED_UPDATE;
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
      const lIdx = ci(px, py) * L + layer;
      headAnim[lIdx] = animation;
      headAnimPhase[lIdx] = phase;
      headAnimSpeed[lIdx] = animSpeed;
      headAnimLeft[lIdx] = speedLeft;
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
            cellFlags[ci(pl_pos.x + bc.x - bx, pl_pos.y + bc.y - by)] =
              cellFlags[ci(pl_pos.x + bc.x - bx, pl_pos.y + bc.y - by)]! |
              FLAG_NEED_UPDATE;
          }
        }
      }
    }
  } else {
    for (let x = 0; x < viewWidth; x++) {
      for (let y = 0; y < viewHeight; y++) {
        cellFlags[ci(pl_pos.x + x, pl_pos.y + y)] =
          cellFlags[ci(pl_pos.x + x, pl_pos.y + y)]! | FLAG_NEED_UPDATE;
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

  // Clear all cells and mark them all needUpdate.
  cellState.fill(MapCellState.Empty);
  cellDarkness.fill(0);
  cellFlags.fill(FLAG_NEED_UPDATE);
  headFace.fill(0);
  headSizeX.fill(1);
  headSizeY.fill(1);
  headAnim.fill(0);
  headAnimSpeed.fill(0);
  headAnimLeft.fill(0);
  headAnimPhase.fill(0);
  tailFace.fill(0);
  tailSizeX.fill(0);
  tailSizeY.fill(0);
  smooth.fill(0);
  cellLabels.clear();

  // Clear bigfaces.
  for (const bc of activeBigfaces) {
    expandClearBigfaceFromLayer(bc.x, bc.y, bc.layer, false);
  }

  clearMoveToInternal();
}

// ──────────────────────────────────────────────────────────────────────────────
// Fog-of-war snapshot: save
// ──────────────────────────────────────────────────────────────────────────────

export function mapdata_save_fog(key: string): void {
  if (!key) {
    return;
  }

  const originX = pl_pos.x - script_pos.x;
  const originY = pl_pos.y - script_pos.y;

  const cells_to_save: FogCacheCell[] = [];

  for (let ax = 0; ax < mapWidth; ax++) {
    for (let ay = 0; ay < mapHeight; ay++) {
      const idx = ci(ax, ay);
      if (cellState[idx]! === MapCellState.Empty) {
        continue;
      }

      const lBase = idx * L;
      let hasContent = cellDarkness[idx]! !== 0;
      if (!hasContent) {
        for (let l = 0; l < L; l++) {
          if (headFace[lBase + l]! !== 0 || tailFace[lBase + l]! !== 0) {
            hasContent = true;
            break;
          }
        }
      }
      if (!hasContent) {
        continue;
      }

      const heads: MapCellLayer[] = [];
      const tails: MapCellTailLayer[] = [];
      const smoothArr: number[] = [];
      for (let l = 0; l < L; l++) {
        const lOff = lBase + l;
        heads.push({
          face: headFace[lOff]!,
          sizeX: headSizeX[lOff]!,
          sizeY: headSizeY[lOff]!,
          animation: headAnim[lOff]!,
          animationSpeed: headAnimSpeed[lOff]!,
          animationLeft: headAnimLeft[lOff]!,
          animationPhase: headAnimPhase[lOff]!,
        });
        tails.push({
          face: tailFace[lOff]!,
          sizeX: tailSizeX[lOff]!,
          sizeY: tailSizeY[lOff]!,
        });
        smoothArr.push(smooth[lOff]!);
      }

      const savedLabels = cellLabels.get(idx);
      cells_to_save.push({
        x: ax - originX,
        y: ay - originY,
        heads,
        tails,
        smooth: smoothArr,
        darkness: cellDarkness[idx]!,
        labels: savedLabels ? savedLabels.map((lab) => ({ ...lab })) : [],
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

  const visibleCells: Array<{
    vx: number;
    vy: number;
    faces: (number | undefined)[];
  }> = [];

  for (let ax = 0; ax < mapWidth; ax++) {
    for (let ay = 0; ay < mapHeight; ay++) {
      const idx = ci(ax, ay);
      if (cellState[idx]! !== MapCellState.Visible) continue;
      const faces: (number | undefined)[] = [];
      let hasContent = false;
      for (let layer = 0; layer < FOG_ALIGN_LAYERS; layer++) {
        const f = headFace[idx * L + layer]!;
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
    LOG(
      LogLevel.Info,
      "mapdata",
      "Fog alignment: no correlation data; skipping restore",
    );
    return null;
  }

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
      // to the baseline.
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
 */
export function mapdata_restore_fog(key: string): void {
  const snapshot = cacheGetFog(key);
  if (!snapshot || snapshot.cells.length === 0) {
    return;
  }

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

    const idx = ci(ax, ay);
    if (cellState[idx]! !== MapCellState.Empty) {
      continue;
    }

    const lBase = idx * L;
    for (let l = 0; l < L; l++) {
      const lOff = lBase + l;
      const h = entry.heads[l]!;
      const t = entry.tails[l]!;
      headFace[lOff] = h.face;
      headSizeX[lOff] = h.sizeX;
      headSizeY[lOff] = h.sizeY;
      headAnim[lOff] = h.animation;
      headAnimSpeed[lOff] = h.animationSpeed;
      headAnimLeft[lOff] = h.animationLeft;
      headAnimPhase[lOff] = h.animationPhase;
      tailFace[lOff] = t.face;
      tailSizeX[lOff] = t.sizeX;
      tailSizeY[lOff] = t.sizeY;
      smooth[lOff] = entry.smooth[l]!;
    }
    cellDarkness[idx] = entry.darkness;
    if (entry.labels.length > 0) {
      cellLabels.set(
        idx,
        entry.labels.map((lab) => ({ ...lab })),
      );
    }
    cellState[idx] = MapCellState.Fog;
    cellFlags[idx] = cellFlags[idx]! | FLAG_NEED_UPDATE;
    restored++;
  }

  LOG(
    LogLevel.Info,
    "mapdata",
    `Restored ${restored} fog cells for map "${key}"`,
  );
}

// ──────────────────────────────────────────────────────────────────────────────
// Magicmap
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Fill empty virtual-map cells with data derived from a server magicmap packet.
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
        continue;
      }

      const ax = playerAbsX + (mx - pmapx);
      const ay = playerAbsY + (my - pmapy);

      if (ax < 0 || ay < 0 || ax >= mapWidth || ay >= mapHeight) {
        continue;
      }

      const idx = ci(ax, ay);
      if (cellState[idx]! !== MapCellState.Empty) {
        continue;
      }

      const lBase = idx * L;
      headFace[lBase + 0] = MAGIC_MAP_FACE_BASE | colorIdx;
      headSizeX[lBase + 0] = 1;
      headSizeY[lBase + 0] = 1;

      if (val & FACE_WALL) {
        const neighborMask =
          (isWall(mx, my - 1) ? MAGIC_MAP_WALL_ABOVE : 0) |
          (isWall(mx, my + 1) ? MAGIC_MAP_WALL_BELOW : 0) |
          (isWall(mx - 1, my) ? MAGIC_MAP_WALL_LEFT : 0) |
          (isWall(mx + 1, my) ? MAGIC_MAP_WALL_RIGHT : 0);

        headFace[lBase + 1] = MAGIC_MAP_WALL_FACE_BASE | neighborMask;
        headSizeX[lBase + 1] = 1;
        headSizeY[lBase + 1] = 1;
      }

      cellState[idx] = MapCellState.Fog;
      cellFlags[idx] = cellFlags[idx]! | FLAG_NEED_UPDATE;
      applied++;
    }
  }

  LOG(LogLevel.Info, "mapdata", `Applied ${applied} magicmap cells`);
}

// ──────────────────────────────────────────────────────────────────────────────
// Animation tick
// ──────────────────────────────────────────────────────────────────────────────

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
      const idx = ci(pl_pos.x + x, pl_pos.y + y);

      if (cellState[idx]! !== MapCellState.Visible) {
        continue;
      }

      const lBase = idx * L;

      for (let layer = 0; layer < MAXLAYERS; layer++) {
        // Animate cell heads.
        const lIdx = lBase + layer;
        const anim = headAnim[lIdx]!;
        if (anim) {
          headAnimLeft[lIdx] = headAnimLeft[lIdx]! + 1;
          if (headAnimLeft[lIdx]! >= headAnimSpeed[lIdx]!) {
            headAnimLeft[lIdx] = 0;
            headAnimPhase[lIdx] = headAnimPhase[lIdx]! + 1;
            if (
              headAnimPhase[lIdx]! >= (animations[anim]?.numAnimations ?? 0)
            ) {
              headAnimPhase[lIdx] = 0;
            }
            const face = animations[anim]?.faces[headAnimPhase[lIdx]] ?? 0;
            if (face > 0) {
              expandSetFace(pl_pos.x + x, pl_pos.y + y, layer, face, false);
            } else {
              expandClearFaceFromLayer(pl_pos.x + x, pl_pos.y + y, layer);
            }
          }
        }

        // Animate bigface heads (bigfaces[] still uses objects).
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
// Scalar accessor exports (hot render path – no object allocation)
// ──────────────────────────────────────────────────────────────────────────────

/** State of the cell at absolute map coordinates. */
export function mapdata_get_state(mx: number, my: number): MapCellState {
  return cellState[ci(mx, my)]! as MapCellState;
}

/** Head face ID for the given layer at absolute map coordinates. */
export function mapdata_get_head_face(
  mx: number,
  my: number,
  layer: number,
): number {
  return headFace[ci(mx, my) * L + layer]!;
}

/** Smooth value for the given layer at absolute map coordinates. */
export function mapdata_get_smooth(
  mx: number,
  my: number,
  layer: number,
): number {
  return smooth[ci(mx, my) * L + layer]!;
}

/** Darkness value at absolute map coordinates. */
export function mapdata_get_darkness(mx: number, my: number): number {
  return cellDarkness[ci(mx, my)]!;
}

const EMPTY_LABELS: readonly MapLabel[] = Object.freeze([]);

/** Labels at absolute map coordinates (empty frozen array when none). */
export function mapdata_get_labels(
  mx: number,
  my: number,
): readonly MapLabel[] {
  return cellLabels.get(ci(mx, my)) ?? EMPTY_LABELS;
}

// ──────────────────────────────────────────────────────────────────────────────
// Memory diagnostics
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Return a snapshot of current map memory usage.
 *
 * `estimatedBytes` is the exact number of bytes held by the typed arrays.
 * `heapBytes` is `performance.memory.usedJSHeapSize` when available
 * (Chrome with --enable-precise-memory-info; not available in Firefox).
 */
export function mapdata_memory_stats(): {
  mapDimensions: { width: number; height: number };
  viewDimensions: { width: number; height: number };
  totalCells: number;
  nonEmptyCells: number;
  labeledCells: number;
  typedArrayBytes: number;
  heapBytes: number | null;
} {
  const totalCells = mapWidth * mapHeight;
  let nonEmpty = 0;
  for (let i = 0; i < totalCells; i++) {
    if (cellState[i]! !== MapCellState.Empty) nonEmpty++;
  }

  const typedArrayBytes =
    cellState.byteLength +
    cellDarkness.byteLength +
    cellFlags.byteLength +
    headFace.byteLength +
    headSizeX.byteLength +
    headSizeY.byteLength +
    headAnim.byteLength +
    headAnimSpeed.byteLength +
    headAnimLeft.byteLength +
    headAnimPhase.byteLength +
    tailFace.byteLength +
    tailSizeX.byteLength +
    tailSizeY.byteLength +
    smooth.byteLength;

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const heapBytes = (performance as any).memory?.usedJSHeapSize ?? null;

  return {
    mapDimensions: { width: mapWidth, height: mapHeight },
    viewDimensions: { width: viewWidth, height: viewHeight },
    totalCells,
    nonEmptyCells: nonEmpty,
    labeledCells: cellLabels.size,
    typedArrayBytes,
    heapBytes,
  };
}

// ──────────────────────────────────────────────────────────────────────────────
// Public getters
// ──────────────────────────────────────────────────────────────────────────────

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

  const cell = mapdata_cell(ax, ay);
  lines.push(
    `  state=${stateName(cell.state)} darkness=${cell.darkness} needUpdate=${cell.needUpdate} needResmooth=${cell.needResmooth}`,
  );

  for (let i = 0; i < MAXLAYERS; i++) {
    const hl = formatLayer(cell.heads[i]!, i);
    if (hl) lines.push(hl);
    const tl = formatTail(cell.tails[i]!, i);
    if (tl) lines.push(tl);
    if (cell.smooth[i]! !== 0) {
      lines.push(`  smooth ${i}: ${cell.smooth[i]}`);
    }
  }

  if (cell.labels.length > 0) {
    for (const lbl of cell.labels) {
      lines.push(`  label: subtype=${lbl.subtype} "${lbl.label}"`);
    }
  }

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

  const cell = mapdata_cell(ax, ay);
  lines.push(`  cell state=${stateName(cell.state)} darkness=${cell.darkness}`);

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
      const hx = ax + t.sizeX;
      const hy = ay + t.sizeY;
      if (mapdata_contains(hx, hy)) {
        const headCell = mapdata_cell(hx, hy);
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
 * Returns an array of lines suitable for logging.
 */
export function mapdata_debug_all_bigfaces(): string[] {
  const lines: string[] = [];
  lines.push(
    `  pl_pos=(${pl_pos.x}, ${pl_pos.y}) view=${viewWidth}x${viewHeight}`,
  );

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
      const idx = ci(ax, ay);
      const lBase = idx * L;
      for (let layer = 0; layer < MAXLAYERS; layer++) {
        const lOff = lBase + layer;
        const face = headFace[lOff]!;
        const sx = headSizeX[lOff]!;
        const sy = headSizeY[lOff]!;
        if (face !== 0 && (sx > 1 || sy > 1)) {
          cellsBigfaces.push({ vx, vy, layer, face, sizeX: sx, sizeY: sy });
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
    for (let dx = 0; dx < bc.head.sizeX; dx++) {
      for (let dy = dx === 0 ? 1 : 0; dy < bc.head.sizeY; dy++) {
        const tvx = bc.x - dx;
        const tvy = bc.y - dy;
        if (tvx >= 0 && tvx < viewWidth && tvy >= 0 && tvy < viewHeight) {
          lines.push(
            `    tail covers view=(${tvx}, ${tvy}) abs=(${ax - dx}, ${ay - dy}) offset=(${dx}, ${dy})`,
          );
        }
      }
    }
    index++;
  }

  return lines;
}
