/**
 * cmd_dialog.ts — Server dialog-system state machine.
 *
 * Tracks the sequence of drawextinfo messages that make up the server's
 * NPC-dialog protocol:
 *
 *   MSG_TYPE_DIALOG  → drawextinfo type 6 (NPC speech)
 *   MSG_TYPE_COMMUNICATION "Replies:" → marks start of reply options
 *   MSG_TYPE_COMMUNICATION " - key: value" → one reply option per line
 *   Any other server packet → ends the option list
 *
 * Usage:
 *   - Call processDialogExtInfo() for every incoming drawextinfo packet
 *     (BEFORE forwarding to the InfoPanel).  Returns true when the packet
 *     should be suppressed from the InfoPanel.
 *   - Call notifyNonDrawExtInfoCommand() whenever any server packet that is
 *     NOT a drawextinfo is dispatched, so the state machine can finalise any
 *     pending option list.
 */

import { MSG_TYPE_COMMUNICATION, MSG_TYPE_DIALOG } from "./protocol.js";
import { gameEvents } from "./events.js";

// ── State machine ─────────────────────────────────────────────────────────────

const enum DialogState {
  Idle, // No dialog in progress
  AfterDialog, // Received MSG_TYPE_DIALOG, waiting for "Replies:"
  Collecting, // Got "Replies:", accumulating " - key: value" lines
}

let state: DialogState = DialogState.Idle;
let pendingOptions: Array<{ key: string; value: string }> = [];

// ── Internal helpers ──────────────────────────────────────────────────────────

/**
 * Finalise a collection run:
 * - If clearOnly is true (new MSG_TYPE_DIALOG arrived), emit clearDialogOptions
 *   so any currently displayed buttons are removed immediately.
 * - Otherwise, if options were collected, emit dialogOptions to show them.
 */
function endCollection(clearOnly: boolean): void {
  if (clearOnly) {
    gameEvents.emit("clearDialogOptions");
  } else if (pendingOptions.length > 0) {
    gameEvents.emit("dialogOptions", pendingOptions);
  }
  pendingOptions = [];
}

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * Process a single drawextinfo packet through the dialog state machine.
 *
 * Returns true when the packet should be suppressed from the InfoPanel
 * (i.e. "Replies:" header lines and " - key: value" option lines).
 * Returns false for all other packets.
 */
export function processDialogExtInfo(
  _color: number,
  type: number,
  _subtype: number,
  message: string,
): boolean {
  if (type === MSG_TYPE_DIALOG) {
    // A new NPC-dialog message always resets the state and clears any
    // previously displayed reply buttons.
    endCollection(true);
    state = DialogState.AfterDialog;
    return false; // Forward the NPC speech to the InfoPanel as normal
  }

  if (type !== MSG_TYPE_COMMUNICATION) {
    // A drawextinfo of a different type ends any active collection.
    endCollection(false);
    state = DialogState.Idle;
    return false;
  }

  // type === MSG_TYPE_COMMUNICATION
  if (state === DialogState.AfterDialog) {
    if (message === "Replies:") {
      state = DialogState.Collecting;
      return true; // Suppress "Replies:" from InfoPanel
    }
    // Any other communication line while waiting — cancel dialog state.
    state = DialogState.Idle;
    return false;
  }

  if (state === DialogState.Collecting) {
    // Match " - key: value" (leading space, dash, space, then key:value).
    // Keys are expected to be simple identifiers (word characters and hyphens).
    const match = /^ - ([\w-]+): (.+)$/.exec(message);
    if (match) {
      // match[1] and match[2] are always defined when the regex matches.
      pendingOptions.push({ key: match[1]!, value: match[2]! });
      return true; // Suppress option line from InfoPanel
    }
    // A communication line that doesn't match the option format ends collection.
    endCollection(false);
    state = DialogState.Idle;
    return false;
  }

  // state === DialogState.Idle and type === MSG_TYPE_COMMUNICATION
  return false;
}

/**
 * Notify the state machine that a non-drawextinfo server command was received.
 * This finalises any in-progress option collection (emitting dialogOptions if
 * options were gathered) and resets the state to Idle.
 */
export function notifyNonDrawExtInfoCommand(): void {
  endCollection(false);
  state = DialogState.Idle;
}
