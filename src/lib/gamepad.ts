/**
 * Gamepad/joystick handling for the Crossfire web client.
 *
 * Provides:
 *  • Polling-based gamepad input (requestAnimationFrame loop)
 *  • Analog stick → direction conversion with walk/run/fire zones
 *  • Button → command mapping with edge-detection (press/release)
 *  • Per-controller profiles (auto-detected by name)
 *  • Persistence of user-customised bindings via localStorage
 *  • Interactive axis-configuration (move a stick to assign it)
 *  • Interactive button-binding (press a button to assign a command)
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
    sendCommand, walkDir,
} from "./player";
import { LOG } from "./misc";
import { LogLevel, SC_NORMAL } from "./protocol";

// ──────────────────────────────────────────────────────────────────────────────
// Direction helpers
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Convert an (x, y) analog stick position to a direction index 0–8.
 *   0 = stay, 1 = north, 2 = NE, 3 = east, 4 = SE,
 *   5 = south, 6 = SW, 7 = west, 8 = NW.
 *
 * Returns 0 if the magnitude is below `deadzone`.
 */
function stickToDirection(x: number, y: number, deadzone: number): number {
    const mag = Math.sqrt(x * x + y * y);
    if (mag < deadzone) return 0; // stay / dead-zone

    // atan2 gives angle in radians; convert to 0–360 degrees (0 = right/east).
    let angle = Math.atan2(-y, x) * (180 / Math.PI);
    if (angle < 0) angle += 360;

    // Map angle to one of eight compass directions.
    // Each sector is 45° wide, centred on the compass direction.
    //   East=0°, NE=45°, North=90°, NW=135°, West=180°, SW=225°, South=270°, SE=315°
    if (angle < 22.5  || angle >= 337.5) return 3; // east
    if (angle < 67.5)  return 2; // northeast
    if (angle < 112.5) return 1; // north
    if (angle < 157.5) return 8; // northwest
    if (angle < 202.5) return 7; // west
    if (angle < 247.5) return 6; // southwest
    if (angle < 292.5) return 5; // south
    return 4; // southeast
}

// ──────────────────────────────────────────────────────────────────────────────
// Storage key
// ──────────────────────────────────────────────────────────────────────────────

const STORAGE_KEY = "cf_gamepad_bindings";

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
/** Whether we were running last frame. */
let prevRunning = false;
/** Whether we were firing (via fire stick) last frame. */
let prevFiring = false;

/** Animation frame handle for the polling loop. */
let pollHandle: number | null = null;

// ── Axis-configuration mode ─────────────────────────────────────────────────

export type AxisConfigTarget = "walk" | "fire";

let axisConfigActive = false;
let axisConfigTarget: AxisConfigTarget = "walk";
let axisConfigCallback: ((axes: StickAxes) => void) | null = null;
/** Track which axis moved first (becomes X), and which second (becomes Y). */
let axisConfigFirst: number | null = null;
let axisConfigSecond: number | null = null;

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
// Persistence
// ──────────────────────────────────────────────────────────────────────────────

function saveGamepadConfig(): void {
    if (!activeProfile) return;
    const data: SavedGamepadConfig = {
        walkStick: activeProfile.walkStick,
        fireStick: activeProfile.fireStick,
        walkThreshold: activeProfile.walkThreshold,
        runThreshold: activeProfile.runThreshold,
        fireThreshold: activeProfile.fireThreshold,
        buttons: activeProfile.buttons,
    };
    saveConfig(STORAGE_KEY, data);
}

function loadSavedConfig(): SavedGamepadConfig | null {
    return loadConfig<SavedGamepadConfig | null>(STORAGE_KEY, null);
}

// ──────────────────────────────────────────────────────────────────────────────
// Gamepad connection management
// ──────────────────────────────────────────────────────────────────────────────

