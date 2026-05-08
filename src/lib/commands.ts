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

import {
  NDI_COLOR_MASK,
  CS_STAT_SKILLINFO,
  CS_NUM_SKILLS,
  MSG_TYPE_ADMIN,
  MSG_TYPE_ADMIN_HISCORE,
  SC_ALWAYS,
} from "./protocol.js";

import { getStringFromData, CrossfireSocket } from "./newsocket.js";
import { BinaryReader } from "./binary_reader.js";
import { setCSocket as setItemSocket } from "./item.js";
import {
  getImageInfo,
  getImageSums,
  Face2Cmd as imageFace2Cmd,
  Image2Cmd as imageImage2Cmd,
} from "./image.js";
import { getLastNcomSeqSent, notifyNcomAck, sendCommand } from "./player.js";
import { LOG } from "./misc.js";
import { LogLevel } from "./protocol.js";
import { gameEvents, type AccountPlayer } from "./events.js";
import {
  parseRaceClassList,
  parseRaceClassInfo,
  parseNewCharInfo,
  parseStartingMapInfo,
} from "./cmd_chargen.js";

// ── Re-exports from split modules ──────────────────────────────────────────
// Keep the public API surface compatible so downstream files don't need to
// change their import paths.

export {
  playerStats,
  skillNames,
  skillDescriptions,
  expTable,
  expBarPercent,
} from "./cmd_stats.js";
export { spells } from "./cmd_items.js";
export {
  quests,
  knowledgeItems,
  knowledgeTypeInfos,
  clearNotifications,
} from "./cmd_notifications.js";
export type {
  Quest,
  KnowledgeItem,
  KnowledgeTypeInfo,
} from "./cmd_notifications.js";

// ── Handlers from split modules ────────────────────────────────────────────

import { StatsCmd } from "./cmd_stats.js";
import {
  playerStats,
  skillNames,
  skillDescriptions,
  expTable,
} from "./cmd_stats.js";
import {
  PlayerCmd,
  Item2Cmd,
  UpdateItemCmd,
  DeleteItemCmd,
  DeleteInventoryCmd,
  AddspellCmd,
  UpdspellCmd,
  DeleteSpellCmd,
} from "./cmd_items.js";
import {
  SetupCmd,
  NewmapCmd,
  Map2Cmd,
  mapScrollCmd,
  MagicMapCmd,
  AnimCmd,
  SmoothCmd,
  maybeCaptureMapinfoExtInfo,
  maybeProcessMapinfoComc,
} from "./cmd_map.js";
import { Sound2Cmd, MusicCmd } from "./cmd_sound.js";
import {
  AddQuestCmd,
  UpdQuestCmd,
  AddKnowledgeCmd,
  parseKnowledgeInfo,
} from "./cmd_notifications.js";
import {
  processDialogExtInfo,
  notifyNonDrawExtInfoCommand,
} from "./cmd_dialog.js";

// ── Hiscore menu tracking ──────────────────────────────────────────────────

/**
 * ncom sequence number of the pending menu-initiated hiscore command,
 * or -1 if no such request is in-flight.
 */
let hiscoreNcomSeq = -1;

/**
 * Lines buffered from ADMIN_HISCORE drawextinfo messages while the hiscore
 * command is in-flight.  Emitted as a single 'hiscoreResult' event when the
 * matching comc arrives.
 */
let hiscoreBuffer: string[] = [];

/**
 * Send the "hiscore" command on behalf of the Info menu.
 * Lines will be buffered until the matching comc is received, then emitted
 * as a 'hiscoreResult' event (shown in a dialog) rather than forwarded to
 * the InfoPanel.
 */
export function requestMenuHiscore(): void {
  if (hiscoreNcomSeq !== -1) return; // request already in-flight
  hiscoreBuffer = [];
  sendCommand("hiscore", 0, SC_ALWAYS);
  hiscoreNcomSeq = getLastNcomSeqSent();
}

/**
 * Cancel any pending menu hiscore request (called when the menu unmounts).
 * Clears buffered lines so subsequent ADMIN_HISCORE responses go to the
 * InfoPanel as normal.
 */
