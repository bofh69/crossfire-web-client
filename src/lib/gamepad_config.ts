/**
 * gamepad_config.ts — Axis and button configuration state machines.
 * Extracted from gamepad.ts.
 *
 * This module contains the interactive configuration flows for:
 * - Axis calibration (multi-step: N/E/S/W + test mode)
 * - Button capture (single-press detection)
 */

import type { StickAxes, GamepadProfile } from "./gamepad_defaults";

// ── Types ───────────────────────────────────────────────────────────────────

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

// ── Constants ───────────────────────────────────────────────────────────────

/** Axis threshold used during axis-configuration to detect stick movement. */
const AXIS_CONFIG_THRESHOLD = 0.7;

/**
 * After capturing a direction, all axes must drop below this value
 * before the next direction is accepted.
 */
const AXIS_CONFIG_CENTER_THRESHOLD = 0.3;

// ── State ───────────────────────────────────────────────────────────────────

interface AxisConfigState {
    active: boolean;
    target: AxisConfigTarget;
    step: AxisConfigStep;
    callback: ((axes: StickAxes | null) => void) | null;
    stepCallback: ((step: AxisConfigStep) => void) | null;
    waitingForCenter: boolean;
    northIdx: number | null;
    eastIdx: number | null;
    yInverted: boolean;
    xInverted: boolean;
    pending: StickAxes | null;
}

interface ButtonConfigState {
    active: boolean;
    callback: ((button: number) => void) | null;
}

const axisConfig: AxisConfigState = {
    active: false,
    target: "walk",
    step: "move-north",
    callback: null,
    stepCallback: null,
    waitingForCenter: false,
    northIdx: null,
    eastIdx: null,
    yInverted: false,
    xInverted: false,
    pending: null,
};

const buttonConfig: ButtonConfigState = {
    active: false,
    callback: null,
};

// ── Axis configuration public API ───────────────────────────────────────────

export function isAxisConfigActive(): boolean {
    return axisConfig.active;
}

export function isButtonConfigActive(): boolean {
    return buttonConfig.active;
}

/**
 * Start axis-configuration mode.
 */
export function startAxisConfig(
    target: AxisConfigTarget,
    onStepChange: (step: AxisConfigStep) => void,
    onDone: (axes: StickAxes | null) => void,
): void {
    axisConfig.active = true;
    axisConfig.target = target;
    axisConfig.step = "move-north";
    axisConfig.stepCallback = onStepChange;
    axisConfig.callback = onDone;
    axisConfig.waitingForCenter = false;
    axisConfig.northIdx = null;
    axisConfig.eastIdx = null;
    axisConfig.yInverted = false;
    axisConfig.xInverted = false;
    axisConfig.pending = null;
}

/** Cancel a running axis configuration. */
export function cancelAxisConfig(): void {
    axisConfig.active = false;
    axisConfig.waitingForCenter = false;
    axisConfig.callback = null;
    axisConfig.stepCallback = null;
    axisConfig.northIdx = null;
    axisConfig.eastIdx = null;
    axisConfig.yInverted = false;
    axisConfig.xInverted = false;
    axisConfig.pending = null;
}

/** Accept the current axis configuration (called from test mode). */
export function acceptAxisConfig(
    activeProfile: GamepadProfile | null,
    saveGamepadConfig: () => void,
): void {
    if (!axisConfig.active || axisConfig.step !== "testing") return;
    if (axisConfig.pending && activeProfile) {
        if (axisConfig.target === "walk") {
            activeProfile.walkStick = axisConfig.pending;
        } else {
            activeProfile.fireStick = axisConfig.pending;
        }
        saveGamepadConfig();
    }
    const doneCb = axisConfig.callback;
    const result = axisConfig.pending;
    cancelAxisConfig();
    doneCb?.(result);
}

/**
 * Get the live stick direction during axis-config test mode.
 * Returns the direction index (0–8) or 0 if not in test mode.
 */
export function getAxisTestDirection(
    activeGamepadIndex: number,
    stickToDirection: (x: number, y: number, threshold: number, prevDir: number) => number,
): number {
    if (!axisConfig.active || axisConfig.step !== "testing") return 0;
    if (!axisConfig.pending) return 0;

    if (activeGamepadIndex < 0) return 0;
    const gamepads = navigator.getGamepads();
    const gp = gamepads[activeGamepadIndex];
    if (!gp) return 0;

    const rawX = gp.axes[axisConfig.pending.axisX] ?? 0;
    const rawY = gp.axes[axisConfig.pending.axisY] ?? 0;
    const x = axisConfig.pending.invertX ? -rawX : rawX;
    const y = axisConfig.pending.invertY ? -rawY : rawY;
    return stickToDirection(x, y, 0.2, 0);
}

