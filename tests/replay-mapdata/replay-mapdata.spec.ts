import { readFileSync } from "node:fs";
import path from "node:path";
import { describe, expect, test, beforeAll } from "vitest";
import type { ReplayMapdataSnapshot } from "../../src/lib/replay_mapdata_state";

interface ReplayMapdataTestCase {
  name: string;
  log: string;
  mark: string;
  state: string;
}

const rootDir = path.resolve(
  path.dirname(new URL(import.meta.url).pathname),
  "../..",
);
const testConfigPath = path.join(rootDir, "tests/replay-mapdata/tests.json");
const cases = JSON.parse(
  readFileSync(testConfigPath, "utf8"),
) as ReplayMapdataTestCase[];

let dispatchPacket: (buffer: ArrayBuffer) => void;
let clientInit: () => void;
let initCommands: () => void;
let setGetMapImageSize: (
  fn: (face: number) => { w: number; h: number },
) => void;
let mapdata_cell: (
  mx: number,
  my: number,
) => {
  heads: Array<{ face: number; sizeX: number; sizeY: number }>;
  tails: Array<{ face: number; sizeX: number; sizeY: number }>;
  state: number;
};
let mapdata_contains: (x: number, y: number) => boolean;
let pl_mpos: () => { px: number; py: number };
let getFaceTileSize: (face: number) => { w: number; h: number };
let MAGIC_MAP_FACE_BASE: number;
let MAGIC_MAP_WALL_FACE_BASE: number;
let MapCellState: typeof import("../../src/lib/protocol").MapCellState;
let parseReplayLogFile: (text: string) => {
  entries: Array<{ type: "MARK" | "RX" | "TX"; payload: Uint8Array | null }>;
  marks: Array<{ label: string; entryIndex: number }>;
};
let resetReplaySandboxState: () => void;
let toPacketBuffer: (payload: Uint8Array) => ArrayBuffer;
let compareReplayMapdataSnapshot: (expected: ReplayMapdataSnapshot) => {
  ok: boolean;
  checkedCells: number;
  mismatches: string[];
};

/** Provide the browser-only globals that the client modules use. */
function installNodeTestGlobals(): void {
  const urlObj = URL as unknown as {
    createObjectURL?: (obj: unknown) => string;
    revokeObjectURL?: (url: string) => void;
  };

  if (typeof urlObj.createObjectURL !== "function") {
    urlObj.createObjectURL = () => "blob:test";
  }
  if (typeof urlObj.revokeObjectURL !== "function") {
    urlObj.revokeObjectURL = () => {};
  }
}

function replayToMark(logText: string, markLabel: string): void {
  const parsed = parseReplayLogFile(logText);
  const targetMark = parsed.marks.find((mark) => mark.label === markLabel);
  expect(
    targetMark,
    `MARK \"${markLabel}\" must exist in replay`,
  ).toBeDefined();
  if (!targetMark) {
    return;
  }

  for (let index = 0; index <= targetMark.entryIndex; index++) {
    const entry = parsed.entries[index]!;
    if (entry.type !== "RX" || entry.payload === null) {
      continue;
    }
    dispatchPacket(toPacketBuffer(entry.payload));
  }
}

/**
 * Return the payload of one RX log line, in compact or converted format,
 * or null for other lines.
 */
function parseRawRxLine(line: string): Uint8Array | null {
  const entry = parseReplayLogFile(line).entries[0];
  return entry?.type === "RX" ? entry.payload : null;
}

/**
 * Count the magic-mapped cells near the player, and those of them that also
 * have a wall face.  Every magic-mapped cell must be in fog.
 */
function countMagicMapCells(): { cells: number; walls: number } {
  const isFace = (face: number, base: number, mask: number): boolean =>
    (face & mask) >>> 0 === base;
  const player = pl_mpos();
  let cells = 0;
  let walls = 0;
  for (let ax = player.px - 30; ax <= player.px + 30; ax++) {
    for (let ay = player.py - 30; ay <= player.py + 30; ay++) {
      if (!mapdata_contains(ax, ay)) {
        continue;
      }
      const cell = mapdata_cell(ax, ay);
      if (
        !isFace(cell.heads[0]!.face, MAGIC_MAP_FACE_BASE, MAGIC_MAP_FACE_BASE)
      ) {
        continue;
      }
      cells++;
      expect(cell.state, `state at (${ax}, ${ay})`).toBe(MapCellState.Fog);
      if (
        isFace(
          cell.heads[1]!.face,
          MAGIC_MAP_WALL_FACE_BASE,
          MAGIC_MAP_WALL_FACE_BASE,
        )
      ) {
        walls++;
      }
    }
  }
  return { cells, walls };
}

