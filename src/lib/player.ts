/**
 * Player action functions for the Crossfire web client.
 * Port of old/common/player.c — handles commands from client to server.
 */

import {
    COMMAND_MAX,
    CONFIG_CWINDOW,
    InputState,
    LogLevel,
    SC_FIRERUN,
    SC_MOVETO,
    SC_NORMAL,
    type Player,
} from "./protocol";
import { CrossfireSocket, SockList } from "./newsocket";
import { useConfig } from "./init";
import { LOG } from "./misc";

/** Direction strings indexed by numeric direction (0–8). */
const directions: readonly string[] = [
    "stay", "north", "northeast",
    "east", "southeast", "south",
    "southwest", "west", "northwest",
];

let csocket: CrossfireSocket | null = null;
let cpl: Player | null = null;

/** Last fire direction (or -1 if not firing). */
let dfire = -1;
/** Last run direction (or -1 if not running). */
let drun = -1;
/** Last command string sent (for dedup). */
let lastCommand = "";

// ── Key-repeat throttle (comc-ack based) ────────────────────────────────────

/**
 * The command string currently being throttled during key-repeat.
 * Reset to "" when the key is released or a different command is sent.
 */
let repeatPendingCmd = "";
/**
 * The ncom packet sequence number that was sent for the last repeat of
 * repeatPendingCmd.  We wait for the server to ack this via comc before
 * allowing another repeat of the same command.  -1 means "not yet sent".
 */
let repeatPendingSeq = -1;
/**
 * The most recent ncom packet sequence number acknowledged by the server
 * via a comc response.  -1 means no ack has been received yet.
 */
let lastNcomAcked = -1;

// ── Module wiring ────────────────────────────────────────────────────────────

export function setSocket(sock: CrossfireSocket): void {
    csocket = sock;
}

export function setCpl(p: Player): void {
    cpl = p;
}

// ── Player initialization ────────────────────────────────────────────────────

/**
 * Initialise the player object with data received from the server.
 */
export function newPlayer(tag: number, name: string, weight: number, face: number): void {
    if (!cpl || !cpl.ob) {
        return;
    }
    cpl.ob.tag = tag;
    cpl.ob.nrof = 1;
    cpl.ob.dName = name;
    cpl.ob.weight = weight / 1000;
    cpl.ob.face = face;

    // Clear spell data on new player.
    cpl.spelldata = [];
}

// ── Simple commands ──────────────────────────────────────────────────────────

export function lookAt(x: number, y: number): void {
    csocket?.sendString(`lookat ${x} ${y}`);
}

export function clientSendApply(tag: number): void {
    csocket?.sendString(`apply ${tag}`);
}

export function clientSendExamine(tag: number): void {
    csocket?.sendString(`examine ${tag}`);
}

export function clientSendMove(loc: number, tag: number, nrof: number): void {
    csocket?.sendString(`move ${loc} ${tag} ${nrof}`);
}

// ── Fire / Run ───────────────────────────────────────────────────────────────

export function fireDir(dir: number): void {
    if (cpl && cpl.inputState !== InputState.Playing) {
        return;
    }
    if (dir !== dfire) {
        if (sendCommand(`fire ${dir}`, cpl ? cpl.count : 0, SC_NORMAL)) {
            dfire = dir;
            if (cpl) {
                cpl.count = 0;
            }
        }
    } else {
        dfire &= 0xff;
    }
}

export function stopFire(): void {
    if (cpl && cpl.inputState !== InputState.Playing) {
        return;
    }
    dfire |= 0x100;
}

/** Actually send fire_stop if we were firing. */
export function clearFire(): void {
    if (dfire !== -1) {
        sendCommand("fire_stop", -1, SC_FIRERUN);
        dfire = -1;
    }
}

export function runDir(dir: number): void {
    if (dir !== drun) {
        if (sendCommand(`run ${dir}`, -1, SC_NORMAL)) {
            drun = dir;
        }
    } else {
        drun &= 0xff;
    }
}

export function stopRun(): void {
    sendCommand("run_stop", -1, SC_FIRERUN);
    drun |= 0x100;
}

