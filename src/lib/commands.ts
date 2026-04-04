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
  MAP2_LAYER_START, MAX_VIEW,
  UPD_LOCATION, UPD_FLAGS, UPD_WEIGHT, UPD_FACE, UPD_NAME, UPD_ANIM, UPD_ANIMSPEED, UPD_NROF,
  UPD_SP_MANA, UPD_SP_GRACE, UPD_SP_DAMAGE,
  NDI_COLOR_MASK,
  FACE_IS_ANIM,
  MAXLAYERS,
  VERSION_CS, VERSION_SC,
  type Stats, type Spell, type Animation,
} from './protocol.js';
import {
  getCharFromData, getShortFromData, getIntFromData, getInt64FromData,
  getStringFromData, SockList, CrossfireSocket,
} from './newsocket.js';
import { locateItem, removeItem, removeItemInventory, updateItem, playerItem, animations, setCSocket as setItemSocket, registerPlayerTag } from './item.js';
import { mapdata_newmap, mapdata_scroll, mapdata_set_face_layer, mapdata_set_anim_layer, mapdata_set_darkness, mapdata_set_smooth, mapdata_clear_space, mapdata_set_check_space, mapdata_clear_old, mapdata_set_size, mapdata_clear_label, mapdata_add_label } from './mapdata.js';
import { addSmooth, getImageInfo, getImageSums, Face2Cmd as imageFace2Cmd, Image2Cmd as imageImage2Cmd } from './image.js';
import { wantConfig, useConfig, resetPlayerData, getCpl } from './init.js';
import { newPlayer } from './player.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { playSound, playMusic } from './sound.js';

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
  onPickupUpdate?: (mode: number) => void;
  onAccountPlayers?: (players: AccountPlayer[]) => void;
  onFailure?: (command: string, message: string) => void;
  onTick?: (tickNo: number) => void;
  onGoodbye?: () => void;
  onAddMeSuccess?: () => void;
  onAddMeFail?: () => void;
  onVersion?: (csVersion: number, scVersion: number, versionString: string) => void;
  onDisconnect?: () => void;
  onReplyInfo?: (infoType: string, text: string) => void;
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
  range: '',
  title: '',
};

/** Known spells */
export const spells: Spell[] = [];

/**
 * Skill names populated from the server's skill_info reply.
 * Index i corresponds to CS_STAT_SKILLINFO + i.
 */
export const skillNames: string[] = new Array(CS_NUM_SKILLS).fill('');

/**
 * Experience table populated from the server's exp_table reply.
 * expTable[level] is the total experience required to reach that level.
 * expTable[0] = 0n (level 0 requires 0 exp, used as base for level 1).
 * expTable[1] is the exp needed for level 1, expTable[2] for level 2, etc.
 */
export const expTable: bigint[] = [];

/**
 * Compute the 0–100 progress percentage of `exp` within the current level band.
 * Returns 100 if the player is at the maximum known level.
 */
export function expBarPercent(exp: bigint, level: number): number {
  if (expTable.length < 2 || level <= 0) return 0;
  const curIdx = Math.min(level, expTable.length - 1);
  const curLevelExp = expTable[curIdx];
  if (curIdx + 1 >= expTable.length) return 100; // at or beyond max level
  const nextLevelExp = expTable[curIdx + 1];
  if (nextLevelExp <= curLevelExp) return 100;
  const ratio = Number(exp - curLevelExp) / Number(nextLevelExp - curLevelExp);
  return Math.max(0, Math.min(100, ratio * 100));
}

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
  // Parse the three leading integers (color type subtype), then take everything
  // after them as the message — mirroring the C code that advances a pointer
  // past three space-separated tokens.
  const s1 = data.indexOf(' ');
  const s2 = s1 >= 0 ? data.indexOf(' ', s1 + 1) : -1;
  const s3 = s2 >= 0 ? data.indexOf(' ', s2 + 1) : -1;
  if (s2 < 0) return; // need at least color, type, subtype
  const color = parseInt(data);
  const type = parseInt(data.substring(s1 + 1));
  const subtype = parseInt(data.substring(s2 + 1));
  const message = s3 >= 0 ? data.substring(s3 + 1) : '';
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
      case CS_STAT_STR: playerStats.Str = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_INT: playerStats.Int = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_WIS: playerStats.Wis = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_DEX: playerStats.Dex = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_CON: playerStats.Con = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_CHA: playerStats.Cha = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_POW: playerStats.Pow = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_LEVEL: playerStats.level = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_WC: playerStats.wc = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_AC: playerStats.ac = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_DAM: playerStats.dam = getShortFromData(data, pos); pos += 2; break;
      case CS_STAT_ARMOUR: playerStats.resists[0] = getShortFromData(data, pos); pos += 2; break;
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
        // 1-byte length prefix + string (not null-terminated)
        const rlen = getCharFromData(data, pos); pos += 1;
        playerStats.range = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, rlen);
        pos += rlen;
        break;
      }
      case CS_STAT_TITLE: {
        // 1-byte length prefix + string (not null-terminated)
        const tlen = getCharFromData(data, pos); pos += 1;
        let titleStr = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, tlen);
        pos += tlen;
        if (titleStr.startsWith('Player: ')) {
          titleStr = titleStr.slice('Player: '.length);
        }
        playerStats.title = titleStr;
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
  let prompt = spaceIdx > 0 ? data.substring(spaceIdx + 1) : data;
  // Server sometimes appends "\n:" as a colon-prompt on a new line; strip it
  // when there is more text before it.
  if (prompt.endsWith('\n:') && prompt.length > 2) {
    prompt = prompt.slice(0, -2);
  }
  callbacks.onQuery?.(flags, prompt.trim());
}

