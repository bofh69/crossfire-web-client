/**
 * Client-side slash-command handling for the Crossfire web client.
 * Port of old/common/p_cmd.c (excluding script-related commands).
 */

import { CommCat, LogLevel, type ConsoleCommand } from "./protocol";
import { LOG } from "./misc";
import { sendCommand, setLastCommand } from "./player";
import { resetBindings } from "./keys";
import { getCpl } from "./init";

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

/** Commands sorted by name (populated by initCommands). */
let commandList: ConsoleCommand[] = [];

/** Commands sorted by category then name (lazily built). */
let catSorted: ConsoleCommand[] | null = null;

/**
 * Server-side command names used only for tab-completion.
 * Subset of the ~100 entries from p_cmd.c's server_commands[].
 */
const serverCommands: string[] = [
    "apply", "cast", "drop", "east", "examine", "get", "invoke",
    "killpets", "listen", "maps", "mark", "motd", "north", "northeast",
    "northwest", "output-count", "output-sync", "party", "peaceful",
    "pickup", "pray", "quit", "rename", "reply", "rotateshoottype",
    "say", "shout", "skills", "south", "southeast", "southwest",
    "stay", "tell", "title", "use_skill", "version", "west",
    "who", "wimpy",
];

// ---------------------------------------------------------------------------
// Category display names
// ---------------------------------------------------------------------------

function getCategoryName(cat: CommCat): string {
    switch (cat) {
        case CommCat.Misc:  return "Miscellaneous";
        case CommCat.Info:  return "Informational";
        case CommCat.Setup: return "Configuration";
        case CommCat.Debug: return "Debugging";
        default:            return "Other";
    }
}

// ---------------------------------------------------------------------------
// Callbacks (wired by App.svelte after MenuBar mounts)
// ---------------------------------------------------------------------------

export interface PCmdCallbacks {
    /** Display a message in the info panel. */
    drawInfo: (message: string) => void;
    /** Open the keyboard key-bind dialog (reads lastCommand internally). */
    openKeyBind: () => void;
    /** Open the gamepad button-bind dialog (reads lastCommand internally). */
    openGamepadBind: () => void;
    /** Show the magic map overlay (re-display last received data). */
    showMagicMap?: () => void;
}

let pcmdCallbacks: PCmdCallbacks | null = null;

/** Wire callbacks from the UI layer so local commands can open dialogs. */
export function setPCmdCallbacks(cbs: PCmdCallbacks): void {
    pcmdCallbacks = cbs;
}

/** Display a message in the info panel (falls back to console).
 *  Multi-line strings are split so each line is shown on its own row. */
function drawInfo(message: string): void {
    const lines = message.split("\n");
    if (pcmdCallbacks) {
        for (const line of lines) {
            pcmdCallbacks.drawInfo(line);
        }
    } else {
        for (const line of lines) {
            LOG(LogLevel.Info, 'p_cmd', line);
        }
    }
}

// ---------------------------------------------------------------------------
// Built-in command handlers
// ---------------------------------------------------------------------------

function commandHelp(args: string): void {
    if (args.length === 0) {
        const sorted = getCatSortedCommands();
        let lastCat: CommCat | null = null;
        const lines: string[] = ["Available client commands:"];
        for (const cmd of sorted) {
            if (cmd.category !== lastCat) {
                lastCat = cmd.category;
                lines.push(`\n  ${getCategoryName(cmd.category)}:`);
            }
            const desc = cmd.description ?? "";
            lines.push(`    ${cmd.name.padEnd(16)} ${desc}`);
        }
        drawInfo(lines.join("\n"));
        sendCommand('help', 0, 1);
        return;
    }

    const target = findCommand(args.trim());
    if (target) {
        drawInfo(`${target.name}: ${target.description ?? "(no description)"}`);
        if (target.longDescription) {
            drawInfo(target.longDescription);
        }
    } else {
        sendCommand(`help ${args.trim()}`, 0, 1);
    }
}

function commandBind(args: string): void {
    if (!args || args.trim().length === 0) {
        drawInfo(
            "Usage: bind <command>\n" +
            "  Sets <command> as the pending command and opens the key-bind dialog.");
        return;
    }
    setLastCommand(args.trim());
    pcmdCallbacks?.openKeyBind();
}

function commandGamepadBind(args: string): void {
    if (!args || args.trim().length === 0) {
        drawInfo(
            "Usage: gamepad_bind <command>\n" +
            "  Sets <command> as the pending command and opens the gamepad button-bind dialog.");
        return;
    }
    setLastCommand(args.trim());
    pcmdCallbacks?.openGamepadBind();
}

function commandResetKeys(_args: string): void {
    resetBindings();
}

function commandMagicmap(_args: string): void {
    const cpl = getCpl();
    if (!cpl || !cpl.magicmap) {
        drawInfo("No magic map data available.");
        return;
    }
    cpl.showmagic = 1;
    pcmdCallbacks?.showMagicMap?.();
}

function commandInv(_args: string): void {
    LOG(LogLevel.Info, "p_cmd::inv", "Requesting inventory display.");
    sendCommand("inv", 0, 1);
}

function commandTake(args: string): void {
    const what = args.length > 0 ? args : "";
    sendCommand(`take ${what}`.trim(), 0, 1);
}

// ---------------------------------------------------------------------------
// Command table
// ---------------------------------------------------------------------------

