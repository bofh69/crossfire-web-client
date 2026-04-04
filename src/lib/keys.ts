/**
 * Keyboard handling for the Crossfire web client.
 *
 * Faithfully ported from old/gtk-v2/src/keys.c.  Provides:
 *  • Keybinding storage, lookup, insert, and removal
 *  • parse_key / parse_key_release equivalents for web KeyboardEvent
 *  • bind / unbind command implementations
 *  • Persistence via localStorage
 *
 * Modifier key mapping (old → new):
 *   Shift      → fire modifier
 *   Alt        → run modifier
 *   Control    → ctrl modifier
 *   Meta       → meta modifier
 *   apostrophe → command mode
 */

import { InputState, type Player } from "./protocol";
import { loadConfig, saveConfig } from "./storage";
import { extendedCommand } from "./p_cmd";
import {
    fireDir, runDir, clearFire, clearRun, stopFire, stopRun,
    sendCommand, checkRepeatThrottle, resetRepeatThrottle, recordRepeatSend,
} from "./player";
import { LOG } from "./misc";
import { LogLevel } from "./protocol";

// ──────────────────────────────────────────────────────────────────────────────
// Flag definitions (matching the C KEYF_* constants)
// ──────────────────────────────────────────────────────────────────────────────

export const KEYF_MOD_SHIFT = 1 << 0; // Fire modifier
export const KEYF_MOD_CTRL  = 1 << 1; // Run modifier
export const KEYF_MOD_ALT   = 1 << 2;
export const KEYF_MOD_META  = 1 << 3;
export const KEYF_MOD_MASK  = KEYF_MOD_SHIFT | KEYF_MOD_CTRL |
                              KEYF_MOD_ALT | KEYF_MOD_META;
export const KEYF_ANY       = 1 << 4; // Match regardless of modifiers
export const KEYF_EDIT      = 1 << 5; // Enter command-editing mode

// ──────────────────────────────────────────────────────────────────────────────
// Direction names (indexed 0–8, matching C `directions[]`)
// ──────────────────────────────────────────────────────────────────────────────

const directions: readonly string[] = [
    "stay", "north", "northeast",
    "east", "southeast", "south",
    "southwest", "west", "northwest",
];

/**
 * If `command` is a direction word return its direction index (0–8),
 * otherwise return -1.
 */