function assertNoDanglingTailsInView(): void {
  const player = pl_mpos();
  for (let dx = -12; dx <= 12; dx++) {
    for (let dy = -12; dy <= 12; dy++) {
      const ax = player.px + dx;
      const ay = player.py + dy;
      if (!mapdata_contains(ax, ay)) {
        continue;
      }
      const cell = mapdata_cell(ax, ay);
      for (let layer = 0; layer < cell.tails.length; layer++) {
        const tail = cell.tails[layer]!;
        if (tail.face === 0) {
          continue;
        }
        const headX = ax + tail.sizeX;
        const headY = ay + tail.sizeY;
        expect(mapdata_contains(headX, headY)).toBe(true);
        const head = mapdata_cell(headX, headY).heads[layer]!;
        expect(head.face).toBe(tail.face);
        expect(head.sizeX > 1 || head.sizeY > 1).toBe(true);
      }
    }
  }
}

beforeAll(() => {
  installNodeTestGlobals();
  // Avoid browser-only globals failing during module evaluation.
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  (globalThis as any).Audio ??= class {
    loop = false;
    currentTime = 0;
    src = "";
    play(): Promise<void> {
      return Promise.resolve();
    }
    pause(): void {}
  };
});

beforeAll(async () => {
  // Keep the config in memory.  This must happen before importing modules
  // that read the config when they are loaded.
  const { useInMemoryConfigStore } = await import("../../src/lib/storage");
  useInMemoryConfigStore();

  ({ dispatchPacket } = await import("../../src/lib/commands"));
  ({ clientInit } = await import("../../src/lib/init"));
  ({ getFaceTileSize, MAGIC_MAP_FACE_BASE, MAGIC_MAP_WALL_FACE_BASE } =
    await import("../../src/lib/image"));
  ({ MapCellState } = await import("../../src/lib/protocol"));
  ({ setGetMapImageSize, mapdata_cell, mapdata_contains, pl_mpos } =
    await import("../../src/lib/mapdata"));
  ({ initCommands } = await import("../../src/lib/p_cmd"));
  ({ parseReplayLogFile, resetReplaySandboxState, toPacketBuffer } =
    await import("../../src/lib/replay"));
  ({ compareReplayMapdataSnapshot } =
    await import("../../src/lib/replay_mapdata_state"));

  clientInit();
  initCommands();
  setGetMapImageSize(getFaceTileSize);
});

