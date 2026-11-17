/**
 * mapdata_magicmap.ts — Filling the virtual map from a server magicmap
 * packet.
 */

import { FACE_COLOR_MASK, FACE_WALL, LogLevel, MapCellState } from "./protocol";
import {
  MAGIC_MAP_FACE_BASE,
  MAGIC_MAP_WALL_ABOVE,
  MAGIC_MAP_WALL_BELOW,
  MAGIC_MAP_WALL_FACE_BASE,
  MAGIC_MAP_WALL_LEFT,
  MAGIC_MAP_WALL_RIGHT,
} from "./image";
import { useConfig } from "./init";
import {
  cellFlags,
  cellState,
  ci,
  FLAG_NEED_UPDATE,
  headFace,
  headSizeX,
  headSizeY,
  L,
  mapHeight,
  mapWidth,
  pl_pos,
  viewHeight,
  viewWidth,
} from "./mapdata_state";
import { LOG } from "./misc";

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
