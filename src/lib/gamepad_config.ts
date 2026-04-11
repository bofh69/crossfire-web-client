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

let axisConfigActive = false;
let axisConfigTarget: AxisConfigTarget = "walk";
let axisConfigStep: AxisConfigStep = "move-north";
let axisConfigCallback: ((axes: StickAxes | null) => void) | null = null;
let axisConfigStepCallback: ((step: AxisConfigStep) => void) | null = null;
let axisConfigWaitingForCenter = false;
let axisNorthIdx: number | null = null;
let axisEastIdx: number | null = null;
let axisConfigPending: StickAxes | null = null;

let buttonConfigActive = false;
let buttonConfigCallback: ((button: number) => void) | null = null;

// ── Axis configuration public API ───────────────────────────────────────────

export function isAxisConfigActive(): boolean {
    return axisConfigActive;
}

export function isButtonConfigActive(): boolean {
    return buttonConfigActive;
}

/**
 * Start axis-configuration mode.
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
export function acceptAxisConfig(
    activeProfile: GamepadProfile | null,
    saveGamepadConfig: () => void,
): void {
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
export function getAxisTestDirection(
    activeGamepadIndex: number,
    stickToDirection: (x: number, y: number, threshold: number, prevDir: number) => number,
): number {
    if (!axisConfigActive || axisConfigStep !== "testing") return 0;
    if (!axisConfigPending) return 0;

    if (activeGamepadIndex < 0) return 0;
    const gamepads = navigator.getGamepads();
    const gp = gamepads[activeGamepadIndex];
    if (!gp) return 0;

    const x = gp.axes[axisConfigPending.axisX] ?? 0;
    const y = gp.axes[axisConfigPending.axisY] ?? 0;
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
    if (axisConfigWaitingForCenter) {
        let allCentered = true;
        for (let i = 0; i < gp.axes.length; i++) {
            if (Math.abs(gp.axes[i]!) > AXIS_CONFIG_CENTER_THRESHOLD) {
                allCentered = false;
                break;
            }
        }
        if (!allCentered) return;
        axisConfigWaitingForCenter = false;
    }

    switch (axisConfigStep) {
        case "move-north":
            handleAxisStep(gp, (idx, _value) => {
                axisNorthIdx = idx;
                advanceAxisStep("move-east");
            });
            break;

        case "move-east":
            handleAxisStep(gp, (idx, _value) => {
                axisEastIdx = idx;
                advanceAxisStep("move-south");
            });
            break;

        case "move-south":
            handleAxisStep(gp, (idx, _value) => {
                if (idx !== axisNorthIdx) {
                    axisNorthIdx = idx;
                }
                advanceAxisStep("move-west");
            });
            break;

        case "move-west":
            handleAxisStep(gp, (idx, _value) => {
                if (idx !== axisEastIdx) {
                    axisEastIdx = idx;
                }
                axisConfigPending = {
                    axisX: axisEastIdx!,
                    axisY: axisNorthIdx!,
                };
                axisConfigStep = "testing";
                axisConfigStepCallback?.("testing");
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
    axisConfigWaitingForCenter = true;
    axisConfigStep = nextStep;
    axisConfigStepCallback?.(nextStep);
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
    buttonConfigActive = true;
    buttonConfigCallback = onDone;
}

/** Cancel a running button configuration. */
export function cancelButtonConfig(): void {
    buttonConfigActive = false;
    buttonConfigCallback = null;
}

/**
 * Process button configuration during the polling loop.
 */
export function handleButtonConfig(gp: Gamepad, prevButtons: boolean[]): void {
    for (let i = 0; i < gp.buttons.length; i++) {
        if (gp.buttons[i]!.pressed && !(prevButtons[i] ?? false)) {
            const doneCb = buttonConfigCallback;
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
