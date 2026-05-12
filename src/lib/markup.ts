/**
 * Markup parsing shared between InfoPanel and the Login page.
 *
 * The Crossfire server can embed simple formatting tags in messages:
 *   [b]/[/b]           bold
 *   [i]/[/i]           italic
 *   [ul]/[/ul]         underline
 *   [color=rrggbb]/[/color]  hex colour
 * This mirrors the C client's add_marked_text_to_pane() logic.
 */

import {
  NDI_BLACK,
  NDI_WHITE,
  NDI_NAVY,
  NDI_RED,
  NDI_ORANGE,
  NDI_BLUE,
  NDI_DK_ORANGE,
  NDI_GREEN,
  NDI_LT_GREEN,
  NDI_GREY,
  NDI_BROWN,
  NDI_GOLD,
  NDI_TAN,
} from "./protocol";
import { gameEvents } from "./events";
import { loadConfig, saveConfig } from "./storage";

export interface MessageSpan {
  text: string;
  color: string;
  bold: boolean;
  italic: boolean;
  underline: boolean;
}

/** A single parsed line from a server info block (motd/news/rules). */
export interface InfoLine {
  /** True when the original line started with '%', marking it as a section title. */
  isTitle: boolean;
  spans: MessageSpan[];
}

const DEFAULT_NDI_COLORS: Record<number, string> = {
  [NDI_BLACK]: "#cccccc", // Black on dark bg → light gray
  [NDI_WHITE]: "#ffffff",
  [NDI_NAVY]: "#8080ee",
  [NDI_RED]: "#ff4444",
  [NDI_ORANGE]: "#ff8800",
  [NDI_BLUE]: "#66aaff",
  [NDI_DK_ORANGE]: "#cc6600",
  [NDI_GREEN]: "#44cc44",
  [NDI_LT_GREEN]: "#88ff88",
  [NDI_GREY]: "#999999",
  [NDI_BROWN]: "#aa7744",
  [NDI_GOLD]: "#ffcc00",
  [NDI_TAN]: "#ccaa88",
};
const DEFAULT_NDI_FALLBACK = "#cccccc";

export const NDI_COLORS: Record<number, string> = { ...DEFAULT_NDI_COLORS };
export const INFO_PANEL_BACKGROUND_DEFAULT = "#1a1a1a";

export interface NdiColorDefinition {
  id: number;
  name: string;
  defaultColor: string;
}

export const NDI_COLOR_DEFINITIONS: NdiColorDefinition[] = [
  {
    id: NDI_BLACK,
    name: "NDI_BLACK",
    defaultColor: DEFAULT_NDI_COLORS[NDI_BLACK]!,
  },
  {
    id: NDI_WHITE,
    name: "NDI_WHITE",
    defaultColor: DEFAULT_NDI_COLORS[NDI_WHITE]!,
  },
  {
    id: NDI_NAVY,
    name: "NDI_NAVY",
    defaultColor: DEFAULT_NDI_COLORS[NDI_NAVY]!,
  },
  { id: NDI_RED, name: "NDI_RED", defaultColor: DEFAULT_NDI_COLORS[NDI_RED]! },
  {
    id: NDI_ORANGE,
    name: "NDI_ORANGE",
    defaultColor: DEFAULT_NDI_COLORS[NDI_ORANGE]!,
  },
  {
    id: NDI_BLUE,
    name: "NDI_BLUE",
    defaultColor: DEFAULT_NDI_COLORS[NDI_BLUE]!,
  },
  {
    id: NDI_DK_ORANGE,
    name: "NDI_DK_ORANGE",
    defaultColor: DEFAULT_NDI_COLORS[NDI_DK_ORANGE]!,
  },
  {
    id: NDI_GREEN,
    name: "NDI_GREEN",
    defaultColor: DEFAULT_NDI_COLORS[NDI_GREEN]!,
  },
  {
    id: NDI_LT_GREEN,
    name: "NDI_LT_GREEN",
    defaultColor: DEFAULT_NDI_COLORS[NDI_LT_GREEN]!,
  },
  {
    id: NDI_GREY,
    name: "NDI_GREY",
    defaultColor: DEFAULT_NDI_COLORS[NDI_GREY]!,
  },
  {
    id: NDI_BROWN,
    name: "NDI_BROWN",
    defaultColor: DEFAULT_NDI_COLORS[NDI_BROWN]!,
  },
  {
    id: NDI_GOLD,
    name: "NDI_GOLD",
    defaultColor: DEFAULT_NDI_COLORS[NDI_GOLD]!,
  },
  {
    id: NDI_TAN,
    name: "NDI_TAN",
    defaultColor: DEFAULT_NDI_COLORS[NDI_TAN]!,
  },
];