describe("replay mapdata states", () => {
  test.each(cases)("$name", ({ log, mark, state }) => {
    const logText = readFileSync(path.join(rootDir, log), "utf8");
    const expectedState = JSON.parse(
      readFileSync(path.join(rootDir, state), "utf8"),
    ) as ReplayMapdataSnapshot;

    resetReplaySandboxState();
    replayToMark(logText, mark);

    const comparison = compareReplayMapdataSnapshot(expectedState);
    expect(
      comparison.ok,
      comparison.mismatches.length > 0
        ? comparison.mismatches.join("\n\n")
        : "State comparison failed without mismatch details",
    ).toBe(true);
  });

  test("clear_space in fog resets stale preserved layers", () => {
    const lines = readFileSync(
      path.join(rootDir, "tests/replay-mapdata/logs/zoo.log"),
      "utf8",
    ).split(/\r?\n/);

    resetReplaySandboxState();

    let absoluteX = 0;
    let absoluteY = 0;
    for (let i = 0; i < 1829; i++) {
      const rx = parseRawRxLine(lines[i] ?? "");
      if (rx) {
        dispatchPacket(toPacketBuffer(rx));
      }
      if (i === 1828) {
        const player = pl_mpos();
        absoluteX = player.px - 4;
        absoluteY = player.py - 2;
      }
    }

    for (let i = 1829; i < 1878; i++) {
      const rx = parseRawRxLine(lines[i] ?? "");
      if (rx) {
        dispatchPacket(toPacketBuffer(rx));
      }
    }

    const player = pl_mpos();
    const cell = mapdata_cell(absoluteX, absoluteY);
    expect({ dx: absoluteX - player.px, dy: absoluteY - player.py }).toEqual({
      dx: -2,
      dy: -3,
    });
    expect(cell.state).toBe(1);
    expect(cell.heads[0]!.face).toBe(1005);
    expect(cell.heads[6]!.face).toBe(0);
  });

  test("big-face head cell stays empty when only fog bookkeeping changes", () => {
    const logText = readFileSync(
      path.join(
        rootDir,
        "tests/replay-mapdata/logs/around-scorn-with-ui-log.log",
      ),
      "utf8",
    );

    resetReplaySandboxState();
    replayToMark(logText, "before arena disapears #1");

    const headX = 280;
    const headY = 272;
    let cell = mapdata_cell(headX, headY);
    expect(cell.state).toBe(0);
    expect(cell.heads[0]!.face).toBe(0);
    expect(cell.heads[1]!.face).toBe(645);

    resetReplaySandboxState();
    replayToMark(logText, "after arena disapears #1");

    cell = mapdata_cell(headX, headY);
    expect(cell.state).toBe(0);
    expect(cell.heads[0]!.face).toBe(0);
    expect(cell.heads[1]!.face).toBe(645);
  });

  test("remembered big monster is removed when it reappears overlapping", () => {
    const lines = readFileSync(
      path.join(
        rootDir,
        "tests/replay-mapdata/logs/broken-devil.converted.log",
      ),
      "utf8",
    ).split(/\r?\n/);

    resetReplaySandboxState();

    // The devil (4x4, layer 6) is remembered outside the view with its head
    // at (325, 268). After the player steps west it is seen again with its
    // head at (327, 268), overlapping the remembered image.
    const devilFace = 2427;
    const oldHeadX = 325;
    const oldHeadY = 268;
    for (let i = 0; i < 2440; i++) {
      const rx = parseRawRxLine(lines[i] ?? "");
      if (rx) {
        dispatchPacket(toPacketBuffer(rx));
      }
      if (i === 2413) {
        expect(mapdata_cell(oldHeadX, oldHeadY).heads[6]!.face).toBe(devilFace);
      }
    }

    expect(mapdata_cell(oldHeadX, oldHeadY).heads[6]!.face).not.toBe(devilFace);
    for (let x = oldHeadX - 3; x <= oldHeadX; x++) {
      for (let y = oldHeadY - 3; y <= oldHeadY; y++) {
        const tail = mapdata_cell(x, y).tails[6]!;
        const pointsToOldHead =
          tail.face === devilFace &&
          x + tail.sizeX === oldHeadX &&
          y + tail.sizeY === oldHeadY;
        expect(pointsToOldHead, `tail at (${x}, ${y})`).toBe(false);
      }
    }
  });

  test("big monster head in unseen cell is removed when it moves", () => {
    const lines = readFileSync(
      path.join(
        rootDir,
        "tests/replay-mapdata/logs/broken-wizard.converted.log",
      ),
      "utf8",
    ).split(/\r?\n/);

    resetReplaySandboxState();

    // The wizard (3x3, layer 6) has its head in a never-seen cell at
    // (270, 258). Line 542 clears that cell and puts the head at (271, 258).
    const oldHeadX = 270;
    const oldHeadY = 258;
    for (let i = 0; i < 542; i++) {
      const rx = parseRawRxLine(lines[i] ?? "");
      if (rx) {
        dispatchPacket(toPacketBuffer(rx));
      }
      if (i === 540) {
        const head = mapdata_cell(oldHeadX, oldHeadY).heads[6]!;
        expect(head.face).not.toBe(0);
        expect({ sizeX: head.sizeX, sizeY: head.sizeY }).toEqual({
          sizeX: 3,
          sizeY: 3,
        });
      }
    }

    expect(mapdata_cell(oldHeadX, oldHeadY).heads[6]!.face).toBe(0);
    expect(mapdata_cell(oldHeadX + 1, oldHeadY).heads[6]!.sizeX).toBe(3);
    // The old leftmost column must no longer be covered.
    for (let y = oldHeadY - 2; y <= oldHeadY; y++) {
      expect(mapdata_cell(oldHeadX - 2, y).tails[6]!.face, `y=${y}`).toBe(0);
    }
  });

  test("remembered big monster is removed when it reappears below the view", () => {
    const lines = readFileSync(
      path.join(rootDir, "tests/replay-mapdata/logs/zoo.log"),
      "utf8",
    ).split(/\r?\n/);

    resetReplaySandboxState();

    // A 2x3 monster on layer 6 is remembered in fog with its head at
    // (352, 291). Line 2710 scrolls south and the server shows it again from
    // just below the view with its head one cell to the right.
    const oldHeadX = 352;
    const oldHeadY = 291;
    for (let i = 0; i < 2710; i++) {
      const rx = parseRawRxLine(lines[i] ?? "");
      if (rx) {
        dispatchPacket(toPacketBuffer(rx));
      }
      if (i === 2708) {
        const head = mapdata_cell(oldHeadX, oldHeadY).heads[6]!;
        expect(head.face).not.toBe(0);
        expect({ sizeX: head.sizeX, sizeY: head.sizeY }).toEqual({
          sizeX: 2,
          sizeY: 3,
        });
      }
    }

    expect(mapdata_cell(oldHeadX, oldHeadY).heads[6]!.face).toBe(0);
    for (let x = oldHeadX - 1; x <= oldHeadX; x++) {
      for (let y = oldHeadY - 2; y <= oldHeadY; y++) {
        const tail = mapdata_cell(x, y).tails[6]!;
        const pointsToOldHead =
          tail.face !== 0 &&
          x + tail.sizeX === oldHeadX &&
          y + tail.sizeY === oldHeadY;
        expect(pointsToOldHead, `tail at (${x}, ${y})`).toBe(false);
      }
    }
  });

  test("big face the server does not refresh is removed, also from fog", () => {
    const lines = readFileSync(
      path.join(rootDir, "tests/replay-mapdata/logs/zoo.log"),
      "utf8",
    ).split(/\r?\n/);

    resetReplaySandboxState();

    // A 3x3 monster on layer 6 has its head in a fog cell at (354, 286) with
    // its top rows visible. Line 1614 clears that cell and shows the monster
    // one row lower, so the old copy is no longer refreshed by the server.
    const oldHeadX = 354;
    const oldHeadY = 286;
    for (let i = 0; i < 1614; i++) {
      const rx = parseRawRxLine(lines[i] ?? "");
      if (rx) {
        dispatchPacket(toPacketBuffer(rx));
      }
      if (i === 1612) {
        expect(mapdata_cell(oldHeadX, oldHeadY).heads[6]!.sizeX).toBe(3);
      }
    }

    expect(mapdata_cell(oldHeadX, oldHeadY).heads[6]!.face).toBe(0);
    for (let x = oldHeadX - 2; x <= oldHeadX; x++) {
      for (let y = oldHeadY - 2; y <= oldHeadY; y++) {
        const tail = mapdata_cell(x, y).tails[6]!;
        const pointsToOldHead =
          tail.face !== 0 &&
          x + tail.sizeX === oldHeadX &&
          y + tail.sizeY === oldHeadY;
        expect(pointsToOldHead, `tail at (${x}, ${y})`).toBe(false);
      }
    }
    expect(mapdata_cell(oldHeadX, oldHeadY + 1).heads[6]!.sizeX).toBe(3);
  });

  test("zoo replay has no dangling in-view tail-to-head links", () => {
    const logText = readFileSync(
      path.join(rootDir, "tests/replay-mapdata/logs/zoo.log"),
      "utf8",
    );

    resetReplaySandboxState();
    replayToMark(logText, "after retributor moves right #2");
    assertNoDanglingTailsInView();
  });

  test("magic mapping fills unseen cells around the player", () => {
    const logText = readFileSync(
      path.join(
        rootDir,
        "tests/replay-mapdata/logs/magic-mapping.converted.log",
      ),
      "utf8",
    );

    resetReplaySandboxState();
    replayToMark(logText, "before magic mapping");
    expect(countMagicMapCells()).toEqual({ cells: 0, walls: 0 });

    resetReplaySandboxState();
    replayToMark(logText, "after magic mapping");
    expect(countMagicMapCells()).toEqual({ cells: 534, walls: 57 });
  });
});