function PlayerCmd(data: DataView, len: number): void {
  let pos = 0;
  const tag = getIntFromData(data, pos); pos += 4;
  const weight = getIntFromData(data, pos); pos += 4;
  const face = getIntFromData(data, pos); pos += 4;
  const nameLen = getCharFromData(data, pos); pos += 1;
  const name = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, nameLen);
  pos += nameLen;
  if (pos !== len) {
    LOG(LogLevel.Warning, 'PlayerCmd', `lengths do not match (${pos}!=${len})`);
  }
  // Clear stale inventory and spell data from any previous session.
  const cpl = getCpl();
  if (cpl) {
    removeItemInventory(cpl.ob);
    removeItemInventory(cpl.below);
  }
  spells.length = 0;
  resetPlayerData();
  newPlayer(tag, name, weight, face);
  registerPlayerTag(tag);
  LOG(LogLevel.Info, 'PlayerCmd', `Player: ${name} tag=${tag}`);
  callbacks.onPlayerUpdate?.();
}

function Item2Cmd(data: DataView, len: number): void {
  let pos = 0;
  const location = getIntFromData(data, pos); pos += 4;
  while (pos < len) {
    const tag = getIntFromData(data, pos); pos += 4;
    const flags = getIntFromData(data, pos); pos += 4;
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
  callbacks.onPlayerUpdate?.();
}

function UpdateItemCmd(data: DataView, len: number): void {
  let pos = 0;
  const updateFlags = getCharFromData(data, pos); pos += 1;
  const tag = getIntFromData(data, pos); pos += 4;
  const item = locateItem(tag);
  if (!item) return;

  // Follow the old C pattern: copy all current values, then update only the
  // fields indicated by updateFlags, and finally call updateItem() which
  // handles location changes, re-sorting, name parsing, and event firing.
  let loc = item.env ? item.env.tag : 0;
  let weight = item.weight * 1000;
  let face = item.face;
  let flags = item.flagsval;
  let anim = item.animationId;
  let animspeed = item.animSpeed;
  let nrof = item.nrof;
  let name = '';

  if (updateFlags & UPD_LOCATION) {
    loc = getIntFromData(data, pos); pos += 4;
  }
  if (updateFlags & UPD_FLAGS) {
    flags = getIntFromData(data, pos); pos += 4;
  }
  if (updateFlags & UPD_WEIGHT) {
    weight = getIntFromData(data, pos); pos += 4;
  }
  if (updateFlags & UPD_FACE) {
    face = getIntFromData(data, pos); pos += 4;
  }
  if (updateFlags & UPD_NAME) {
    const nameLen = getCharFromData(data, pos); pos += 1;
    name = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, nameLen);
    pos += nameLen;
  }
  if (pos > len) {
    LOG(LogLevel.Warning, 'UpdateItemCmd', `Overread buffer: ${pos} > ${len}`);
    return;
  }
  if (updateFlags & UPD_ANIM) {
    anim = getShortFromData(data, pos); pos += 2;
  }
  if (updateFlags & UPD_ANIMSPEED) {
    animspeed = getCharFromData(data, pos); pos += 1;
  }
  if (updateFlags & UPD_NROF) {
    nrof = getIntFromData(data, pos); pos += 4;
  }

  updateItem(tag, loc, name, weight, face, flags, anim, animspeed, nrof, item.type);
  callbacks.onPlayerUpdate?.();
}

