import { MAXLAYERS, MapCellState } from "./protocol";
import {
  getPlayerPosition,
  getViewSize,
  mapdata_cell,
  mapdata_contains,
  mapdata_face_info,
  mapdata_size,
  pl_mpos,
} from "./mapdata";

export interface ReplayMapdataLayerHeadRecord {
  layer: number;
  face?: number;
  sizeX?: number;
  sizeY?: number;
  animation?: number;
  animationSpeed?: number;
}

export interface ReplayMapdataLayerTailRecord {
  layer: number;
  face: number;
  sizeX: number;
  sizeY: number;
}

export interface ReplayMapdataSmoothRecord {
  layer: number;
  value: number;
}

export interface ReplayMapdataLabelRecord {
  subtype: number;
  label: string;
}

export interface ReplayMapdataCellRecord {
  dx: number;
  dy: number;
  state: keyof typeof MapCellState;
  darkness: number;
  heads: ReplayMapdataLayerHeadRecord[];
  tails: ReplayMapdataLayerTailRecord[];
  smooth: ReplayMapdataSmoothRecord[];
  labels: ReplayMapdataLabelRecord[];
}

export interface ReplayMapdataSnapshot {
  schemaVersion: 1;
  source?: {
    replayFileName?: string;
    markLabel?: string;
    entryIndex?: number;
  };
  capturedAtIso: string;
  mapSize: { width: number; height: number };
  viewSize: { width: number; height: number };
  playerAbsolute: { x: number; y: number };
  cells: ReplayMapdataCellRecord[];
}

export interface ReplayMapdataComparisonResult {
  ok: boolean;
  checkedCells: number;
  mismatches: string[];
}

function isKnownStateName(value: unknown): value is keyof typeof MapCellState {
  return typeof value === "string" && value in MapCellState;
}

function mapCellStateName(state: MapCellState): keyof typeof MapCellState {
  switch (state) {
    case MapCellState.Empty:
      return "Empty";
    case MapCellState.Visible:
      return "Visible";
    case MapCellState.Fog:
      return "Fog";
    default:
      return "Empty";
  }
}

function hasRelevantCellData(
  cell: ReturnType<typeof mapdata_cell>,
  includeEmptyCells: boolean,
): boolean {
  if (includeEmptyCells) {
    return true;
  }
  if (cell.state !== MapCellState.Empty) {
    return true;
  }
  if (cell.darkness !== 0) {
    return true;
  }
  if (cell.labels.length > 0) {
    return true;
  }
  for (let layer = 0; layer < MAXLAYERS; layer++) {
    const head = cell.heads[layer]!;
    const tail = cell.tails[layer]!;
    if (head.face !== 0 || tail.face !== 0 || cell.smooth[layer]! !== 0) {
      return true;
    }
  }
  return false;
}

function normalizeCellAtAbsolute(
  absoluteX: number,
  absoluteY: number,
  dx: number,
  dy: number,
): ReplayMapdataCellRecord {
  const cell = mapdata_cell(absoluteX, absoluteY);
  const heads: ReplayMapdataLayerHeadRecord[] = [];
  const tails: ReplayMapdataLayerTailRecord[] = [];
  const smooth: ReplayMapdataSmoothRecord[] = [];

  for (let layer = 0; layer < MAXLAYERS; layer++) {
    const head = cell.heads[layer]!;
    const tail = cell.tails[layer]!;
    const smoothValue = cell.smooth[layer]!;
    const faceInfo = mapdata_face_info(absoluteX, absoluteY, layer);
    let hasRecordedLayer = false;

    if (
      head.face !== 0 ||
      head.sizeX !== 1 ||
      head.sizeY !== 1 ||
      head.animation !== 0 ||
      head.animationSpeed !== 0 ||
      head.animationLeft !== 0 ||
      head.animationPhase !== 0
    ) {
      if (head.animation !== 0) {
        heads.push({
          layer,
          animation: head.animation,
          animationSpeed: head.animationSpeed,
        });
      } else {
        heads.push({
          layer,
          face: head.face,
          sizeX: head.sizeX,
          sizeY: head.sizeY,
        });
      }
      hasRecordedLayer = true;
    }

    if (tail.face !== 0 || tail.sizeX !== 0 || tail.sizeY !== 0) {
      tails.push({
        layer,
        face: tail.face,
        sizeX: tail.sizeX,
        sizeY: tail.sizeY,
      });
      hasRecordedLayer = true;
    }

    if (!hasRecordedLayer && faceInfo.face !== 0) {
      if (faceInfo.dx === 0 && faceInfo.dy === 0) {
        heads.push({
          layer,
          face: faceInfo.face,
        });
      } else {
        tails.push({
          layer,
          face: faceInfo.face,
          sizeX: faceInfo.dx,
          sizeY: faceInfo.dy,
        });
      }
    }

    if (smoothValue !== 0) {
      smooth.push({ layer, value: smoothValue });
    }
  }

  return {
    dx,
    dy,
    state: mapCellStateName(cell.state),
    darkness: cell.darkness,
    heads,
    tails,
    smooth,
    labels: cell.labels.map((label) => ({
      subtype: label.subtype,
      label: label.label,
    })),
  };
}