interface StoredInfoPanelColors {
  background: string;
  ndiColors: Record<string, string>;
}

const INFO_PANEL_COLOR_STORAGE_KEY = "info_panel_colors";
let configuredNdiColors: Record<number, string> = { ...DEFAULT_NDI_COLORS };
let configuredInfoPanelBackgroundColor = INFO_PANEL_BACKGROUND_DEFAULT;

function normalizeHexColor(value: string, fallback: string): string {
  const trimmed = value.trim();
  if (/^#([0-9a-fA-F]{6})$/.test(trimmed)) {
    return trimmed.toLowerCase();
  }
  if (/^[0-9a-fA-F]{6}$/.test(trimmed)) {
    return `#${trimmed.toLowerCase()}`;
  }
  return fallback;
}

function applyInfoPanelColorCssVariables(): void {
  if (typeof document === "undefined") return;
  const root = document.documentElement;
  for (const def of NDI_COLOR_DEFINITIONS) {
    root.style.setProperty(
      `--ndi-color-${def.id}`,
      configuredNdiColors[def.id]!,
    );
  }
  root.style.setProperty("--info-panel-bg", configuredInfoPanelBackgroundColor);
}

function persistInfoPanelColors(): void {
  const ndiColors: Record<string, string> = {};
  for (const def of NDI_COLOR_DEFINITIONS) {
    ndiColors[String(def.id)] = configuredNdiColors[def.id]!;
  }
  saveConfig<StoredInfoPanelColors>(INFO_PANEL_COLOR_STORAGE_KEY, {
    background: configuredInfoPanelBackgroundColor,
    ndiColors,
  });
}

function emitInfoPanelColorsChanged(): void {
  gameEvents.emit("infoPanelColorsChanged");
}

function loadInfoPanelColorsFromStorage(): void {
  if (typeof localStorage === "undefined") {
    applyInfoPanelColorCssVariables();
    return;
  }
  const stored = loadConfig<StoredInfoPanelColors | null>(
    INFO_PANEL_COLOR_STORAGE_KEY,
    null,
  );
  if (stored !== null && typeof stored === "object") {
    configuredInfoPanelBackgroundColor = normalizeHexColor(
      stored.background ?? INFO_PANEL_BACKGROUND_DEFAULT,
      INFO_PANEL_BACKGROUND_DEFAULT,
    );
    const nextColors: Record<number, string> = { ...DEFAULT_NDI_COLORS };
    for (const def of NDI_COLOR_DEFINITIONS) {
      nextColors[def.id] = normalizeHexColor(
        stored.ndiColors?.[String(def.id)] ?? def.defaultColor,
        def.defaultColor,
      );
    }
    configuredNdiColors = nextColors;
  }
  applyInfoPanelColorCssVariables();
}

loadInfoPanelColorsFromStorage();

/**
 * Returns a CSS variable reference for the configured NDI color, with a hex
 * fallback.
 */
export function colorForNdi(ndi: number): string {
  const fallback = configuredNdiColors[ndi] ?? DEFAULT_NDI_FALLBACK;
  return `var(--ndi-color-${ndi}, ${fallback})`;
}

export function getDefaultNdiColor(ndi: number): string {
  return DEFAULT_NDI_COLORS[ndi] ?? DEFAULT_NDI_FALLBACK;
}

export function getInfoPanelBackgroundColor(): string {
  return configuredInfoPanelBackgroundColor;
}

export function getConfiguredNdiColor(ndi: number): string {
  return configuredNdiColors[ndi] ?? getDefaultNdiColor(ndi);
}

export function getConfiguredNdiColors(): Record<number, string> {
  return { ...configuredNdiColors };
}

export function setInfoPanelColors(
  background: string,
  ndiColors: Record<number, string>,
): void {
  const nextColors: Record<number, string> = { ...DEFAULT_NDI_COLORS };
  for (const def of NDI_COLOR_DEFINITIONS) {
    nextColors[def.id] = normalizeHexColor(
      ndiColors[def.id] ?? configuredNdiColors[def.id] ?? def.defaultColor,
      def.defaultColor,
    );
  }
  configuredInfoPanelBackgroundColor = normalizeHexColor(
    background,
    INFO_PANEL_BACKGROUND_DEFAULT,
  );
  configuredNdiColors = nextColors;
  applyInfoPanelColorCssVariables();
  persistInfoPanelColors();
  emitInfoPanelColorsChanged();
}

