import { readFileSync } from "node:fs";
import { describe, test, beforeAll } from "vitest";

let dispatchPacket: (buffer: ArrayBuffer) => void;
let clientInit: () => void;
let initCommands: () => void;
let getFaceTileSize: (face: number) => { w: number; h: number };
let setGetMapImageSize: (fn: (face: number) => { w: number; h: number }) => void;
let mapdata_cell: (mx: number, my: number) => any;
let pl_mpos: () => { px: number; py: number };
let pl_pos_debug: () => { x: number; y: number }; 
let parseReplayLogFile: (text: string) => any;
let resetReplaySandboxState: () => void;
let toPacketBuffer: (payload: Uint8Array) => ArrayBuffer;

function installNodeTestGlobals(): void {
  if (!("localStorage" in globalThis)) {
    const store = new Map<string, string>();
    Object.defineProperty(globalThis, "localStorage", {
      configurable: true,
      value: {
        getItem(key: string) { return store.get(String(key)) ?? null; },
        setItem(key: string, value: string) { store.set(String(key), String(value)); },
        removeItem(key: string) { store.delete(String(key)); },
        clear() { store.clear(); },
        key(index: number) { return Array.from(store.keys())[index] ?? null; },
        get length() { return store.size; },
      },
    });
  }
  const urlObj = URL as any;
  if (typeof urlObj.createObjectURL !== "function") urlObj.createObjectURL = () => "blob:test";
  if (typeof urlObj.revokeObjectURL !== "function") urlObj.revokeObjectURL = () => {};
}

beforeAll(() => {
  installNodeTestGlobals();
  (globalThis as any).Audio ??= class {
    loop = false; currentTime = 0; src = "";
    play(): Promise<void> { return Promise.resolve(); }
    pause(): void {}
  };
});

beforeAll(async () => {
  ({ dispatchPacket } = await import("../../src/lib/commands"));
  ({ clientInit } = await import("../../src/lib/init"));
  ({ getFaceTileSize } = await import("../../src/lib/image"));
  ({ setGetMapImageSize, mapdata_cell, pl_mpos } = await import("../../src/lib/mapdata"));
  ({ initCommands } = await import("../../src/lib/p_cmd"));
  ({ parseReplayLogFile, resetReplaySandboxState, toPacketBuffer } = await import("../../src/lib/replay"));
  clientInit();
  initCommands();
  setGetMapImageSize(getFaceTileSize);
});

describe("debug3", () => {
  test("step by entry", () => {
    const logText = readFileSync("tests/replay-mapdata/logs/big-face-in-scorn.log", "utf8");
    const parsed = parseReplayLogFile(logText);
    
    const afterUpdateMark = parsed.marks.find((m: any) => m.label === "after update")!;
    const fromTailMark = parsed.marks.find((m: any) => m.label === "from tail to head")!;
    
    resetReplaySandboxState();
    
    // Process up to "after update"
    for (let i = 0; i <= afterUpdateMark.entryIndex; i++) {
      const entry = parsed.entries[i]!;
      if (entry.type !== "RX" || entry.payload === null) continue;
      dispatchPacket(toPacketBuffer(entry.payload));
    }
    
    console.log("After 'after update': pl_mpos =", JSON.stringify(pl_mpos()));
    
    // Process entries one by one to "from tail to head"
    for (let i = afterUpdateMark.entryIndex + 1; i <= fromTailMark.entryIndex; i++) {
      const entry = parsed.entries[i]!;
      if (entry.type === "RX" && entry.payload !== null) {
        const before = JSON.stringify(pl_mpos());
        dispatchPacket(toPacketBuffer(entry.payload));
        const after = JSON.stringify(pl_mpos());
        console.log("Entry " + i + " cmd=" + (entry.commandName || 'unknown') + ": pl_mpos " + before + " -> " + after);
        
        // Check face645
        const p = pl_mpos();
        for (let dx = -2; dx <= 3; dx++) {
          const cell = mapdata_cell(p.px + dx, p.py);
          for (let l = 0; l < 9; l++) {
            if (cell.heads[l]?.face === 645) {
              console.log("  face645 head at dx=" + dx + " L" + l + " state=" + cell.state + " dark=" + cell.darkness);
            }
          }
        }
      }
    }
    
    console.log("Final pl_mpos =", JSON.stringify(pl_mpos()));
    const p = pl_mpos();
    const cell = mapdata_cell(p.px + 1, p.py);
    console.log("Cell dx=1: state=" + cell.state + " dark=" + cell.darkness);
    for (let l = 0; l < 9; l++) {
      if (cell.heads[l]?.face) {
        console.log("  head L" + l + " face=" + cell.heads[l].face + " size=" + cell.heads[l].sizeX + "x" + cell.heads[l].sizeY);
      }
    }
  });
});
