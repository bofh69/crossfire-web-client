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
let mapdata_cell: (mx: number, my: number) => {
  heads: Array<{ face: number }>;
  state: number;
};
let pl_mpos: () => { px: number; py: number };
let getFaceTileSize: (face: number) => { w: number; h: number };
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

function installNodeTestGlobals(): void {
  if (!("localStorage" in globalThis)) {
    const store = new Map<string, string>();
    Object.defineProperty(globalThis, "localStorage", {
      configurable: true,
      value: {
        getItem(key: string) {
          return store.get(String(key)) ?? null;
        },
        setItem(key: string, value: string) {
          store.set(String(key), String(value));
        },
        removeItem(key: string) {
          store.delete(String(key));
        },
        clear() {
          store.clear();
        },
        key(index: number) {
          return Array.from(store.keys())[index] ?? null;
        },
        get length() {
          return store.size;
        },
      },
    });
  }

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

  function parseRawRxLine(line: string): Uint8Array | null {
    const parts = line.trim().split(/\s+/);
    if (parts[1] !== "RX") {
      return null;
    }
    const b64 = parts[3] ?? "";
    const buf = Buffer.from(b64, "base64");
    return new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
  }

  for (let index = 0; index <= targetMark.entryIndex; index++) {
    const entry = parsed.entries[index]!;
    if (entry.type !== "RX" || entry.payload === null) {
      continue;
    }
    dispatchPacket(toPacketBuffer(entry.payload));
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
  ({ dispatchPacket } = await import("../../src/lib/commands"));
  ({ clientInit } = await import("../../src/lib/init"));
  ({ getFaceTileSize } = await import("../../src/lib/image"));
  ({ setGetMapImageSize, mapdata_cell, pl_mpos } =
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
});
