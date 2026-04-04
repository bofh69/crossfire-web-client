/**
 * Gamepad/joystick handling for the Crossfire web client.
 *
 * Provides:
 *  • Polling-based gamepad input (requestAnimationFrame loop)
 *  • Analog stick → direction conversion with walk/run/fire zones
 *  • Button → command mapping with edge-detection (press/release)
 *  • Per-controller profiles (auto-detected by name, stored per-controller)
 *  • Persistence of user-customised bindings via localStorage
 *  • Interactive axis-configuration (move stick N/E/S/W + test mode)
 *  • Interactive button-binding (press button, binds last command sent)
 *  • Hysteresis for walk↔run transitions
 *  • Relaxed diagonal detection for near-threshold second axis
 */

import {
    findProfileForGamepad,
    type GamepadProfile,
    type StickAxes,
    type ButtonMapping,
} from "./gamepad_defaults";
import { loadConfig, saveConfig } from "./storage";
import { extendedCommand } from "./p_cmd";
import {
    fireDir, clearFire,
    runDir, clearRun,
    walkDir,
    getLastNcomSeqSent, isNcomAcked,
} from "./player";
import { LOG } from "./misc";
import { LogLevel } from "./protocol";

// ──────────────────────────────────────────────────────────────────────────────
// Constants
// ──────────────────────────────────────────────────────────────────────────────

/** Axis threshold used during axis-configuration to detect stick movement. */
const AXIS_CONFIG_THRESHOLD = 0.7;

/**
 * After capturing a direction, all axes must drop below this value
 * before the next direction is accepted.  Provides hysteresis so the
 * user doesn't accidentally advance two steps at once.
 */
const AXIS_CONFIG_CENTER_THRESHOLD = 0.3;

/**
 * Hysteresis margin for walk↔run transitions.
 * Once running (magnitude ≥ runThreshold), stay running until magnitude
 * drops below (runThreshold − HYSTERESIS).
 */
const HYSTERESIS = 0.1;

/**
 * When the main axis is above the walk threshold but the secondary axis
 * is below it, still count as a diagonal if the secondary axis is at
 * least this fraction of the walk threshold.
 * E.g. with walkThreshold=0.2 and DIAGONAL_RATIO=0.75, a secondary axis
 * value of 0.15 (≥ 0.2 × 0.75 = 0.15) qualifies for diagonal.
 */
const DIAGONAL_RATIO = 0.75;

/**
 * Delay (in milliseconds) before sending a walk command from the gamepad.
 * If the stick reaches the run threshold within this window, we send
 * a run command instead — avoiding an unwanted walk-then-run on fast flicks.
 */
const WALK_DELAY_MS = 80;

// ──────────────────────────────────────────────────────────────────────────────
// Direction helpers
// ──────────────────────────────────────────────────────────────────────────────

/** Direction names indexed 0–8 (matching protocol direction indices). */
const directionNames: readonly string[] = [
    "stay", "north", "northeast",
    "east", "southeast", "south",
    "southwest", "west", "northwest",
];

/**
 * Convert a direction index (0–8) to a human-readable name.
 */
export function directionName(dir: number): string {
    return directionNames[dir] ?? "stay";
}

/**
 * Convert an (x, y) analog stick position to a direction index 0–8,
 * with relaxed diagonal detection.
 *
 *   0 = stay, 1 = north, 2 = NE, 3 = east, 4 = SE,
 *   5 = south, 6 = SW, 7 = west, 8 = NW.
 *
 * The `threshold` is the main dead-zone.  If only one axis is above
 * `threshold`, the other axis is still considered if it exceeds
 * `threshold * DIAGONAL_RATIO`, enabling diagonals when the stick
 * is close to the threshold boundary.
 */
