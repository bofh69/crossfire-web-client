/**
 * cmd_chargen.ts — Character-creation protocol helpers.
 *
 * Parses the binary payloads returned by the server for:
 *   replyinfo race_list   – pipe-separated list of race archetype names
 *   replyinfo class_list  – pipe-separated list of class archetype names
 *   replyinfo race_info   – detailed race data (name, stat bonuses, description, choices)
 *   replyinfo class_info  – detailed class data (same format as race_info)
 *   replyinfo newcharinfo – stat-point budget and constraints
 *
 * All parsing follows the format described in the Crossfire server protocol
 * and mirrored in old/common/commands.c (process_race_class_info,
 * get_new_char_info) and old/gtk-v2/src/create_char.c.
 */

import type { RaceClassEntry, NewCharInfo, StartingMapEntry } from './events.js';

/** Number of character stats used in the new-character flow. */
const NUM_CHARGEN_STATS = 7;

// ── Stat-ID → short name mapping ──────────────────────────────────────────
// Mirrors the stat_mapping[] array in old/gtk-v2/src/create_char.c and the
// CS_STAT_* constants in newclient.h.
const CS_STAT_TO_NAME: Readonly<Record<number, string>> = {
  5:  'Str',  // CS_STAT_STR
  6:  'Int',  // CS_STAT_INT
  7:  'Wis',  // CS_STAT_WIS
  8:  'Dex',  // CS_STAT_DEX
  9:  'Con',  // CS_STAT_CON
  10: 'Cha',  // CS_STAT_CHA
  22: 'Pow',  // CS_STAT_POW
};

/**
 * Default stat names in the display order used by the GTK client
 * (old/gtk-v2/src/create_char.c stat_mapping[]).
 * Note: this order differs from the CS_STAT_* numeric ordering above —
 * the server may also override these names via the `statname` field in
 * `replyinfo newcharinfo`, so always prefer server-supplied names.
 */
const DEFAULT_STAT_NAMES = ['Str', 'Dex', 'Con', 'Int', 'Wis', 'Pow', 'Cha'];

// ── Public parsing functions ───────────────────────────────────────────────

/**
 * Parse the text payload of `replyinfo race_list` or `replyinfo class_list`.
 *
 * The payload is a pipe-separated list of archetype names:
 *   `human|elf|dwarf|gnome|…`
 *
 * Empty segments (e.g. a leading `|`) are discarded.
 */
export function parseRaceClassList(text: string): string[] {
  return text.split('|').map(s => s.trim()).filter(s => s.length > 0);
}

/**
 * Parse the binary payload of `replyinfo race_info <arch>` or
 * `replyinfo class_info <arch>`.
 *
 * Binary format (from process_race_class_info in old/common/commands.c):
 *
 *   <archname>\n
 *   { keyword SP <binary-data> }…
 *
 * Supported keywords and their data formats:
 *   name   – 1-byte length, then that many UTF-8 bytes
 *   stats  – pairs of (1-byte CS_STAT id, 2-byte big-endian signed value)
 *            terminated by a zero stat-id byte
 *   msg    – 2-byte big-endian length, then that many UTF-8 bytes
 *   choice – 1-byte len, name bytes; 1-byte len, desc bytes;
 *            then zero or more (1-byte arch-len, arch; 1-byte desc-len, desc) pairs
 *            terminated by a zero arch-len byte
 */
export function parseRaceClassInfo(data: Uint8Array): RaceClassEntry {
  const dec = new TextDecoder();

  // ── archname (everything up to the first \n) ──
  let nl = -1;
  for (let i = 0; i < data.length; i++) {
    if (data[i] === 10) { nl = i; break; }
  }
  if (nl < 0) {
    return { archName: '', publicName: '', description: '', statAdj: {}, choices: [] };
  }

  const archName = dec.decode(data.subarray(0, nl));
  const result: RaceClassEntry = {
    archName,
    publicName: archName,  // fallback; overwritten by the 'name' field if present
    description: '',
    statAdj: {},
    choices: [],
  };

  // DataView wrapping the same underlying buffer so we can read multi-byte
  // integers relative to `data` offsets.
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  let pos = nl + 1;

  while (pos < data.length) {
    // ── keyword: text until the next space ──
    const kwStart = pos;
    while (pos < data.length && data[pos] !== 32 /* ' ' */) pos++;
    if (pos >= data.length) break;
    const keyword = dec.decode(data.subarray(kwStart, pos));
    pos++;  // skip the space

    if (keyword === 'name') {
      if (pos >= data.length) break;
      const len = data[pos]!;
      pos++;
      result.publicName = dec.decode(data.subarray(pos, pos + len));
      pos += len;

    } else if (keyword === 'stats') {
      // Sequence of (statId, int16) pairs terminated by statId == 0.
      while (pos < data.length) {
        const statId = data[pos]!;
        if (statId === 0) { pos++; break; }
        if (pos + 3 > data.length) break;
        const statVal = dv.getInt16(pos + 1, false);
        const statName = CS_STAT_TO_NAME[statId];
        if (statName) result.statAdj[statName] = statVal;
        pos += 3;
      }

    } else if (keyword === 'msg') {
      if (pos + 2 > data.length) break;
      const len = dv.getUint16(pos, false);
      pos += 2;
      result.description = dec.decode(data.subarray(pos, pos + len));
      pos += len;

    } else if (keyword === 'choice') {
      if (pos >= data.length) break;
      // choice_name
      const nameLen = data[pos]!; pos++;
      if (pos + nameLen > data.length) break;
      const choiceName = dec.decode(data.subarray(pos, pos + nameLen)); pos += nameLen;
      // choice_desc
      if (pos >= data.length) break;
      const descLen = data[pos]!; pos++;
      if (pos + descLen > data.length) break;
      const choiceDesc = dec.decode(data.subarray(pos, pos + descLen)); pos += descLen;
      // value pairs terminated by zero arch-length
      const values: Array<{ arch: string; desc: string }> = [];
      while (pos < data.length) {
        const vLen = data[pos]!; pos++;
        if (vLen === 0) break;
        if (pos + vLen > data.length) break;
        const arch = dec.decode(data.subarray(pos, pos + vLen)); pos += vLen;
        if (pos >= data.length) break;
        const dLen = data[pos]!; pos++;
        if (pos + dLen > data.length) break;
        const desc = dec.decode(data.subarray(pos, pos + dLen)); pos += dLen;
        values.push({ arch, desc });
      }
      result.choices.push({ name: choiceName, desc: choiceDesc, values });

    } else {
      // Unknown keyword — we can't determine its data length, so stop.
      break;
    }
  }

  return result;
}

