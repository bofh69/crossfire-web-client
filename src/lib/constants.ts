/**
 * constants.ts – Shared runtime constants for the Crossfire web client.
 *
 * Centralises magic numbers that were previously scattered across the
 * codebase, making them easy to find and change in one place.
 */

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

/**
 * Interval (ms) for the self-tick fallback timer used when the server does
 * not send tick packets.  8 fps ≈ 125 ms, matching the old C client.
 */
export const SELF_TICK_INTERVAL_MS = 125;

// ---------------------------------------------------------------------------
// Info-panel message buffer
// ---------------------------------------------------------------------------

/** Maximum number of messages kept in the info-panel before trimming. */
export const MSG_BUFFER_MAX = 500;

/** Target message count after trimming the info-panel buffer. */
export const MSG_BUFFER_TRIM = 400;

// ---------------------------------------------------------------------------
// Sound / music
// ---------------------------------------------------------------------------

/**
 * Maximum music volume expressed as a fraction of 1.0.
 * Caps the effective volume at 75 % to match the old GTK client behaviour.
 */
export const MUSIC_VOLUME_CAP = 0.75;

// ---------------------------------------------------------------------------
// Performance thresholds
// ---------------------------------------------------------------------------

/** Log a warning if processing a network packet takes longer than this (ms). */
export const SLOW_PACKET_THRESHOLD_MS = 5;

/** Log a warning if drawing the game map takes longer than this (ms). */
export const SLOW_DRAW_THRESHOLD_MS = 5;

/** How often (ms) to emit per-second map-draw statistics to the console. */
export const DRAW_STATS_INTERVAL_MS = 5000;

// ---------------------------------------------------------------------------
// Sound / music fade
// ---------------------------------------------------------------------------

/** Duration (ms) to fade out the old track when switching to a new one. */
export const MUSIC_FADE_OUT_MS = 2000;

/** Duration (ms) to fade out the current track when stopping music. */
export const MUSIC_FADE_STOP_MS = 1000;

/** Duration (ms) to fade in a new track when music was already playing. */
export const MUSIC_FADE_IN_MS = 2000;

/** Interval (ms) between volume steps during a music fade-in or fade-out. */
export const MUSIC_FADE_STEP_MS = 50;

// ---------------------------------------------------------------------------
// Gamepad
// ---------------------------------------------------------------------------

/** Duration (ms) of the gamepad rumble effect triggered by an HP drop. */
export const VIBRATION_DURATION_MS = 400;

/**
 * Minimum HP drop (as a fraction of max HP) that triggers a controller rumble.
 * 0.1 = 10 % of the player's maximum HP.
 */
export const VIBRATION_HP_DROP_THRESHOLD = 0.1;

/** Weak-motor intensity (0–1) used for the HP-drop rumble effect. */
export const VIBRATION_WEAK_MAGNITUDE = 0.5;

/** Strong-motor intensity (0–1) used for the HP-drop rumble effect. */
export const VIBRATION_STRONG_MAGNITUDE = 1.0;

/**
 * Default dead-zone threshold for walk movement (0–1).
 * Stick positions closer to center than this are ignored.
 */
export const DEFAULT_WALK_THRESHOLD = 0.2;

/**
 * Default threshold beyond which walk becomes run (0–1).
 * The stick must reach this magnitude to trigger a run command.
 */
export const DEFAULT_RUN_THRESHOLD = 0.7;

/**
 * Default threshold for fire-stick direction detection (0–1).
 * The fire stick must reach this magnitude to choose a direction.
 */
export const DEFAULT_FIRE_THRESHOLD = 0.7;

// ---------------------------------------------------------------------------
// Game / protocol
// ---------------------------------------------------------------------------

/**
 * Base tile size in pixels.  Crossfire face images are always exact multiples
 * of this value in each dimension (32 px, 64 px, 96 px, …).
 */
export const TILE_SIZE = 32;

/**
 * Maximum food value used by the Crossfire protocol.
 * The food stat ranges 0–999; 999 is full (no hunger).
 */
export const MAX_FOOD = 999;
