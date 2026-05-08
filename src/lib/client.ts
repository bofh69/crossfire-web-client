/**
 * Client connection management for the Crossfire web client.
 * Port of old/common/client.c — handles connecting, negotiation, and
 * high-level protocol setup.
 */

import { VERSION_CS, VERSION_SC, LogLevel } from "./protocol";
import { CrossfireSocket, SockList } from "./newsocket";
import {
  dispatchPacket,
  setSocket as setCommandsSocket,
  clearNotifications,
} from "./commands";
import { notifyMapsizeSent } from "./cmd_map";
import { gameEvents } from "./events";
import { wantConfig, useConfig } from "./init";
import { setSocket as setPlayerSocket } from "./player";
import { setCSocket as setItemSocket } from "./item";
import { LOG } from "./misc";

let csocket: CrossfireSocket | null = null;

// ── Heartbeat ─────────────────────────────────────────────────────────────────

/**
 * Heartbeat interval in milliseconds.  A `beat` no-op is sent whenever no
 * other command has been sent within this window.  Set to 2500 ms to provide
 * a comfortable safety margin below the server's 3-second deadline.
 */
const HEARTBEAT_INTERVAL_MS = 2500;

/** Interval handle for the active heartbeat timer, or null when inactive. */
let beatTimer: ReturnType<typeof setInterval> | null = null;

/** Unsubscribe function for the per-connection `beatEnabled` event listener. */
let beatUnsubscribe: (() => void) | null = null;

/**
 * Start the heartbeat timer.
 *
 * Fires every 2500 ms.  If nothing has been sent to the server in the last
 * 2500 ms, a `beat` no-op is sent to keep the connection alive.
 */
function startHeartbeat(): void {
  stopHeartbeat();
  LOG(LogLevel.Info, "client", "Heartbeat enabled");
  beatTimer = setInterval(() => {
    if (!csocket) {
      stopHeartbeat();
      return;
    }
    const elapsed = Date.now() - csocket.lastSentAt;
    if (elapsed >= HEARTBEAT_INTERVAL_MS) {
      csocket.sendString("beat");
    }
  }, HEARTBEAT_INTERVAL_MS);
}

/** Stop the heartbeat timer if it is running. */
function stopHeartbeat(): void {
  if (beatTimer !== null) {
    clearInterval(beatTimer);
    beatTimer = null;
  }
}

// ── Public accessors ─────────────────────────────────────────────────────────

/** Return the current socket, if any. */
export function getSocket(): CrossfireSocket | null {
  return csocket;
}

/** Check whether the client is currently connected. */
export function clientIsConnected(): boolean {
  return csocket !== null;
}

// ── Connection lifecycle ─────────────────────────────────────────────────────

/**
 * Connect to a Crossfire server over WebSocket.
 *
 * @param hostname  WebSocket URL (e.g. "ws://server:port").
 * @param port      Optional port; appended if the hostname does not already
 *                  include one.  Defaults to the configured port or EPORT.
 */
export async function clientConnect(
  hostname: string,
  port?: number,
): Promise<void> {
  const effectivePort = port ?? useConfig.port;

  // Build WebSocket URL if the caller supplied a bare hostname.
  let url = hostname;
  if (!url.startsWith("ws://") && !url.startsWith("wss://")) {
    url = `ws://${hostname}:${effectivePort}`;
  }

  const sock = new CrossfireSocket(url);

  // Wire incoming packets to the command dispatcher.
  sock.onPacket = (data: Uint8Array) => {
    dispatchPacket(data.buffer as ArrayBuffer);
  };

  // Subscribe to the beatEnabled event for this connection.
  beatUnsubscribe = gameEvents.on("beatEnabled", startHeartbeat);

  sock.onDisconnect = () => {
    LOG(LogLevel.Info, "client", "Server disconnected");
    csocket = null;
    stopHeartbeat();
    beatUnsubscribe?.();
    beatUnsubscribe = null;
    clearNotifications();
    gameEvents.emit("disconnect");
  };

  sock.onError = (_ev: Event) => {
    LOG(LogLevel.Error, "client", "WebSocket error");
  };

  await sock.connect();

  csocket = sock;

  // Propagate the socket to the other modules.
  setCommandsSocket(sock);
  setPlayerSocket(sock);
  setItemSocket(sock);

  LOG(LogLevel.Info, "client", `Connected to ${url}`);
}

