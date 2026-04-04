/**
 * Client-side slash-command handling for the Crossfire web client.
 * Port of old/common/p_cmd.c (excluding script-related commands).
 */

import { CommCat, LogLevel, type ConsoleCommand } from "./protocol";
import { LOG } from "./misc";
import { saveConfig } from "./storage";
import { sendCommand } from "./player";
import { bindKey, unbindKey, resetBindings } from "./keys";

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
        LOG(LogLevel.Info, "p_cmd::help", lines.join("\n"));
        return;
    }

    const target = findCommand(args.trim());
    if (target) {
        LOG(LogLevel.Info, "p_cmd::help",
            `${target.name}: ${target.description ?? "(no description)"}`);
        if (target.longDescription) {
            LOG(LogLevel.Info, "p_cmd::help", target.longDescription);
        }
    } else {
        LOG(LogLevel.Info, "p_cmd::help",
            `Unknown command '${args.trim()}'. Type 'help' for a list.`);
    }
}

function commandBind(args: string): void {
    bindKey(args);
}

function commandUnbind(args: string): void {
    unbindKey(args);
}

function commandResetKeys(_args: string): void {
    resetBindings();
}

function commandMagicmap(_args: string): void {
    LOG(LogLevel.Info, "p_cmd::magicmap", "Requesting magic map from server.");
    sendCommand("magicmap", 0, 1);
}

function commandInv(_args: string): void {
    LOG(LogLevel.Info, "p_cmd::inv", "Requesting inventory display.");
    sendCommand("inv", 0, 1);
}

function commandTake(args: string): void {
    const what = args.length > 0 ? args : "";
    sendCommand(`take ${what}`.trim(), 0, 1);
}

function commandSaveDefaults(_args: string): void {
    saveConfig("defaults_saved", true);
    LOG(LogLevel.Info, "p_cmd::savedefaults", "Defaults saved.");
}

// ---------------------------------------------------------------------------
// Command table
// ---------------------------------------------------------------------------

const builtinCommands: ConsoleCommand[] = [
    {
        name: "bind",
        category: CommCat.Setup,
        description: "Bind a key to a command",
        longDescription:
            "Syntax:\n" +
            "  bind [options] <command>\n" +
            "\n" +
            "Options:\n" +
            "  -e   enter edit mode when key is pressed (pre-fills command input)\n" +
            "  -i   ignore modifiers — binding works regardless of Shift/Ctrl/Alt\n" +
            "  -n   bind for normal mode (no modifier keys)\n" +
            "  -f   bind for fire mode (Shift held)\n" +
            "  -r   bind for run mode (Alt held)\n" +
            "  -a   bind for alt mode (Ctrl held)\n" +
            "  -m   bind for meta mode (Meta/Win key held)\n" +
            "  -g   global scope (ignored in web client)\n" +
            "\n" +
            "Without -n/-f/-r/-a/-m, modifiers are read from the keyboard when\n" +
            "you press the key to bind.  With those flags the modifier combination\n" +
            "is set explicitly, so you do not need to hold any keys.\n" +
            "\n" +
            "Examples:\n" +
            "  bind -f cast fireball\n" +
            "    Then press F3 → binds Shift+F3 to 'cast fireball' (fire mode)\n" +
            "  bind -e shout \n" +
            "    Then press \" → pre-fills 'shout' in the command input",
        handler: commandBind,
    },
    { name: "help",         category: CommCat.Misc,  description: "Show help on commands",         handler: commandHelp },
    { name: "inv",          category: CommCat.Debug, description: "Show inventory",                handler: commandInv },
    { name: "inventory",    category: CommCat.Debug, description: "Show inventory (alias)",        handler: commandInv },
    { name: "magicmap",     category: CommCat.Misc,  description: "Request the magic map overlay", handler: commandMagicmap },
    { name: "resetkeys",    category: CommCat.Setup, description: "Reset key bindings to defaults",handler: commandResetKeys },
    { name: "savedefaults", category: CommCat.Setup, description: "Save current configuration",   handler: commandSaveDefaults },
    { name: "take",         category: CommCat.Misc,  description: "Take items from the ground",   handler: commandTake },
    {
        name: "unbind",
        category: CommCat.Setup,
        description: "Remove a key binding",
        longDescription:
            "Syntax:\n" +
            "  unbind              — list all current bindings with indices\n" +
            "  unbind <number>     — remove the binding at the given index\n" +
            "  unbind <key>        — remove all bindings for the named key",
        handler: commandUnbind,
    },
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
 * Tab-complete a partial command name.
 * Returns the longest common prefix among all matches (client + server),
 * or the original string if nothing matches.
 */
export function completeCommand(partial: string): string {
    if (partial.length === 0) {
        return partial;
    }
    const lower = partial.toLowerCase();

    // Gather all matching names from client commands and server commands.
    const matches: string[] = [];
    for (const c of commandList) {
        if (c.name.startsWith(lower)) {
            matches.push(c.name);
        }
    }
    for (const s of serverCommands) {
        if (s.startsWith(lower)) {
            matches.push(s);
        }
    }

    if (matches.length === 0) {
        return partial;
    }
    if (matches.length === 1) {
        return matches[0];
    }

    // Find longest common prefix.
    let prefix = matches[0];
    for (let i = 1; i < matches.length; i++) {
        while (!matches[i].startsWith(prefix)) {
            prefix = prefix.slice(0, -1);
        }
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
 * Leading "/" is stripped if present.  If the command matches a local
 * handler it is executed directly; otherwise it is sent to the server.
 */
export function extendedCommand(command: string): void {
    let trimmed = command.trim();
    if (trimmed.length === 0) {
        return;
    }

    // Strip leading slash.
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
