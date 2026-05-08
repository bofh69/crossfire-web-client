/**
 * cmd_map.ts — Map-related command handlers.
 * Extracted from commands.ts.
 */

import {
  MAP2_COORD_OFFSET,
  MAP2_TYPE_CLEAR,
  MAP2_TYPE_DARKNESS,
  MAP2_TYPE_LABEL,
  MAP2_LAYER_START,
  MAX_VIEW,
  FACE_IS_ANIM,
  MAXLAYERS,
  MSG_TYPE_COMMAND,
  SC_ALWAYS,
  LogLevel,
  type Animation,
} from "./protocol.js";
import { BinaryReader } from "./binary_reader.js";
import {
  mapdata_newmap,
  mapdata_scroll,
  mapdata_set_face_layer,
  mapdata_set_anim_layer,
  mapdata_set_darkness,
  mapdata_set_smooth,
  mapdata_clear_space,
  mapdata_set_check_space,
  mapdata_clear_old,
  mapdata_set_size,
  mapdata_clear_label_view,
  mapdata_add_label,
  mapdata_save_fog,
  mapdata_restore_fog,
  mapdata_apply_magicmap,
} from "./mapdata.js";
import { animations } from "./item.js";
import { addSmooth } from "./image.js";
import { useConfig, getCpl } from "./init.js";
import { LOG } from "./misc.js";
import { gameEvents } from "./events.js";
import { perfLogging } from "../lib/debug";
import { sendCommand, getLastNcomSeqSent } from "./player.js";

// ──────────────────────────────────────────────────────────────────────────────
// Mapinfo-based fog-cache key detection
// ──────────────────────────────────────────────────────────────────────────────

/**
 * When true, the client sends a `mapinfo` command immediately after each
 * `newmap` packet and uses the server's response to derive the LRU cache key
 * for the fog-of-war snapshot.  Set to false to disable the feature entirely.
 */
export const USE_MAPINFO_FOR_FOG_CACHE = false;

/**
 * The server-supplied path of the map the player is currently on.
 * Empty string means the path is not yet known (first login, or feature disabled).
 */
let currentMapKey = "";

/** ncom sequence number of the pending `mapinfo` command, or -1 if not pending. */
let mapinfoNcomSeq = -1;

/**
 * Number of server `newmap` packets that must be ignored because they are
 * responses to our own outstanding "setup mapsize" requests.
 */
let pendingMapsizeSuppressions = 0;

/** Last mapsize requested by the client (used to learn server-side clamping). */
let lastRequestedMapsizeW = -1;
let lastRequestedMapsizeH = -1;

/**
 * Learned upper bound on effective server mapsize, inferred when the server
 * acknowledges a request with both width and height smaller than requested.
 * A zero value means "unknown".
 */
let learnedMapsizeCapW = 0;
let learnedMapsizeCapH = 0;

/**
 * Call this before sending a "setup mapsize" command to the server.
 *
 * Returns false when the request should be skipped:
 *  - another mapsize request is already outstanding (awaiting its `newmap`)
 *  - the request is larger than a previously learned server cap in both dims
 *
 * Returns true when the request should be sent and arms the guard in
 * {@link NewmapCmd} so that the server-triggered `newmap` packet is ignored.
 */
export function notifyMapsizeSent(width: number, height: number): boolean {
  if (pendingMapsizeSuppressions > 0) {
    return false;
  }
  if (
    learnedMapsizeCapW > 0 &&
    learnedMapsizeCapH > 0 &&
    width > learnedMapsizeCapW &&
    height > learnedMapsizeCapH
  ) {
    return false;
  }

  lastRequestedMapsizeW = width;
  lastRequestedMapsizeH = height;
  pendingMapsizeSuppressions++;
  return true;
}

/** A captured drawextinfo entry (type=MSG_TYPE_COMMAND, subtype=0). */
export interface DrawExtInfoEntry {
  color: number;
  type: number;
  subtype: number;
  message: string;
}

/**
 * Drawextinfo messages with type=MSG_TYPE_COMMAND, subtype=0 that arrived
 * while a mapinfo command is in flight.  The server sends up to three such
 * messages before the comc ack: the first contains the map path in parens,
 * an optional second has created/modified info, and an optional third is
 * free-form.  Other player-command results that happen to use the same
 * type/subtype are also captured and later forwarded to the InfoPanel.
 */
let mapinfoBuffer: DrawExtInfoEntry[] = [];

/**
 * Called by `DrawExtInfoCmd` while a mapinfo command is in flight.
 * Returns true if the entry was captured (caller must NOT emit it to the
 * InfoPanel immediately — it will be forwarded or suppressed when the comc
 * arrives).
 */