function stickToDirection(x: number, y: number, threshold: number): number {
    const ax = Math.abs(x);
    const ay = Math.abs(y);
    const diagThreshold = threshold * DIAGONAL_RATIO;

    // Both axes below the relaxed threshold → dead-zone.
    if (ax < diagThreshold && ay < diagThreshold) return 0;

    // At least one axis must be above the full threshold.
    if (ax < threshold && ay < threshold) return 0;

    // Determine which axes contribute to direction.
    const hasX = ax >= diagThreshold;
    const hasY = ay >= diagThreshold;

    if (hasX && hasY) {
        // Diagonal
        if (x > 0 && y < 0) return 2; // NE
        if (x > 0 && y > 0) return 4; // SE
        if (x < 0 && y > 0) return 6; // SW
        return 8; // NW
    }
    if (hasX) {
        return x > 0 ? 3 : 7; // east / west
    }
    // hasY
    return y < 0 ? 1 : 5; // north / south (y-axis: negative=up)
}

// ──────────────────────────────────────────────────────────────────────────────
// Storage – per-controller keys
// ──────────────────────────────────────────────────────────────────────────────

const STORAGE_KEY_PREFIX = "cf_gamepad_bindings_";

/** Sanitise a gamepad name for use as a localStorage key suffix. */
function sanitiseName(name: string): string {
    return name.replace(/[^a-zA-Z0-9_-]/g, "_").substring(0, 80);
}

/** The sanitised name of the currently active controller (for storage). */
let activeControllerKey = "";

interface SavedGamepadConfig {
    walkStick: StickAxes;
    fireStick: StickAxes;
    walkThreshold: number;
    runThreshold: number;
    fireThreshold: number;
    buttons: ButtonMapping[];
}

// ──────────────────────────────────────────────────────────────────────────────
// Module state
// ──────────────────────────────────────────────────────────────────────────────

/** Active gamepad profile (resolved per-controller, possibly user-customised). */
let activeProfile: GamepadProfile | null = null;

/** The gamepad index we're currently tracking (-1 = none). */
let activeGamepadIndex = -1;

/** Previous button states for edge detection (true = was pressed). */
let prevButtons: boolean[] = [];

/** Previous direction from walk stick (for change detection). */
let prevWalkDir = 0;
/** Previous direction from fire stick. */
let prevFireDir = 0;
/** Whether we were running last frame (with hysteresis). */
let prevRunning = false;
/** Whether we were firing (via fire stick) last frame. */
let prevFiring = false;

/** Animation frame handle for the polling loop. */
let pollHandle: number | null = null;

// ── Walk-delay state (prevents walk-then-run on fast flick) ─────────────────

/** Timer handle for the pending walk command. */
let walkDelayTimer: ReturnType<typeof setTimeout> | null = null;
/** Direction of the pending walk (set when timer fires or promoted to run). */
let walkDelayDir = 0;

// ── Run-stop guard (prevents extra walk step after releasing run) ────────────

/**
 * When we send `run_stop`, we record the ncom sequence number here.
 * We suppress any walk commands from the gamepad until the server has
 * acknowledged that `run_stop` via a comc response, preventing an extra
 * walk step that would otherwise be triggered while the stick passes
 * through the walk zone on its way back to center.
 */
let runStopSeq = -1;

// ── Axis-configuration mode ─────────────────────────────────────────────────

export type AxisConfigTarget = "walk" | "fire";

/**
 * Multi-step axis configuration flow:
 *   1. "move-north" — user pushes stick north
 *   2. "move-east"  — user pushes stick east
 *   3. "move-south" — user pushes stick south (confirms Y axis & sign)
 *   4. "move-west"  — user pushes stick west  (confirms X axis & sign)
 *   5. "testing"    — user sees live direction feedback, Accept/Abort
 */
export type AxisConfigStep =
    | "move-north"
    | "move-east"
    | "move-south"
    | "move-west"
    | "testing";

let axisConfigActive = false;
let axisConfigTarget: AxisConfigTarget = "walk";
let axisConfigStep: AxisConfigStep = "move-north";
let axisConfigCallback: ((axes: StickAxes | null) => void) | null = null;

/**
 * When true, we've captured a direction and are waiting for all axes
 * to return to center before advancing to the next step.
 */
let axisConfigWaitingForCenter = false;

/** Axis index and sign captured during each calibration step. */
let axisNorthIdx: number | null = null;
let axisEastIdx: number | null = null;
/** Whether the Y axis is inverted (positive = north instead of negative). */
let axisYInverted = false;
/** Whether the X axis is inverted (positive = west instead of east). */
let axisXInverted = false;