/**
 * Parse the binary payload of `replyinfo newcharinfo`.
 *
 * The payload is a sequence of length-prefixed lines (from get_new_char_info
 * in old/common/commands.c).  Each line has the form:
 *
 *   [L: 1-byte length][datatype: 1-byte][space?][varname][space][value…]
 *   followed by a null byte at offset L from the length byte.
 *
 * Recognised variables:
 *   points    N           – total attribute points to spend
 *   statrange MIN MAX     – valid range for a single stat's total value
 *   statname  S1 S2 …    – short names of the 7 stats (e.g. "Str Dex …")
 *
 * Other variables (race, class, startingmap) are silently skipped for unknown values;
 * `startingmap requestinfo` sets `wantsStartingMap` to true in the result.
 */
export function parseNewCharInfo(data: Uint8Array): NewCharInfo {
  const dec = new TextDecoder();
  const result: NewCharInfo = {
    statPoints: 0,
    statMin: 1,
    statMax: 20,
    statNames: [...DEFAULT_STAT_NAMES],
    wantsStartingMap: false,
  };

  let olen = 0;
  while (olen < data.length) {
    const L = data[olen]!;
    if (L === 0) break;
    const llen = olen + L;  // position of the null terminator
    if (llen >= data.length) break;

    olen++;  // skip the L byte
    olen++;  // skip the datatype byte

    // Skip any leading whitespace before the variable name.
    while (olen < llen && (data[olen] === 32 || data[olen] === 9)) olen++;

    // Read variable name (until space or null).
    const nameStart = olen;
    while (olen < llen && data[olen] !== 32 && data[olen] !== 0) olen++;
    const varName = dec.decode(data.subarray(nameStart, olen)).toLowerCase();
    if (olen < llen) olen++;  // skip the space after the name

    // Value occupies the rest of the line up to (but not including) the null.
    const value = dec.decode(data.subarray(olen, llen)).trim();

    if (varName === 'points') {
      result.statPoints = parseInt(value, 10) || 0;
    } else if (varName === 'statrange') {
      const parts = value.split(/\s+/);
      if (parts.length >= 2) {
        result.statMin = parseInt(parts[0]!, 10) || 1;
        result.statMax = parseInt(parts[1]!, 10) || 20;
      }
    } else if (varName === 'statname') {
      const names = value.split(/\s+/).filter(n => n.length > 0);
      if (names.length === NUM_CHARGEN_STATS) result.statNames = names;
    } else if (varName === 'startingmap') {
      if (value.trim().toLowerCase() === 'requestinfo') {
        result.wantsStartingMap = true;
      }
    }

    olen = llen + 1;  // advance past the null terminator to the next line
  }

  return result;
}

// ── INFO_MAP type constants (from newclient.h) ─────────────────────────────
const INFO_MAP_ARCH_NAME    = 1;
const INFO_MAP_NAME         = 2;
const INFO_MAP_DESCRIPTION  = 3;

/**
 * Parse the binary payload of `replyinfo startingmap`.
 *
 * Binary format (from get_starting_map_info in old/common/commands.c):
 *
 *   Loop of: [1-byte type][2-byte big-endian length][length UTF-8 bytes]
 *
 * INFO_MAP_ARCH_NAME (1) starts a new map entry.
 * INFO_MAP_NAME      (2) sets the human-readable name of the current entry.
 * INFO_MAP_DESCRIPTION (3) sets the description.
 * Unknown types are silently skipped (length is still consumed).
 */
export function parseStartingMapInfo(data: Uint8Array): StartingMapEntry[] {
  const dec = new TextDecoder();
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const results: StartingMapEntry[] = [];
  let current: StartingMapEntry | null = null;
  let pos = 0;

  while (pos < data.length) {
    if (pos + 3 > data.length) break;
    const type = data[pos]!;
    pos++;
    const length = dv.getUint16(pos, false);
    pos += 2;
    if (pos + length > data.length) break;
    const text = dec.decode(data.subarray(pos, pos + length));
    pos += length;

    if (type === INFO_MAP_ARCH_NAME) {
      if (current) results.push(current);
      current = { archName: text, publicName: text, description: '' };
    } else if (current) {
      if (type === INFO_MAP_NAME) {
        current.publicName = text;
      } else if (type === INFO_MAP_DESCRIPTION) {
        current.description = text;
      }
    }
  }
  if (current) results.push(current);

  return results;
}
