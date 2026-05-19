import { clearNotifications } from "./cmd_notifications";
import { resetMapCommandState } from "./cmd_map";
import { resetStatsState } from "./cmd_stats";
import { clearWatchedCell } from "./debug";
import { gameEvents } from "./events";
import { resetImageCacheData } from "./image";
import { clientReset, wantConfig } from "./init";
import { resetItemState } from "./item";
import { mapdata_free, mapdata_newmap, mapdata_set_size } from "./mapdata";
import { resetPlayerCommandState } from "./player";

const textDecoder = new TextDecoder();

/**
 * One parsed ws-recording entry.
 */
export interface ReplayEntry {
  timestamp: number;
  type: "MARK" | "RX" | "TX";
  lineNumber: number;
  commandName: string | null;
  markerText: string | null;
  payload: Uint8Array | null;
  preview: string;
}

/**
 * One parsed MARK entry together with its index in the entry list.
 */
export interface ReplayMark {
  entryIndex: number;
  label: string;
  lineNumber: number;
  timestamp: number;
}

/**
 * Parsed replay log data.
 */
export interface ReplayParseResult {
  entries: ReplayEntry[];
  marks: ReplayMark[];
}

function parseCompactReplayLine(
  line: string,
  lineNumber: number,
): ReplayEntry | null {
  if (!line.trim()) {
    return null;
  }
  const parts = line.split("\t");
  if (parts.length < 3) {
    return null;
  }
  const timestamp = parseTimestamp(parts[0]!, lineNumber);
  const type = parts[1]!;
  if (type === "MARK") {
    const rawMarker = parts.slice(2).join("\t");
    const markerText = parseJsonMarker(rawMarker, lineNumber);
    return {
      timestamp,
      type: "MARK",
      lineNumber,
      commandName: null,
      markerText,
      payload: null,
      preview: markerText,
    };
  }
  if (type !== "RX" && type !== "TX") {
    return null;
  }
  if (parts.length < 4) {
    throw new Error(`Line ${lineNumber}: malformed ${type} entry`);
  }
  const expectedLength = parseByteLength(parts[2]!, lineNumber);
  const payload = decodeBase64(
    parts.slice(3).join("\t"),
    expectedLength,
    lineNumber,
  );
  return createTrafficEntry(timestamp, type, lineNumber, payload);
}

function parseConvertedReplayLine(
  line: string,
  lineNumber: number,
): ReplayEntry | null {
  if (!line.trim()) {
    return null;
  }
  const match = /^(\d+)\s+(TX|RX|MARK)(?:\s(.*))?$/.exec(line);
  if (!match) {
    throw new Error(`Line ${lineNumber}: unsupported replay format`);
  }
  const timestampText = match[1]!;
  const type = match[2]! as "MARK" | "RX" | "TX";
  const rest = match[3] ?? "";
  const timestamp = parseTimestamp(timestampText, lineNumber);
  if (type === "MARK") {
    const markerBytes = parseCString(rest, lineNumber);
    const markerText = textDecoder.decode(markerBytes);
    return {
      timestamp,
      type: "MARK",
      lineNumber,
      commandName: null,
      markerText,
      payload: null,
      preview: markerText,
    };
  }
  const payload = parseCString(rest, lineNumber);
  return createTrafficEntry(timestamp, type, lineNumber, payload);
}

function createTrafficEntry(
  timestamp: number,
  type: "RX" | "TX",
  lineNumber: number,
  payload: Uint8Array,
): ReplayEntry {
  const commandName = extractCommandName(payload);
  return {
    timestamp,
    type,
    lineNumber,
    commandName,
    markerText: null,
    payload,
    preview: payloadToPreview(payload),
  };
}

function parseTimestamp(value: string, lineNumber: number): number {
  if (!/^\d+$/.test(value)) {
    throw new Error(`Line ${lineNumber}: invalid timestamp "${value}"`);
  }
  const timestamp = Number(value);
  if (!Number.isSafeInteger(timestamp) || timestamp < 0) {
    throw new Error(`Line ${lineNumber}: invalid timestamp "${value}"`);
  }
  return timestamp;
}

function parseByteLength(value: string, lineNumber: number): number {
  if (!/^\d+$/.test(value)) {
    throw new Error(`Line ${lineNumber}: invalid byte length "${value}"`);
  }
  const byteLength = Number(value);
  if (!Number.isSafeInteger(byteLength) || byteLength < 0) {
    throw new Error(`Line ${lineNumber}: invalid byte length "${value}"`);
  }
  return byteLength;
}

function parseJsonMarker(value: string, lineNumber: number): string {
  let parsed: unknown;
  try {
    parsed = JSON.parse(value);
  } catch {
    throw new Error(`Line ${lineNumber}: invalid MARK JSON payload`);
  }
  if (typeof parsed !== "string") {
    throw new Error(`Line ${lineNumber}: MARK payload must be a string`);
  }
  return parsed;
}