export function clearRun(): void {
    if (drun !== -1) {
        sendCommand("run_stop", -1, SC_FIRERUN);
        drun = -1;
    }
}

// ── Direction helpers ────────────────────────────────────────────────────────

/**
 * Convert a direction number (0–8) to its command string.
 */
export function dirToCommand(dir: number): string {
    if (dir >= 0 && dir < directions.length) {
        return directions[dir];
    }
    return "stay";
}

/**
 * Send a directional walk command for the given direction.
 */
export function walkDir(dir: number): void {
    sendCommand(dirToCommand(dir), -1, SC_MOVETO);
}

// ── Core command sending ─────────────────────────────────────────────────────

/**
 * Send a command to the server using the ncom protocol.
 * Returns 1 if the command was sent, 0 otherwise.
 */
export function sendCommand(command: string, repeat: number, mustSend: number): number {
    if (!csocket) {
        return 0;
    }

    if (cpl && cpl.inputState === InputState.ReplyOne) {
        LOG(LogLevel.Error, "player::sendCommand",
            `Won't send command '${command}' — in reply mode`);
        if (cpl) {
            cpl.count = 0;
        }
        return 0;
    }

    const commdiff = ((csocket.commandSent - csocket.commandReceived) + 256) % 256;

    // Drop duplicate commands when the command window is full.
    if (commdiff > useConfig[CONFIG_CWINDOW] && !mustSend && command === lastCommand) {
        if (repeat !== -1 && cpl) {
            cpl.count = 0;
        }
        return 0;
    }

    // Track last command for keybinding and dedup purposes.
    lastCommand = command;

    csocket.commandSent = (csocket.commandSent + 1) % COMMAND_MAX;

    const sl = new SockList();
    sl.addString("ncom ");
    sl.addShort(csocket.commandSent);
    sl.addInt(repeat);
    sl.addString(command);
    csocket.send(sl);

    if (repeat !== -1 && cpl) {
        cpl.count = 0;
    }
    return 1;
}

// ── Reply ────────────────────────────────────────────────────────────────────

/**
 * Send a text reply to a server query.
 */
export function sendReply(text: string): void {
    csocket?.sendString(`reply ${text}`);
}

/** Return the last command string sent via sendCommand (for keybinding use). */
export function getLastCommand(): string {
    return lastCommand;
}

// ── Key-repeat throttle (comc-ack based) ────────────────────────────────────

/**
 * Record that the server has acknowledged a ncom packet.
 * Called by the `comc` command handler with the packet number from the server.
 */
export function notifyNcomAck(seq: number): void {
    lastNcomAcked = seq;
}

/**
 * Check whether a key-repeat event for `cmd` should be forwarded.
 *
 * Returns true (and clears the pending sequence) when:
 *  - The command differs from the one currently being throttled (new key), or
 *  - The server has already acknowledged the previous ncom for this command.
 *
 * Returns false (drop) when the same command is still in-flight (no comc yet).
 */
export function checkRepeatThrottle(cmd: string): boolean {
    if (cmd !== repeatPendingCmd) {
        // Different command: allow and start tracking the new one.
        repeatPendingCmd = cmd;
        repeatPendingSeq = -1;
        return true;
    }
    // Same command: allow only if no ack is expected (first repeat) or the
    // previous ncom has already been acknowledged.
    if (repeatPendingSeq === -1 || lastNcomAcked === repeatPendingSeq) {
        return true;
    }
    return false; // still waiting for comc ack
}

/**
 * Record the ncom packet sequence number that was just sent for the current
 * repeat command.  Must be called immediately after a successful send so we
 * know which comc ack to wait for.
 */
export function recordRepeatSend(): void {
    if (csocket) {
        repeatPendingSeq = csocket.commandSent;
    }
}

/**
 * Reset the key-repeat throttle.  Call this when the player releases a key
 * so that immediately re-pressing the same key sends the command right away
 * without waiting for the comc ack.
 */
export function resetRepeatThrottle(): void {
    repeatPendingCmd = "";
    repeatPendingSeq = -1;
}
