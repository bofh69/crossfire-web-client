/**
 * Client connection management for the Crossfire web client.
 * Port of old/common/client.c — handles connecting, negotiation, and
 * high-level protocol setup.
 */

import {
    CONFIG_CACHE,
    CONFIG_DOWNLOAD,
    CONFIG_LIGHTING,
    CONFIG_MAPHEIGHT,
    CONFIG_MAPWIDTH,
    CONFIG_PORT,
    CONFIG_SERVER_TICKS,
    EPORT,
    VERSION_CS,
    VERSION_SC,
    LogLevel,
} from "./protocol";
import { CrossfireSocket } from "./newsocket";
import {
    dispatchPacket,
    setSocket as setCommandsSocket,
} from "./commands";
import { gameEvents } from "./events";
import { wantConfig, useConfig } from "./init";
import { setSocket as setPlayerSocket } from "./player";
import { setCSocket as setItemSocket } from "./item";
import { LOG } from "./misc";

let csocket: CrossfireSocket | null = null;

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
export async function clientConnect(hostname: string, port?: number): Promise<void> {
    const effectivePort = port ?? useConfig[CONFIG_PORT] ?? EPORT;

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

    sock.onDisconnect = () => {
        LOG(LogLevel.Info, "client", "Server disconnected");
        csocket = null;
        gameEvents.emit('disconnect');
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
}

// ── Protocol negotiation ─────────────────────────────────────────────────────

/**
 * Send the client version string to the server.
 */
export function sendVersion(): void {
    csocket?.sendString(`version ${VERSION_CS} ${VERSION_SC} crossfire-web-client`);
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
export function clientMapsize(width: number, height: number): void {
    csocket?.sendString(`setup mapsize ${width}x${height}`);
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
    const ticks = wantConfig[CONFIG_SERVER_TICKS];
    const darkness = wantConfig[CONFIG_LIGHTING] ? 1 : 0;
    const cache = wantConfig[CONFIG_CACHE];

    csocket.sendString(
        `setup map2cmd 1 tick ${ticks} sound2 ${sound} darkness ${darkness} ` +
        `spellmon 1 spellmon 2 faceset 0 facecache ${cache} ` +
        `want_pickup 1 newmapcmd 1 extendedTextInfos 1`,
    );

    csocket.sendString("requestinfo skill_info");
    csocket.sendString("requestinfo exp_table");
    csocket.sendString("requestinfo motd");
    csocket.sendString("requestinfo news");
    csocket.sendString("requestinfo rules");

    clientMapsize(wantConfig[CONFIG_MAPWIDTH], wantConfig[CONFIG_MAPHEIGHT]);

    useConfig[CONFIG_DOWNLOAD] = wantConfig[CONFIG_DOWNLOAD];
}