export function cancelPendingMenuHiscore(): void {
  hiscoreNcomSeq = -1;
  hiscoreBuffer = [];
}

// ── Socket management ──────────────────────────────────────────────────────

export function setSocket(sock: CrossfireSocket): void {
  setItemSocket(sock);
}

// ── Small / protocol-level handlers (kept here) ────────────────────────────

function DrawInfoCmd(data: string): void {
  const spaceIdx = data.indexOf(" ");
  if (spaceIdx < 0) return;
  const color = parseInt(data.substring(0, spaceIdx));
  const message = data.substring(spaceIdx + 1);
  LOG(LogLevel.Debug, "DrawInfoCmd", message);
  gameEvents.emit("drawInfo", color & NDI_COLOR_MASK, message);
}

/** Returns true when a drawextinfo message should be buffered for the hiscore dialog. */
function shouldBufferHiscoreMessage(type: number, subtype: number): boolean {
  return (
    type === MSG_TYPE_ADMIN &&
    subtype === MSG_TYPE_ADMIN_HISCORE &&
    hiscoreNcomSeq !== -1
  );
}

function DrawExtInfoCmd(data: string): void {
  const s1 = data.indexOf(" ");
  const s2 = s1 >= 0 ? data.indexOf(" ", s1 + 1) : -1;
  const s3 = s2 >= 0 ? data.indexOf(" ", s2 + 1) : -1;
  if (s2 < 0) return;
  const color = parseInt(data);
  const type = parseInt(data.substring(s1 + 1));
  const subtype = parseInt(data.substring(s2 + 1));
  const message = s3 >= 0 ? data.substring(s3 + 1) : "";
  if (maybeCaptureMapinfoExtInfo(color, type, subtype, message)) {
    // Captured for mapinfo comc processing — not forwarded to InfoPanel yet.
    return;
  }
  if (shouldBufferHiscoreMessage(type, subtype)) {
    // Hiscore messages are buffered for the dialog popup and skip all other
    // processing, including the dialog state machine.
    hiscoreBuffer.push(message);
    return;
  }
  if (processDialogExtInfo(color, type, subtype, message)) {
    // Suppressed by dialog state machine (Replies: header or key/value line).
    return;
  }
  gameEvents.emit(
    "drawExtInfo",
    color & NDI_COLOR_MASK,
    type,
    subtype,
    message,
  );
}

function handleQuery(data: string): void {
  const spaceIdx = data.indexOf(" ");
  const flags = spaceIdx > 0 ? parseInt(data.substring(0, spaceIdx)) : 0;
  let prompt = spaceIdx > 0 ? data.substring(spaceIdx + 1) : data;
  if (prompt.endsWith("\n:") && prompt.length > 2) {
    prompt = prompt.slice(0, -2);
  }
  gameEvents.emit("query", flags, prompt.trim());
}

function TickCmd(data: DataView, len: number): void {
  const tickNo = new BinaryReader(data, len).readInt32();
  gameEvents.emit("tick", tickNo);
}

function ComcCmd(data: DataView, len: number): void {
  if (len < 2) {
    LOG(LogLevel.Error, "ComcCmd", `Invalid comc length ${len} - ignoring`);
    return;
  }
  const seq = new BinaryReader(data, len).readInt16();
  notifyNcomAck(seq);

  // If this comc acknowledges the pending hiscore command, emit all buffered
  // lines as a single hiscoreResult event.
  if (hiscoreNcomSeq !== -1 && seq === hiscoreNcomSeq) {
    const text = hiscoreBuffer.join("\n");
    hiscoreNcomSeq = -1;
    hiscoreBuffer = [];
    gameEvents.emit("hiscoreResult", text);
  }

  // If this comc acknowledges the pending mapinfo command, process the
  // buffered drawextinfo entries: extract the map path and forward any
  // user-command responses that were captured alongside mapinfo's responses.
  const toForward = maybeProcessMapinfoComc(seq);
  if (toForward) {
    for (const e of toForward) {
      gameEvents.emit(
        "drawExtInfo",
        e.color & NDI_COLOR_MASK,
        e.type,
        e.subtype,
        e.message,
      );
    }
  }
}

