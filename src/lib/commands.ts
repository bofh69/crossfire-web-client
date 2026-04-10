/**
 * commands.ts — Server command dispatch table and packet router.
 *
 * Handler implementations live in dedicated modules:
 *   cmd_stats.ts  — stats / skill / experience table
 *   cmd_items.ts  — items, inventory, spells
 *   cmd_map.ts    — map tiles, scroll, magic map, setup, animations, smoothing
 *   cmd_sound.ts  — sound2, music
 *
 * This file re-exports shared state (playerStats, spells, etc.) so that
 * existing import sites continue to work unchanged.
 */

import { NDI_COLOR_MASK, CS_STAT_SKILLINFO, CS_NUM_SKILLS } from './protocol.js';
import { getIntFromData, getShortFromData, getStringFromData, CrossfireSocket } from './newsocket.js';
import { setCSocket as setItemSocket } from './item.js';
import { getImageInfo, getImageSums, Face2Cmd as imageFace2Cmd, Image2Cmd as imageImage2Cmd } from './image.js';
import { notifyNcomAck } from './player.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { gameEvents, type AccountPlayer } from './events.js';

// ── Re-exports from split modules ──────────────────────────────────────────
// Keep the public API surface compatible so downstream files don't need to
// change their import paths.

export { playerStats, skillNames, expTable, expBarPercent } from './cmd_stats.js';
export { spells } from './cmd_items.js';

// ── Handlers from split modules ────────────────────────────────────────────

import { StatsCmd } from './cmd_stats.js';
import { playerStats, skillNames, expTable } from './cmd_stats.js';
import {
  PlayerCmd, Item2Cmd, UpdateItemCmd, DeleteItemCmd, DeleteInventoryCmd,
  AddspellCmd, UpdspellCmd, DeleteSpellCmd,
} from './cmd_items.js';
import {
  SetupCmd, NewmapCmd, Map2Cmd, mapScrollCmd, MagicMapCmd,
  AnimCmd, SmoothCmd,
} from './cmd_map.js';
import { Sound2Cmd, MusicCmd } from './cmd_sound.js';

// ── Socket management ──────────────────────────────────────────────────────

let csocket: CrossfireSocket | null = null;

export function setSocket(sock: CrossfireSocket): void {
  csocket = sock;
  setItemSocket(sock);
}

// ── Small / protocol-level handlers (kept here) ────────────────────────────

function DrawInfoCmd(data: string): void {
  const spaceIdx = data.indexOf(' ');
  if (spaceIdx < 0) return;
  const color = parseInt(data.substring(0, spaceIdx));
  const message = data.substring(spaceIdx + 1);
  console.log(`[drawinfo] ${message}`);
  gameEvents.emit('drawInfo', color & NDI_COLOR_MASK, message);
}

function DrawExtInfoCmd(data: string): void {
  const s1 = data.indexOf(' ');
  const s2 = s1 >= 0 ? data.indexOf(' ', s1 + 1) : -1;
  const s3 = s2 >= 0 ? data.indexOf(' ', s2 + 1) : -1;
  if (s2 < 0) return;
  const color = parseInt(data);
  const type = parseInt(data.substring(s1 + 1));
  const subtype = parseInt(data.substring(s2 + 1));
  const message = s3 >= 0 ? data.substring(s3 + 1) : '';
  gameEvents.emit('drawExtInfo', color & NDI_COLOR_MASK, type, subtype, message);
}

function handleQuery(data: string): void {
  const spaceIdx = data.indexOf(' ');
  const flags = spaceIdx > 0 ? parseInt(data.substring(0, spaceIdx)) : 0;
  let prompt = spaceIdx > 0 ? data.substring(spaceIdx + 1) : data;
  if (prompt.endsWith('\n:') && prompt.length > 2) {
    prompt = prompt.slice(0, -2);
  }
  gameEvents.emit('query', flags, prompt.trim());
}

function TickCmd(data: DataView, _len: number): void {
  const tickNo = getIntFromData(data, 0);
  gameEvents.emit('tick', tickNo);
}

function ComcCmd(data: DataView, len: number): void {
  if (len < 2) {
    LOG(LogLevel.Error, 'ComcCmd', `Invalid comc length ${len} - ignoring`);
    return;
  }
  const seq = getShortFromData(data, 0);
  notifyNcomAck(seq);
}

function PickupCmd(data: DataView, _len: number): void {
  const mode = getIntFromData(data, 0);
  LOG(LogLevel.Debug, 'PickupCmd', `Pickup mode: ${mode}`);
  gameEvents.emit('pickupUpdate', mode >>> 0);
}