function onGamepadConnected(e: GamepadEvent): void {
    const gp = e.gamepad;
    LOG(LogLevel.Info, "gamepad::connected",
        `Gamepad connected: "${gp.id}" (index ${gp.index}, ${gp.buttons.length} buttons, ${gp.axes.length} axes)`);

    if (activeGamepadIndex >= 0) {
        // Already tracking a gamepad; ignore additional ones.
        return;
    }

    activeGamepadIndex = gp.index;

    // Try to load saved config, otherwise use a matching profile.
    const saved = loadSavedConfig();
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
// Stick processing
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
            // Magnitude above threshold but direction resolved to stay (0).
            clearFire();
            prevFiring = false;
            prevFireDir = 0;
        }
    } else if (prevFiring) {
        clearFire();
        prevFiring = false;
        prevFireDir = 0;
    }

    // ── Walk / run stick ────────────────────────────────────────────
    const isRunning = walkMag >= activeProfile.runThreshold;
    const isWalking = walkMag >= activeProfile.walkThreshold;

    if (isWalking) {
        const dir = stickToDirection(wxRaw, wyRaw, activeProfile.walkThreshold);
        if (dir > 0) {
            if (isRunning) {
                if (dir !== prevWalkDir || !prevRunning) {
                    // When changing run direction, stop the previous run first.
                    if (prevRunning) {
                        clearRun();
                    }
                    runDir(dir);
                }
                prevRunning = true;
            } else {
                // Walking speed
                if (prevRunning) {
                    clearRun();
                    prevRunning = false;
                }
                if (dir !== prevWalkDir) {
                    walkDir(dir);
                }
            }
            prevWalkDir = dir;
        }
    } else {
        // Stick returned to center.
        if (prevRunning) {
            clearRun();
            prevRunning = false;
        }
        prevWalkDir = 0;
    }
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
// Axis configuration (interactive)
// ──────────────────────────────────────────────────────────────────────────────

const AXIS_CONFIG_THRESHOLD = 0.7;

/**
 * Start axis-configuration mode.
 * The user moves a stick, and the first two distinct axes that exceed
 * the threshold become the chosen stick.
 *
 * @param target Which stick to configure ("walk" or "fire").
 * @param onDone Called when two axes have been captured.
 */
export function startAxisConfig(
    target: AxisConfigTarget,
    onDone: (axes: StickAxes) => void,
): void {
    axisConfigActive = true;
    axisConfigTarget = target;
    axisConfigCallback = onDone;
    axisConfigFirst = null;
    axisConfigSecond = null;
}

/** Cancel a running axis configuration. */
export function cancelAxisConfig(): void {
    axisConfigActive = false;
    axisConfigCallback = null;
    axisConfigFirst = null;
    axisConfigSecond = null;
}

function handleAxisConfig(gp: Gamepad): void {
    for (let i = 0; i < gp.axes.length; i++) {
        if (Math.abs(gp.axes[i]) < AXIS_CONFIG_THRESHOLD) continue;

        if (axisConfigFirst === null) {
            axisConfigFirst = i;
        } else if (axisConfigSecond === null && i !== axisConfigFirst) {
            axisConfigSecond = i;
        }
    }

    if (axisConfigFirst !== null && axisConfigSecond !== null) {
        const axes: StickAxes = {
            axisX: axisConfigFirst,
            axisY: axisConfigSecond,
        };

        if (activeProfile) {
            if (axisConfigTarget === "walk") {
                activeProfile.walkStick = axes;
            } else {
                activeProfile.fireStick = axes;
            }
            saveGamepadConfig();
        }

        const doneCb = axisConfigCallback;
        cancelAxisConfig();
        doneCb?.(axes);
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
    activeProfile.buttons = activeProfile.buttons.filter(b => b.button !== button);
    saveGamepadConfig();
}

/** Get the current button mappings (read-only snapshot). */
export function getButtonMappings(): readonly ButtonMapping[] {
    return activeProfile?.buttons ?? [];
}

/** Get the current stick configuration. */
export function getStickConfig(): { walk: StickAxes; fire: StickAxes } | null {
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
            onGamepadConnected(new GamepadEvent("gamepadconnected", { gamepad: gp }));
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

    if (prevRunning) clearRun();
    if (prevFiring) clearFire();

    activeGamepadIndex = -1;
    activeProfile = null;
    prevButtons = [];
    prevWalkDir = 0;
    prevFireDir = 0;
    prevRunning = false;
    prevFiring = false;
}