/**
 * Disconnect from the current server.
 */
export function clientDisconnect(): void {
  if (csocket) {
    LOG(LogLevel.Debug, "client", "Closing server connection");
    csocket.disconnect();
    csocket = null;
  }
  stopHeartbeat();
  beatUnsubscribe?.();
  beatUnsubscribe = null;
}

// ── Protocol negotiation ─────────────────────────────────────────────────────

/**
 * Send the client version string to the server.
 */
export function sendVersion(): void {
  csocket?.sendString(
    `version ${VERSION_CS} ${VERSION_SC} crossfire-web-client`,
  );
}

/**
 * Send the "addme" command to enter the game world.
 */
export function sendAddMe(): void {
  csocket?.sendString("addme");
}

/**
 * Request a specific map size from the server.
 */
export function clientMapsize(width: number, height: number): boolean {
  if (!notifyMapsizeSent(width, height)) {
    return false;
  }
  csocket?.sendString(`setup mapsize ${width}x${height}`);
  return true;
}

/**
 * Perform initial protocol negotiation after connecting.
 *
 * Sends the version command and a setup string requesting the desired
 * features, then asks for supplementary info (skills, exp table, motd,
 * etc.) and the preferred map size.
 */
export function clientNegotiate(): void {
  if (!csocket) {
    return;
  }

  sendVersion();

  const sound = 3; // request both sound effects and music
  const ticks = wantConfig.serverTicks ? 1 : 0;
  const darkness = wantConfig.lighting > 0 ? 1 : 0;
  const cache = wantConfig.cache ? 1 : 0;

  csocket.sendString(
    `setup map2cmd 1 tick ${ticks} sound2 ${sound} darkness ${darkness} ` +
      `spellmon 1 spellmon 2 faceset 0 facecache ${cache} ` +
      `want_pickup 1 newmapcmd 1 extendedTextInfos 1 extended_stats 1 notifications 2 ` +
      `loginmethod ${wantConfig.loginMethod}`,
  );

  csocket.sendString("requestinfo skill_info");
  csocket.sendString("requestinfo skill_extra 1");
  csocket.sendString("requestinfo exp_table");
  csocket.sendString("requestinfo knowledge_info");
  csocket.sendString("requestinfo motd");
  csocket.sendString("requestinfo news");
  csocket.sendString("requestinfo rules");

  clientMapsize(wantConfig.mapWidth, wantConfig.mapHeight);

  useConfig.download = wantConfig.download;
}

// ── Account-based login commands (loginmethod >= 1) ─────────────────────────

/**
 * Build and send a length-prefixed string pair used by accountlogin and
 * accountnew packets.  Each string is preceded by a single byte giving its
 * UTF-8 encoded byte length.
 */
function buildAccountAuthPacket(
  command: string,
  name: string,
  password: string,
): SockList {
  const sl = new SockList();
  const nameBytes = new TextEncoder().encode(name);
  const pwBytes = new TextEncoder().encode(password);
  sl.addString(`${command} `);
  sl.addChar(nameBytes.length);
  sl.addString(name);
  sl.addChar(pwBytes.length);
  sl.addString(password);
  return sl;
}

/**
 * Send an `accountlogin` command (loginmethod >= 1).
 *
 * @param name     Account name.
 * @param password Account password.
 */
export function sendAccountLogin(name: string, password: string): void {
  if (!csocket) return;
  csocket.send(buildAccountAuthPacket("accountlogin", name, password));
}

/**
 * Send an `accountnew` command to create a new account (loginmethod >= 1).
 *
 * @param name     Desired account name.
 * @param password Desired account password.
 */
export function sendAccountNew(name: string, password: string): void {
  if (!csocket) return;
  csocket.send(buildAccountAuthPacket("accountnew", name, password));
}

/**
 * Send an `accountplay` command to start playing a character (loginmethod >= 1).
 *
 * @param characterName Name of the character to play, as returned by accountplayers.
 */
export function sendAccountPlay(characterName: string): void {
  csocket?.sendString(`accountplay ${characterName}`);
}

