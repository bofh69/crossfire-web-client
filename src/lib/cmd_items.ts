/**
 * cmd_items.ts — Item and spell command handlers.
 * Extracted from commands.ts.
 */

import {
  UPD_LOCATION, UPD_FLAGS, UPD_WEIGHT, UPD_FACE, UPD_NAME, UPD_ANIM, UPD_ANIMSPEED, UPD_NROF,
  UPD_SP_MANA, UPD_SP_GRACE, UPD_SP_DAMAGE,
  type Spell,
} from './protocol.js';
import { BinaryReader } from './binary_reader.js';
import { locateItem, removeItem, removeItemInventory, updateItem, registerPlayerTag } from './item.js';
import { resetPlayerData, getCpl } from './init.js';
import { newPlayer } from './player.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { gameEvents } from './events.js';
import { updateHotbarFacesFromSpell, updateHotbarSlotFromItem, resetHotbarSession, setCurrentCharacter as setHotbarCurrentCharacter } from './hotbar.js';

/** Known spells */
export const spells: Spell[] = [];

export function PlayerCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const tag = reader.readInt32();
  const weight = reader.readInt32();
  const face = reader.readInt32();
  const nameLen = reader.readUint8();
  const name = reader.readString(nameLen);
  if (reader.pos !== len) {
    LOG(LogLevel.Warning, 'PlayerCmd', `lengths do not match (${reader.pos}!=${len})`);
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
  // Reset the hotbar session guard then immediately load slots for this
  // character.  This must happen before emitting playerUpdate (and before
  // any addspell / item2 packets that follow) so that hotbar face/tag
  // matching works regardless of loginmethod ordering.
  resetHotbarSession();
  setHotbarCurrentCharacter(name);
  gameEvents.emit('playerUpdate');
}

export function Item2Cmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const location = reader.readInt32();
  let hotbarChanged = false;
  while (reader.remaining > 0) {
    const tag = reader.readInt32();
    const flags = reader.readInt32();
    const weight = reader.readInt32();
    const face = reader.readInt32();
    const nameLen = reader.readUint8();
    const name = reader.readString(nameLen);
    const anim = reader.readInt16();
    const animSpeed = reader.readUint8();
    const nrof = reader.readInt32();
    const type = reader.readInt16();
    updateItem(tag, location, name, weight, face, flags, anim, animSpeed, nrof, type);
    // Only sync hotbar for player-inventory items; ground/map items cannot be
    // in hotbar slots and the extra work causes unnecessary UI churn.
    const playerTag = getCpl()?.ob?.tag;
    if (playerTag && location === playerTag) {
      const sName = locateItem(tag)?.sName ?? '';
      if (sName) hotbarChanged = updateHotbarSlotFromItem(sName, tag, face) || hotbarChanged;
    }
  }
  gameEvents.emit('playerUpdate');
  if (hotbarChanged) gameEvents.emit('hotbarUpdate');
}

export function UpdateItemCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const updateFlags = reader.readUint8();
  const tag = reader.readInt32();
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
    loc = reader.readInt32();
  }
  if (updateFlags & UPD_FLAGS) {
    flags = reader.readInt32();
  }
  if (updateFlags & UPD_WEIGHT) {
    weight = reader.readInt32();
  }
  if (updateFlags & UPD_FACE) {
    face = reader.readInt32();
  }
  if (updateFlags & UPD_NAME) {
    const nameLen = reader.readUint8();
    name = reader.readString(nameLen);
  }
  if (reader.pos > len) {
    LOG(LogLevel.Warning, 'UpdateItemCmd', `Overread buffer: ${reader.pos} > ${len}`);
    return;
  }
  if (updateFlags & UPD_ANIM) {
    anim = reader.readInt16();
  }
  if (updateFlags & UPD_ANIMSPEED) {
    animspeed = reader.readUint8();
  }
  if (updateFlags & UPD_NROF) {
    nrof = reader.readInt32();
  }

  updateItem(tag, loc, name, weight, face, flags, anim, animspeed, nrof, item.type);
  gameEvents.emit('playerUpdate');
}

export function DeleteItemCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  while (reader.remaining > 0) {
    const tag = reader.readInt32();
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
  const reader = new BinaryReader(data, len);
  let hotbarChanged = false;
  while (reader.remaining > 0) {
    const tag = reader.readInt32();
    const spellLevel = reader.readInt16();
    const castingTime = reader.readInt16();
    const mana = reader.readInt16();
    const grace = reader.readInt16();
    const damage = reader.readInt16();
    const skill = reader.readUint8();
    const path = reader.readInt32();
    const face = reader.readInt32();
    const nameLen = reader.readUint8();
    const name = reader.readString(nameLen);
    const msgLen = reader.readInt16();
    const message = reader.readString(msgLen);

    // Consume spellmon 2 extension fields (always present: client requests spellmon 2).
    // usage: 1 byte; requirements: 1-byte length + string
    if (reader.remaining > 0) {
      reader.skip(1); // usage
      if (reader.remaining > 0) {
        const reqLen = reader.readUint8();
        reader.skip(reqLen);
      }
    }

    const spell: Spell = {
      name, message, tag, level: spellLevel, time: castingTime,
      sp: mana, grace: grace, dam: damage, skillNumber: skill,
      skill: '', path, face, usage: 0, requirements: '',
    };
    spells.push(spell);
    hotbarChanged = updateHotbarFacesFromSpell(name, face) || hotbarChanged;
  }
  gameEvents.emit('spellUpdate');
  if (hotbarChanged) gameEvents.emit('hotbarUpdate');
}

export function UpdspellCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const flags = reader.readUint8();
  const tag = reader.readInt32();
  const spell = spells.find(s => s.tag === tag);
  if (!spell) return;
  if (flags & UPD_SP_MANA) { spell.sp = reader.readInt16(); }
  if (flags & UPD_SP_GRACE) { spell.grace = reader.readInt16(); }
  if (flags & UPD_SP_DAMAGE) { spell.dam = reader.readInt16(); }
  gameEvents.emit('spellUpdate');
}

export function DeleteSpellCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const tag = reader.readInt32();
  const idx = spells.findIndex(s => s.tag === tag);
  if (idx >= 0) spells.splice(idx, 1);
  gameEvents.emit('spellUpdate');
}