function PickupCmd(data: DataView, len: number): void {
  const mode = new BinaryReader(data, len).readUint32();
  LOG(LogLevel.Debug, "PickupCmd", `Pickup mode: ${mode}`);
  gameEvents.emit("pickupUpdate", mode);
}

function FailureCmd(data: string): void {
  const spaceIdx = data.indexOf(" ");
  const command = spaceIdx > 0 ? data.substring(0, spaceIdx) : data;
  const message = spaceIdx > 0 ? data.substring(spaceIdx + 1) : "";
  LOG(LogLevel.Warning, "FailureCmd", `${command}: ${message}`);
  gameEvents.emit("failure", command, message);
}

function AccountPlayersCmd(data: DataView, len: number): void {
  const bytes = new Uint8Array(data.buffer, data.byteOffset, len);
  const players: AccountPlayer[] = [];
  const decoder = new TextDecoder();

  // ACL_ type constants (from old/common/shared/newclient.h)
  const ACL_NAME = 1;
  const ACL_CLASS = 2;
  const ACL_RACE = 3;
  const ACL_LEVEL = 4;
  const ACL_FACE = 5;
  const ACL_PARTY = 6;
  const ACL_MAP = 7;
  const ACL_FACE_NUM = 8;

  // byte 0 is the number of characters; skip it and start reading fields.
  let pos = 1;
  let name = "",
    charClass = "",
    race = "",
    face = "",
    party = "",
    map = "";
  let level = 0,
    faceNum = 0;

  while (pos < len) {
    const flen = bytes[pos]!;
    if (flen === 0) {
      // End of this character's data — emit the accumulated record.
      players.push({ name, charClass, race, face, party, map, level, faceNum });
      name = "";
      charClass = "";
      race = "";
      face = "";
      party = "";
      map = "";
      level = 0;
      faceNum = 0;
      pos++;
      continue;
    }
    pos++;
    if (pos + flen > len) {
      LOG(LogLevel.Error, "AccountPlayersCmd", "data overran buffer");
      break;
    }
    const fieldType = bytes[pos]!;
    // value bytes start after the type byte; flen includes the type byte itself.
    const fieldValue = bytes.subarray(pos + 1, pos + flen);
    if (fieldValue.length >= 2) {
      const fieldDv = new DataView(
        fieldValue.buffer,
        fieldValue.byteOffset,
        fieldValue.byteLength,
      );
      switch (fieldType) {
        case ACL_LEVEL:
          level = fieldDv.getInt16(0, false);
          break;
        case ACL_FACE_NUM:
          faceNum = fieldDv.getUint16(0, false);
          break;
      }
    }
    switch (fieldType) {
      case ACL_NAME:
        name = decoder.decode(fieldValue);
        break;
      case ACL_CLASS:
        charClass = decoder.decode(fieldValue);
        break;
      case ACL_RACE:
        race = decoder.decode(fieldValue);
        break;
      case ACL_FACE:
        face = decoder.decode(fieldValue);
        break;
      case ACL_PARTY:
        party = decoder.decode(fieldValue);
        break;
      case ACL_MAP:
        map = decoder.decode(fieldValue);
        break;
    }
    pos += flen;
  }

  gameEvents.emit("accountPlayers", players);
}

