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
import { clear_move_to } from "./mapdata";
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
import {
    isHotbarGamepadMode,
    enterHotbarGamepadMode,
    exitHotbarGamepadMode,
    setHotbarGamepadHighlight,
    getHotbarGamepadHighlight,
} from "./hotbar";

// ──────────────────────────────────────────────────────────────────────────────
// Constants
// ──────────────────────────────────────────────────────────────────────────────

// Re-export types from gamepad_config.
export type { AxisConfigTarget, AxisConfigStep } from "./gamepad_config";
// Re-export ButtonMapping so consumers can use it without importing from gamepad_defaults.
export type { ButtonMapping } from "./gamepad_defaults";

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
const CHAR_BUTTON_KEY_PREFIX = "cf_gamepad_char_buttons_";

/** Sanitise a gamepad name for use as a localStorage key suffix. */
function sanitiseName(name: string): string {
    return name.replace(/[^a-zA-Z0-9_-]/g, "_").substring(0, 80);
}

// ──────────────────────────────────────────────────────────────────────────────
// Module state
// ──────────────────────────────────────────────────────────────────────────────

interface GamepadState {
    /** The sanitised name of the currently active controller (for storage). */
    activeControllerKey: string;
    /** Active gamepad profile (resolved per-controller, possibly user-customised). */
    activeProfile: GamepadProfile | null;
    /** The gamepad index we're currently tracking (-1 = none). */
    activeGamepadIndex: number;
    /** Previous button states for edge detection (true = was pressed). */
    prevButtons: boolean[];
    /** Previous direction from walk stick (for change detection). */
    prevWalkDir: number;
    /** Previous direction from fire stick. */
    prevFireDir: number;
    /** Whether we were running last frame (with hysteresis). */
    prevRunning: boolean;
    /** Whether we were firing (via fire stick) last frame. */
    prevFiring: boolean;
    /** Animation frame handle for the polling loop. */
    pollHandle: number | null;
    /** Timer handle for the pending walk command. */
    walkDelayTimer: ReturnType<typeof setTimeout> | null;
    /** Direction of the pending walk (set when timer fires or promoted to run). */
    walkDelayDir: number;
    /**
     * When we send `run_stop`, we record the ncom sequence number here.
     * We suppress any walk commands from the gamepad until the server has
     * acknowledged that `run_stop` via a comc response, preventing an extra
     * walk step that would otherwise be triggered while the stick passes
     * through the walk zone on its way back to center.
     */
    runStopSeq: number;
    /** Registered UI callbacks. */
    callbacks: GamepadCallbacks | null;
    /** Previous walk-stick direction used during hotbar radial-select mode. */
    prevHotbarDir: number;
    /**
     * Set to true when the hotbar gamepad mode exits while the walk stick is
     * still deflected.  Suppresses walk/run until the stick returns to neutral
     * to prevent an unintentional step immediately after a hotbar activation.
     */
    needsWalkReset: boolean;
    /** Global button mappings (shared across characters). */
    globalButtons: ButtonMapping[];
    /** Character-specific button mappings for the current character. */
    charButtons: ButtonMapping[];
    /** The name of the currently logged-in character (empty = none). */
    currentCharName: string;
}

const gamepadState: GamepadState = {
    activeControllerKey: "",
    activeProfile: null,
    activeGamepadIndex: -1,
    prevButtons: [],
    prevWalkDir: 0,
    prevFireDir: 0,
    prevRunning: false,
    prevFiring: false,
    pollHandle: null,
    walkDelayTimer: null,
    walkDelayDir: 0,
    runStopSeq: -1,
    callbacks: null,
    prevHotbarDir: 0,
    needsWalkReset: false,
    globalButtons: [],
    charButtons: [],
    currentCharName: "",
};

interface SavedGamepadConfig {
    walkStick: StickAxes;
    fireStick: StickAxes;
    walkThreshold: number;
    runThreshold: number;
    fireThreshold: number;
    buttons: ButtonMapping[];
}

/** Saved format for per-character button overrides. */
type SavedCharButtons = ButtonMapping[];

