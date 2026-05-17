/**
 * Client-side slash-command handling for the Crossfire web client.
 * Port of old/common/p_cmd.c (excluding script-related commands).
 */

import { CommCat, LogLevel, type ConsoleCommand } from "./protocol";
import { NDI_COLOR_DEFINITIONS, NDI_COLORS } from "./markup";
import { LOG } from "./misc";
import { sendCommand, setLastCommand } from "./player";
import { resetBindings } from "./keys";
import { getCpl } from "./init";
import { gameEvents } from "./events";
import {
  perfLogging,
  setPerfLogging,
  getWatchedCell,
  clearWatchedCell,
  setWatchedCell,
} from "./debug";
import {
  mapdata_debug_tile,
  mapdata_debug_bigface,
  mapdata_debug_all_bigfaces,
  mapdata_debug_player_pos,
  mapdata_memory_stats,
} from "./mapdata";
import { fogCacheStats } from "./map_fog_cache";
import { image_debug_face } from "./image";

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
  "apply",
  "cast",
  "drop",
  "east",
  "examine",
  "get",
  "invoke",
  "killpets",
  "listen",
  "maps",
  "mark",
  "motd",
  "north",
  "northeast",
  "northwest",
  "output-count",
  "output-sync",
  "party",
  "peaceful",
  "pickup",
  "pray",
  "quit",
  "rename",
  "reply",
  "rotateshoottype",
  "say",
  "shout",
  "skills",
  "south",
  "southeast",
  "southwest",
  "stay",
  "tell",
  "title",
  "use_skill",
  "version",
  "west",
  "who",
  "wimpy",
];

// ---------------------------------------------------------------------------
// Category display names
// ---------------------------------------------------------------------------

function getCategoryName(cat: CommCat): string {
  switch (cat) {
    case CommCat.Misc:
      return "Miscellaneous";
    case CommCat.Info:
      return "Informational";
    case CommCat.Setup:
      return "Configuration";
    case CommCat.Debug:
      return "Debugging";
    default:
      return "Other";
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
      LOG(LogLevel.Info, "p_cmd", line);
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
    sendCommand("help", 0, 1);
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
        "  Sets <command> as the pending command and opens the key-bind dialog.",
    );
    return;
  }
  setLastCommand(args.trim());
  pcmdCallbacks?.openKeyBind();
}

