/**
 * Gamepad button identifiers.
 *
 * Bindings refer to buttons by a {@link ButtonId} rather than by raw index.
 * When the browser reports `Gamepad.mapping === "standard"`, buttons 0–16
 * have fixed positions (W3C Standard Gamepad layout) and are named with the
 * SDL_GameControllerDB names ("a", "leftshoulder", "dpup", …).  Any other
 * button, and every button of a non-standard gamepad, is named "b<index>",
 * which is also the SDL_GameControllerDB syntax for a raw button.
 */

/** A button identifier: a standard button name or "b<index>". */
export type ButtonId = string;

/**
 * Names of the W3C standard-layout buttons, indexed by their button index.
 * The names are the ones used by SDL_GameControllerDB.
 */
export const STANDARD_BUTTON_NAMES: readonly string[] = [
  "a",
  "b",
  "x",
  "y",
  "leftshoulder",
  "rightshoulder",
  "lefttrigger",
  "righttrigger",
  "back",
  "start",
  "leftstick",
  "rightstick",
  "dpup",
  "dpdown",
  "dpleft",
  "dpright",
  "guide",
];

/** Human-readable labels for {@link STANDARD_BUTTON_NAMES}, same order. */
const STANDARD_BUTTON_LABELS: readonly string[] = [
  "A",
  "B",
  "X",
  "Y",
  "LB",
  "RB",
  "LT",
  "RT",
  "Back",
  "Start",
  "L3",
  "R3",
  "D-pad up",
  "D-pad down",
  "D-pad left",
  "D-pad right",
  "Home",
];

const RAW_BUTTON_RE = /^b(\d+)$/;

/** Whether the browser has remapped `gp` to the W3C standard layout. */
export function isStandardGamepad(gp: Gamepad): boolean {
  return gp.mapping === "standard";
}

/**
 * The button index that `id` refers to, or -1 if `id` is unknown.
 * Standard names resolve to their standard-layout index on any gamepad.
 */
export function buttonIndex(id: ButtonId): number {
  const raw = RAW_BUTTON_RE.exec(id);
  if (raw) return Number(raw[1]);
  return STANDARD_BUTTON_NAMES.indexOf(id);
}

/**
 * The identifier for button `index`: its standard name when `standard` is
 * true and it has one, otherwise "b<index>".
 */
export function buttonIdForIndex(index: number, standard: boolean): ButtonId {
  return (standard && STANDARD_BUTTON_NAMES[index]) || `b${index}`;
}

/**
 * Convert a stored button reference to the canonical identifier for a
 * gamepad with (`standard` = true) or without the standard mapping, so that
 * every physical button has exactly one identifier.  Numbers are raw indices
 * from bindings saved by older versions.  Returns null if it is invalid.
 */
export function normaliseButtonId(
  button: ButtonId | number,
  standard: boolean,
): ButtonId | null {
  const index = typeof button === "number" ? button : buttonIndex(button);
  if (!Number.isInteger(index) || index < 0) return null;
  return buttonIdForIndex(index, standard);
}

/** A short human-readable label for a button, e.g. "A", "LB" or "B17". */
export function buttonLabel(id: ButtonId): string {
  const index = STANDARD_BUTTON_NAMES.indexOf(id);
  if (index >= 0) return STANDARD_BUTTON_LABELS[index]!;
  return id.toUpperCase();
}