function VersionCmd(data: string): void {
  const parts = data.split(" ", 3);
  const csVer = parts.length > 0 ? parseInt(parts[0]!) : 0;
  const scVer = parts.length > 1 ? parseInt(parts[1]!) : 0;
  const verStr = parts.length > 2 ? parts[2]! : "";
  LOG(
    LogLevel.Info,
    "VersionCmd",
    `Server version: cs=${csVer} sc=${scVer} ${verStr}`,
  );
  gameEvents.emit("version", csVer, scVer, verStr);
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

  if (infoType === "image_info") {
    getImageInfo(bytes.subarray(spaceIdx + 1), len - spaceIdx - 1);
  } else if (infoType === "image_sums") {
    const sumsData = getStringFromData(bytes, spaceIdx + 1, len - spaceIdx - 1);
    getImageSums(sumsData, len - spaceIdx - 1);
  } else if (infoType === "skill_info") {
    const text = new TextDecoder().decode(bytes.subarray(spaceIdx + 1));
    for (const line of text.split("\n")) {
      const colonIdx = line.indexOf(":");
      if (colonIdx < 0) continue;
      const statNum = parseInt(line.substring(0, colonIdx));
      const name = line.substring(colonIdx + 1).trim();
      const idx = statNum - CS_STAT_SKILLINFO;
      if (idx >= 0 && idx < CS_NUM_SKILLS && name.length > 0) {
        skillNames[idx] = name;
      }
    }
    gameEvents.emit("statsUpdate", playerStats);
  } else if (infoType === "skill_extra") {
    // Binary format: repeated { uint16 skill_number, uint16 desc_len, string desc }
    // terminated by skill_number == 0
    const rest = bytes.subarray(spaceIdx + 1);
    const dv = new DataView(rest.buffer, rest.byteOffset, rest.byteLength);
    let pos = 0;
    while (pos + 2 <= rest.length) {
      const skillNum = dv.getUint16(pos, false);
      pos += 2;
      if (skillNum === 0) break;
      if (pos + 2 > rest.length) break;
      const descLen = dv.getUint16(pos, false);
      pos += 2;
      if (pos + descLen > rest.length) break;
      const desc = new TextDecoder().decode(rest.subarray(pos, pos + descLen));
      pos += descLen;
      const idx = skillNum - CS_STAT_SKILLINFO;
      if (idx >= 0 && idx < CS_NUM_SKILLS) {
        skillDescriptions[idx] = desc;
      }
    }
    gameEvents.emit("statsUpdate", playerStats);
  } else if (infoType === "exp_table") {
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
  } else if (
    infoType === "motd" ||
    infoType === "news" ||
    infoType === "rules"
  ) {
    const text = new TextDecoder().decode(bytes.subarray(spaceIdx + 1));
    gameEvents.emit("replyInfo", infoType, text);
  } else if (infoType === "knowledge_info") {
    const text = new TextDecoder().decode(bytes.subarray(spaceIdx + 1));
    parseKnowledgeInfo(text);
  } else if (infoType === "race_list") {
    const text = new TextDecoder().decode(bytes.subarray(spaceIdx + 1));
    gameEvents.emit("raceListReceived", parseRaceClassList(text));
  } else if (infoType === "class_list") {
    const text = new TextDecoder().decode(bytes.subarray(spaceIdx + 1));
    gameEvents.emit("classListReceived", parseRaceClassList(text));
  } else if (infoType === "race_info") {
    gameEvents.emit(
      "raceInfoReceived",
      parseRaceClassInfo(bytes.subarray(spaceIdx + 1)),
    );
  } else if (infoType === "class_info") {
    gameEvents.emit(
      "classInfoReceived",
      parseRaceClassInfo(bytes.subarray(spaceIdx + 1)),
    );
  } else if (infoType === "newcharinfo") {
    gameEvents.emit(
      "newCharInfoReceived",
      parseNewCharInfo(bytes.subarray(spaceIdx + 1)),
    );
  } else if (infoType === "startingmap") {
    gameEvents.emit(
      "startingMapReceived",
      parseStartingMapInfo(bytes.subarray(spaceIdx + 1)),
    );
  }
  LOG(LogLevel.Debug, "ReplyInfoCmd", `Info type: ${infoType}`);
}

function GoodbyeCmd(): void {
  LOG(LogLevel.Info, "GoodbyeCmd", "Server said goodbye");
  gameEvents.emit("goodbye");
}

function AddMeFail(): void {
  LOG(LogLevel.Warning, "AddMeFail", "Failed to add player");
  gameEvents.emit("addMeFail");
}

function AddMeSuccess(): void {
  LOG(LogLevel.Info, "AddMeSuccess", "Player added successfully");
  gameEvents.emit("addMeSuccess");
}

// ── Command dispatch table ─────────────────────────────────────────────────

type TextHandler = (data: string) => void;
type BinaryHandler = (data: DataView, len: number) => void;
type NoArgHandler = () => void;