function commandGamepadBind(args: string): void {
  if (!args || args.trim().length === 0) {
    drawInfo(
      "Usage: gamepad_bind <command>\n" +
        "  Sets <command> as the pending command and opens the gamepad button-bind dialog.",
    );
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

function commandTake(args: string): void {
  const what = args.length > 0 ? args : "";
  sendCommand(`take ${what}`.trim(), 0, 1);
}

function commandClear(_args: string): void {
  gameEvents.emit("clearMessages");
}

// ---------------------------------------------------------------------------
// Debug command
// ---------------------------------------------------------------------------

/** Unsubscribe function for the current debug-tile-click listener. */
let debugClickUnsub: (() => void) | null = null;

/** Start a debug-pick flow: enter pick mode and log the result when clicked. */
function debugPickAndLog(
  mode: "bigface" | "tile",
  prompt: string,
  dumpFn: (ax: number, ay: number) => string[],
): void {
  drawInfo(prompt);
  debugClickUnsub?.();
  debugClickUnsub = gameEvents.on("debugTileClicked", (ax, ay, _mode) => {
    debugClickUnsub?.();
    debugClickUnsub = null;
    for (const line of dumpFn(ax, ay)) {
      drawInfo(line);
    }
  });
  gameEvents.emit("debugPickTile", mode);
}

function commandDebugText(): void {
  drawInfo("Message colors:");
  for (const def of NDI_COLOR_DEFINITIONS) {
    const hex = NDI_COLORS[def.id] ?? "#cccccc";
    const hexCode = hex.slice(1); // strip '#'
    drawInfo(`[color=${hexCode}]${def.name}: ${hex}[/color]`);
  }
  let s = "Message fonts:";

  for (const name of ["fixed", "arcane", "hand", "strange"]) {
    s += `[${name}]${name}[print] `;
  }
  drawInfo(s);
}

function commandDebug(args: string): void {
  const trimmed = args.trim();
  const spaceIdx = trimmed.indexOf(" ");
  const sub = (
    spaceIdx === -1 ? trimmed : trimmed.slice(0, spaceIdx)
  ).toLowerCase();
  const subArgs = spaceIdx === -1 ? "" : trimmed.slice(spaceIdx + 1).trim();

  if (sub === "perf") {
    const newState = !perfLogging;
    setPerfLogging(newState);
    drawInfo(`Performance logging ${newState ? "enabled" : "disabled"}.`);
  } else if (sub === "bigface") {
    debugPickAndLog(
      "bigface",
      "Click a tile on the map to inspect bigface/multitile data…",
      mapdata_debug_bigface,
    );
  } else if (sub === "tile") {
    debugPickAndLog(
      "tile",
      "Click a tile on the map to inspect tile data…",
      mapdata_debug_tile,
    );
  } else if (sub === "bigfaces") {
    for (const line of mapdata_debug_all_bigfaces()) {
      drawInfo(line);
    }
    drawInfo("All active bigface data displayed above.");
  } else if (sub === "watch") {
    const current = getWatchedCell();
    if (current !== null) {
      const { ax, ay } = current;
      clearWatchedCell();
      drawInfo(`Stopped watching cell at absolute (${ax}, ${ay}).`);
    } else {
      drawInfo("Click a tile on the map to start watching its server updates…");
      debugClickUnsub?.();
      debugClickUnsub = gameEvents.on("debugTileClicked", (ax, ay, _mode) => {
        debugClickUnsub?.();
        debugClickUnsub = null;
        setWatchedCell({ ax, ay }, (event) =>
          drawInfo(`[debug watch] (${ax},${ay}) ${event}`),
        );
        drawInfo(
          `Now watching cell at absolute (${ax}, ${ay}). Run debug watch again to stop.`,
        );
      });
      gameEvents.emit("debugPickTile", "tile");
    }
  } else if (sub === "face") {
    const faceNum = parseInt(subArgs, 10);
    if (subArgs === "" || isNaN(faceNum) || faceNum < 0) {
      drawInfo("Usage: debug face <face-number>\nExample: debug face 1234");
    } else {
      for (const line of image_debug_face(faceNum)) {
        drawInfo(line);
      }
      drawInfo(`Displayed face ${faceNum} data.`);
    }
  } else if (sub === "pos") {
    for (const line of mapdata_debug_player_pos()) {
      drawInfo(line);
    }
    drawInfo("Displayed player virtual-map position.");
  } else if (sub === "mem") {
    const ms = mapdata_memory_stats();
    const fc = fogCacheStats();
    const mapMiB = (ms.typedArrayBytes / 1048576).toFixed(2);
    const heapMiB =
      ms.heapBytes !== null
        ? `${(ms.heapBytes / 1048576).toFixed(1)} MiB`
        : "n/a (Chrome with --enable-precise-memory-info only)";
    drawInfo(
      "Map memory statistics:\n" +
        `  Virtual map: ${ms.mapDimensions.width}×${ms.mapDimensions.height}` +
        ` (${ms.totalCells.toLocaleString()} cells)\n` +
        `  View: ${ms.viewDimensions.width}×${ms.viewDimensions.height}\n` +
        `  Non-empty cells: ${ms.nonEmptyCells.toLocaleString()}\n` +
        `  Labeled cells:   ${ms.labeledCells}\n` +
        `  Typed-array bytes: ${ms.typedArrayBytes.toLocaleString()} B (${mapMiB} MiB)\n` +
        `  Fog cache: ${fc.entries}/${fc.maxEntries} entries, ${fc.totalCells.toLocaleString()} cells\n` +
        `  JS heap: ${heapMiB}`,
    );
  } else if (sub === "text") {
    commandDebugText();
  } else {
    // Keep the subcommands sorted alphabetically
    drawInfo(
      "Usage: debug <subcommand>\n" +
        "Subcommands:\n" +
        "  bigface      Click a tile to display bigface/multitile info\n" +
        "  bigfaces     Display all currently active bigface entries\n" +
        "  face <num>   Display all known data for face <num>, including pixel size\n" +
        "  mem          Show map and fog-cache memory usage statistics\n" +
        "  perf         Toggle performance logging on/off\n" +
        "  pos          Display the player's current position in the virtual map\n" +
        "  text         Show all message colors and fonts\n" +
        "  tile         Click a tile to display all tile info\n" +
        "  watch        Pick a tile to watch; displays all server updates to it (run again to stop)",
    );
  }
}

// ---------------------------------------------------------------------------
// Command table
// ---------------------------------------------------------------------------

// Keep these commands sorted alphabetically by name
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
    name: "clear",
    category: CommCat.Misc,
    description: "Clear the message panel",
    handler: commandClear,
  },
  {
    name: "debug",
    category: CommCat.Debug,
    description: "Debugging tools",
    longDescription:
      "Syntax:\n" +
      "  debug bigface      Click a tile to display bigface/multitile info in the info panel\n" +
      "  debug bigfaces     Display all currently active bigface entries in the info panel\n" +
      "  debug colors       Show all message colors with their names in the info panel\n" +
      "  debug face <num>   Display all known data for face <num>, including pixel size,\n" +
      "                     image URL, smooth face mapping, and any pending name/checksum\n" +
      "  debug mem          Show virtual-map and fog-cache memory usage statistics\n" +
      "  debug perf         Toggle periodic performance logging on/off\n" +
      "  debug pos          Display the player's current position in the virtual map\n" +
      "  debug tile         Click a tile to display all tile data in the info panel\n" +
      "  debug watch        Click a tile to start watching; every server update to that\n" +
      "                     cell is displayed in the info panel.  Run again to stop watching.\n" +
      "\n" +
      "Performance logging is off by default.  The bigface, tile and watch\n" +
      "subcommands prompt you to click on the game map; the data is\n" +
      "displayed in the info panel.",
    handler: commandDebug,
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
  {
    name: "help",
    category: CommCat.Misc,
    description: "Show help on commands",
    handler: commandHelp,
  },
  {
    name: "magicmap",
    category: CommCat.Misc,
    description: "Show last received magic map",
    handler: commandMagicmap,
  },
  {
    name: "resetkeys",
    category: CommCat.Setup,
    description: "Reset all key bindings to default",
    handler: commandResetKeys,
  },
  {
    name: "take",
    category: CommCat.Misc,
    description: "Take items from the ground",
    handler: commandTake,
  },
];

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/** Register all built-in commands. Must be called once at startup. */
export function initCommands(): void {
  commandList = [...builtinCommands].sort((a, b) =>
    a.name.localeCompare(b.name),
  );
  catSorted = null;
  LOG(
    LogLevel.Debug,
    "p_cmd::initCommands",
    `Registered ${commandList.length} client commands.`,
  );
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
export function getCompletionMatches(
  partial: string,
  extraCandidates: string[] = [],
): string[] {
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
export function completeCommand(
  partial: string,
  extraCandidates: string[] = [],
): string {
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
    while (
      j < prefix.length &&
      j < m.length &&
      prefix[j]!.toLowerCase() === m[j]!.toLowerCase()
    ) {
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