/**
 * Process axis configuration during the polling loop.
 */
export function handleAxisConfig(
    gp: Gamepad,
    prevButtons: boolean[],
    activeProfile: GamepadProfile | null,
    saveGamepadConfig: () => void,
): void {
    if (axisConfig.waitingForCenter) {
        let allCentered = true;
        for (let i = 0; i < gp.axes.length; i++) {
            if (Math.abs(gp.axes[i]!) > AXIS_CONFIG_CENTER_THRESHOLD) {
                allCentered = false;
                break;
            }
        }
        if (!allCentered) return;
        axisConfig.waitingForCenter = false;
    }

    switch (axisConfig.step) {
        case "move-north":
            handleAxisStep(gp, (idx, value) => {
                axisConfig.northIdx = idx;
                // North should be negative Y (positive = south); if the stick
                // reads positive when pushed north, Y is inverted.
                axisConfig.yInverted = value > 0;
                advanceAxisStep("move-east");
            });
            break;

        case "move-east":
            handleAxisStep(gp, (idx, value) => {
                axisConfig.eastIdx = idx;
                // East should be positive X; if the stick reads negative when
                // pushed east, X is inverted.
                axisConfig.xInverted = value < 0;
                advanceAxisStep("move-south");
            });
            break;

        case "move-south":
            handleAxisStep(gp, (idx, _value) => {
                if (idx !== axisConfig.northIdx) {
                    axisConfig.northIdx = idx;
                }
                advanceAxisStep("move-west");
            });
            break;

        case "move-west":
            handleAxisStep(gp, (idx, _value) => {
                if (idx !== axisConfig.eastIdx) {
                    axisConfig.eastIdx = idx;
                }
                axisConfig.pending = {
                    axisX: axisConfig.eastIdx!,
                    axisY: axisConfig.northIdx!,
                    invertX: axisConfig.xInverted,
                    invertY: axisConfig.yInverted,
                };
                axisConfig.step = "testing";
                axisConfig.stepCallback?.("testing");
            });
            break;

        case "testing":
            for (let i = 0; i < gp.buttons.length; i++) {
                if (gp.buttons[i]!.pressed && !(prevButtons[i] ?? false)) {
                    const mapping = activeProfile?.buttons.find(
                        b => b.button === i);
                    if (mapping?.command === "apply") {
                        acceptAxisConfig(activeProfile, saveGamepadConfig);
                        break;
                    }
                }
            }
            for (let i = 0; i < gp.buttons.length; i++) {
                prevButtons[i] = gp.buttons[i]!.pressed;
            }
            break;
    }
}

function advanceAxisStep(nextStep: AxisConfigStep): void {
    axisConfig.waitingForCenter = true;
    axisConfig.step = nextStep;
    axisConfig.stepCallback?.(nextStep);
}

function handleAxisStep(
    gp: Gamepad,
    onDetected: (axisIndex: number, value: number) => void,
): void {
    for (let i = 0; i < gp.axes.length; i++) {
        const value = gp.axes[i]!;
        if (Math.abs(value) >= AXIS_CONFIG_THRESHOLD) {
            onDetected(i, value);
            return;
        }
    }
}

// ── Button configuration public API ─────────────────────────────────────────

/**
 * Start button-configuration mode.
 */
export function startButtonConfig(onDone: (button: number) => void): void {
    buttonConfig.active = true;
    buttonConfig.callback = onDone;
}

/** Cancel a running button configuration. */
export function cancelButtonConfig(): void {
    buttonConfig.active = false;
    buttonConfig.callback = null;
}

/**
 * Process button configuration during the polling loop.
 */
export function handleButtonConfig(gp: Gamepad, prevButtons: boolean[]): void {
    for (let i = 0; i < gp.buttons.length; i++) {
        if (gp.buttons[i]!.pressed && !(prevButtons[i] ?? false)) {
            const doneCb = buttonConfig.callback;
            cancelButtonConfig();

            for (let j = 0; j < gp.buttons.length; j++) {
                prevButtons[j] = gp.buttons[j]!.pressed;
            }

            doneCb?.(i);
            return;
        }
    }

    for (let j = 0; j < gp.buttons.length; j++) {
        prevButtons[j] = gp.buttons[j]!.pressed;
    }
}