export function maybeCaptureMapinfoExtInfo(
  color: number,
  type: number,
  subtype: number,
  message: string,
): boolean {
  if (!USE_MAPINFO_FOR_FOG_CACHE) return false;
  if (mapinfoNcomSeq === -1) return false;
  if (type !== MSG_TYPE_COMMAND || subtype !== 0) return false;
  mapinfoBuffer.push({ color, type, subtype, message });
  return true;
}

/**
 * Called by `ComcCmd` with every acknowledged ncom sequence number.
 *
 * If `seq` matches the pending mapinfo command this function:
 *  1. Identifies which (if any) of the buffered drawextinfo entries contain
 *     the map path and extracts it.
 *  2. Saves `currentMapKey` to that path and restores fog for the new map.
 *  3. Returns the entries that should be forwarded to the InfoPanel (those
 *     that came from user commands, not from mapinfo itself).
 *
 * Returns null if `seq` does not match the pending mapinfo command.
 */
export function maybeProcessMapinfoComc(
  seq: number,
): DrawExtInfoEntry[] | null {
  if (!USE_MAPINFO_FOR_FOG_CACHE) return null;
  if (seq !== mapinfoNcomSeq) return null;

  mapinfoNcomSeq = -1;
  const buffer = mapinfoBuffer;
  mapinfoBuffer = [];

  const forInfoPanel: DrawExtInfoEntry[] = [];
  const len = buffer.length;

  if (len === 0) {
    return forInfoPanel;
  }

  // The mapinfo response is at most 3 messages, always at the end of the
  // buffer (most recently received).  Everything before the last 3 entries
  // came from user commands and must be forwarded to the InfoPanel.
  const candidateStart = Math.max(0, len - 3);
  for (let i = 0; i < candidateStart; i++) {
    forInfoPanel.push(buffer[i]!);
  }

  // candidates[0] = oldest of the last 3 ("third latest")
  // candidates[1] = middle ("second latest")
  // candidates[2] = newest, just before comc ("latest")
  const candidates = buffer.slice(candidateStart); // length 1, 2, or 3

  let mapKeyEntry: DrawExtInfoEntry | undefined;

  if (candidates.length === 1) {
    mapKeyEntry = candidates[0];
  } else if (candidates.length === 2) {
    // "If there are only two, use the second latest" = oldest of the pair (candidates[0]),
    // since "latest" is candidates[1] (most recent before comc).
    mapKeyEntry = candidates[0];
    // candidates[1] (latest/most-recent) is a mapinfo response — suppress it.
  } else {
    // candidates.length === 3
    const oldest = candidates[0]!; // "third latest" — expected to contain map path
    const middle = candidates[1]!; // "second latest"
    // candidates[2] is "latest" (free-form last message, always suppressed)

    if (hasParens(middle.message)) {
      // Unusual: map path ended up in the middle position.
      // "Use it and send the third latest (oldest) to the InfoPanel."
      mapKeyEntry = middle;
      forInfoPanel.push(oldest);
    } else {
      // Normal: map path is in the oldest of the three.
      // "If the second latest one doesn't have parenthesis, use the third latest."
      mapKeyEntry = oldest;
      // middle and latest are mapinfo responses — suppress them.
    }
  }

  // Extract the map path from the chosen entry and restore fog.
  if (mapKeyEntry) {
    const key = extractMapPath(mapKeyEntry.message);
    if (key) {
      currentMapKey = key;
      LOG(LogLevel.Info, "mapinfo", `Map path: ${key}`);
      mapdata_restore_fog(key);
      gameEvents.emit("mapUpdate");
    }
  }

  return forInfoPanel;
}

/** Return true if `message` contains a parenthesised substring. */
function hasParens(message: string): boolean {
  return message.includes("(");
}

/** Extract the first parenthesised substring from `message`, e.g. "/scorn/taverns/inn". */
function extractMapPath(message: string): string {
  const match = /\(([^)]+)\)/.exec(message);
  return match ? match[1]! : "";
}

