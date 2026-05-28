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
  ({ setGetMapImageSize } = await import("../../src/lib/mapdata"));
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
});