function FailureCmd(data: string): void {
  const spaceIdx = data.indexOf(' ');
  const command = spaceIdx > 0 ? data.substring(0, spaceIdx) : data;
  const message = spaceIdx > 0 ? data.substring(spaceIdx + 1) : '';
  LOG(LogLevel.Warning, 'FailureCmd', `${command}: ${message}`);
  gameEvents.emit('failure', command, message);
}

function AccountPlayersCmd(data: string): void {
  const players: AccountPlayer[] = [];
  const lines = data.split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    const parts = line.split(':');
    if (parts.length >= 6) {
      players.push({
        name: parts[0], charClass: parts[1], race: parts[2],
        face: parts[3], party: parts[4], map: parts[5],
        level: parts.length > 6 ? parseInt(parts[6]) : 0,
        faceNum: parts.length > 7 ? parseInt(parts[7]) : 0,
      });
    }
  }
  gameEvents.emit('accountPlayers', players);
}

function VersionCmd(data: string): void {
  const parts = data.split(' ', 3);
  const csVer = parts.length > 0 ? parseInt(parts[0]) : 0;
  const scVer = parts.length > 1 ? parseInt(parts[1]) : 0;
  const verStr = parts.length > 2 ? parts[2] : '';
  LOG(LogLevel.Info, 'VersionCmd', `Server version: cs=${csVer} sc=${scVer} ${verStr}`);
  gameEvents.emit('version', csVer, scVer, verStr);
}

function ReplyInfoCmd(data: DataView, len: number): void {
  const bytes = new Uint8Array(data.buffer, data.byteOffset, len);
  let spaceIdx = -1;
  for (let i = 0; i < len; i++) {
    if (bytes[i] === 32 || bytes[i] === 10) {
      spaceIdx = i;
      break;
    }
  }
  if (spaceIdx < 0) return;
  const infoType = getStringFromData(bytes, 0, spaceIdx);

  if (infoType === 'image_info') {
    getImageInfo(bytes.subarray(spaceIdx + 1), len - spaceIdx - 1);
  } else if (infoType === 'image_sums') {
    const sumsData = getStringFromData(bytes, spaceIdx + 1, len - spaceIdx - 1);
    getImageSums(sumsData, len - spaceIdx - 1);
  } else if (infoType === 'skill_info') {
    const text = new TextDecoder().decode(bytes.subarray(spaceIdx + 1));
    for (const line of text.split('\n')) {
      const colonIdx = line.indexOf(':');
      if (colonIdx < 0) continue;
      const statNum = parseInt(line.substring(0, colonIdx));
      const name = line.substring(colonIdx + 1).trim();
      const idx = statNum - CS_STAT_SKILLINFO;
      if (idx >= 0 && idx < CS_NUM_SKILLS && name.length > 0) {
        skillNames[idx] = name;
      }
    }
    gameEvents.emit('statsUpdate', playerStats);
  } else if (infoType === 'exp_table') {
    const rest = bytes.subarray(spaceIdx + 1);
    if (rest.length < 2) return;
    const dv = new DataView(rest.buffer, rest.byteOffset, rest.byteLength);
    const maxLevel = dv.getUint16(0, false);
    expTable.length = 0;
    expTable.push(BigInt(0));
    for (let level = 1; level <= maxLevel; level++) {
      const pos = 2 + (level - 1) * 8;
      if (pos + 8 > rest.length) break;
      expTable.push(dv.getBigInt64(pos, false));
    }
  } else if (infoType === 'motd' || infoType === 'news' || infoType === 'rules') {
    const text = new TextDecoder().decode(bytes.subarray(spaceIdx + 1));
    gameEvents.emit('replyInfo', infoType, text);
  }
  LOG(LogLevel.Debug, 'ReplyInfoCmd', `Info type: ${infoType}`);
}

function GoodbyeCmd(): void {
  LOG(LogLevel.Info, 'GoodbyeCmd', 'Server said goodbye');
  gameEvents.emit('goodbye');
}

function AddMeFail(): void {
  LOG(LogLevel.Warning, 'AddMeFail', 'Failed to add player');
  gameEvents.emit('addMeFail');
}

function AddMeSuccess(): void {
  LOG(LogLevel.Info, 'AddMeSuccess', 'Player added successfully');
  gameEvents.emit('addMeSuccess');
}

// ── Command dispatch table ─────────────────────────────────────────────────

type TextHandler = (data: string) => void;
type BinaryHandler = (data: DataView, len: number) => void;
type NoArgHandler = () => void;

interface CommandEntry {
  type: 'text' | 'binary' | 'none';
  handler: TextHandler | BinaryHandler | NoArgHandler;
}

