import { readFileSync } from 'node:fs';
import { test, expect } from 'vitest';

function parseRx(line: string): Uint8Array | null {
  const parts = line.trim().split(/\s+/);
  if (parts[1] !== 'RX') return null;
  const b64 = parts[3] ?? '';
  const buf = Buffer.from(b64, 'base64');
  return new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
}

test('zoo line 1878 clears stale layer6', async () => {
  if (!("localStorage" in globalThis)) {
    const store = new Map<string, string>();
    Object.defineProperty(globalThis, 'localStorage', { value: {
      getItem: (k: string) => store.get(String(k)) ?? null,
      setItem: (k: string, v: string) => store.set(String(k), String(v)),
      removeItem: (k: string) => store.delete(String(k)),
      clear: () => store.clear(),
      key: (i: number) => Array.from(store.keys())[i] ?? null,
      get length() { return store.size; },
    }});
  }
  const u = URL as any;
  u.createObjectURL ??= () => 'blob:test';
  u.revokeObjectURL ??= () => {};
  (globalThis as any).Audio ??= class { play(){ return Promise.resolve(); } pause(){} };

  const { dispatchPacket } = await import('/tmp/workspace/bofh69/crossfire-web-client/src/lib/commands.ts');
  const { clientInit } = await import('/tmp/workspace/bofh69/crossfire-web-client/src/lib/init.ts');
  const { getFaceTileSize } = await import('/tmp/workspace/bofh69/crossfire-web-client/src/lib/image.ts');
  const { setGetMapImageSize, mapdata_cell, pl_mpos } = await import('/tmp/workspace/bofh69/crossfire-web-client/src/lib/mapdata.ts');
  const { initCommands } = await import('/tmp/workspace/bofh69/crossfire-web-client/src/lib/p_cmd.ts');
  const { resetReplaySandboxState, toPacketBuffer } = await import('/tmp/workspace/bofh69/crossfire-web-client/src/lib/replay.ts');

  clientInit();
  initCommands();
  setGetMapImageSize(getFaceTileSize);
  resetReplaySandboxState();

  const lines = readFileSync('/tmp/workspace/bofh69/crossfire-web-client/tests/replay-mapdata/logs/zoo.log', 'utf8').split(/\r?\n/);

  let absX = 0, absY = 0;
  for (let i = 0; i < 1829; i++) {
    const rx = parseRx(lines[i] ?? '');
    if (rx) dispatchPacket(toPacketBuffer(rx));
    if (i === 1828) {
      const p = pl_mpos();
      absX = p.px - 4;
      absY = p.py - 2;
    }
  }

  let p = pl_mpos();
  let c = mapdata_cell(absX, absY);
  expect(c.heads[6].face).toBe(2374);

  for (let i = 1829; i < 1878; i++) {
    const rx = parseRx(lines[i] ?? '');
    if (rx) dispatchPacket(toPacketBuffer(rx));
  }

  p = pl_mpos();
  c = mapdata_cell(absX, absY);
  expect({dx: absX - p.px, dy: absY - p.py}).toEqual({dx: -2, dy: -3});
  expect(c.state).toBe(1); // Visible
  expect(c.heads[0].face).toBe(1005);
  expect(c.heads[6].face).toBe(0);
});
