/**
 * commands.ts - Server command handlers (converted from old/common/commands.c)
 * Handles dispatching and processing of server-to-client commands.
 */

import {
  CS_STAT_HP, CS_STAT_MAXHP, CS_STAT_SP, CS_STAT_MAXSP,
  CS_STAT_STR, CS_STAT_INT, CS_STAT_WIS, CS_STAT_DEX, CS_STAT_CON, CS_STAT_CHA,
  CS_STAT_LEVEL, CS_STAT_WC, CS_STAT_AC, CS_STAT_DAM, CS_STAT_ARMOUR,
  CS_STAT_SPEED, CS_STAT_FOOD, CS_STAT_WEAP_SP, CS_STAT_RANGE, CS_STAT_TITLE,
  CS_STAT_POW, CS_STAT_GRACE, CS_STAT_MAXGRACE, CS_STAT_FLAGS, CS_STAT_WEIGHT_LIM,
  CS_STAT_EXP64, CS_STAT_SPELL_ATTUNE, CS_STAT_SPELL_REPEL, CS_STAT_SPELL_DENY,
  CS_STAT_RESIST_START, CS_STAT_RESIST_END, CS_STAT_SKILLINFO, CS_NUM_SKILLS,
  CS_STAT_GOLEM_HP, CS_STAT_GOLEM_MAXHP,
  MAP2_COORD_OFFSET, MAP2_TYPE_CLEAR, MAP2_TYPE_DARKNESS, MAP2_TYPE_LABEL,
  MAP2_LAYER_START,
  UPD_LOCATION, UPD_FLAGS, UPD_WEIGHT, UPD_FACE, UPD_NAME, UPD_ANIM, UPD_ANIMSPEED, UPD_NROF,
  UPD_SP_MANA, UPD_SP_GRACE, UPD_SP_DAMAGE,
  NDI_COLOR_MASK,
  FACE_IS_ANIM, ANIM_MASK, ANIM_FLAGS_MASK,
  MAXLAYERS,
  VERSION_CS, VERSION_SC,
  type Stats, type Spell, type Animation,
} from './protocol.js';
import {
  getCharFromData, getShortFromData, getIntFromData, getInt64FromData,
  getStringFromData, SockList, CrossfireSocket,
} from './newsocket.js';
import { locateItem, removeItem, removeItemInventory, updateItem, playerItem, animations, setCSocket as setItemSocket } from './item.js';
import { mapdata_newmap, mapdata_scroll, mapdata_set_face_layer, mapdata_set_anim_layer, mapdata_set_darkness, mapdata_set_smooth, mapdata_clear_space, mapdata_set_check_space, mapdata_set_size, mapdata_clear_label, mapdata_add_label } from './mapdata.js';
import { addSmooth, getImageInfo, getImageSums, Face2Cmd as imageFace2Cmd, Image2Cmd as imageImage2Cmd } from './image.js';
import { wantConfig, useConfig, resetPlayerData } from './init.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';

/** Account player info */
export interface AccountPlayer {
  name: string;
  charClass: string;
  race: string;
  face: string;
  party: string;
  map: string;
  level: number;
  faceNum: number;
}

/** UI callback interface */
export interface CommandCallbacks {
  onDrawInfo?: (color: number, message: string) => void;
  onDrawExtInfo?: (color: number, type: number, subtype: number, message: string) => void;
  onStatsUpdate?: (stats: Partial<Stats>) => void;
  onQuery?: (flags: number, prompt: string) => void;
  onNewMap?: () => void;
  onMapUpdate?: () => void;
  onPlayerUpdate?: () => void;
  onSpellUpdate?: () => void;
  onAccountPlayers?: (players: AccountPlayer[]) => void;
  onFailure?: (command: string, message: string) => void;
  onTick?: (tickNo: number) => void;
  onGoodbye?: () => void;
  onAddMeSuccess?: () => void;
  onAddMeFail?: () => void;
  onVersion?: (csVersion: number, scVersion: number, versionString: string) => void;
}

export const callbacks: CommandCallbacks = {};