function DeleteItemCmd(data: DataView, len: number): void {
  let pos = 0;
  while (pos < len) {
    const tag = getIntFromData(data, pos); pos += 4;
    const item = locateItem(tag);
    if (item) removeItem(item);
  }
  callbacks.onPlayerUpdate?.();
}

function DeleteInventoryCmd(data: string): void {
  const tag = parseInt(data, 10);
  const item = locateItem(tag);
  if (item) {
    removeItemInventory(item);
  } else {
    LOG(LogLevel.Warning, 'DeleteInventoryCmd', `Invalid tag: ${tag}`);
  }
  callbacks.onPlayerUpdate?.();
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

    // Consume spellmon 2 extension fields (always present: client requests spellmon 2).
    // usage: 1 byte; requirements: 1-byte length + string
    if (pos < len) {
      pos += 1; // usage
      if (pos < len) {
        const reqLen = getCharFromData(data, pos); pos += 1;
        pos += reqLen;
      }
    }

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
  const t0 = performance.now();
  let tileCount = 0;
  let pos = 0;
  while (pos < len) {
    const mask = getShortFromData(data, pos); pos += 2;
    const x = ((mask >> 10) & 0x3F) - MAP2_COORD_OFFSET;
    const y = ((mask >> 4) & 0x3F) - MAP2_COORD_OFFSET;

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
    while (pos < len) {
      const typeByte = getCharFromData(data, pos); pos += 1;

      if (typeByte === 255) {
        mapdata_set_check_space(cx, cy);
        break;
      }

      // Upper 3 bits encode the number of additional data bytes for this entry;
      // lower 5 bits are the entry type.
      const spaceLen = typeByte >> 5;
      const type = typeByte & 0x1F;

      if (type === MAP2_TYPE_CLEAR) {
        mapdata_clear_space(cx, cy);
      } else if (type === MAP2_TYPE_DARKNESS) {
        const value = getCharFromData(data, pos); pos += 1;
        mapdata_set_darkness(cx, cy, value);
      } else if (type === MAP2_TYPE_LABEL) {
        // spaceLen === 7 signals variable-length data: next byte is total length.
        /* labelTotalLen */ getCharFromData(data, pos); pos += 1;
        const subtype = getCharFromData(data, pos); pos += 1;
        const strLen = getCharFromData(data, pos); pos += 1;
        const label = getStringFromData(new Uint8Array(data.buffer, data.byteOffset), pos, strLen);
        pos += strLen;
        mapdata_add_label(cx, cy, subtype, label);
      } else if (type >= MAP2_LAYER_START && type < MAP2_LAYER_START + MAXLAYERS) {
        const layer = type & 0xF;
        const faceOrAnim = getShortFromData(data, pos); pos += 2;
        if (!(faceOrAnim & FACE_IS_ANIM)) {
          mapdata_set_face_layer(cx, cy, faceOrAnim, layer);
        }
        if (spaceLen > 2) {
          const opt = getCharFromData(data, pos); pos += 1;
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
          const opt = getCharFromData(data, pos); pos += 1;
          mapdata_set_smooth(cx, cy, opt, layer);
        }
      } else {
        // Unknown type: skip the declared number of data bytes.
        if (spaceLen !== 7) {
          pos += spaceLen;
        } else {
          const extraLen = getCharFromData(data, pos); pos += 1;
          pos += extraLen;
        }
      }
    }
  }
  const elapsed = performance.now() - t0;
  if (elapsed > 1 || tileCount > 10) {
    LOG(LogLevel.Debug, 'perf:map2', `parsed ${tileCount} tiles from ${len}B in ${elapsed.toFixed(1)}ms`);
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
  callbacks.onPickupUpdate?.(mode >>> 0); // unsigned
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
  // The separator between the info type and its data can be either a space or
  // a newline, depending on what the server sends (mirrors the C client logic).
  let spaceIdx = -1;
  for (let i = 0; i < len; i++) {
    if (bytes[i] === 32 /* space */ || bytes[i] === 10 /* newline */) {
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
    // Each line is "stat_number:skill_name\n"
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
    // Notify the UI so skill names are shown immediately (stats may already be set).
    callbacks.onStatsUpdate?.(playerStats);
  } else if (infoType === 'exp_table') {
    // Binary format: 2-byte uint16 (max level count N), then N×8-byte int64 values
    // for levels 1..N.  expTable[0]=0n is the implicit base; expTable[level] is the
    // total experience required to reach that level.
    const rest = bytes.subarray(spaceIdx + 1);
    if (rest.length < 2) return;
    const dv = new DataView(rest.buffer, rest.byteOffset, rest.byteLength);
    const maxLevel = dv.getUint16(0, false);
    expTable.length = 0;
    expTable.push(BigInt(0)); // level 0 base
    for (let level = 1; level <= maxLevel; level++) {
      const pos = 2 + (level - 1) * 8;
      if (pos + 8 > rest.length) break;
      expTable.push(dv.getBigInt64(pos, false));
    }
  } else if (infoType === 'motd' || infoType === 'news' || infoType === 'rules') {
    const text = new TextDecoder().decode(bytes.subarray(spaceIdx + 1));
    callbacks.onReplyInfo?.(infoType, text);
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

// --- Sound / Music commands ------------------------------------------------

/**
 * Handle the sound2 command from the server.
 *
 * Binary format:
 *   sound2 {x:int8}{y:int8}{dir:uint8}{vol:uint8}{type:uint8}
 *          {len_sound:uint8}{sound:str}{len_source:uint8}{source:str}
 */
function Sound2Cmd(data: DataView, len: number): void {
  if (len < 8) {
    LOG(LogLevel.Warning, 'Sound2Cmd', `Command too short: ${len} bytes`);
    return;
  }
  const x = data.getInt8(0);
  const y = data.getInt8(1);
  const dir = data.getUint8(2);
  const vol = data.getUint8(3);
  const type = data.getUint8(4);
  const lenSound = data.getUint8(5);

  if (6 + lenSound + 1 > len) {
    LOG(LogLevel.Warning, 'Sound2Cmd', `Sound length overflows: ${lenSound}`);
    return;
  }

  const bytes = new Uint8Array(data.buffer, data.byteOffset);
  const sound = getStringFromData(bytes, 6, lenSound);
  const lenSource = data.getUint8(6 + lenSound);

  if (6 + lenSound + 1 + lenSource > len) {
    LOG(LogLevel.Warning, 'Sound2Cmd', `Source length overflows: ${lenSource}`);
    return;
  }

  const source = getStringFromData(bytes, 6 + lenSound + 1, lenSource);
  playSound(x, y, dir, vol, type, sound, source);
}

/**
 * Handle the music command from the server.
 *
 * Text format: music {name}
 * The name is the music track to play (without path/extension), or "NONE".
 */
function MusicCmd(data: string): void {
  const name = data.trim();
  playMusic(name);
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
  ['delinv', { type: 'text', handler: DeleteInventoryCmd }],
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
  ['sound2', { type: 'binary', handler: Sound2Cmd }],
  ['music', { type: 'text', handler: MusicCmd }],
]);

/**
 * When true, every received server command is logged to the console.
 * Defaults to false to avoid console noise in production.
 * Always logs when a handler throws.
 */
let logReceivedCommands = false;

/** Toggle (or explicitly set) command-receive logging. */
export function setLogReceivedCommands(enabled: boolean): void {
  logReceivedCommands = enabled;
}

/**
 * Dispatch a received command from the server.
 * The raw packet is: command_name (ASCII) + space + binary/text data
 */
export function dispatchPacket(packet: ArrayBuffer): void {
  const t0 = performance.now();
  const bytes = new Uint8Array(packet);
  // Find the space separating command name from data
  let spaceIdx = bytes.indexOf(32); // ASCII space
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
    // Always log on exception regardless of the logging flag.
    LOG(LogLevel.Error, 'dispatch', `Exception handling ${cmdName}: ${err}`);
    console.error(err);
  }

  const elapsed = performance.now() - t0;
  if (elapsed > 2) {
    LOG(LogLevel.Warning, 'perf:dispatch', `${cmdName} handler took ${elapsed.toFixed(1)}ms (${dataLen}B)`);
  }
}
