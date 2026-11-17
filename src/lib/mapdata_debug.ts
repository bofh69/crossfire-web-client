/**
 * mapdata_debug.ts — Memory statistics and text dumps of the virtual map,
 * for the `debug` command and the replay page.
 */

import { MapCellState, MAX_VIEW, MAXLAYERS } from "./protocol";
import type { MapCellLayer, MapCellTailLayer } from "./protocol";
import {
  mapdata_cell,
  mapdata_contains,
  mapdata_face_info,
  pl_mpos,
} from "./mapdata";
import {
  activeBigfaces,
  bigfaceAt,
  cellDarkness,
  cellFlags,
  cellLabels,
  cellState,
  ci,
  headAnim,
  headAnimLeft,
  headAnimPhase,
  headAnimSpeed,
  headFace,
  headSizeX,
  headSizeY,
  L,
  mapHeight,
  mapWidth,
  MAX_FACE_SIZE,
  pl_pos,
  script_pos,
  smooth,
  tailFace,
  tailSizeX,
  tailSizeY,
  viewHeight,
  viewWidth,
} from "./mapdata_state";

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
  const player = pl_mpos();
  const playerDx = ax - player.px;
  const playerDy = ay - player.py;
  lines.push(
    `Tile at absolute (${ax}, ${ay}), view-relative (${viewX}, ${viewY}), player-relative (${playerDx}, ${playerDy})`,
  );
  lines.push(
    `  pl_pos=(${pl_pos.x}, ${pl_pos.y}) player=(${player.px}, ${player.py}) view=${viewWidth}x${viewHeight}`,
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
  const player = pl_mpos();
  const playerDx = ax - player.px;
  const playerDy = ay - player.py;
  lines.push(
    `Bigface info at absolute (${ax}, ${ay}), view-relative (${viewX}, ${viewY}), player-relative (${playerDx}, ${playerDy})`,
  );
  lines.push(
    `  pl_pos=(${pl_pos.x}, ${pl_pos.y}) player=(${player.px}, ${player.py}) view=${viewWidth}x${viewHeight}`,
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