let csocket: CrossfireSocket | null = null;

export function setSocket(sock: CrossfireSocket): void {
  csocket = sock;
  setItemSocket(sock);
}

/** Current player stats */
export const playerStats: Stats = {
  Str: 0, Dex: 0, Con: 0, Wis: 0, Cha: 0, Int: 0, Pow: 0,
  wc: 0, ac: 0, level: 0, hp: 0, maxhp: 0, sp: 0, maxsp: 0,
  grace: 0, maxgrace: 0, exp: BigInt(0), food: 0, dam: 0,
  speed: 0, weaponSp: 0, attuned: 0, repelled: 0, denied: 0,
  flags: 0, resists: new Array(30).fill(0), resistChange: false,
  skillLevel: new Array(CS_NUM_SKILLS).fill(0),
  skillExp: new Array(CS_NUM_SKILLS).fill(BigInt(0)),
  weightLimit: 0,
  golemHp: 0, golemMaxhp: 0,
};

/** Known spells */
export const spells: Spell[] = [];

/** Text decoder for binary data */
const textDecoder = new TextDecoder();

function parseString(data: DataView, offset: number): { str: string; newOffset: number } {
  const bytes = new Uint8Array(data.buffer, data.byteOffset + offset);
  const nullIdx = bytes.indexOf(0);
  const len = nullIdx >= 0 ? nullIdx : bytes.length;
  const str = textDecoder.decode(bytes.subarray(0, len));
  return { str, newOffset: offset + len + (nullIdx >= 0 ? 1 : 0) };
}

// --- Command Handlers ---

function SetupCmd(data: string): void {
  const parts = data.split(' ');
  for (let i = 0; i < parts.length - 1; i += 2) {
    const key = parts[i];
    const value = parts[i + 1];
    if (key === 'mapsize' && value !== 'FALSE') {
      const [w, h] = value.split('x').map(Number);
      if (w > 0 && h > 0) {
        useConfig[19] = w; // CONFIG_MAPWIDTH
        useConfig[20] = h; // CONFIG_MAPHEIGHT
        mapdata_set_size(w, h);
      }
    }
    LOG(LogLevel.Debug, 'SetupCmd', `${key} = ${value}`);
  }
}

function DrawInfoCmd(data: string): void {
  const spaceIdx = data.indexOf(' ');
  if (spaceIdx < 0) return;
  const color = parseInt(data.substring(0, spaceIdx));
  const message = data.substring(spaceIdx + 1);
  console.log(`[drawinfo] ${message}`);
  callbacks.onDrawInfo?.(color & NDI_COLOR_MASK, message);
}

function DrawExtInfoCmd(data: string): void {
  const parts = data.split(' ', 4);
  if (parts.length < 4) return;
  const color = parseInt(parts[0]);
  const type = parseInt(parts[1]);
  const subtype = parseInt(parts[2]);
  const message = data.substring(data.indexOf(parts[3]));
  callbacks.onDrawExtInfo?.(color & NDI_COLOR_MASK, type, subtype, message);
}