export function SetupCmd(data: string): void {
  const parts = data.split(" ");
  for (let i = 0; i < parts.length - 1; i += 2) {
    const key = parts[i]!;
    const value = parts[i + 1]!;
    if (key === "mapsize" && value !== "FALSE") {
      const [w = NaN, h = NaN] = value.split("x").map(Number);
      if (!isNaN(w) && !isNaN(h) && w > 0 && h > 0) {
        if (
          lastRequestedMapsizeW > 0 &&
          lastRequestedMapsizeH > 0 &&
          w < lastRequestedMapsizeW &&
          h < lastRequestedMapsizeH
        ) {
          learnedMapsizeCapW = w;
          learnedMapsizeCapH = h;
        }
        useConfig.mapWidth = w;
        useConfig.mapHeight = h;
        mapdata_set_size(w, h);
      }
    } else if (key === "beat" && value !== "FALSE" && value !== "0") {
      // Server confirmed heartbeat support: the client must send a command
      // at least every three seconds or use the beat no-op.  The heartbeat
      // timer in client.ts fires every 2.5 s to provide a safety margin.
      gameEvents.emit("beatEnabled");
    } else if (key === "loginmethod" && value !== "FALSE") {
      const method = parseInt(value, 10);
      if (!isNaN(method)) {
        gameEvents.emit("loginMethodConfirmed", method);
      }
    }
    LOG(LogLevel.Debug, "SetupCmd", `${key} = ${value}`);
  }
}

export function NewmapCmd(): void {
  if (pendingMapsizeSuppressions > 0) {
    pendingMapsizeSuppressions--;
    // This newmap was triggered by our own "setup mapsize" request.
    // The server will follow it with the accepted size in a SetupCmd.
    // The player has not changed maps, so fog data must be preserved.
    return;
  }
  mapdata_save_fog(currentMapKey);
  mapdata_newmap();
  gameEvents.emit("newMap");

  if (USE_MAPINFO_FOR_FOG_CACHE) {
    mapinfoBuffer = [];
    mapinfoNcomSeq = -1;
    if (sendCommand("mapinfo", -1, SC_ALWAYS)) {
      mapinfoNcomSeq = getLastNcomSeqSent();
    }
  }
}

export function Map2Cmd(data: DataView, len: number): void {
  const t0 = performance.now();
  let tileCount = 0;
  const reader = new BinaryReader(data, len);
  while (reader.remaining > 0) {
    const mask = reader.readInt16();
    const x = ((mask >> 10) & 0x3f) - MAP2_COORD_OFFSET;
    const y = ((mask >> 4) & 0x3f) - MAP2_COORD_OFFSET;

    // A coord word with the scroll flag set encodes a map scroll, not a tile.
    if (mask & 0x1) {
      mapdata_scroll(x, y);
      continue;
    }

    // Clamp to the valid view coordinate range.
    const cx = Math.max(0, Math.min(x, MAX_VIEW - 1));
    const cy = Math.max(0, Math.min(y, MAX_VIEW - 1));

    mapdata_clear_old(cx, cy);
    tileCount++;

    // Inner loop: read per-tile type bytes until the 255 end-of-space marker.
    let labelsCleared = false;
    while (reader.remaining > 0) {
      const typeByte = reader.readUint8();

      if (typeByte === 255) {
        mapdata_set_check_space(cx, cy);
        break;
      }

      // Upper 3 bits encode the number of additional data bytes for this entry;
      // lower 5 bits are the entry type.
      const spaceLen = typeByte >> 5;
      const type = typeByte & 0x1f;

      if (type === MAP2_TYPE_CLEAR) {
        mapdata_clear_space(cx, cy);
      } else if (type === MAP2_TYPE_DARKNESS) {
        const value = reader.readUint8();
        mapdata_set_darkness(cx, cy, value);
      } else if (type === MAP2_TYPE_LABEL) {
        // spaceLen === 7 signals variable-length data: next byte is total length.
        reader.skip(1); // labelTotalLen (unused)
        const subtype = reader.readUint8();
        const strLen = reader.readUint8();
        const label = reader.readString(strLen);
        if (!labelsCleared) {
          mapdata_clear_label_view(cx, cy);
          labelsCleared = true;
        }
        mapdata_add_label(cx, cy, subtype, label);
      } else if (
        type >= MAP2_LAYER_START &&
        type < MAP2_LAYER_START + MAXLAYERS
      ) {
        const layer = type & 0xf;
        const faceOrAnim = reader.readInt16();
        if (!(faceOrAnim & FACE_IS_ANIM)) {
          mapdata_set_face_layer(cx, cy, faceOrAnim, layer);
        }
        if (spaceLen > 2) {
          const opt = reader.readUint8();
          if (faceOrAnim & FACE_IS_ANIM) {
            // opt is the animation speed.
            mapdata_set_anim_layer(cx, cy, faceOrAnim, opt, layer);
          } else {
            // opt is a smooth value.
            mapdata_set_smooth(cx, cy, opt, layer);
          }
        }
        // A fourth byte (when present) is always a smooth value.
        if (spaceLen > 3) {
          const opt = reader.readUint8();
          mapdata_set_smooth(cx, cy, opt, layer);
        }
      } else {
        // Unknown type: skip the declared number of data bytes.
        if (spaceLen !== 7) {
          reader.skip(spaceLen);
        } else {
          const extraLen = reader.readUint8();
          reader.skip(extraLen);
        }
      }
    }
  }
  const elapsed = performance.now() - t0;
  if (perfLogging && (elapsed > 1 || tileCount > 10)) {
    LOG(
      LogLevel.Debug,
      "perf:map2",
      `parsed ${tileCount} tiles from ${len}B in ${elapsed.toFixed(1)}ms`,
    );
  }
  gameEvents.emit("mapUpdate");
}