/** Pending StickAxes during the testing phase (not yet saved). */
let axisConfigPending: StickAxes | null = null;

/** Callback for step transitions (so UI can update its state). */
let axisConfigStepCallback: ((step: AxisConfigStep) => void) | null = null;

// ── Button-configuration mode ───────────────────────────────────────────────

let buttonConfigActive = false;
let buttonConfigCallback: ((button: number) => void) | null = null;

// ──────────────────────────────────────────────────────────────────────────────
// Callbacks (like keys.ts KeyCallbacks)
// ──────────────────────────────────────────────────────────────────────────────

export interface GamepadCallbacks {
    drawInfo: (message: string) => void;
}

let cb: GamepadCallbacks | null = null;

export function setGamepadCallbacks(c: GamepadCallbacks): void {
    cb = c;
}

// ──────────────────────────────────────────────────────────────────────────────
// Persistence – per-controller
// ──────────────────────────────────────────────────────────────────────────────

function saveGamepadConfig(): void {
    if (!activeProfile || !activeControllerKey) return;
    const data: SavedGamepadConfig = {
        walkStick: activeProfile.walkStick,
        fireStick: activeProfile.fireStick,
        walkThreshold: activeProfile.walkThreshold,
        runThreshold: activeProfile.runThreshold,
        fireThreshold: activeProfile.fireThreshold,
        buttons: activeProfile.buttons,
    };
    saveConfig(STORAGE_KEY_PREFIX + activeControllerKey, data);
}

function loadSavedConfig(controllerKey: string): SavedGamepadConfig | null {
    return loadConfig<SavedGamepadConfig | null>(
        STORAGE_KEY_PREFIX + controllerKey, null);
}

// ──────────────────────────────────────────────────────────────────────────────
// Gamepad connection management
// ──────────────────────────────────────────────────────────────────────────────

function onGamepadConnected(e: GamepadEvent): void {
    const gp = e.gamepad;
    LOG(LogLevel.Info, "gamepad::connected",
        `Gamepad connected: "${gp.id}" (index ${gp.index}, ` +
        `${gp.buttons.length} buttons, ${gp.axes.length} axes)`);

    if (activeGamepadIndex >= 0) {
        // Already tracking a gamepad; ignore additional ones.
        return;
    }

    activeGamepadIndex = gp.index;
    activeControllerKey = sanitiseName(gp.id);

    // Try to load saved config for this controller, otherwise use a
    // matching built-in profile.
    const saved = loadSavedConfig(activeControllerKey);
    const profile = findProfileForGamepad(gp.id);
    if (saved) {
        profile.walkStick = saved.walkStick;
        profile.fireStick = saved.fireStick;
        profile.walkThreshold = saved.walkThreshold;
        profile.runThreshold = saved.runThreshold;
        profile.fireThreshold = saved.fireThreshold;
        profile.buttons = saved.buttons;
    }
    activeProfile = profile;

    prevButtons = new Array(gp.buttons.length).fill(false);
    prevWalkDir = 0;
    prevFireDir = 0;
    prevRunning = false;
    prevFiring = false;

    cb?.drawInfo(`Gamepad connected: ${profile.name}`);
    startPolling();
}