/**
 * Rebuild `activeProfile.buttons` as the de-duplicated merged list used at
 * runtime.  Character-specific buttons take priority over global ones for the
 * same button number.
 */
function rebuildProfileButtons(): void {
    if (!gamepadState.activeProfile) return;
    const seen = new Set<number>();
    const merged: ButtonMapping[] = [];

    for (const b of gamepadState.charButtons) {
        seen.add(b.button);
        merged.push({ ...b, global: false });
    }
    for (const b of gamepadState.globalButtons) {
        if (!seen.has(b.button)) {
            seen.add(b.button);
            merged.push({ ...b, global: true });
        }
    }
    gamepadState.activeProfile.buttons = merged;
}

// ──────────────────────────────────────────────────────────────────────────────
// Callbacks (like keys.ts KeyCallbacks)
// ──────────────────────────────────────────────────────────────────────────────

export interface GamepadCallbacks {
    drawInfo: (message: string) => void;
}

export function setGamepadCallbacks(c: GamepadCallbacks): void {
    gamepadState.callbacks = c;
}

// ──────────────────────────────────────────────────────────────────────────────
// Persistence – per-controller
// ──────────────────────────────────────────────────────────────────────────────

function saveGamepadConfig(): void {
    if (!gamepadState.activeProfile || !gamepadState.activeControllerKey) return;
    // Save global config (stick settings + global buttons).
    const globalData: SavedGamepadConfig = {
        walkStick: gamepadState.activeProfile.walkStick,
        fireStick: gamepadState.activeProfile.fireStick,
        walkThreshold: gamepadState.activeProfile.walkThreshold,
        runThreshold: gamepadState.activeProfile.runThreshold,
        fireThreshold: gamepadState.activeProfile.fireThreshold,
        buttons: gamepadState.globalButtons,
    };
    saveConfig(STORAGE_KEY_PREFIX + gamepadState.activeControllerKey, globalData);

    // Save character-specific buttons separately.
    if (gamepadState.currentCharName) {
        saveConfig(
            charButtonStorageKey(gamepadState.activeControllerKey, gamepadState.currentCharName),
            gamepadState.charButtons,
        );
    }
}

function loadSavedConfig(controllerKey: string): SavedGamepadConfig | null {
    return loadConfig<SavedGamepadConfig | null>(
        STORAGE_KEY_PREFIX + controllerKey, null);
}

function charButtonStorageKey(controllerKey: string, charName: string): string {
    return CHAR_BUTTON_KEY_PREFIX + controllerKey + "_" + sanitiseName(charName);
}

function loadCharButtons(controllerKey: string, charName: string): SavedCharButtons {
    return loadConfig<SavedCharButtons>(charButtonStorageKey(controllerKey, charName), []);
}

// ──────────────────────────────────────────────────────────────────────────────
// Gamepad connection management
// ──────────────────────────────────────────────────────────────────────────────

function onGamepadConnected(e: GamepadEvent): void {
    const gp = e.gamepad;
    LOG(LogLevel.Info, "gamepad::connected",
        `Gamepad connected: "${gp.id}" (index ${gp.index}, ` +
        `${gp.buttons.length} buttons, ${gp.axes.length} axes)`);

    if (gamepadState.activeGamepadIndex >= 0) {
        // Already tracking a gamepad; ignore additional ones.
        return;
    }

    gamepadState.activeGamepadIndex = gp.index;
    gamepadState.activeControllerKey = sanitiseName(gp.id);

    // Try to load saved config for this controller, otherwise use a
    // matching built-in profile.
    const saved = loadSavedConfig(gamepadState.activeControllerKey);
    const profile = findProfileForGamepad(gp.id);
    if (saved) {
        profile.walkStick = saved.walkStick;
        profile.fireStick = saved.fireStick;
        profile.walkThreshold = saved.walkThreshold;
        profile.runThreshold = saved.runThreshold;
        profile.fireThreshold = saved.fireThreshold;
        // Global buttons come from the saved config.
        gamepadState.globalButtons = saved.buttons.map(b => ({ ...b, global: true }));
    } else {
        // Use built-in profile buttons as global defaults.
        gamepadState.globalButtons = profile.buttons.map(b => ({ ...b, global: true }));
    }

    // Load character-specific buttons if a character is already known.
    if (gamepadState.currentCharName) {
        gamepadState.charButtons = loadCharButtons(
            gamepadState.activeControllerKey, gamepadState.currentCharName);
    } else {
        gamepadState.charButtons = [];
    }

    gamepadState.activeProfile = profile;
    rebuildProfileButtons();

    gamepadState.prevButtons = new Array(gp.buttons.length).fill(false);
    gamepadState.prevWalkDir = 0;
    gamepadState.prevFireDir = 0;
    gamepadState.prevRunning = false;
    gamepadState.prevFiring = false;

    gamepadState.callbacks?.drawInfo(`Gamepad connected: ${profile.name}`);
    startPolling();
}

