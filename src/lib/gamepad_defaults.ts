/**
 * Default gamepad profiles for the Crossfire web client.
 *
 * Gamepads that the browser maps to the W3C standard layout all use the
 * standard profile.  Other gamepads use the first profile whose `matchId`
 * is found in their `Gamepad.id` string (case-insensitive), or the fallback
 * profile.  Profiles define which axes map to the walk/run stick and fire
 * stick, and what command each button sends.
 */

import type { ButtonId } from "./gamepad_buttons";
import { LOG } from "./misc";
import { LogLevel } from "./protocol";
import {
  DEFAULT_WALK_THRESHOLD,
  DEFAULT_RUN_THRESHOLD,
  DEFAULT_FIRE_THRESHOLD,
} from "./constants";

// ──────────────────────────────────────────────────────────────────────────────
// Types
// ──────────────────────────────────────────────────────────────────────────────

/** A pair of axis indices (horizontal, vertical) with optional inversion flags. */
export interface StickAxes {
  /** Axis index for left/right. */
  axisX: number;
  /** Axis index for up/down. */
  axisY: number;
  /** If true, negate the X axis value before use. */
  invertX: boolean;
  /** If true, negate the Y axis value before use. */
  invertY: boolean;
}

/** Button-to-command mapping. */
export interface ButtonMapping {
  /** The button, see {@link ButtonId}. */
  button: ButtonId;
  command: string;
  /** true = applies to all characters; false (default) = current character only. */
  global?: boolean;
}

/** A complete gamepad profile. */
export interface GamepadProfile {
  /** Human-readable profile name. */
  name: string;
  /**
   * Substring matched (case-insensitive) against `Gamepad.id` to
   * determine if this profile should be used.
   */
  matchId: string[];
  /** Axes used for the walk/run stick. */
  walkStick: StickAxes;
  /** Axes used for the fire stick. */
  fireStick: StickAxes;
  /** Dead-zone threshold for walk movement (0–1). */
  walkThreshold: number;
  /** Threshold beyond which walk becomes run (0–1). */
  runThreshold: number;
  /** Threshold for fire-stick direction (0–1). */
  fireThreshold: number;
  /** Button → command mappings. */
  buttons: ButtonMapping[];
}

// ──────────────────────────────────────────────────────────────────────────────
// Built-in profiles
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Default button bindings for gamepads with a standard-like layout, in
 * button order.  Search and disarm are on the left and right face buttons
 * since they are used together, and praying and meditation on the left
 * bumper and trigger.  Home is left unbound: the system or Steam often
 * takes it.
 */
const standardButtons: readonly ButtonMapping[] = [
  { button: "a", command: "apply", global: true },
  { button: "b", command: "disarm", global: true },
  { button: "x", command: "search", global: true },
  { button: "y", command: "hotbar", global: true },
  { button: "leftshoulder", command: "use_skill pray", global: true },
  { button: "rightshoulder", command: "brace", global: true },
  { button: "lefttrigger", command: "use_skill meditation", global: true },
  { button: "righttrigger", command: "reply", global: true },
  { button: "back", command: "help", global: true },
  { button: "start", command: "ui_nav", global: true },
  { button: "leftstick", command: "examine", global: true },
  { button: "rightstick", command: "stay fire", global: true },
  { button: "dpup", command: "ready_skill throwing", global: true },
  { button: "dpdown", command: "take", global: true },
  { button: "dpleft", command: "rotateshoottype -1", global: true },
  { button: "dpright", command: "rotateshoottype", global: true },
];

/** The profile for every gamepad that has the standard mapping. */
export const standardProfile: GamepadProfile = {
  name: "Standard Gamepad",
  matchId: [],
  walkStick: { axisX: 0, axisY: 1, invertX: false, invertY: false },
  fireStick: { axisX: 2, axisY: 3, invertX: false, invertY: false },
  walkThreshold: DEFAULT_WALK_THRESHOLD,
  runThreshold: DEFAULT_RUN_THRESHOLD,
  fireThreshold: DEFAULT_FIRE_THRESHOLD,
  buttons: [...standardButtons],
};

const xboxProfile: GamepadProfile = {
  ...standardProfile,
  name: "Xbox One Controller",
  matchId: [
    "045e-02dd-",
    "Vendor: 045e Product: 02dd",
    "Microsoft Controller",
    "Microsoft X-Box One",
  ],
};

/**
 * Profiles for gamepads without the standard mapping, ordered by
 * specificity (most specific first).  The first profile whose `matchId` is
 * found in the gamepad's `id` string will be used.
 */
export const defaultProfiles: readonly GamepadProfile[] = [xboxProfile];

/**
 * A fallback profile used when no built-in profile matches.
 * Uses the standard axes (0–1 for L stick, 2–3 for R stick).
 */
export const fallbackProfile: GamepadProfile = {
  name: "Generic Controller",
  matchId: [""],
  walkStick: { axisX: 0, axisY: 1, invertX: false, invertY: false },
  fireStick: { axisX: 2, axisY: 3, invertX: false, invertY: false },
  walkThreshold: DEFAULT_WALK_THRESHOLD,
  runThreshold: DEFAULT_RUN_THRESHOLD,
  fireThreshold: DEFAULT_FIRE_THRESHOLD,
  buttons: [
    { button: "a", command: "apply", global: true },
    { button: "b", command: "hotbar", global: true },
    { button: "x", command: "use_skill pray", global: true },
    { button: "y", command: "search", global: true },
    // Button 9 is Start on most DirectInput gamepads too.
    { button: "start", command: "ui_nav", global: true },
  ],
};

/**
 * Find the best matching profile for a gamepad: the standard profile if
 * `standard` is true, otherwise based on its `id` string.
 * Returns a deep copy so the caller can modify it freely.
 */
export function findProfileForGamepad(
  gamepadId: string,
  standard: boolean,
): GamepadProfile {
  if (standard) {
    LOG(
      LogLevel.Debug,
      "gamepad::profile",
      `${gamepadId} has the standard mapping`,
    );
    return structuredClone(standardProfile);
  }
  const lower = gamepadId.toLowerCase();
  for (const profile of defaultProfiles) {
    for (const matchId of profile.matchId) {
      if (lower.includes(matchId.toLowerCase())) {
        LOG(
          LogLevel.Debug,
          "gamepad::profile",
          `${lower} matched with: ${matchId}`,
        );
        return structuredClone(profile);
      }
    }
  }
  LOG(
    LogLevel.Debug,
    "gamepad::profile",
    `${lower} didn't match a default profile`,
  );

  return structuredClone(fallbackProfile);
}
