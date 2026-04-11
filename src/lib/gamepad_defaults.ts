/**
 * Default gamepad profiles for the Crossfire web client.
 *
 * Each profile matches controllers whose `Gamepad.id` string contains
 * a specific substring (case-insensitive).  Profiles define which axes
 * map to the walk/run stick and fire stick, and what command each button
 * sends.
 */

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

/** Button-to-command mapping.  Button index → command string. */
export interface ButtonMapping {
    button: number;
    command: string;
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

const xboxProfile: GamepadProfile = {
    name: "Xbox One Controller",
    matchId: ["045e-02dd-", "Vendor: 045e Product: 02dd", "Microsoft Controller", "Microsoft X-Box One"],
    walkStick: { axisX: 0, axisY: 1, invertX: false, invertY: false },
    fireStick: { axisX: 2, axisY: 3, invertX: false, invertY: false },
    walkThreshold: 0.2,
    runThreshold: 0.7,
    fireThreshold: 0.7,
    buttons: [
        { button: 0,  command: "apply" },
        { button: 1,  command: "disarm" },
        { button: 2,  command: "use_skill pray" },
        { button: 3,  command: "search" },
        { button: 11, command: "stay fire" },
        { button: 12, command: "ready_skill throwing" },
        { button: 13, command: "take" },
        { button: 14, command: "rotateshoottype -1" },
        { button: 15, command: "rotateshoottype" },
        { button: 16, command: "brace" },
    ],
};

/**
 * All built-in profiles, ordered by specificity (most specific first).
 * The first profile whose `matchId` is found in the gamepad's `id`
 * string will be used.
 */
export const defaultProfiles: readonly GamepadProfile[] = [
    xboxProfile,
];

/**
 * A fallback profile used when no built-in profile matches.
 * Uses the standard mapping (axes 0–1 for L stick, 2–3 for R stick).
 */
export const fallbackProfile: GamepadProfile = {
    name: "Generic Controller",
    matchId: [""],
    walkStick: { axisX: 0, axisY: 1, invertX: false, invertY: false },
    fireStick: { axisX: 2, axisY: 3, invertX: false, invertY: false },
    walkThreshold: 0.2,
    runThreshold: 0.7,
    fireThreshold: 0.7,
    buttons: [
        { button: 0,  command: "apply" },
        { button: 1,  command: "disarm" },
        { button: 2,  command: "use_skill pray" },
        { button: 3,  command: "search" },
    ],
};

/**
 * Find the best matching profile for a gamepad, based on its `id` string.
 * Returns a deep copy so the caller can modify it freely.
 */
export function findProfileForGamepad(gamepadId: string): GamepadProfile {
    const lower = gamepadId.toLowerCase();
    for (const profile of defaultProfiles) {
        for (const matchId of profile.matchId) {
          if (lower.includes(matchId.toLowerCase())) {
              console.log(lower + " matched with: ", matchId)
              return structuredClone(profile);
          }
        }
    }
    console.log(lower + " didn't match a default profile")

    return structuredClone(fallbackProfile);
}