interface CommandEntry {
  type: "text" | "binary" | "none";
  handler: TextHandler | BinaryHandler | NoArgHandler;
}

const commandTable = new Map<string, CommandEntry>([
  ["setup", { type: "text", handler: SetupCmd }],
  ["drawinfo", { type: "text", handler: DrawInfoCmd }],
  ["drawextinfo", { type: "text", handler: DrawExtInfoCmd }],
  ["stats", { type: "binary", handler: StatsCmd }],
  ["query", { type: "text", handler: handleQuery }],
  ["player", { type: "binary", handler: PlayerCmd }],
  ["item2", { type: "binary", handler: Item2Cmd }],
  ["upditem", { type: "binary", handler: UpdateItemCmd }],
  ["delitem", { type: "binary", handler: DeleteItemCmd }],
  ["delinv", { type: "text", handler: DeleteInventoryCmd }],
  ["addspell", { type: "binary", handler: AddspellCmd }],
  ["updspell", { type: "binary", handler: UpdspellCmd }],
  ["delspell", { type: "binary", handler: DeleteSpellCmd }],
  ["newmap", { type: "none", handler: NewmapCmd }],
  ["map2", { type: "binary", handler: Map2Cmd }],
  ["map_scroll", { type: "text", handler: mapScrollCmd }],
  ["magicmap", { type: "binary", handler: MagicMapCmd }],
  ["tick", { type: "binary", handler: TickCmd }],
  ["comc", { type: "binary", handler: ComcCmd }],
  ["pickup", { type: "binary", handler: PickupCmd }],
  ["failure", { type: "text", handler: FailureCmd }],
  ["accountplayers", { type: "binary", handler: AccountPlayersCmd }],
  ["anim", { type: "binary", handler: AnimCmd }],
  ["smooth", { type: "binary", handler: SmoothCmd }],
  ["version", { type: "text", handler: VersionCmd }],
  ["replyinfo", { type: "binary", handler: ReplyInfoCmd }],
  ["goodbye", { type: "none", handler: GoodbyeCmd }],
  ["addme_failed", { type: "none", handler: AddMeFail }],
  ["addme_success", { type: "none", handler: AddMeSuccess }],
  ["face2", { type: "binary", handler: imageFace2Cmd }],
  ["image2", { type: "binary", handler: imageImage2Cmd }],
  ["sound2", { type: "binary", handler: Sound2Cmd }],
  ["music", { type: "text", handler: MusicCmd }],
  ["addquest", { type: "binary", handler: AddQuestCmd }],
  ["updquest", { type: "binary", handler: UpdQuestCmd }],
  ["addknowledge", { type: "binary", handler: AddKnowledgeCmd }],
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
    LOG(
      LogLevel.Debug,
      "dispatch",
      `Unknown command: ${cmdName} (${dataLen}B)`,
    );
    return;
  }

  try {
    switch (entry.type) {
      case "text": {
        const textData = new TextDecoder().decode(bytes.subarray(dataStart));
        if (logReceivedCommands)
          LOG(LogLevel.Debug, "RX", `${cmdName} ${textData}`);
        (entry.handler as TextHandler)(textData);
        break;
      }
      case "binary": {
        if (logReceivedCommands)
          LOG(LogLevel.Debug, "RX", `${cmdName} <binary ${dataLen}B>`);
        const dataView = new DataView(packet, dataStart);
        (entry.handler as BinaryHandler)(dataView, dataLen);
        break;
      }
      case "none":
        if (logReceivedCommands) LOG(LogLevel.Debug, "RX", cmdName);
        (entry.handler as NoArgHandler)();
        break;
    }
  } catch (err) {
    LOG(LogLevel.Error, "dispatch", `Exception handling ${cmdName}: ${err}`);
  }

  // Any server packet other than drawextinfo can end an active dialog
  // option-collection run.
  if (cmdName !== "drawextinfo") {
    notifyNonDrawExtInfoCommand();
  }

  const elapsed = performance.now() - t0;
  if (elapsed > 2) {
    LOG(
      LogLevel.Warning,
      "perf:dispatch",
      `${cmdName} handler took ${elapsed.toFixed(1)}ms (${dataLen}B)`,
    );
  }
}