/**
 * Send an `accountaddplayer` command to associate an existing character with
 * the logged-in account (loginmethod >= 1).
 *
 * @param force        0 for a normal request; 1 to override an existing
 *                     account association (only valid when the server's
 *                     failure response indicated force is allowed).
 * @param charName     Name of the existing character to add.
 * @param charPassword The character's own password.
 */
export function sendAccountAddPlayer(
  force: number,
  charName: string,
  charPassword: string,
): void {
  if (!csocket) return;
  const sl = new SockList();
  const enc = new TextEncoder();
  sl.addString("accountaddplayer ");
  sl.addChar(force);
  const nameBytes = enc.encode(charName);
  sl.addChar(nameBytes.length);
  sl.addString(charName);
  const pwBytes = enc.encode(charPassword);
  sl.addChar(pwBytes.length);
  sl.addString(charPassword);
  csocket.send(sl);
}

// ── Account password cache ───────────────────────────────────────────────────

/** The most-recently used account password, kept for `createplayer`. */
let _accountPassword = "";

/** Store the account password so it can be included in `createplayer` packets. */
export function setAccountPassword(password: string): void {
  _accountPassword = password;
}

/** Return the stored account password. */
export function getAccountPassword(): string {
  return _accountPassword;
}

// ── Character creation (loginmethod >= 1) ────────────────────────────────────

/**
 * Send a `requestinfo` packet to the server.
 *
 * Used by the character-creation UI to fetch race_list, class_list, etc.
 */
export function sendRequestInfo(type: string): void {
  csocket?.sendString(`requestinfo ${type}`);
}

/**
 * Build and send a `createplayer` packet (loginmethod >= 1).
 *
 * For loginmethod 1 only `charName` and `accountPassword` are required.
 * For loginmethod 2 pass `raceArch`, `classArch`, `raceChoices`,
 * `classChoices`, and `statAlloc` as well.
 *
 * Binary format (mirroring send_create_player_to_server in
 * old/gtk-v2/src/create_char.c):
 *
 *   "createplayer "
 *   [1-byte name_len][name_bytes]
 *   [1-byte password_len][password_bytes]
 *   // loginmethod 2 only:
 *   { [1-byte attr_str_len+1][attr_string][0x00] }…
 *
 * where each attr string is one of:
 *   "race <arch>"
 *   "choice <choice_name> <value_arch>"
 *   "class <arch>"
 *   "<StatName> <N>"  (e.g. "Str 5")
 */
export function sendCreatePlayer(
  charName: string,
  accountPassword: string,
  raceArch?: string,
  classArch?: string,
  raceChoices?: Array<{ choiceName: string; valueArch: string }>,
  classChoices?: Array<{ choiceName: string; valueArch: string }>,
  statAlloc?: Array<{ statName: string; value: number }>,
  startingMapArch?: string,
): void {
  if (!csocket) return;

  const sl = new SockList();
  const enc = new TextEncoder();

  sl.addString("createplayer ");

  const nameBytes = enc.encode(charName);
  sl.addChar(nameBytes.length);
  sl.addString(charName);

  const pwBytes = enc.encode(accountPassword);
  sl.addChar(pwBytes.length);
  sl.addString(accountPassword);

  // loginmethod 2: append race, choices, class, and stat allocations.
  if (raceArch && classArch) {
    const addAttr = (str: string): void => {
      const bytes = enc.encode(str);
      // The length byte is strlen + 1 (one extra for the null terminator
      // that follows the string bytes).
      sl.addChar(bytes.length + 1);
      sl.addString(str);
      sl.addChar(0); // null terminator
    };

    addAttr(`race ${raceArch}`);
    for (const c of raceChoices ?? []) {
      addAttr(`choice ${c.choiceName} ${c.valueArch}`);
    }
    addAttr(`class ${classArch}`);
    for (const c of classChoices ?? []) {
      addAttr(`choice ${c.choiceName} ${c.valueArch}`);
    }
    if (startingMapArch) {
      addAttr(`starting_map ${startingMapArch}`);
    }
    for (const s of statAlloc ?? []) {
      addAttr(`${s.statName} ${s.value}`);
    }
  }

  csocket.send(sl);
}
