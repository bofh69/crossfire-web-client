/**
 * cmd_items.ts — Item and spell command handlers.
 * Extracted from commands.ts.
 */

import {
  UPD_LOCATION, UPD_FLAGS, UPD_WEIGHT, UPD_FACE, UPD_NAME, UPD_ANIM, UPD_ANIMSPEED, UPD_NROF,
  UPD_SP_MANA, UPD_SP_GRACE, UPD_SP_DAMAGE,
  type Spell,
} from './protocol.js';
import {
  getCharFromData, getShortFromData, getIntFromData,
  getStringFromData,
} from './newsocket.js';
import { locateItem, removeItem, removeItemInventory, updateItem, registerPlayerTag } from './item.js';
import { resetPlayerData, getCpl } from './init.js';
import { newPlayer } from './player.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { gameEvents } from './events.js';

/** Known spells */
export const spells: Spell[] = [];

export function PlayerCmd(data: DataView, len: number): void {
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
  gameEvents.emit('playerUpdate');
}

export function Item2Cmd(data: DataView, len: number): void {
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
  gameEvents.emit('playerUpdate');
}

export function UpdateItemCmd(data: DataView, len: number): void {
  let pos = 0;
  const updateFlags = getCharFromData(data, pos); pos += 1;
  const tag = getIntFromData(data, pos); pos += 4;
  const item = locateItem(tag);
  if (!item) return;

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
  gameEvents.emit('playerUpdate');
}

export function DeleteItemCmd(data: DataView, len: number): void {
  let pos = 0;
  while (pos < len) {
    const tag = getIntFromData(data, pos); pos += 4;
    const item = locateItem(tag);
    if (item) removeItem(item);
  }
  gameEvents.emit('playerUpdate');
}

export function DeleteInventoryCmd(data: string): void {
  const tag = parseInt(data, 10);
  const item = locateItem(tag);
  if (item) {
    removeItemInventory(item);
  } else {
    LOG(LogLevel.Warning, 'DeleteInventoryCmd', `Invalid tag: ${tag}`);
  }
  gameEvents.emit('playerUpdate');
}

export function AddspellCmd(data: DataView, len: number): void {
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
  gameEvents.emit('spellUpdate');
}

export function UpdspellCmd(data: DataView, len: number): void {
  let pos = 0;
  const flags = getCharFromData(data, pos); pos += 1;
  const tag = getIntFromData(data, pos); pos += 4;
  const spell = spells.find(s => s.tag === tag);
  if (!spell) return;
  if (flags & UPD_SP_MANA) { spell.sp = getShortFromData(data, pos); pos += 2; }
  if (flags & UPD_SP_GRACE) { spell.grace = getShortFromData(data, pos); pos += 2; }
  if (flags & UPD_SP_DAMAGE) { spell.dam = getShortFromData(data, pos); pos += 2; }
  gameEvents.emit('spellUpdate');
}

export function DeleteSpellCmd(data: DataView, len: number): void {
  const tag = getIntFromData(data, 0);
  const idx = spells.findIndex(s => s.tag === tag);
  if (idx >= 0) spells.splice(idx, 1);
  gameEvents.emit('spellUpdate');
}