export function mapScrollCmd(data: string): void {
  const parts = data.trim().split(" ");
  if (parts.length >= 2) {
    mapdata_scroll(parseInt(parts[0]!), parseInt(parts[1]!));
    gameEvents.emit("mapUpdate");
  }
}

export function MagicMapCmd(data: DataView, len: number): void {
  // The magicmap packet is MIXED format: ASCII header + binary data.
  // Header: "%d %d %d %d " (mmapx mmapy pmapx pmapy), then raw bytes.
  const bytes = new Uint8Array(data.buffer, data.byteOffset, len);

  // Find the 4 spaces that separate the ASCII header values from the binary data.
  let spaceCount = 0;
  let dataOffset = 0;
  for (let i = 0; i < len; i++) {
    if (bytes[i] === 0x20) {
      // ASCII space
      spaceCount++;
      if (spaceCount === 4) {
        dataOffset = i + 1;
        break;
      }
    }
  }
  if (spaceCount !== 4) {
    LOG(
      LogLevel.Warning,
      "MagicMapCmd",
      "Unable to find start of magic map data",
    );
    return;
  }

  // Parse the ASCII header.
  const header = new TextDecoder().decode(bytes.subarray(0, dataOffset));
  const headerParts = header.trim().split(/\s+/);
  if (headerParts.length < 4) {
    LOG(LogLevel.Warning, "MagicMapCmd", "Could not parse magic map header");
    return;
  }

  const mmapx = parseInt(headerParts[0]!, 10);
  const mmapy = parseInt(headerParts[1]!, 10);
  const pmapx = parseInt(headerParts[2]!, 10);
  const pmapy = parseInt(headerParts[3]!, 10);

  if (mmapx === 0 || mmapy === 0) {
    LOG(LogLevel.Warning, "MagicMapCmd", "Empty magic map");
    return;
  }

  const dataLen = len - dataOffset;
  if (dataLen !== mmapx * mmapy) {
    LOG(
      LogLevel.Warning,
      "MagicMapCmd",
      `Magic map size mismatch. Have ${dataLen} bytes, should have ${mmapx * mmapy}`,
    );
    return;
  }

  const cpl = getCpl();
  if (!cpl) return;

  cpl.mmapx = mmapx;
  cpl.mmapy = mmapy;
  cpl.pmapx = pmapx;
  cpl.pmapy = pmapy;
  cpl.magicmap = new Uint8Array(
    bytes.subarray(dataOffset, dataOffset + dataLen),
  );
  cpl.showmagic = 1;

  // Apply magicmap data to the virtual map: fill Empty cells with fog tiles
  // derived from the magicmap colour palette.  This lets the game map render
  // previously-explored areas even before map2 packets arrive for them.
  mapdata_apply_magicmap(cpl.magicmap, mmapx, mmapy, pmapx, pmapy);

  LOG(
    LogLevel.Info,
    "MagicMapCmd",
    `Received magic map ${mmapx}x${mmapy}, player at (${pmapx},${pmapy})`,
  );

  // Redraw the game map to show the newly filled fog cells.
  gameEvents.emit("mapUpdate");
  // Notify MagicMap.svelte to redraw if the overlay is currently open.
  gameEvents.emit("magicMap");
}

export function AnimCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const animId = reader.readInt16();
  const animFlags = reader.readInt16();
  const faces: number[] = [];
  while (reader.remaining > 0) {
    faces.push(reader.readInt16());
  }
  const anim: Animation = {
    flags: animFlags,
    numAnimations: faces.length,
    speed: 0,
    speedLeft: 0,
    phase: 0,
    faces,
  };
  animations[animId] = anim;
}

export function SmoothCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const face = reader.readInt16();
  const smooth = reader.readInt16();
  addSmooth(face, smooth);
}
