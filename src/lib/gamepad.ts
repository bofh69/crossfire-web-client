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
import {
    isAxisConfigActive, isButtonConfigActive,
    handleAxisConfig as doHandleAxisConfig,
    handleButtonConfig as doHandleButtonConfig,
    acceptAxisConfig as doAcceptAxisConfig,
    getAxisTestDirection as doGetAxisTestDirection,
} from "./gamepad_config";
import {
    notifyHpUpdate as doNotifyHpUpdate,
    resetHpTracking as doResetHpTracking,
} from "./gamepad_vibration";

// ──────────────────────────────────────────────────────────────────────────────
// Constants
// ──────────────────────────────────────────────────────────────────────────────

// Re-export types from gamepad_config.
export type { AxisConfigTarget, AxisConfigStep } from "./gamepad_config";

/**
 * Hysteresis margin for walk↔run transitions.
 * Once running (magnitude ≥ runThreshold), stay running until magnitude
 * drops below (runThreshold − HYSTERESIS).
 */
const HYSTERESIS = 0.1;

/**
 * Half-width of the angular hysteresis band on each side of a sector boundary.
 * The stick must move this far past the 22.5° boundary before the direction
 * changes, preventing rapid alternation between adjacent directions near the
 * boundary.  7.5° = π/24 radians.
 */
const ANGLE_HYSTERESIS = Math.PI / 24;

/**
 * Delay (in milliseconds) before sending a walk command from the gamepad.
 * If the stick reaches the run threshold within this window, we send
 * a run command instead — avoiding an unwanted walk-then-run on fast flicks.
 * 80 ms is short enough to feel responsive but long enough to catch
 * a fast flick that passes through the walk zone on its way to run.
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
 * Convert an (x, y) analog stick position to a direction index 0–8, using
 * polar coordinates with angular hysteresis to prevent flickering between
 * adjacent directions near sector boundaries.
 *
 *   0 = stay, 1 = north, 2 = NE, 3 = east, 4 = SE,
 *   5 = south, 6 = SW, 7 = west, 8 = NW.
 *
 * @param x        Stick X axis (positive = east).
 * @param y        Stick Y axis (positive = south / down).
 * @param threshold Dead-zone distance; returns 0 (stay) when magnitude < threshold.
 * @param prevDir  Previously returned direction (0 = none).  Used to apply
 *                 angular hysteresis: the direction only changes when the angle
 *                 has moved more than (22.5° + ANGLE_HYSTERESIS) from the
 *                 current sector centre.
 */
function stickToDirection(x: number, y: number, threshold: number, prevDir: number): number {
    // Use vector magnitude for the dead-zone check.
    const dist = Math.sqrt(x * x + y * y);
    if (dist < threshold) return 0;

    // Compute clockwise-from-north angle in [0, 2π).
    // atan2(x, -y): north (y<0) → 0, east (x>0) → π/2, south (y>0) → π,
    //               west (x<0) → 3π/2.
    let angle = Math.atan2(x, -y);
    if (angle < 0) angle += 2 * Math.PI;

    // Each of the 8 sectors spans π/4 (45°).
    // Sector 0 = north (N), 1 = NE, 2 = east, …, 7 = NW.
    // Shifting by π/8 before flooring places boundaries between sector centres.
    const rawSector = Math.floor((angle + Math.PI / 8) / (Math.PI / 4)) % 8;

    // Apply angular hysteresis: only leave the current sector when the angle
    // has moved more than half-sector-width + ANGLE_HYSTERESIS from its centre.
    if (prevDir > 0) {
        const prevSector = prevDir - 1;
        const sectorCenter = prevSector * (Math.PI / 4);
        let diff = angle - sectorCenter;
        // Normalise to (-π, π] so the comparison works across the 0/2π seam.
        if (diff >  Math.PI) diff -= 2 * Math.PI;
        if (diff < -Math.PI) diff += 2 * Math.PI;
        if (Math.abs(diff) < Math.PI / 8 + ANGLE_HYSTERESIS) {
            return prevDir;
        }
    }

    return rawSector + 1;
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
    if (isAxisConfigActive()) {
        doHandleAxisConfig(gp, prevButtons, activeProfile, saveGamepadConfig);
        return;
    }

    // ── Button-configuration mode ───────────────────────────────────
    if (isButtonConfigActive()) {
        doHandleButtonConfig(gp, prevButtons);
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

    // Read axes, applying per-stick inversion flags set during axis configuration.
    const wxRaw = gp.axes[ws.axisX] ?? 0;
    const wyRaw = gp.axes[ws.axisY] ?? 0;
    const fxRaw = gp.axes[fs.axisX] ?? 0;
    const fyRaw = gp.axes[fs.axisY] ?? 0;

    const wx = ws.invertX ? -wxRaw : wxRaw;
    const wy = ws.invertY ? -wyRaw : wyRaw;
    const fx = fs.invertX ? -fxRaw : fxRaw;
    const fy = fs.invertY ? -fyRaw : fyRaw;

    const walkMag = Math.sqrt(wx * wx + wy * wy);
    const fireMag = Math.sqrt(fx * fx + fy * fy);

    // ── Fire stick (takes priority for direction-sending) ───────────
    if (fireMag >= activeProfile.fireThreshold) {
        const dir = stickToDirection(fx, fy, activeProfile.fireThreshold, prevFireDir);
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
            // suppress walk to avoid an extra step.  We intentionally
            // skip all state updates so the next frame re-evaluates
            // from scratch once the ack arrives.
            return;
        }
    }

    if (isWalking) {
        const dir = stickToDirection(wx, wy, activeProfile.walkThreshold, prevWalkDir);
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

        const pressed = gp.buttons[mapping.button]!.pressed;
        const wasPressed = prevButtons[mapping.button] ?? false;

        // Fire on rising edge only.
        if (pressed && !wasPressed) {
            extendedCommand(mapping.command);
        }
    }

    // Update previous state for all buttons.
    for (let i = 0; i < gp.buttons.length; i++) {
        prevButtons[i] = gp.buttons[i]!.pressed;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Axis & button configuration (delegated to gamepad_config.ts)
// ──────────────────────────────────────────────────────────────────────────────

export { startAxisConfig, cancelAxisConfig } from "./gamepad_config";
export { startButtonConfig, cancelButtonConfig } from "./gamepad_config";

export function acceptAxisConfig(): void {
    doAcceptAxisConfig(activeProfile, saveGamepadConfig);
}

export function getAxisTestDirection(): number {
    return doGetAxisTestDirection(activeGamepadIndex, stickToDirection);
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
// HP vibration (delegated to gamepad_vibration.ts)
// ──────────────────────────────────────────────────────────────────────────────

export function notifyHpUpdate(hp: number, maxHp: number): void {
    doNotifyHpUpdate(hp, maxHp, activeGamepadIndex);
}

export function resetHpTracking(): void {
    doResetHpTracking();
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
