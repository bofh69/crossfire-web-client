/**
 * mapdata_fog.ts — Saving the fog-of-war map when leaving a map, and
 * restoring it, aligned with what the server shows, when entering it again.
 */

import { MapCellState, LogLevel, MAXLAYERS } from "./protocol";
import type { MapCellLayer, MapCellTailLayer } from "./protocol";
import {
  cacheGetFog,
  cacheSaveFog,
  type FogCacheCell,
  type FogSnapshot,
} from "./map_fog_cache";
import {
  L,
  FLAG_NEED_UPDATE,
  ci,
  cellDarkness,
  cellFlags,
  cellLabels,
  cellState,
  headAnim,
  headAnimLeft,
  headAnimPhase,
  headAnimSpeed,
  headFace,
  headSizeX,
  headSizeY,
  mapHeight,
  mapWidth,
  pl_pos,
  script_pos,
  smooth,
  tailFace,
  tailSizeX,
  tailSizeY,
} from "./mapdata_state";
import { LOG } from "./misc";

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