export function resetInfoPanelColorsToDefaults(): void {
  configuredNdiColors = { ...DEFAULT_NDI_COLORS };
  configuredInfoPanelBackgroundColor = INFO_PANEL_BACKGROUND_DEFAULT;
  applyInfoPanelColorCssVariables();
  persistInfoPanelColors();
  emitInfoPanelColorsChanged();
}

/**
 * Parse a server message that may contain markup tags.
 * Returns an array of styled spans.
 */
export function parseMarkup(text: string, baseColor: string): MessageSpan[] {
  const spans: MessageSpan[] = [];
  let bold = false;
  let italic = false;
  let underline = false;
  let color = baseColor;
  let current = text;

  while (true) {
    const openBracket = current.indexOf("[");
    if (openBracket < 0) break;

    // Emit text before the bracket
    if (openBracket > 0) {
      spans.push({
        text: current.substring(0, openBracket),
        color,
        bold,
        italic,
        underline,
      });
    }
    current = current.substring(openBracket + 1);

    const closeBracket = current.indexOf("]");
    if (closeBracket < 0) break; // malformed — stop

    const tag = current.substring(0, closeBracket);
    current = current.substring(closeBracket + 1);

    if (tag === "b") {
      bold = true;
    } else if (tag === "/b") {
      bold = false;
    } else if (tag === "i") {
      italic = true;
    } else if (tag === "/i") {
      italic = false;
    } else if (tag === "ul") {
      underline = true;
    } else if (tag === "/ul") {
      underline = false;
    } else if (tag === "/color") {
      color = baseColor;
    } else if (tag.startsWith("color=")) {
      color = "#" + tag.substring(6);
    }
    // Ignore other tags (fixed, arcane, hand, strange, print, etc.)
  }

  // Emit any remaining text
  if (current.length > 0) {
    spans.push({ text: current, color, bold, italic, underline });
  }

  return spans;
}

/**
 * Strip all `[tag]` markup from a server message, returning plain text.
 */
export function stripMarkupTags(text: string): string {
  return text.replace(/\[[^\]]*\]/g, "");
}

/** One entry in the high-score list. */
export interface HiscoreRow {
  rank: string;
  score: string;
  who: string;
  maxHp: string;
  maxSp: string;
  maxGrace: string;
}

/**
 * Matches a single hiscore data row sent by the server:
 *   [fixed]  <rank>  <score>[print] <who text> <maxhp><maxsp><maxgrace>
 *
 * Groups: 1=rank, 2=score, 3=who (trimmed), 4=maxHp, 5=maxSp, 6=maxGrace
 */
const HISCORE_ROW_RE =
  /^\[fixed\]\s*(\d+)\s+(\d+)\[print\]\s*([^<]*?)\s*<(\d+)><(\d+)><(\d+)>\.$/;

/**
 * Parse the raw hiscore text sent by the server into structured rows.
 *
 * The server sends:
 *   Line 0: "Overall high scores:"  (title — skipped)
 *   Line 1: header (skipped)
 *   Lines 2+: data rows matching HISCORE_ROW_RE
 */
export function parseHiscoreRows(text: string): HiscoreRow[] {
  const lines = text.split("\n");
  const rows: HiscoreRow[] = [];

  // Skip line 0 (title) and line 1 (header).
  for (let i = 2; i < lines.length; i++) {
    const line = lines[i];
    if (!line?.trim()) continue;

    const m = HISCORE_ROW_RE.exec(line);
    if (!m) {
      console.warn("Unrecognized hiscore line:", line);
      continue;
    }

    rows.push({
      rank: m[1]!,
      score: m[2]!,
      who: m[3]!,
      maxHp: m[4]!,
      maxSp: m[5]!,
      maxGrace: m[6]!,
    });
  }

  return rows;
}

/**
 * Split a multi-line text block and parse each line with parseMarkup.
 * Lines that begin with '%' are treated as section titles: the '%' is
 * stripped and the line is returned with isTitle set to true.
 * Trailing empty lines are dropped.
 */
export function parseMarkupLines(text: string, baseColor: string): InfoLine[] {
  const rawLines = text.split("\n");
  // Drop trailing blank lines
  while (rawLines.length > 0 && rawLines[rawLines.length - 1]!.trim() === "") {
    rawLines.pop();
  }
  return rawLines.map((line) => {
    if (line.startsWith("%")) {
      return {
        isTitle: true,
        spans: parseMarkup(line.substring(1), baseColor),
      };
    }
    return { isTitle: false, spans: parseMarkup(line, baseColor) };
  });
}
