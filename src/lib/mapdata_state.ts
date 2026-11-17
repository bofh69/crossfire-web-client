/**
 * mapdata_state.ts — The cells of the virtual fog-of-war map and the
 * player's position on it, shared by the mapdata modules.
 *
 * The cell fields are flat typed arrays indexed by {@link ci} (or by
 * `ci(x, y) * L + layer` for per-layer fields).  {@link mapdataAlloc}
 * replaces the arrays when the map is resized; other modules see the current
 * arrays through these live bindings, but only this module assigns them.
 */

import { MAX_VIEW, MAXLAYERS } from "./protocol";
import type {
  MapCellLayer,
  MapCellTailLayer,
  MapLabel,
  PlayerPosition,
} from "./protocol";

/** Short alias kept in sync with the protocol constant. */
export const L = MAXLAYERS;

/** Maximum size of a big face image in tiles. */
export const MAX_FACE_SIZE = 16;

/** True in development builds where extra bounds assertions should run. */
const DEV_ASSERTIONS = import.meta.env.DEV;

// cellFlags bit encoding
/** cellFlags bit: the cell must be redrawn. */
export const FLAG_NEED_UPDATE = 0x01;
/** cellFlags bit: the cell's smoothing must be recalculated. */
export const FLAG_NEED_RESMOOTH = 0x02;

/** Internal player position on the virtual map (top-left of view). */
export const pl_pos: PlayerPosition = { x: 0, y: 0 };

/** Player position reported after newmap resets. */
export const script_pos: PlayerPosition = { x: 0, y: 0 };

/** View dimensions (tiles). */
export let viewWidth = 0;
export let viewHeight = 0;

/** Virtual map dimensions. */
export let mapWidth = 0;
export let mapHeight = 0;

// ── Scalar per-cell typed arrays ─────────────────────────────────────────────
/** {@link MapCellState} of each cell. */
export let cellState = new Uint8Array(0);
/** Darkness of each cell. */
export let cellDarkness = new Uint8Array(0);
/** bit 0 = needUpdate, bit 1 = needResmooth */
export let cellFlags = new Uint8Array(0);

// ── Head layer fields [idx * L + layer] ──────────────────────────────────────
/** Face of the head on each layer. */
export let headFace = new Uint32Array(0);
/** Width in tiles of the head on each layer. */
export let headSizeX = new Uint8Array(0);
/** Height in tiles of the head on each layer. */
export let headSizeY = new Uint8Array(0);
/** Animation of the head on each layer. */
export let headAnim = new Uint16Array(0);
/** Animation speed of the head on each layer. */
export let headAnimSpeed = new Uint8Array(0);
/** Ticks left until the head's next animation frame. */
export let headAnimLeft = new Uint8Array(0);
/** Current animation frame of the head on each layer. */
export let headAnimPhase = new Uint16Array(0);
/**
 * 1 when the head on this layer was sent by the server and has not been
 * cleared since, i.e. the server knows it is showing it. 0 for remembered
 * (fog) heads.
 */
export let headCurrent = new Uint8Array(0);

// ── Tail layer fields [idx * L + layer] ──────────────────────────────────────
/** Face of the big-face tail on each layer. */
export let tailFace = new Uint32Array(0);
/** X offset from each tail to its head. */
export let tailSizeX = new Uint8Array(0);
/** Y offset from each tail to its head. */
export let tailSizeY = new Uint8Array(0);

// ── Smooth values [idx * L + layer] ──────────────────────────────────────────
/** Smoothing level of each layer. */
export let smooth = new Uint8Array(0);

// ── Per-layer "updated after clear" flag [idx * L + layer] ───────────────────
/**
 * Non-zero when the server explicitly re-sent face data for this layer while
 * the cell was in Fog state (after receiving a clear_space for that cell).
 * Used by begin() to preserve big-face head layers that the server confirmed
 * are still valid even though they were preceded by a clear_space.
 */
export let layerUpdatedAfterClear = new Uint8Array(0);

/** Sparse label storage: keyed by flat cell index. */
export let cellLabels = new Map<number, MapLabel[]>();

/** Cell index (column-major): ci(x, y) = x * mapHeight + y */
export function ci(x: number, y: number): number {
  return x * mapHeight + y;
}

/** Replace the cells with empty ones for a `w` × `h` map. */
export function mapdataAlloc(w: number, h: number): void {
  mapWidth = w;
  mapHeight = h;
  const N = w * h;
  const NL = N * L;

  cellState = new Uint8Array(N);
  cellDarkness = new Uint8Array(N);
  cellFlags = new Uint8Array(N);

  headFace = new Uint32Array(NL);
  headCurrent = new Uint8Array(NL);
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

  layerUpdatedAfterClear = new Uint8Array(NL);

  // headSizeX/Y default to 1 (single tile) so that callers can read them
  // before any face has been placed.
  headSizeX.fill(1);
  headSizeY.fill(1);

  cellLabels = new Map();
}

/** Replace all cell labels with `labels`. */
export function setCellLabels(labels: Map<number, MapLabel[]>): void {
  cellLabels = labels;
}

/** Set the view dimensions, in tiles. */
export function setViewSize(width: number, height: number): void {
  viewWidth = width;
  viewHeight = height;
}

// ──────────────────────────────────────────────────────────────────────────────
// BigCell – tracks multi-tile faces outside the view area
// ──────────────────────────────────────────────────────────────────────────────

/** A big face's head or tail on one layer of a view position. */
export interface BigCell {
  head: MapCellLayer;
  tail: MapCellTailLayer;
  x: number;
  y: number;
  layer: number;
}

/** Big-face tracking outside the view area (objects, small fixed size). */
export let bigfaces: BigCell[][][] = [];
/** The entries of {@link bigfaces} that hold a face. */
export let activeBigfaces: Set<BigCell> = new Set();

/** The {@link bigfaces} entry for view position (x, y) and `layer`. */
export function bigfaceAt(x: number, y: number, layer: number): BigCell {
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

/** Fill {@link bigfaces} with empty entries for every view position. */
export function initBigfaces(): void {
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

/** Remove all entries from {@link bigfaces}. */
export function clearBigfaces(): void {
  bigfaces = [];
  activeBigfaces = new Set();
}