function StatsCmd(data: DataView, len: number): void {
  let pos = 0;
  while (pos < len) {
    const stat = getCharFromData(data, pos); pos += 1;
    switch (stat) {
      case CS_STAT_HP: playerStats.hp = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_MAXHP: playerStats.maxhp = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_SP: playerStats.sp = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_MAXSP: playerStats.maxsp = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_GRACE: playerStats.grace = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_MAXGRACE: playerStats.maxgrace = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_STR: playerStats.Str = getCharFromData(data, pos); pos += 1; break;
      case CS_STAT_INT: playerStats.Int = getCharFromData(data, pos); pos += 1; break;
      case CS_STAT_WIS: playerStats.Wis = getCharFromData(data, pos); pos += 1; break;
      case CS_STAT_DEX: playerStats.Dex = getCharFromData(data, pos); pos += 1; break;
      case CS_STAT_CON: playerStats.Con = getCharFromData(data, pos); pos += 1; break;
      case CS_STAT_CHA: playerStats.Cha = getCharFromData(data, pos); pos += 1; break;
      case CS_STAT_POW: playerStats.Pow = getCharFromData(data, pos); pos += 1; break;
      case CS_STAT_LEVEL: playerStats.level = getCharFromData(data, pos); pos += 1; break;
      case CS_STAT_WC: playerStats.wc = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_AC: playerStats.ac = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_DAM: playerStats.dam = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_ARMOUR: playerStats.ac = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_SPEED: playerStats.speed = getIntFromData(data, pos); pos += 4; break;
      case CS_STAT_FOOD: playerStats.food = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_WEAP_SP: playerStats.weaponSp = getIntFromData(data, pos); pos += 4; break;
      case CS_STAT_FLAGS: playerStats.flags = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_WEIGHT_LIM: playerStats.weightLimit = getIntFromData(data, pos); pos += 4; break;
      case CS_STAT_EXP64: playerStats.exp = getInt64FromData(data, pos); pos += 8; break;
      case CS_STAT_SPELL_ATTUNE: playerStats.attuned = getIntFromData(data, pos); pos += 4; break;
      case CS_STAT_SPELL_REPEL: playerStats.repelled = getIntFromData(data, pos); pos += 4; break;
      case CS_STAT_SPELL_DENY: playerStats.denied = getIntFromData(data, pos); pos += 4; break;
      case CS_STAT_GOLEM_HP: playerStats.golemHp = getIntFromData(data, pos); pos += 4; break;
      case CS_STAT_GOLEM_MAXHP: playerStats.golemMaxhp = getIntFromData(data, pos); pos += 4; break;
      case CS_STAT_RANGE: {
        const r = parseString(data, pos);
        pos = r.newOffset;
        break;
      }
      case CS_STAT_TITLE: {
        const t = parseString(data, pos);
        pos = t.newOffset;
        break;
      }
      default:
        if (stat >= CS_STAT_RESIST_START && stat <= CS_STAT_RESIST_END) {
          playerStats.resists[stat - CS_STAT_RESIST_START] = getShortFromData(data, pos);
          playerStats.resistChange = true;
          pos += 2;
        } else if (stat >= CS_STAT_SKILLINFO && stat < CS_STAT_SKILLINFO + CS_NUM_SKILLS) {
          const skillIdx = stat - CS_STAT_SKILLINFO;
          playerStats.skillLevel[skillIdx] = getCharFromData(data, pos); pos += 1;
          playerStats.skillExp[skillIdx] = getInt64FromData(data, pos); pos += 8;
        } else {
          LOG(LogLevel.Warning, 'StatsCmd', `Unknown stat ${stat}`);
          return; // Can't continue - unknown length
        }
        break;
    }
  }
  callbacks.onStatsUpdate?.(playerStats);
}

function handleQuery(data: string): void {
  const spaceIdx = data.indexOf(' ');
  const flags = spaceIdx > 0 ? parseInt(data.substring(0, spaceIdx)) : 0;
  const prompt = spaceIdx > 0 ? data.substring(spaceIdx + 1) : data;
  callbacks.onQuery?.(flags, prompt);
}

function PlayerCmd(data: DataView, len: number): void {
  let pos = 0;
  const tag = getIntFromData(data, pos); pos += 4;
  const weight = getIntFromData(data, pos); pos += 4;
  const face = getIntFromData(data, pos); pos += 4;
  const nameResult = parseString(data, pos);
  LOG(LogLevel.Info, 'PlayerCmd', `Player: ${nameResult.str} tag=${tag}`);
  callbacks.onPlayerUpdate?.();
}

function Item2Cmd(data: DataView, len: number): void {
  let pos = 0;
  const location = getIntFromData(data, pos); pos += 4;
  while (pos < len) {
    const tag = getIntFromData(data, pos); pos += 4;
    const flags = getShortFromData(data, pos); pos += 2;
    const weight = getIntFromData(data, pos); pos += 4;
    const face = getIntFromData(data, pos); pos += 4;
    const nameLen = getCharFromData(data, pos); pos += 1;
    const name = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, nameLen);
    pos += nameLen;
    const anim = getShortFromData(data, pos); pos += 2;
    const animSpeed = getCharFromData(data, pos); pos += 1;
    const nrof = getIntFromData(data, pos); pos += 4;
    const type = getShortFromData(data, pos); pos += 2;
    updateItem(tag, location, name, weight, face, flags, anim, animSpeed, nrof, type);
  }
}

