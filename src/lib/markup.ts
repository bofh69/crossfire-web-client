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

export const NDI_COLORS: Record<number, string> = {
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

export function colorForNdi(ndi: number): string {
  return NDI_COLORS[ndi] ?? "#cccccc";
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