function onGamepadDisconnected(e: GamepadEvent): void {
    const gp = e.gamepad;
    LOG(LogLevel.Info, "gamepad::disconnected",
        `Gamepad disconnected: "${gp.id}" (index ${gp.index})`);

    if (gp.index === gamepadState.activeGamepadIndex) {
        gamepadState.activeGamepadIndex = -1;
        gamepadState.activeControllerKey = "";
        gamepadState.activeProfile = null;
        gamepadState.prevButtons = [];
        stopPolling();

        // Clean up any in-progress fire/run.
        clearFire();
        clearRun();

        gamepadState.callbacks?.drawInfo("Gamepad disconnected.");
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Polling loop
// ──────────────────────────────────────────────────────────────────────────────

function startPolling(): void {
    if (gamepadState.pollHandle !== null) return;
    gamepadState.pollHandle = requestAnimationFrame(pollGamepad);
}

function stopPolling(): void {
    if (gamepadState.pollHandle !== null) {
        cancelAnimationFrame(gamepadState.pollHandle);
        gamepadState.pollHandle = null;
    }
}

function pollGamepad(): void {
    gamepadState.pollHandle = requestAnimationFrame(pollGamepad);

    if (gamepadState.activeGamepadIndex < 0 || !gamepadState.activeProfile) return;

    const gamepads = navigator.getGamepads();
    const gp = gamepads[gamepadState.activeGamepadIndex];
    if (!gp) return;

    // ── Axis-configuration mode ─────────────────────────────────────
    if (isAxisConfigActive()) {
        doHandleAxisConfig(gp, gamepadState.prevButtons, gamepadState.activeProfile, saveGamepadConfig);
        return;
    }

    // ── Button-configuration mode ───────────────────────────────────
    if (isButtonConfigActive()) {
        doHandleButtonConfig(gp, gamepadState.prevButtons);
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
    if (!gamepadState.activeProfile) return;

    // While the hotbar radial-select is active, walk-stick movement is used
    // to highlight slots rather than move the player.
    if (isHotbarGamepadMode()) {
        const ws = gamepadState.activeProfile.walkStick;
        const wxRaw = gp.axes[ws.axisX] ?? 0;
        const wyRaw = gp.axes[ws.axisY] ?? 0;
        const wx = ws.invertX ? -wxRaw : wxRaw;
        const wy = ws.invertY ? -wyRaw : wyRaw;
        // Use the walk threshold as dead-zone; directions 1–8 map to slots.
        const dir = stickToDirection(wx, wy, gamepadState.activeProfile.walkThreshold, gamepadState.prevHotbarDir);
        gamepadState.prevHotbarDir = dir;
        // Slot mapping: NW(8)→0, N(1)→1, NE(2)→2, …, W(7)→7  (dir % 8)
        setHotbarGamepadHighlight(dir > 0 ? dir % 8 : -1);
        return;
    }
    gamepadState.prevHotbarDir = 0;

    const ws = gamepadState.activeProfile.walkStick;
    const fs = gamepadState.activeProfile.fireStick;

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
    if (fireMag >= gamepadState.activeProfile.fireThreshold) {
        const dir = stickToDirection(fx, fy, gamepadState.activeProfile.fireThreshold, gamepadState.prevFireDir);
        if (dir > 0) {
            if (dir !== gamepadState.prevFireDir || !gamepadState.prevFiring) {
                clear_move_to();
                fireDir(dir);
            }
            gamepadState.prevFireDir = dir;
            gamepadState.prevFiring = true;
        } else if (gamepadState.prevFiring) {
            clearFire();
            gamepadState.prevFiring = false;
            gamepadState.prevFireDir = 0;
        }
    } else if (gamepadState.prevFiring) {
        clearFire();
        gamepadState.prevFiring = false;
        gamepadState.prevFireDir = 0;
    }

    // ── Walk / run stick (with hysteresis, walk delay, run-stop guard) ──
    const runEnter = gamepadState.activeProfile.runThreshold;
    const runExit = Math.max(gamepadState.activeProfile.walkThreshold,
                             gamepadState.activeProfile.runThreshold - HYSTERESIS);
    const isRunning = gamepadState.prevRunning
        ? walkMag >= runExit    // already running: use lower exit threshold
        : walkMag >= runEnter;  // not running: use full enter threshold
    const isWalking = walkMag >= gamepadState.activeProfile.walkThreshold;

    // ── Post-hotbar neutral-zone guard ──────────────────────────────────────
    // After exiting hotbar mode the stick may still be deflected.  Suppress
    // walk/run until the stick drops below the walk threshold to avoid an
    // unintentional step.
    if (gamepadState.needsWalkReset) {
        if (!isWalking) {
            gamepadState.needsWalkReset = false;
        } else {
            return;
        }
    }

    // ── Run-stop guard: check if the pending run_stop has been acked ──
    if (gamepadState.runStopSeq !== -1) {
        if (isNcomAcked(gamepadState.runStopSeq)) {
            // Server acknowledged the run_stop; we can process walk again.
            gamepadState.runStopSeq = -1;
        } else if (isWalking && !isRunning) {
            // Still waiting for run_stop ack and stick is in walk zone —
            // suppress walk to avoid an extra step.  We intentionally
            // skip all state updates so the next frame re-evaluates
            // from scratch once the ack arrives.
            return;
        }
    }

    if (isWalking) {
        const dir = stickToDirection(wx, wy, gamepadState.activeProfile.walkThreshold, gamepadState.prevWalkDir);
        if (dir > 0) {
            if (isRunning) {
                // Cancel any pending walk delay — we're running now.
                cancelWalkDelay();
                if (dir !== gamepadState.prevWalkDir || !gamepadState.prevRunning) {
                    if (gamepadState.prevRunning) {
                        clearRun();
                    }
                    clear_move_to();
                    runDir(dir);
                }
                gamepadState.prevRunning = true;
                gamepadState.prevWalkDir = dir;
            } else {
                // Walking speed.
                if (gamepadState.prevRunning) {
                    clearRun();
                    // Record the run_stop sequence for the guard.
                    gamepadState.runStopSeq = getLastNcomSeqSent();
                    gamepadState.prevRunning = false;
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
        if (gamepadState.prevRunning) {
            clearRun();
            gamepadState.runStopSeq = getLastNcomSeqSent();
            gamepadState.prevRunning = false;
        }
        gamepadState.prevWalkDir = 0;
    }
}

// ── Walk delay helpers ──────────────────────────────────────────────────────

/**
 * Schedule a walk command after WALK_DELAY_MS.  If a walk is already
 * pending for the same direction, do nothing (avoid re-scheduling).
 * If the direction changed, replace the pending walk.
 */
function scheduleWalk(dir: number): void {
    if (gamepadState.walkDelayTimer !== null && gamepadState.walkDelayDir === dir) {
        // Already pending for this direction.
        return;
    }
    cancelWalkDelay();
    gamepadState.walkDelayDir = dir;
    gamepadState.walkDelayTimer = setTimeout(() => {
        gamepadState.walkDelayTimer = null;
        // Re-check: if the stick has since reached run threshold, the
        // run branch in processSticks will have cancelled us already,
        // but just in case, verify we're still in walk-only state.
        if (!gamepadState.prevRunning && gamepadState.walkDelayDir > 0) {
            if (gamepadState.walkDelayDir !== gamepadState.prevWalkDir || gamepadState.prevWalkDir === 0) {
                clear_move_to();
                walkDir(gamepadState.walkDelayDir);
            }
            gamepadState.prevWalkDir = gamepadState.walkDelayDir;
        }
        gamepadState.walkDelayDir = 0;
    }, WALK_DELAY_MS);
}

function cancelWalkDelay(): void {
    if (gamepadState.walkDelayTimer !== null) {
        clearTimeout(gamepadState.walkDelayTimer);
        gamepadState.walkDelayTimer = null;
    }
    gamepadState.walkDelayDir = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Button processing
// ──────────────────────────────────────────────────────────────────────────────

function processButtons(gp: Gamepad): void {
    if (!gamepadState.activeProfile) return;

    for (const mapping of gamepadState.activeProfile.buttons) {
        if (mapping.button >= gp.buttons.length) continue;

        const pressed = gp.buttons[mapping.button]!.pressed;
        const wasPressed = gamepadState.prevButtons[mapping.button] ?? false;

        // The special command "hotbar" drives radial slot-selection mode.
        if (mapping.command === "hotbar") {
            if (pressed && !wasPressed) {
                // Rising edge: enter hotbar radial-select mode.
                enterHotbarGamepadMode();
            } else if (!pressed && wasPressed) {
                // Falling edge: activate the highlighted slot (or cancel).
                exitHotbarGamepadMode(getHotbarGamepadHighlight());
                gamepadState.prevHotbarDir = 0;
                // Guard against the walk stick still being deflected after
                // slot selection — require neutral before allowing walk/run.
                gamepadState.needsWalkReset = true;
            }
            // While held, processSticks() handles direction updates.
            continue;
        }

        // Normal button: fire command on rising edge only.
        if (pressed && !wasPressed) {
            extendedCommand(mapping.command);
        }
    }

    // Update previous state for all buttons.
    for (let i = 0; i < gp.buttons.length; i++) {
        gamepadState.prevButtons[i] = gp.buttons[i]!.pressed;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Axis & button configuration (delegated to gamepad_config.ts)
// ──────────────────────────────────────────────────────────────────────────────

export { startAxisConfig, cancelAxisConfig } from "./gamepad_config";
export { startButtonConfig, cancelButtonConfig } from "./gamepad_config";

export function acceptAxisConfig(): void {
    doAcceptAxisConfig(gamepadState.activeProfile, saveGamepadConfig);
}

export function getAxisTestDirection(): number {
    return doGetAxisTestDirection(gamepadState.activeGamepadIndex, stickToDirection);
}

// ──────────────────────────────────────────────────────────────────────────────
// Public API – binding management
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Set or replace the command for a specific button.
 * New bindings are always character-specific (global = false) by default.
 */
export function setButtonCommand(button: number, command: string): void {
    if (!gamepadState.activeProfile) return;

    // Add/replace in charButtons (character-specific by default).
    const existingChar = gamepadState.charButtons.find(b => b.button === button);
    if (existingChar) {
        existingChar.command = command;
    } else {
        // Also remove from globalButtons if overriding a global binding.
        gamepadState.globalButtons = gamepadState.globalButtons.filter(b => b.button !== button);
        gamepadState.charButtons.push({ button, command, global: false });
    }
    rebuildProfileButtons();
    saveGamepadConfig();
}

/** Remove the command mapping for a specific button from both global and char lists. */
export function removeButtonCommand(button: number): void {
    if (!gamepadState.activeProfile) return;
    gamepadState.globalButtons = gamepadState.globalButtons.filter(b => b.button !== button);
    gamepadState.charButtons = gamepadState.charButtons.filter(b => b.button !== button);
    rebuildProfileButtons();
    saveGamepadConfig();
}

/** Get the current button mappings (merged, with global flag). */
export function getButtonMappings(): readonly ButtonMapping[] {
    return gamepadState.activeProfile?.buttons ?? [];
}

/** Get the current stick configuration. */
export function getStickConfig(): {
    walk: StickAxes; fire: StickAxes } | null {
    if (!gamepadState.activeProfile) return null;
    return {
        walk: { ...gamepadState.activeProfile.walkStick },
        fire: { ...gamepadState.activeProfile.fireStick },
    };
}

/** Reset gamepad bindings to defaults for the current controller. */
export function resetGamepadBindings(): void {
    if (gamepadState.activeGamepadIndex < 0) return;
    const gamepads = navigator.getGamepads();
    const gp = gamepads[gamepadState.activeGamepadIndex];
    if (!gp) return;

    const defaultProfile = findProfileForGamepad(gp.id);
    gamepadState.activeProfile = defaultProfile;
    gamepadState.globalButtons = defaultProfile.buttons.map(b => ({ ...b, global: true }));
    gamepadState.charButtons = [];
    rebuildProfileButtons();
    saveGamepadConfig();
    gamepadState.callbacks?.drawInfo("Gamepad bindings reset to defaults.");
}

/**
 * Called when a character logs in.  Loads character-specific button bindings
 * and rebuilds the merged profile.
 */
export function setCurrentCharacter(charName: string): void {
    gamepadState.currentCharName = charName;
    if (gamepadState.activeControllerKey && charName) {
        gamepadState.charButtons = loadCharButtons(
            gamepadState.activeControllerKey, charName);
        rebuildProfileButtons();
    } else {
        gamepadState.charButtons = [];
        rebuildProfileButtons();
    }
}

/**
 * Change the global/character scope of one or more button bindings, then
 * persist.  Each entry identifies a button by number and the desired scope.
 */
export function saveGamepadButtonScopes(
    changes: Array<{ button: number; newGlobal: boolean }>,
): void {
    for (const { button, newGlobal } of changes) {
        // Find in whichever list currently owns it.
        const inChar = gamepadState.charButtons.findIndex(b => b.button === button);
        const inGlobal = gamepadState.globalButtons.findIndex(b => b.button === button);

        if (newGlobal && inChar >= 0) {
            const b = gamepadState.charButtons.splice(inChar, 1)[0]!;
            gamepadState.globalButtons.push({ ...b, global: true });
        } else if (!newGlobal && inGlobal >= 0) {
            const b = gamepadState.globalButtons.splice(inGlobal, 1)[0]!;
            gamepadState.charButtons.push({ ...b, global: false });
        }
    }
    rebuildProfileButtons();
    saveGamepadConfig();
}

/** Check whether a gamepad is currently connected and active. */
export function isGamepadConnected(): boolean {
    return gamepadState.activeGamepadIndex >= 0 && gamepadState.activeProfile !== null;
}

/** Get the name of the active gamepad profile. */
export function getActiveProfileName(): string {
    return gamepadState.activeProfile?.name ?? "(none)";
}

/** Find the existing command for a given button, or null. */
export function getButtonCommand(button: number): string | null {
    return gamepadState.activeProfile?.buttons.find(b => b.button === button)?.command
        ?? null;
}

// ──────────────────────────────────────────────────────────────────────────────
// HP vibration (delegated to gamepad_vibration.ts)
// ──────────────────────────────────────────────────────────────────────────────

export function notifyHpUpdate(hp: number, maxHp: number): void {
    doNotifyHpUpdate(hp, maxHp, gamepadState.activeGamepadIndex);
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

    if (gamepadState.prevRunning) clearRun();
    if (gamepadState.prevFiring) clearFire();

    gamepadState.activeGamepadIndex = -1;
    gamepadState.activeControllerKey = "";
    gamepadState.activeProfile = null;
    gamepadState.prevButtons = [];
    gamepadState.prevWalkDir = 0;
    gamepadState.prevFireDir = 0;
    gamepadState.prevRunning = false;
    gamepadState.prevFiring = false;
    gamepadState.runStopSeq = -1;
    gamepadState.prevHotbarDir = 0;
    gamepadState.needsWalkReset = false;
    gamepadState.globalButtons = [];
    gamepadState.charButtons = [];
    gamepadState.currentCharName = "";
}