function UpdateItemCmd(data: DataView, len: number): void {
  let pos = 0;
  const updateFlags = getCharFromData(data, pos); pos += 1;
  const tag = getIntFromData(data, pos); pos += 4;
  const item = locateItem(tag);
  if (!item) return;

  if (updateFlags & UPD_LOCATION) {
    const loc = getIntFromData(data, pos); pos += 4;
  }
  if (updateFlags & UPD_FLAGS) {
    const flags = getShortFromData(data, pos); pos += 2;
  }
  if (updateFlags & UPD_WEIGHT) {
    const weight = getIntFromData(data, pos); pos += 4;
    item.weight = weight / 1000;
  }
  if (updateFlags & UPD_FACE) {
    item.face = getIntFromData(data, pos); pos += 4;
  }
  if (updateFlags & UPD_NAME) {
    const nameLen = getCharFromData(data, pos); pos += 1;
    const name = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, nameLen);
    pos += nameLen;
    item.dName = name;
  }
  if (updateFlags & UPD_ANIM) {
    item.animationId = getShortFromData(data, pos); pos += 2;
  }
  if (updateFlags & UPD_ANIMSPEED) {
    item.animSpeed = getCharFromData(data, pos); pos += 1;
  }
  if (updateFlags & UPD_NROF) {
    item.nrof = getIntFromData(data, pos); pos += 4;
  }
}

function DeleteItemCmd(data: DataView, len: number): void {
  let pos = 0;
  while (pos < len) {
    const tag = getIntFromData(data, pos); pos += 4;
    const item = locateItem(tag);
    if (item) removeItem(item);
  }
}

function DeleteInventoryCmd(data: DataView, len: number): void {
  const tag = getIntFromData(data, 0);
  const item = locateItem(tag);
  if (item) removeItemInventory(item);
}

function AddspellCmd(data: DataView, len: number): void {
  let pos = 0;
  while (pos < len) {
    const tag = getIntFromData(data, pos); pos += 4;
    const spellLevel = getShortFromData(data, pos); pos += 2;
    const castingTime = getShortFromData(data, pos); pos += 2;
    const mana = getShortFromData(data, pos); pos += 2;
    const grace = getShortFromData(data, pos); pos += 2;
    const damage = getShortFromData(data, pos); pos += 2;
    const skill = getCharFromData(data, pos); pos += 1;
    const path = getIntFromData(data, pos); pos += 4;
    const face = getIntFromData(data, pos); pos += 4;
    const nameLen = getCharFromData(data, pos); pos += 1;
    const name = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, nameLen);
    pos += nameLen;
    const msgLen = getShortFromData(data, pos); pos += 2;
    const message = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, msgLen);
    pos += msgLen;

    const spell: Spell = {
      name, message, tag, level: spellLevel, time: castingTime,
      sp: mana, grace: grace, dam: damage, skillNumber: skill,
      skill: '', path, face, usage: 0, requirements: '',
    };
    spells.push(spell);
  }
  callbacks.onSpellUpdate?.();
}

function UpdspellCmd(data: DataView, len: number): void {
  let pos = 0;
  const flags = getCharFromData(data, pos); pos += 1;
  const tag = getIntFromData(data, pos); pos += 4;
  const spell = spells.find(s => s.tag === tag);
  if (!spell) return;
  if (flags & UPD_SP_MANA) { spell.sp = getShortFromData(data, pos); pos += 2; }
  if (flags & UPD_SP_GRACE) { spell.grace = getShortFromData(data, pos); pos += 2; }
  if (flags & UPD_SP_DAMAGE) { spell.dam = getShortFromData(data, pos); pos += 2; }
  callbacks.onSpellUpdate?.();
}