function onGamepadDisconnected(e: GamepadEvent): void {
    const gp = e.gamepad;
    LOG(LogLevel.Info, "gamepad::disconnected",
        `Gamepad disconnected: "${gp.id}" (index ${gp.index})`);

    if (gp.index === activeGamepadIndex) {
        activeGamepadIndex = -1;
        activeControllerKey = "";
        activeProfile = null;
        prevButtons = [];
        stopPolling();

        // Clean up any in-progress fire/run.
        clearFire();
        clearRun();

        cb?.drawInfo("Gamepad disconnected.");
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Polling loop
// ──────────────────────────────────────────────────────────────────────────────

function startPolling(): void {
    if (pollHandle !== null) return;
    pollHandle = requestAnimationFrame(pollGamepad);
}

function stopPolling(): void {
    if (pollHandle !== null) {
        cancelAnimationFrame(pollHandle);
        pollHandle = null;
    }
}

function pollGamepad(): void {
    pollHandle = requestAnimationFrame(pollGamepad);

    if (activeGamepadIndex < 0 || !activeProfile) return;

    const gamepads = navigator.getGamepads();
    const gp = gamepads[activeGamepadIndex];
    if (!gp) return;

    // ── Axis-configuration mode ─────────────────────────────────────
    if (axisConfigActive) {
        handleAxisConfig(gp);
        return;
    }

    // ── Button-configuration mode ───────────────────────────────────
    if (buttonConfigActive) {
        handleButtonConfig(gp);
        return;
    }

    // ── Normal gameplay ─────────────────────────────────────────────
    processSticks(gp);
    processButtons(gp);
}

// ──────────────────────────────────────────────────────────────────────────────
// Stick processing (with hysteresis and relaxed diagonals)
// ──────────────────────────────────────────────────────────────────────────────

function processSticks(gp: Gamepad): void {
    if (!activeProfile) return;

    const ws = activeProfile.walkStick;
    const fs = activeProfile.fireStick;

    // Read axes (clamped to available range).
    const wxRaw = gp.axes[ws.axisX] ?? 0;
    const wyRaw = gp.axes[ws.axisY] ?? 0;
    const fxRaw = gp.axes[fs.axisX] ?? 0;
    const fyRaw = gp.axes[fs.axisY] ?? 0;

    const walkMag = Math.sqrt(wxRaw * wxRaw + wyRaw * wyRaw);
    const fireMag = Math.sqrt(fxRaw * fxRaw + fyRaw * fyRaw);

    // ── Fire stick (takes priority for direction-sending) ───────────
    if (fireMag >= activeProfile.fireThreshold) {
        const dir = stickToDirection(fxRaw, fyRaw, activeProfile.fireThreshold);
        if (dir > 0) {
            if (dir !== prevFireDir || !prevFiring) {
                fireDir(dir);
            }
            prevFireDir = dir;
            prevFiring = true;
        } else if (prevFiring) {
            clearFire();
            prevFiring = false;
            prevFireDir = 0;
        }
    } else if (prevFiring) {
        clearFire();
        prevFiring = false;
        prevFireDir = 0;
    }

    // ── Walk / run stick (with hysteresis, walk delay, run-stop guard) ──
    const runEnter = activeProfile.runThreshold;
    const runExit = Math.max(activeProfile.walkThreshold,
                             activeProfile.runThreshold - HYSTERESIS);
    const isRunning = prevRunning
        ? walkMag >= runExit    // already running: use lower exit threshold
        : walkMag >= runEnter;  // not running: use full enter threshold
    const isWalking = walkMag >= activeProfile.walkThreshold;

    // ── Run-stop guard: check if the pending run_stop has been acked ──
    if (runStopSeq !== -1) {
        if (isNcomAcked(runStopSeq)) {
            // Server acknowledged the run_stop; we can process walk again.
            runStopSeq = -1;
        } else if (isWalking && !isRunning) {
            // Still waiting for run_stop ack and stick is in walk zone —
            // suppress walk to avoid an extra step.
            return;
        }
    }

    if (isWalking) {
        const dir = stickToDirection(wxRaw, wyRaw, activeProfile.walkThreshold);
        if (dir > 0) {
            if (isRunning) {
                // Cancel any pending walk delay — we're running now.
                cancelWalkDelay();
                if (dir !== prevWalkDir || !prevRunning) {
                    if (prevRunning) {
                        clearRun();
                    }
                    runDir(dir);
                }
                prevRunning = true;
                prevWalkDir = dir;
            } else {
                // Walking speed.
                if (prevRunning) {
                    clearRun();
                    // Record the run_stop sequence for the guard.
                    runStopSeq = getLastNcomSeqSent();
                    prevRunning = false;
                }
                // Use walk delay: schedule the walk command after a short
                // delay.  If the stick reaches run within the window, the
                // timer is cancelled and run is sent instead.
                scheduleWalk(dir);
            }
        }
    } else {
        // Stick returned to center.
        cancelWalkDelay();
        if (prevRunning) {
            clearRun();
            runStopSeq = getLastNcomSeqSent();
            prevRunning = false;
        }
        prevWalkDir = 0;
    }
}

// ── Walk delay helpers ──────────────────────────────────────────────────────

/**
 * Schedule a walk command after WALK_DELAY_MS.  If a walk is already
 * pending for the same direction, do nothing (avoid re-scheduling).
 * If the direction changed, replace the pending walk.
 */
function scheduleWalk(dir: number): void {
    if (walkDelayTimer !== null && walkDelayDir === dir) {
        // Already pending for this direction.
        return;
    }
    cancelWalkDelay();
    walkDelayDir = dir;
    walkDelayTimer = setTimeout(() => {
        walkDelayTimer = null;
        // Re-check: if the stick has since reached run threshold, the
        // run branch in processSticks will have cancelled us already,
        // but just in case, verify we're still in walk-only state.
        if (!prevRunning && walkDelayDir > 0) {
            if (walkDelayDir !== prevWalkDir || prevWalkDir === 0) {
                walkDir(walkDelayDir);
            }
            prevWalkDir = walkDelayDir;
        }
        walkDelayDir = 0;
    }, WALK_DELAY_MS);
}

function cancelWalkDelay(): void {
    if (walkDelayTimer !== null) {
        clearTimeout(walkDelayTimer);
        walkDelayTimer = null;
    }
    walkDelayDir = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Button processing
// ──────────────────────────────────────────────────────────────────────────────

function processButtons(gp: Gamepad): void {
    if (!activeProfile) return;

    for (const mapping of activeProfile.buttons) {
        if (mapping.button >= gp.buttons.length) continue;

        const pressed = gp.buttons[mapping.button].pressed;
        const wasPressed = prevButtons[mapping.button] ?? false;

        // Fire on rising edge only.
        if (pressed && !wasPressed) {
            extendedCommand(mapping.command);
        }
    }

    // Update previous state for all buttons.
    for (let i = 0; i < gp.buttons.length; i++) {
        prevButtons[i] = gp.buttons[i].pressed;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Axis configuration (interactive, multi-step)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Start axis-configuration mode.
 * Guides the user through four directional movements (N, E, S, W) to
 * determine which axes correspond to X and Y, then enters a test phase
 * with live feedback.
 *
 * @param target Which stick to configure ("walk" or "fire").
 * @param onStepChange Called when the configuration step changes (for UI).
 * @param onDone Called when configuration is accepted (axes) or aborted (null).
 */
export function startAxisConfig(
    target: AxisConfigTarget,
    onStepChange: (step: AxisConfigStep) => void,
    onDone: (axes: StickAxes | null) => void,
): void {
    axisConfigActive = true;
    axisConfigTarget = target;
    axisConfigStep = "move-north";
    axisConfigStepCallback = onStepChange;
    axisConfigCallback = onDone;
    axisConfigWaitingForCenter = false;
    axisNorthIdx = null;
    axisEastIdx = null;
    axisYInverted = false;
    axisXInverted = false;
    axisConfigPending = null;
}

/** Cancel a running axis configuration. */
export function cancelAxisConfig(): void {
    axisConfigActive = false;
    axisConfigWaitingForCenter = false;
    axisConfigCallback = null;
    axisConfigStepCallback = null;
    axisNorthIdx = null;
    axisEastIdx = null;
    axisConfigPending = null;
}

/** Accept the current axis configuration (called from test mode). */
export function acceptAxisConfig(): void {
    if (!axisConfigActive || axisConfigStep !== "testing") return;
    if (axisConfigPending && activeProfile) {
        if (axisConfigTarget === "walk") {
            activeProfile.walkStick = axisConfigPending;
        } else {
            activeProfile.fireStick = axisConfigPending;
        }
        saveGamepadConfig();
    }
    const doneCb = axisConfigCallback;
    const result = axisConfigPending;
    cancelAxisConfig();
    doneCb?.(result);
}

/**
 * Get the live stick direction during axis-config test mode.
 * Returns the direction index (0–8) or 0 if not in test mode.
 */
export function getAxisTestDirection(): number {
    if (!axisConfigActive || axisConfigStep !== "testing") return 0;
    if (!axisConfigPending) return 0;

    if (activeGamepadIndex < 0) return 0;
    const gamepads = navigator.getGamepads();
    const gp = gamepads[activeGamepadIndex];
    if (!gp) return 0;

    const x = gp.axes[axisConfigPending.axisX] ?? 0;
    const y = gp.axes[axisConfigPending.axisY] ?? 0;
    return stickToDirection(x, y, 0.2);
}

function handleAxisConfig(gp: Gamepad): void {
    // If waiting for center, check whether all axes are back in the
    // dead-zone before proceeding.
    if (axisConfigWaitingForCenter) {
        let allCentered = true;
        for (let i = 0; i < gp.axes.length; i++) {
            if (Math.abs(gp.axes[i]) > AXIS_CONFIG_CENTER_THRESHOLD) {
                allCentered = false;
                break;
            }
        }
        if (!allCentered) return;  // Still waiting for center.
        axisConfigWaitingForCenter = false;
    }

    switch (axisConfigStep) {
        case "move-north":
            handleAxisStep(gp, (idx, value) => {
                axisNorthIdx = idx;
                // Typically, north is negative Y.  If value is positive,
                // the axis is inverted.
                axisYInverted = value > 0;
                advanceAxisStep("move-east");
            });
            break;

        case "move-east":
            handleAxisStep(gp, (idx, value) => {
                axisEastIdx = idx;
                axisXInverted = value < 0;
                advanceAxisStep("move-south");
            });
            break;

        case "move-south":
            // Confirm it's the same axis as north.
            handleAxisStep(gp, (idx, _value) => {
                if (idx !== axisNorthIdx) {
                    // Different axis — accept the new one.
                    axisNorthIdx = idx;
                }
                advanceAxisStep("move-west");
            });
            break;

        case "move-west":
            // Confirm it's the same axis as east.
            handleAxisStep(gp, (idx, _value) => {
                if (idx !== axisEastIdx) {
                    axisEastIdx = idx;
                }
                // Build the StickAxes — store raw axis indices.
                axisConfigPending = {
                    axisX: axisEastIdx!,
                    axisY: axisNorthIdx!,
                };
                axisConfigStep = "testing";
                axisConfigStepCallback?.("testing");
            });
            break;

        case "testing":
            // In test mode, check if the "apply" button (B0) is pressed
            // to accept.
            for (let i = 0; i < gp.buttons.length; i++) {
                if (gp.buttons[i].pressed && !(prevButtons[i] ?? false)) {
                    // Check if this button is mapped to "apply".
                    const mapping = activeProfile?.buttons.find(
                        b => b.button === i);
                    if (mapping?.command === "apply") {
                        acceptAxisConfig();
                        break;
                    }
                }
            }
            // Update button state for edge detection.
            for (let i = 0; i < gp.buttons.length; i++) {
                prevButtons[i] = gp.buttons[i].pressed;
            }
            break;
    }
}

/**
 * Advance to the next axis-configuration step, requiring the stick to
 * return to center first.
 */
function advanceAxisStep(nextStep: AxisConfigStep): void {
    axisConfigWaitingForCenter = true;
    axisConfigStep = nextStep;
    axisConfigStepCallback?.(nextStep);
}

/**
 * Helper: detect the first axis that exceeds AXIS_CONFIG_THRESHOLD and
 * has just crossed from below to above (edge detection).
 */
function handleAxisStep(
    gp: Gamepad,
    onDetected: (axisIndex: number, value: number) => void,
): void {
    for (let i = 0; i < gp.axes.length; i++) {
        const value = gp.axes[i];
        if (Math.abs(value) >= AXIS_CONFIG_THRESHOLD) {
            onDetected(i, value);
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Button configuration (interactive)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Start button-configuration mode.
 * The user presses a button, and the first button pressed is reported.
 *
 * @param onDone Called with the button index when a button is pressed.
 */
export function startButtonConfig(onDone: (button: number) => void): void {
    buttonConfigActive = true;
    buttonConfigCallback = onDone;
}

/** Cancel a running button configuration. */
export function cancelButtonConfig(): void {
    buttonConfigActive = false;
    buttonConfigCallback = null;
}

function handleButtonConfig(gp: Gamepad): void {
    for (let i = 0; i < gp.buttons.length; i++) {
        if (gp.buttons[i].pressed && !(prevButtons[i] ?? false)) {
            const doneCb = buttonConfigCallback;
            cancelButtonConfig();

            // Update prevButtons so the press isn't re-detected.
            for (let j = 0; j < gp.buttons.length; j++) {
                prevButtons[j] = gp.buttons[j].pressed;
            }

            doneCb?.(i);
            return;
        }
    }

    // Update previous state so we detect edges correctly.
    for (let j = 0; j < gp.buttons.length; j++) {
        prevButtons[j] = gp.buttons[j].pressed;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API – binding management
// ──────────────────────────────────────────────────────────────────────────────

/** Set or replace the command for a specific button. */
export function setButtonCommand(button: number, command: string): void {
    if (!activeProfile) return;
    const existing = activeProfile.buttons.find(b => b.button === button);
    if (existing) {
        existing.command = command;
    } else {
        activeProfile.buttons.push({ button, command });
    }
    saveGamepadConfig();
}

/** Remove the command mapping for a specific button. */
export function removeButtonCommand(button: number): void {
    if (!activeProfile) return;
    activeProfile.buttons = activeProfile.buttons.filter(
        b => b.button !== button);
    saveGamepadConfig();
}

/** Get the current button mappings (read-only snapshot). */
export function getButtonMappings(): readonly ButtonMapping[] {
    return activeProfile?.buttons ?? [];
}

/** Get the current stick configuration. */
export function getStickConfig(): {
    walk: StickAxes; fire: StickAxes } | null {
    if (!activeProfile) return null;
    return {
        walk: { ...activeProfile.walkStick },
        fire: { ...activeProfile.fireStick },
    };
}

/** Reset gamepad bindings to defaults for the current controller. */
export function resetGamepadBindings(): void {
    if (activeGamepadIndex < 0) return;
    const gamepads = navigator.getGamepads();
    const gp = gamepads[activeGamepadIndex];
    if (!gp) return;

    activeProfile = findProfileForGamepad(gp.id);
    saveGamepadConfig();
    cb?.drawInfo("Gamepad bindings reset to defaults.");
}

/** Check whether a gamepad is currently connected and active. */
export function isGamepadConnected(): boolean {
    return activeGamepadIndex >= 0 && activeProfile !== null;
}

/** Get the name of the active gamepad profile. */
export function getActiveProfileName(): string {
    return activeProfile?.name ?? "(none)";
}

/** Find the existing command for a given button, or null. */
export function getButtonCommand(button: number): string | null {
    return activeProfile?.buttons.find(b => b.button === button)?.command
        ?? null;
}

// ──────────────────────────────────────────────────────────────────────────────
// Initialisation / teardown
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Initialise the gamepad subsystem.
 * Registers gamepadconnected/gamepaddisconnected handlers and starts
 * polling if a gamepad is already connected.
 */
export function gamepadInit(): void {
    window.addEventListener("gamepadconnected", onGamepadConnected);
    window.addEventListener("gamepaddisconnected", onGamepadDisconnected);

    // Check for gamepads already connected (e.g. page reload).
    const gamepads = navigator.getGamepads();
    for (const gp of gamepads) {
        if (gp) {
            onGamepadConnected(
                new GamepadEvent("gamepadconnected", { gamepad: gp }));
            break; // Only track the first one.
        }
    }

    LOG(LogLevel.Debug, "gamepad::init", "Gamepad subsystem initialised.");
}

/** Tear down the gamepad subsystem. */
export function gamepadShutdown(): void {
    window.removeEventListener("gamepadconnected", onGamepadConnected);
    window.removeEventListener("gamepaddisconnected", onGamepadDisconnected);
    stopPolling();
    cancelWalkDelay();

    if (prevRunning) clearRun();
    if (prevFiring) clearFire();

    activeGamepadIndex = -1;
    activeControllerKey = "";
    activeProfile = null;
    prevButtons = [];
    prevWalkDir = 0;
    prevFireDir = 0;
    prevRunning = false;
    prevFiring = false;
    runStopSeq = -1;
}