function decodeBase64(
  value: string,
  expectedLength: number,
  lineNumber: number,
): Uint8Array {
  let decoded = "";
  try {
    decoded = atob(value);
  } catch {
    throw new Error(`Line ${lineNumber}: invalid base64 payload`);
  }
  const bytes = new Uint8Array(decoded.length);
  for (let i = 0; i < decoded.length; i++) {
    bytes[i] = decoded.charCodeAt(i);
  }
  if (bytes.length !== expectedLength) {
    throw new Error(
      `Line ${lineNumber}: byte length mismatch (expected ${expectedLength}, got ${bytes.length})`,
    );
  }
  return bytes;
}

function parseCString(text: string, lineNumber: number): Uint8Array {
  const bytes: number[] = [];
  for (let i = 0; i < text.length; i++) {
    const ch = text[i]!;
    if (ch !== "\\") {
      bytes.push(ch.charCodeAt(0));
      continue;
    }
    const next = text[++i];
    if (next === undefined) {
      throw new Error(`Line ${lineNumber}: trailing backslash in escaped text`);
    }
    if (next === "n") {
      bytes.push(0x0a);
      continue;
    }
    if (next === "\\" || next === '"') {
      bytes.push(next.charCodeAt(0));
      continue;
    }
    if (next === "x") {
      const hex = text.slice(i + 1, i + 3);
      if (!/^[0-9A-Fa-f]{2}$/.test(hex)) {
        throw new Error(`Line ${lineNumber}: invalid \\x escape`);
      }
      bytes.push(Number.parseInt(hex, 16));
      i += 2;
      continue;
    }
    bytes.push(next.charCodeAt(0));
  }
  return Uint8Array.from(bytes);
}

function extractCommandName(payload: Uint8Array): string | null {
  const endIndex = payload.findIndex((byte) => byte === 0x20 || byte === 0x0a);
  const commandBytes =
    endIndex === -1 ? payload : payload.subarray(0, endIndex);
  const trimmed = textDecoder.decode(commandBytes).trim();
  return trimmed.length > 0 ? trimmed : null;
}

function payloadToPreview(payload: Uint8Array): string {
  const maxLength = 120;
  const head = payload.subarray(0, Math.min(payload.length, maxLength));
  const preview = encodeCString(head);
  return payload.length > maxLength ? `${preview}…` : preview;
}

function encodeCString(bytes: Uint8Array): string {
  let output = "";
  for (const byte of bytes) {
    if (byte === 0x0a) {
      output += "\\n";
    } else if (byte === 0x5c) {
      output += "\\\\";
    } else if (byte === 0x22) {
      output += '\\"';
    } else if (byte >= 0x20 && byte <= 0x7e) {
      output += String.fromCharCode(byte);
    } else {
      output += `\\x${byte.toString(16).toUpperCase().padStart(2, "0")}`;
    }
  }
  return output;
}

/**
 * Parse a websocket recording log in either the compact downloaded format or
 * the converted `recording:convert` text format.
 */
export function parseReplayLogFile(text: string): ReplayParseResult {
  const entries: ReplayEntry[] = [];
  const marks: ReplayMark[] = [];
  const lines = text.split(/\r?\n/);

  for (let index = 0; index < lines.length; index++) {
    const line = lines[index]!;
    let entry: ReplayEntry | null = null;
    if (line.includes("\t")) {
      entry = parseCompactReplayLine(line, index + 1);
    }
    if (entry === null) {
      entry = parseConvertedReplayLine(line, index + 1);
    }
    if (entry === null) {
      continue;
    }
    entries.push(entry);
    if (entry.type === "MARK" && entry.markerText !== null) {
      marks.push({
        entryIndex: entries.length - 1,
        label: entry.markerText,
        lineNumber: entry.lineNumber,
        timestamp: entry.timestamp,
      });
    }
  }

  return { entries, marks };
}

/**
 * Convert replay payload bytes into a standalone ArrayBuffer that can be passed
 * to the normal packet dispatcher without retaining references to the source
 * Uint8Array view.
 *
 * @param payload Replay payload bytes for one RX entry.
 * @returns A detached ArrayBuffer copy suitable for dispatchPacket().
 */
export function toPacketBuffer(payload: Uint8Array): ArrayBuffer {
  return Uint8Array.from(payload).buffer;
}

/**
 * Reset the shared client modules that the replay page reuses so that replaying
 * from the beginning produces deterministic state.
 *
 * This is the replay tool's top-level reset hook: it orchestrates the lower-
 * level per-module reset helpers, then reinitialises mapdata and emits refresh
 * events so the reused UI components redraw against the cleared state.
 */
export function resetReplaySandboxState(): void {
  clearWatchedCell();
  resetMapCommandState();
  clearNotifications();
  resetStatsState();
  resetPlayerCommandState();
  resetItemState();
  resetImageCacheData();
  clientReset();
  mapdata_free();
  mapdata_set_size(wantConfig.mapWidth, wantConfig.mapHeight);
  mapdata_newmap();
  gameEvents.emit("knowledgeUpdate");
  gameEvents.emit("mapUpdate");
  gameEvents.emit("newMap");
  gameEvents.emit("playerUpdate");
  gameEvents.emit("questUpdate");
  gameEvents.emit("statsUpdate", {});
}

/**
 * Build a short human-readable summary for one parsed replay entry.
 */
export function describeReplayEntry(entry: ReplayEntry): string {
  if (entry.type === "MARK") {
    return entry.markerText ?? "";
  }
  return entry.preview;
}