function directionFromCommand(command: string): number {
    for (let i = 0; i < directions.length; i++) {
        if (command === directions[i]) return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────────────
// Keybinding structure
// ──────────────────────────────────────────────────────────────────────────────

export interface KeyBind {
    keysym: string;   // KeyboardEvent.key (lower-cased for letters)
    flags: number;    // KEYF_* bitmask
    direction: number; // -1 for non-direction, 0-8 for direction
    command: string;
}

// ──────────────────────────────────────────────────────────────────────────────
// Storage – flat array (simple alternative to the C hash table)
// ──────────────────────────────────────────────────────────────────────────────

let bindings: KeyBind[] = [];

// ──────────────────────────────────────────────────────────────────────────────
// Lookup
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Find a binding for the given keysym and modifier flags.
 *
 * If the binding has KEYF_ANY, modifiers are ignored.
 * If the caller passes KEYF_ANY in flags, any binding for that keysym matches.
 * Otherwise, the modifier bits (KEYF_MOD_MASK) must match exactly.
 */
function keybindFind(keysym: string, flags: number): KeyBind | null {
    for (const kb of bindings) {
        if (kb.keysym !== keysym) continue;
        if ((kb.flags & KEYF_ANY) || (flags & KEYF_ANY)) return kb;
        if ((kb.flags & KEYF_MOD_MASK) === (flags & KEYF_MOD_MASK)) return kb;
    }
    return null;
}

// ──────────────────────────────────────────────────────────────────────────────
// Insert / remove
// ──────────────────────────────────────────────────────────────────────────────

function keybindInsert(keysym: string, flags: number, command: string): void {
    // Remove any existing binding with same keysym & flags.
    bindings = bindings.filter(kb => {
        if (kb.keysym !== keysym) return true;
        if ((kb.flags & KEYF_ANY) || (flags & KEYF_ANY)) return false;
        if ((kb.flags & KEYF_MOD_MASK) === (flags & KEYF_MOD_MASK)) return false;
        return true;
    });
    bindings.push({
        keysym,
        flags,
        direction: directionFromCommand(command),
        command,
    });
}

function keybindRemoveIndex(index: number): void {
    bindings.splice(index, 1);
}

// ──────────────────────────────────────────────────────────────────────────────
// Flag-string helpers (for display / persistence)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Convert a KEYF_* bitmask to the compact flag string used in def-keys.
 *
 * 'A' = any-modifier, 'N' = normal (no modifiers),
 * 'F' = fire (shift), 'R' = run (ctrl), 'E' = edit-mode.
 */
function flagsToString(f: number): string {
    let s = "";
    if (f & KEYF_ANY) s += "A";
    if ((f & KEYF_MOD_MASK) === 0) s += "N";
    if (f & KEYF_MOD_SHIFT) s += "F";
    if (f & KEYF_MOD_CTRL) s += "R";
    if (f & KEYF_MOD_ALT) s += "L";
    if (f & KEYF_MOD_META) s += "M";
    if (f & KEYF_EDIT) s += "E";
    return s;
}

/**
 * Parse a flag string back to KEYF_* bitmask.
 */
function parseFlags(s: string): number {
    let f = 0;
    for (const ch of s) {
        switch (ch) {
            case 'A': f |= KEYF_ANY; break;
            case 'N': break; // normal = no modifier bits
            case 'F': f |= KEYF_MOD_SHIFT; break;
            case 'R': f |= KEYF_MOD_CTRL; break;
            case 'L': f |= KEYF_MOD_ALT; break;
            case 'M': f |= KEYF_MOD_META; break;
            case 'E': f |= KEYF_EDIT; break;
        }
    }
    return f;
}

// ──────────────────────────────────────────────────────────────────────────────
// Default bindings (from old/common/def-keys)
// ──────────────────────────────────────────────────────────────────────────────

function loadDefaultBindings(): void {
    const defs: [string, string, string][] = [
        // [keysym, flagString, command]
        // Basic keys
        ['"',           'AE', 'say '],
        ['Enter',       'AE', 'chat '],
        [';',           'NE', 'reply'],
        [',',           'A',  'take'],
        ['<',           'F',  'get all'],
        ['.',           'N',  'stay fire'],
        ['?',           'A',  'help'],
        ['a',           'N',  'apply'],
        ['d',           'N',  'disarm'],
        ['e',           'NR', 'examine'],
        ['s',           'F',  'brace'],
        ['s',           'N',  'search'],
        ['t',           'N',  'ready_skill throwing'],

        // Nethack-style (Normal)
        ['b', 'N', 'southwest'], ['h', 'N', 'west'],
        ['j', 'N', 'south'],    ['k', 'N', 'north'],
        ['l', 'N', 'east'],     ['n', 'N', 'southeast'],
        ['u', 'N', 'northeast'],['y', 'N', 'northwest'],

        // Nethack-style (Run)
        ['b', 'R', 'southwest'], ['h', 'R', 'west'],
        ['j', 'R', 'south'],    ['k', 'R', 'north'],
        ['l', 'R', 'east'],     ['n', 'R', 'southeast'],
        ['u', 'R', 'northeast'],['y', 'R', 'northwest'],

        // Nethack-style (Fire)
        ['b', 'F', 'southwest'], ['h', 'F', 'west'],
        ['j', 'F', 'south'],    ['k', 'F', 'north'],
        ['l', 'F', 'east'],     ['n', 'F', 'southeast'],
        ['u', 'F', 'northeast'],['y', 'F', 'northwest'],

        // Arrow keys
        ['ArrowUp',    'A', 'north'],
        ['ArrowDown',  'A', 'south'],
        ['ArrowLeft',  'A', 'west'],
        ['ArrowRight', 'A', 'east'],

        // Nav cluster / numpad (numlock off)
        ['Home',     'A', 'northwest'], ['End',      'A', 'southwest'],
        ['PageUp',   'A', 'northeast'], ['PageDown', 'A', 'southeast'],

        // Action rotation
        ['+', 'A',  'rotateshoottype'],
        ['-', 'A',  'rotateshoottype -'],
        ['-', 'N',  'rotateshoottype -1'],
        ['+', 'NF', 'rotateshoottype'],
    ];

    for (const [keysym, flagStr, command] of defs) {
        keybindInsert(keysym, parseFlags(flagStr), command);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Persistence (localStorage)
// ──────────────────────────────────────────────────────────────────────────────

interface SavedBinding {
    keysym: string;
    flags: string; // compact flag string
    command: string;
}

const STORAGE_KEY = "cf_keybindings";

function saveBindings(): void {
    const data: SavedBinding[] = bindings.map(kb => ({
        keysym: kb.keysym,
        flags: flagsToString(kb.flags),
        command: kb.command,
    }));
    saveConfig(STORAGE_KEY, data);
}

function loadSavedBindings(): boolean {
    const data = loadConfig<SavedBinding[] | null>(STORAGE_KEY, null);
    if (!data || !Array.isArray(data)) return false;
    bindings = [];
    for (const entry of data) {
        keybindInsert(entry.keysym, parseFlags(entry.flags), entry.command);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Initialization
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Initialise the keybinding system.  If saved bindings exist in
 * localStorage they are loaded; otherwise the hardcoded defaults are used.
 */
export function keybindingsInit(): void {
    bindings = [];
    if (!loadSavedBindings()) {
        loadDefaultBindings();
    }
    LOG(LogLevel.Debug, "keys::keybindingsInit",
        `Loaded ${bindings.length} keybindings.`);
}

// ──────────────────────────────────────────────────────────────────────────────
// Core event handling
// ──────────────────────────────────────────────────────────────────────────────

/** Normalise a KeyboardEvent.key to the keysym we use for lookups. */
function normaliseKey(key: string): string {
    // Single printable characters are lowered (like gdk_keyval_to_lower).
    if (key.length === 1) return key.toLowerCase();
    return key;
}

// Callback interface so keys.ts doesn't depend on UI modules directly.
export interface KeyCallbacks {
    /** Show a message in the info panel. */
    drawInfo: (message: string) => void;
    /** Focus the command input field and optionally pre-fill text. */
    focusCommandInput: (prefill?: string) => void;
    /** Get the current Player object. */
    getCpl: () => Player | null;
}

let cb: KeyCallbacks | null = null;

export function setKeyCallbacks(c: KeyCallbacks): void {
    cb = c;
}

/**
 * Handle a keydown event while in Playing state.
 * Equivalent to the C parse_key() function.
 */
export function parseKey(e: KeyboardEvent): void {
    const cpl = cb?.getCpl();
    if (!cpl) return;

    const keysym = normaliseKey(e.key);

    // ── Command-mode key (apostrophe) ───────────────────────────────
    if (keysym === "'") {
        cb?.focusCommandInput();
        cpl.inputState = InputState.CommandMode;
        cpl.noEcho = false;
        return;
    }

    // ── Modifier keys: just set state, don't look up a binding ──────
    if (e.key === "Shift" || e.key === "ShiftLeft" || e.key === "ShiftRight") {
        cpl.fireOn = true;
        return;
    }
    if (e.key === "Control" || e.key === "ControlLeft" || e.key === "ControlRight") {
        cpl.altOn = true;
        return;
    }
    if (e.key === "Alt" || e.key === "AltLeft" || e.key === "AltRight") {
        cpl.runOn = true;
        return;
    }
    if (e.key === "Meta" || e.key === "MetaLeft" || e.key === "MetaRight") {
        cpl.metaOn = true;
        return;
    }

    // ── Build modifier flags from current player state ──────────────
    let presentFlags = 0;
    if (cpl.runOn)  presentFlags |= KEYF_MOD_CTRL;
    if (cpl.fireOn) presentFlags |= KEYF_MOD_SHIFT;
    if (cpl.altOn)  presentFlags |= KEYF_MOD_ALT;
    if (cpl.metaOn) presentFlags |= KEYF_MOD_META;

    // ── Look up the keybinding ──────────────────────────────────────
    const kb = keybindFind(keysym, presentFlags);

    if (kb) {
        // Edit-mode binding: prefill command input and focus it
        if (kb.flags & KEYF_EDIT) {
            cpl.inputState = InputState.CommandMode;
            cb?.focusCommandInput(kb.command);
            return;
        }

        // Direction binding with fire/run modifier
        if (kb.direction >= 0) {
            if (cpl.fireOn) {
                fireDir(kb.direction);
                return;
            }
            if (cpl.runOn) {
                runDir(kb.direction);
                return;
            }
        }

        // Normal command — throttle key-repeat: only send if the previous
        // ncom for this command has been acknowledged by the server (comc).
        if (e.repeat && !checkRepeatThrottle(kb.command)) {
            return;
        }
        extendedCommand(kb.command);
        if (e.repeat) {
            recordRepeatSend();
        }
        return;
    }

    // ── Numeric count accumulation ──────────────────────────────────
    if (keysym >= '0' && keysym <= '9') {
        cpl.count = cpl.count * 10 + (keysym.charCodeAt(0) - 48);
        if (cpl.count > 10000000) cpl.count %= 10000000;
        return;
    }

    // ── Unbound key ─────────────────────────────────────────────────
    let prefix = "";
    if (cpl.fireOn) prefix += "fire+";
    if (cpl.runOn)  prefix += "run+";
    if (cpl.altOn)  prefix += "alt+";
    if (cpl.metaOn) prefix += "meta+";
    cb?.drawInfo(
        `Key ${prefix}${keysym} is not bound to any command. ` +
        `Use 'bind' to associate this keypress with a command.`
    );
    cpl.count = 0;
}

/**
 * Handle a keyup event (any input state).
 * Equivalent to the C parse_key_release() function.
 */
export function parseKeyRelease(e: KeyboardEvent): void {
    const cpl = cb?.getCpl();
    if (!cpl) return;

    if (e.key === "Shift" || e.key === "ShiftLeft" || e.key === "ShiftRight") {
        cpl.fireOn = false;
        clearFire();
        return;
    }
    if (e.key === "Control" || e.key === "ControlLeft" || e.key === "ControlRight") {
        cpl.altOn = false;
        return;
    }
    if (e.key === "Alt" || e.key === "AltLeft" || e.key === "AltRight") {
        cpl.runOn = false;
        clearRun();
        return;
    }
    if (e.key === "Meta" || e.key === "MetaLeft" || e.key === "MetaRight") {
        cpl.metaOn = false;
        return;
    }

    // If firing and a non-modifier key is released, stop firing
    // (same as old C behaviour).
    if (cpl.fireOn) {
        clearFire();
    }

    // A key was released: reset the repeat throttle so that immediately
    // re-pressing the same key sends the command without waiting for a
    // comc acknowledgement from the server.
    resetRepeatThrottle();
}

/**
 * Called when the browser window/tab loses focus.
 * Clears all modifier state so we don't get stuck fire/run.
 */
export function handleFocusLost(): void {
    const cpl = cb?.getCpl();
    if (!cpl) return;
    if (cpl.fireOn) { cpl.fireOn = false; clearFire(); }
    if (cpl.runOn)  { cpl.runOn = false;  clearRun(); }
    cpl.altOn = false;
    cpl.metaOn = false;
}

// ──────────────────────────────────────────────────────────────────────────────
// bind / unbind command implementations
// ──────────────────────────────────────────────────────────────────────────────

// State used during the Configure_Keys phase.
let bindFlags = 0;
let bindCommand = "";

/**
 * Implements the `/bind` command.
 *
 * Usage: `bind [-e] [-i] [-g] <command>`
 *
 * After parsing flags, the client enters Configure_Keys state and waits
 * for the next keypress, which is then bound to the command.
 *
 * Flags:
 *   -e   enter edit mode (prefill command input)
 *   -i   bind for any modifier combination (KEYF_ANY)
 *   -g   (ignored in web client, kept for compat)
 */
export function bindKey(params: string): void {
    if (!params || params.trim().length === 0) {
        cb?.drawInfo(
            "Usage: bind [-e] [-i] <command>\n" +
            "  -e  enter edit mode when key is pressed\n" +
            "  -i  ignore modifiers (keybinding works no matter if Shift/Ctrl etc are held)\n" +
            "Press the key you want to bind after entering the command."
        );
        return;
    }

    let rest = params.trim();
    bindFlags = 0;

    // Parse option flags
    while (rest.startsWith("-")) {
        const spaceIdx = rest.indexOf(" ");
        const flag = spaceIdx > 0 ? rest.substring(0, spaceIdx) : rest;
        if (flag === "-e") {
            bindFlags |= KEYF_EDIT;
        } else if (flag === "-i") {
            bindFlags |= KEYF_ANY;
        } else if (flag === "-g") {
            // global scope — ignored in web client
        }
        rest = spaceIdx > 0 ? rest.substring(spaceIdx + 1).trimStart() : "";
    }

    if (rest.length === 0) {
        cb?.drawInfo("bind: no command specified.");
        return;
    }

    bindCommand = rest;
    const cpl = cb?.getCpl();
    if (cpl) {
        cpl.inputState = InputState.ConfigureKeys;
    }
    cb?.drawInfo(`Push key to bind to command: ${bindCommand}`);
}

/**
 * Called when a key is pressed while in Configure_Keys state.
 * Completes the bind operation.
 */
export function configureKeys(e: KeyboardEvent): void {
    const cpl = cb?.getCpl();
    if (!cpl) return;

    // Ignore pure modifier keys during configure
    if (["Shift", "Control", "Alt", "Meta"].includes(e.key)) return;

    const keysym = normaliseKey(e.key);

    // Add modifier flags based on what's currently held
    let flags = bindFlags;
    if (!(flags & KEYF_ANY)) {
        if (e.shiftKey) flags |= KEYF_MOD_SHIFT;
        if (e.altKey)   flags |= KEYF_MOD_CTRL;  // Alt = run modifier
        if (e.ctrlKey)  flags |= KEYF_MOD_ALT;   // Ctrl = alt modifier
        if (e.metaKey)  flags |= KEYF_MOD_META;
    }

    keybindInsert(keysym, flags, bindCommand);
    saveBindings();

    const flagStr = flagsToString(flags);
    cb?.drawInfo(`Bound key '${keysym}' [${flagStr}] to: ${bindCommand}`);

    cpl.inputState = InputState.Playing;
}

/**
 * Implements the `/unbind` command.
 *
 * With no arguments: lists all bindings with indices.
 * With a number: removes the binding at that index.
 * With a key name: removes all bindings for that key.
 */
export function unbindKey(params: string): void {
    const arg = params.trim();

    if (arg.length === 0) {
        // List all bindings
        cb?.drawInfo("Current keybindings (use 'unbind <number>' to remove):");
        for (let i = 0; i < bindings.length; i++) {
            const kb = bindings[i];
            const flagStr = flagsToString(kb.flags);
            cb?.drawInfo(`  ${i}: [${flagStr}] ${kb.keysym} → ${kb.command}`);
        }
        return;
    }

    const idx = parseInt(arg, 10);
    if (!isNaN(idx) && idx >= 0 && idx < bindings.length) {
        const kb = bindings[idx];
        cb?.drawInfo(`Removed binding ${idx}: ${kb.keysym} → ${kb.command}`);
        keybindRemoveIndex(idx);
        saveBindings();
        return;
    }

    // Try to remove by key name
    const normalised = normaliseKey(arg);
    const before = bindings.length;
    bindings = bindings.filter(kb => kb.keysym !== normalised);
    if (bindings.length < before) {
        cb?.drawInfo(`Removed ${before - bindings.length} binding(s) for key '${normalised}'.`);
        saveBindings();
    } else {
        cb?.drawInfo(`No binding found for '${arg}'. Try 'unbind' with no options to list.`);
    }
}

/**
 * Reset all keybindings to defaults (removes saved bindings).
 */
export function resetBindings(): void {
    bindings = [];
    loadDefaultBindings();
    saveBindings();
    cb?.drawInfo("Key bindings reset to defaults.");
}

/** Return a read-only view of the current bindings (for debug/display). */
export function getBindings(): readonly KeyBind[] {
    return bindings;
}

/**
 * Convert a KEYF_* bitmask to a human-readable modifier description.
 * E.g. "Alt+Shift" for run+fire, "Any" for KEYF_ANY, "(none)" for normal.
 */
export function flagsToDisplayString(f: number): string {
    const parts: string[] = [];
    if (f & KEYF_ANY)       parts.push("Any");
    if (f & KEYF_MOD_SHIFT) parts.push("Shift");
    if (f & KEYF_MOD_CTRL)  parts.push("Alt");   // run = Alt key
    if (f & KEYF_MOD_ALT)   parts.push("Ctrl");  // alt = Ctrl key
    if (f & KEYF_MOD_META)  parts.push("Meta");
    if (f & KEYF_EDIT)      parts.push("Edit");
    return parts.length > 0 ? parts.join("+") : "(none)";
}

// ──────────────────────────────────────────────────────────────────────────────
// Interactive bind / unbind helpers (used by the MenuBar key-binding dialog)
// ──────────────────────────────────────────────────────────────────────────────

/** Build a human-readable string from a keyboard event (e.g. "Ctrl+Shift+a"). */
export function keyEventToString(e: KeyboardEvent): string {
    const parts: string[] = [];
    if (e.ctrlKey)  parts.push('Ctrl');
    if (e.altKey)   parts.push('Alt');
    if (e.shiftKey) parts.push('Shift');
    if (e.metaKey)  parts.push('Meta');
    parts.push(e.key);
    return parts.join('+');
}

/** Build a KEYF_* bitmask from a keyboard event (modifiers only). */
export function keyEventToFlags(e: KeyboardEvent): number {
    let flags = 0;
    if (e.shiftKey) flags |= KEYF_MOD_SHIFT;
    if (e.altKey)   flags |= KEYF_MOD_CTRL;  // Alt = run modifier
    if (e.ctrlKey)  flags |= KEYF_MOD_ALT;   // Ctrl = alt modifier
    if (e.metaKey)  flags |= KEYF_MOD_META;
    return flags;
}

/**
 * Look up an existing binding for a keyboard event.
 * Returns the matching KeyBind or null.
 */
export function findBindingForEvent(e: KeyboardEvent): KeyBind | null {
    const keysym = normaliseKey(e.key);
    const flags = keyEventToFlags(e);
    return keybindFind(keysym, flags);
}

/**
 * Bind a command to the key described by a keyboard event.
 * Replaces any existing binding for the same key+modifiers.
 * Persists to localStorage.
 */
export function bindCommandToEvent(e: KeyboardEvent, command: string): void {
    const keysym = normaliseKey(e.key);
    const flags = keyEventToFlags(e);
    keybindInsert(keysym, flags, command);
    saveBindings();
}

/**
 * Remove the binding for the key described by a keyboard event.
 * Returns the removed binding, or null if nothing was bound.
 * Persists to localStorage.
 */
export function unbindEvent(e: KeyboardEvent): KeyBind | null {
    const keysym = normaliseKey(e.key);
    const flags = keyEventToFlags(e);
    const kb = keybindFind(keysym, flags);
    if (kb) {
        bindings = bindings.filter(b => b !== kb);
        saveBindings();
    }
    return kb;
}