function DeleteSpellCmd(data: DataView, len: number): void {
  const tag = getIntFromData(data, 0);
  const idx = spells.findIndex(s => s.tag === tag);
  if (idx >= 0) spells.splice(idx, 1);
  callbacks.onSpellUpdate?.();
}

function NewmapCmd(): void {
  mapdata_newmap();
  callbacks.onNewMap?.();
}

function Map2Cmd(data: DataView, len: number): void {
  let pos = 0;
  while (pos < len) {
    const coord = getShortFromData(data, pos); pos += 2;
    const x = ((coord >> 10) & 0x3F) - MAP2_COORD_OFFSET;
    const y = ((coord >> 4) & 0x3F) - MAP2_COORD_OFFSET;
    const coordType = coord & 0x0F;

    if (coordType === MAP2_TYPE_CLEAR) {
      mapdata_clear_space(x, y);
    } else if (coordType === MAP2_TYPE_DARKNESS) {
      const darkness = getCharFromData(data, pos); pos += 1;
      mapdata_set_darkness(x, y, darkness);
    } else if (coordType === MAP2_TYPE_LABEL) {
      const labelLen = getCharFromData(data, pos); pos += 1;
      const subtype = getCharFromData(data, pos); pos += 1;
      const label = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, labelLen - 1);
      pos += labelLen - 1;
      mapdata_add_label(x, y, subtype, label);
    } else if (coordType >= MAP2_LAYER_START) {
      const layer = coordType - MAP2_LAYER_START;
      if (layer < MAXLAYERS) {
        const faceOrAnim = getShortFromData(data, pos); pos += 2;
        if (faceOrAnim & FACE_IS_ANIM) {
          const animId = faceOrAnim & ANIM_MASK;
          const animFlags = faceOrAnim & ANIM_FLAGS_MASK;
          const animSpeed = getCharFromData(data, pos); pos += 1;
          mapdata_set_anim_layer(x, y, animId, animSpeed, layer);
        } else {
          mapdata_set_face_layer(x, y, faceOrAnim, layer);
        }
        // Check for smooth data
        if (pos < len) {
          const nextCoord = getShortFromData(data, pos);
          // Smooth data would follow but we skip complex smooth handling for simplicity
        }
      }
    }
    mapdata_set_check_space(x, y);
  }
  callbacks.onMapUpdate?.();
}

function mapScrollCmd(data: string): void {
  const parts = data.trim().split(' ');
  if (parts.length >= 2) {
    mapdata_scroll(parseInt(parts[0]), parseInt(parts[1]));
    callbacks.onMapUpdate?.();
  }
}

function MagicMapCmd(data: DataView, len: number): void {
  // Simplified - just log receipt
  LOG(LogLevel.Info, 'MagicMapCmd', `Received magic map data (${len} bytes)`);
}

function TickCmd(data: DataView, _len: number): void {
  const tickNo = getIntFromData(data, 0);
  callbacks.onTick?.(tickNo);
}

function PickupCmd(data: DataView, _len: number): void {
  const mode = getIntFromData(data, 0);
  LOG(LogLevel.Debug, 'PickupCmd', `Pickup mode: ${mode}`);
}

function FailureCmd(data: string): void {
  const spaceIdx = data.indexOf(' ');
  const command = spaceIdx > 0 ? data.substring(0, spaceIdx) : data;
  const message = spaceIdx > 0 ? data.substring(spaceIdx + 1) : '';
  LOG(LogLevel.Warning, 'FailureCmd', `${command}: ${message}`);
  callbacks.onFailure?.(command, message);
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
  callbacks.onAccountPlayers?.(players);
}

function AnimCmd(data: DataView, len: number): void {
  let pos = 0;
  const animId = getShortFromData(data, pos); pos += 2;
  const animFlags = getShortFromData(data, pos); pos += 2;
  const faces: number[] = [];
  while (pos < len) {
    faces.push(getShortFromData(data, pos));
    pos += 2;
  }
  const anim: Animation = {
    flags: animFlags,
    numAnimations: faces.length,
    speed: 0, speedLeft: 0, phase: 0,
    faces,
  };
  animations[animId] = anim;
}