export function captureReplayMapdataSnapshot(options?: {
  includeEmptyCells?: boolean;
  source?: ReplayMapdataSnapshot["source"];
}): ReplayMapdataSnapshot {
  const includeEmptyCells = options?.includeEmptyCells ?? false;
  const topLeft = getPlayerPosition();
  const view = getViewSize();
  const map = mapdata_size();
  const player = pl_mpos();
  const cells: ReplayMapdataCellRecord[] = [];

  for (let vx = 0; vx < view.width; vx++) {
    for (let vy = 0; vy < view.height; vy++) {
      const absoluteX = topLeft.x + vx;
      const absoluteY = topLeft.y + vy;
      if (!mapdata_contains(absoluteX, absoluteY)) {
        continue;
      }
      const normalized = normalizeCellAtAbsolute(
        absoluteX,
        absoluteY,
        absoluteX - player.px,
        absoluteY - player.py,
      );
      const cell = mapdata_cell(absoluteX, absoluteY);
      if (!hasRelevantCellData(cell, includeEmptyCells)) {
        continue;
      }
      cells.push(normalized);
    }
  }

  cells.sort((a, b) => (a.dy - b.dy !== 0 ? a.dy - b.dy : a.dx - b.dx));

  return {
    schemaVersion: 1,
    source: options?.source,
    capturedAtIso: new Date().toISOString(),
    mapSize: map,
    viewSize: view,
    playerAbsolute: { x: player.px, y: player.py },
    cells,
  };
}

function sortObjectKeysDeep(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map(sortObjectKeysDeep);
  }
  if (!value || typeof value !== "object") {
    return value;
  }
  const entries = Object.entries(value as Record<string, unknown>)
    .filter(([, v]) => v !== undefined)
    .sort(([a], [b]) => a.localeCompare(b));
  const out: Record<string, unknown> = {};
  for (const [key, val] of entries) {
    out[key] = sortObjectKeysDeep(val);
  }
  return out;
}

function stableStringify(value: unknown): string {
  return JSON.stringify(sortObjectKeysDeep(value));
}

export function compareReplayMapdataSnapshot(
  expected: ReplayMapdataSnapshot,
): ReplayMapdataComparisonResult {
  const mismatches: string[] = [];
  const player = pl_mpos();

  if (expected.schemaVersion !== 1) {
    mismatches.push(
      `Unsupported state schemaVersion ${expected.schemaVersion} (expected 1).`,
    );
    return { ok: false, checkedCells: 0, mismatches };
  }

  for (const expectedCell of expected.cells) {
    if (!isKnownStateName(expectedCell.state)) {
      mismatches.push(
        `Invalid state name at (${expectedCell.dx}, ${expectedCell.dy}): ${expectedCell.state}`,
      );
      continue;
    }

    const absoluteX = player.px + expectedCell.dx;
    const absoluteY = player.py + expectedCell.dy;

    if (!mapdata_contains(absoluteX, absoluteY)) {
      mismatches.push(
        `Cell (${expectedCell.dx}, ${expectedCell.dy}) resolves to (${absoluteX}, ${absoluteY}) outside map bounds.`,
      );
      continue;
    }

    const actualCell = normalizeCellAtAbsolute(
      absoluteX,
      absoluteY,
      expectedCell.dx,
      expectedCell.dy,
    );

    const expectedJson = stableStringify(expectedCell);
    const actualJson = stableStringify(actualCell);

    if (expectedJson !== actualJson) {
      mismatches.push(
        [
          `Mismatch at relative (${expectedCell.dx}, ${expectedCell.dy}) / absolute (${absoluteX}, ${absoluteY})`,
          `expected: ${JSON.stringify(expectedCell, null, 2)}`,
          `actual:   ${JSON.stringify(actualCell, null, 2)}`,
        ].join("\n"),
      );
    }
  }

  return {
    ok: mismatches.length === 0,
    checkedCells: expected.cells.length,
    mismatches,
  };
}