const builtinCommands: ConsoleCommand[] = [
    {
        name: "bind",
        category: CommCat.Setup,
        description: "Store a command and open the key-bind dialog",
        longDescription:
            "Syntax:\n" +
            "  bind <command>\n" +
            "\n" +
            "Stores <command> as the pending command (without sending it to the\n" +
            "server) and opens the keyboard key-bind dialog so you can assign\n" +
            "it to a key.\n" +
            "\n" +
            "Example:\n" +
            "  bind cast fireball\n" +
            "    Opens the bind dialog with 'cast fireball' ready to be assigned.",
        handler: commandBind,
    },
    {
        name: "gamepad_bind",
        category: CommCat.Setup,
        description: "Store a command and open the gamepad button-bind dialog",
        longDescription:
            "Syntax:\n" +
            "  gamepad_bind <command>\n" +
            "\n" +
            "Stores <command> as the pending command (without sending it to the\n" +
            "server) and opens the gamepad button-bind dialog so you can assign\n" +
            "it to a controller button.\n" +
            "\n" +
            "Example:\n" +
            "  gamepad_bind cast fireball\n" +
            "    Opens the gamepad bind dialog with 'cast fireball' ready to be assigned.",
        handler: commandGamepadBind,
    },
    { name: "help",         category: CommCat.Misc,  description: "Show help on commands",            handler: commandHelp },
    { name: "inv",          category: CommCat.Misc,  description: "Request inventory from server",    handler: commandInv },
    { name: "magicmap",     category: CommCat.Misc,  description: "Show last received magic map",     handler: commandMagicmap },
    { name: "resetkeys",    category: CommCat.Setup, description: "Reset all key bindings to default",handler: commandResetKeys },
    { name: "take",         category: CommCat.Misc,  description: "Take items from the ground",       handler: commandTake },
];

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/** Register all built-in commands. Must be called once at startup. */
export function initCommands(): void {
    commandList = [...builtinCommands].sort((a, b) =>
        a.name.localeCompare(b.name));
    catSorted = null;
    LOG(LogLevel.Debug, "p_cmd::initCommands",
        `Registered ${commandList.length} client commands.`);
}

/** Look up a command by exact name. */
export function findCommand(cmd: string): ConsoleCommand | null {
    const lower = cmd.toLowerCase();
    for (const c of commandList) {
        if (c.name === lower) {
            return c;
        }
    }
    return null;
}

/**
 * Return commands sorted by category then name.
 * The result is cached until {@link initCommands} is called again.
 */
export function getCatSortedCommands(): ConsoleCommand[] {
    if (catSorted) {
        return catSorted;
    }
    catSorted = [...commandList].sort((a, b) => {
        if (a.category !== b.category) {
            return a.category - b.category;
        }
        return a.name.localeCompare(b.name);
    });
    return catSorted;
}

/**
 * Gather all tab-completion candidates that start with `partial`.
 * Searches built-in commands, server commands, and any `extraCandidates`
 * (e.g. command history entries).  Duplicates are omitted.
 */
export function getCompletionMatches(partial: string, extraCandidates: string[] = []): string[] {
    if (partial.length === 0) {
        return [];
    }
    const lower = partial.toLowerCase();
    const seen = new Set<string>();
    const matches: string[] = [];

    for (const c of commandList) {
        if (c.name.startsWith(lower) && !seen.has(c.name)) {
            seen.add(c.name);
            matches.push(c.name);
        }
    }
    for (const s of serverCommands) {
        if (s.startsWith(lower) && !seen.has(s)) {
            seen.add(s);
            matches.push(s);
        }
    }
    for (const e of extraCandidates) {
        const eLower = e.toLowerCase();
        if (eLower.startsWith(lower) && !seen.has(eLower)) {
            seen.add(eLower);
            matches.push(e);
        }
    }
    return matches;
}

/**
 * Tab-complete a partial command string.
 * Returns the longest common prefix among all matches (built-in commands,
 * server commands, and optional `extraCandidates` such as history entries),
 * or the original string if nothing matches.
 */
export function completeCommand(partial: string, extraCandidates: string[] = []): string {
    if (partial.length === 0) {
        return partial;
    }

    const matches = getCompletionMatches(partial, extraCandidates);

    if (matches.length === 0) {
        return partial;
    }
    if (matches.length === 1) {
        return matches[0]!;
    }

    // Find longest common prefix (case-insensitive comparison, preserve case
    // of the first match).
    let prefix = matches[0]!;
    for (let i = 1; i < matches.length; i++) {
        const m = matches[i]!;
        let j = 0;
        while (j < prefix.length && j < m.length &&
               prefix[j]!.toLowerCase() === m[j]!.toLowerCase()) {
            j++;
        }
        prefix = prefix.slice(0, j);
    }
    return prefix.length > 0 ? prefix : partial;
}

/**
 * Try to handle a command locally.
 * @returns `true` if the command was handled, `false` if it should be
 *          forwarded to the server.
 */
export function handleLocalCommand(cp: string, cpnext: string): boolean {
    const cmd = findCommand(cp);
    if (!cmd) {
        return false;
    }
    cmd.handler(cpnext);
    return true;
}

/**
 * Parse and execute a command string.
 * An optional leading "/" is stripped before matching — extended command
 * names do not include the slash.  If the command matches a local handler
 * it is executed directly; otherwise it is sent to the server.
 */
export function extendedCommand(command: string): void {
    let trimmed = command.trim();
    if (trimmed.length === 0) {
        return;
    }

    // Strip optional leading slash — commands are registered without it.
    if (trimmed.startsWith("/")) {
        trimmed = trimmed.slice(1);
    }

    // Split into command name and arguments.
    const spaceIdx = trimmed.indexOf(" ");
    let cp: string;
    let cpnext: string;
    if (spaceIdx === -1) {
        cp = trimmed;
        cpnext = "";
    } else {
        cp = trimmed.slice(0, spaceIdx);
        cpnext = trimmed.slice(spaceIdx + 1);
    }

    if (!handleLocalCommand(cp, cpnext)) {
        // Not a local command – forward to the server as-is.
        sendCommand(trimmed, 0, 1);
    }
}