const commandTable = new Map<string, CommandEntry>([
  ['setup', { type: 'text', handler: SetupCmd }],
  ['drawinfo', { type: 'text', handler: DrawInfoCmd }],
  ['drawextinfo', { type: 'text', handler: DrawExtInfoCmd }],
  ['stats', { type: 'binary', handler: StatsCmd }],
  ['query', { type: 'text', handler: handleQuery }],
  ['player', { type: 'binary', handler: PlayerCmd }],
  ['item2', { type: 'binary', handler: Item2Cmd }],
  ['upditem', { type: 'binary', handler: UpdateItemCmd }],
  ['delitem', { type: 'binary', handler: DeleteItemCmd }],
  ['delinv', { type: 'text', handler: DeleteInventoryCmd }],
  ['addspell', { type: 'binary', handler: AddspellCmd }],
  ['updspell', { type: 'binary', handler: UpdspellCmd }],
  ['delspell', { type: 'binary', handler: DeleteSpellCmd }],
  ['newmap', { type: 'none', handler: NewmapCmd }],
  ['map2', { type: 'binary', handler: Map2Cmd }],
  ['map_scroll', { type: 'text', handler: mapScrollCmd }],
  ['magicmap', { type: 'binary', handler: MagicMapCmd }],
  ['tick', { type: 'binary', handler: TickCmd }],
  ['comc', { type: 'binary', handler: ComcCmd }],
  ['pickup', { type: 'binary', handler: PickupCmd }],
  ['failure', { type: 'text', handler: FailureCmd }],
  ['accountplayers', { type: 'text', handler: AccountPlayersCmd }],
  ['anim', { type: 'binary', handler: AnimCmd }],
  ['smooth', { type: 'binary', handler: SmoothCmd }],
  ['version', { type: 'text', handler: VersionCmd }],
  ['replyinfo', { type: 'binary', handler: ReplyInfoCmd }],
  ['goodbye', { type: 'none', handler: GoodbyeCmd }],
  ['addme_failed', { type: 'none', handler: AddMeFail }],
  ['addme_success', { type: 'none', handler: AddMeSuccess }],
  ['face2', { type: 'binary', handler: imageFace2Cmd }],
  ['image2', { type: 'binary', handler: imageImage2Cmd }],
  ['sound2', { type: 'binary', handler: Sound2Cmd }],
  ['music', { type: 'text', handler: MusicCmd }],
]);

let logReceivedCommands = false;

/** Toggle (or explicitly set) command-receive logging. */
export function setLogReceivedCommands(enabled: boolean): void {
  logReceivedCommands = enabled;
}

/**
 * Dispatch a received command from the server.
 */
export function dispatchPacket(packet: ArrayBuffer): void {
  const t0 = performance.now();
  const bytes = new Uint8Array(packet);
  let spaceIdx = bytes.indexOf(32);
  if (spaceIdx < 0) spaceIdx = bytes.length;

  const cmdName = new TextDecoder().decode(bytes.subarray(0, spaceIdx));
  const entry = commandTable.get(cmdName);

  const dataStart = spaceIdx < bytes.length ? spaceIdx + 1 : bytes.length;
  const dataLen = bytes.length - dataStart;

  if (!entry) {
    LOG(LogLevel.Debug, 'dispatch', `Unknown command: ${cmdName} (${dataLen}B)`);
    return;
  }

  try {
    switch (entry.type) {
      case 'text': {
        const textData = new TextDecoder().decode(bytes.subarray(dataStart));
        if (logReceivedCommands) LOG(LogLevel.Debug, 'RX', `${cmdName} ${textData}`);
        (entry.handler as TextHandler)(textData);
        break;
      }
      case 'binary': {
        if (logReceivedCommands) LOG(LogLevel.Debug, 'RX', `${cmdName} <binary ${dataLen}B>`);
        const dataView = new DataView(packet, dataStart);
        (entry.handler as BinaryHandler)(dataView, dataLen);
        break;
      }
      case 'none':
        if (logReceivedCommands) LOG(LogLevel.Debug, 'RX', cmdName);
        (entry.handler as NoArgHandler)();
        break;
    }
  } catch (err) {
    LOG(LogLevel.Error, 'dispatch', `Exception handling ${cmdName}: ${err}`);
    console.error(err);
  }

  const elapsed = performance.now() - t0;
  if (elapsed > 2) {
    LOG(LogLevel.Warning, 'perf:dispatch', `${cmdName} handler took ${elapsed.toFixed(1)}ms (${dataLen}B)`);
  }
}