function SmoothCmd(data: DataView, _len: number): void {
  const face = getShortFromData(data, 0);
  const smooth = getShortFromData(data, 2);
  addSmooth(face, smooth);
}

function VersionCmd(data: string): void {
  const parts = data.split(' ', 3);
  const csVer = parts.length > 0 ? parseInt(parts[0]) : 0;
  const scVer = parts.length > 1 ? parseInt(parts[1]) : 0;
  const verStr = parts.length > 2 ? parts[2] : '';
  LOG(LogLevel.Info, 'VersionCmd', `Server version: cs=${csVer} sc=${scVer} ${verStr}`);
  callbacks.onVersion?.(csVer, scVer, verStr);
}

function ReplyInfoCmd(data: DataView, len: number): void {
  const bytes = new Uint8Array(data.buffer, data.byteOffset, len);
  const spaceIdx = bytes.indexOf(32); // space
  if (spaceIdx < 0) return;
  const infoType = getStringFromData(bytes, 0, spaceIdx);

  if (infoType === 'image_info') {
    getImageInfo(bytes.subarray(spaceIdx + 1), len - spaceIdx - 1);
  } else if (infoType === 'image_sums') {
    const sumsData = getStringFromData(bytes, spaceIdx + 1, len - spaceIdx - 1);
    getImageSums(sumsData, len - spaceIdx - 1);
  }
  LOG(LogLevel.Debug, 'ReplyInfoCmd', `Info type: ${infoType}`);
}

function GoodbyeCmd(): void {
  LOG(LogLevel.Info, 'GoodbyeCmd', 'Server said goodbye');
  callbacks.onGoodbye?.();
}

function AddMeFail(): void {
  LOG(LogLevel.Warning, 'AddMeFail', 'Failed to add player');
  callbacks.onAddMeFail?.();
}

function AddMeSuccess(): void {
  LOG(LogLevel.Info, 'AddMeSuccess', 'Player added successfully');
  callbacks.onAddMeSuccess?.();
}

/** Command dispatch table */
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
  ['delinv', { type: 'binary', handler: DeleteInventoryCmd }],
  ['addspell', { type: 'binary', handler: AddspellCmd }],
  ['updspell', { type: 'binary', handler: UpdspellCmd }],
  ['delspell', { type: 'binary', handler: DeleteSpellCmd }],
  ['newmap', { type: 'none', handler: NewmapCmd }],
  ['map2', { type: 'binary', handler: Map2Cmd }],
  ['map_scroll', { type: 'text', handler: mapScrollCmd }],
  ['magicmap', { type: 'binary', handler: MagicMapCmd }],
  ['tick', { type: 'binary', handler: TickCmd }],
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
]);

/**
 * Dispatch a received command from the server.
 * The raw packet is: command_name (ASCII) + space + binary/text data
 */
export function dispatchPacket(packet: ArrayBuffer): void {
  const bytes = new Uint8Array(packet);
  // Find the space separating command name from data
  let spaceIdx = bytes.indexOf(32); // ASCII space
  if (spaceIdx < 0) spaceIdx = bytes.length;

  const cmdName = new TextDecoder().decode(bytes.subarray(0, spaceIdx));
  const entry = commandTable.get(cmdName);

  if (!entry) {
    LOG(LogLevel.Debug, 'dispatch', `Unknown command: ${cmdName}`);
    return;
  }

  const dataStart = spaceIdx < bytes.length ? spaceIdx + 1 : bytes.length;

  switch (entry.type) {
    case 'text': {
      const textData = new TextDecoder().decode(bytes.subarray(dataStart));
      (entry.handler as TextHandler)(textData);
      break;
    }
    case 'binary': {
      const dataView = new DataView(packet, dataStart);
      (entry.handler as BinaryHandler)(dataView, bytes.length - dataStart);
      break;
    }
    case 'none':
      (entry.handler as NoArgHandler)();
      break;
  }
}
