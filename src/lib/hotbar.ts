/**
 * hotbar.ts — Configurable quick-action hotbar for the Crossfire web client.
 *
 * Provides 12 slots (F1–F12) that can each execute a `cast`, `use_skill`,
 * or `apply` command.  Slots are persisted via localStorage.
 *
 * Also manages the gamepad radial-select mode:  while a "hotbar" button is
 * held the walk-stick selects one of the first 8 slots by direction; releasing
 * the button activates the highlighted slot (or cancels if the stick is neutral).
 */

import { loadConfig, saveConfig } from "./storage";
import { extendedCommand } from "./p_cmd";
import { clientSendApply } from "./player";
import { gameEvents } from "./events";

// ── Types ────────────────────────────────────────────────────────────────────

/** A single hotbar slot.  `command` is sent verbatim via extendedCommand(). */
export interface HotbarSlot {
    /** Short display label shown on the button. */
    label: string;
    /** Command string to execute (e.g. "cast fireball", "use_skill praying"). */
    command: string;
    /** Optional face number used to show a sprite icon. */
    face?: number;
    /** Item tag; when set, activating the slot calls clientSendApply(tag). */
    tag?: number;
}

// ── Module state ─────────────────────────────────────────────────────────────

const HOTBAR_STORAGE_KEY = "hotbar_slots";
const HOTBAR_SLOT_COUNT = 12;

/** The 12 hotbar slots.  null = empty. */
let slots: (HotbarSlot | null)[] = Array(HOTBAR_SLOT_COUNT).fill(null);

// ── Gamepad radial-select state ──────────────────────────────────────────────

/** Whether the gamepad hotbar-select mode is currently active. */
let gamepadModeActive = false;
/** The slot index (0–11) currently highlighted by the gamepad stick; -1 = none. */
let gamepadHighlight = -1;

// ── Persistence ──────────────────────────────────────────────────────────────

/** Load hotbar slots from localStorage.  Call once at startup. */
export function loadHotbar(): void {
    const saved = loadConfig<(HotbarSlot | null)[]>(HOTBAR_STORAGE_KEY, []);
    slots = Array(HOTBAR_SLOT_COUNT).fill(null);
    for (let i = 0; i < Math.min(saved.length, HOTBAR_SLOT_COUNT); i++) {
        slots[i] = saved[i] ?? null;
    }
}

function saveHotbar(): void {
    saveConfig(HOTBAR_STORAGE_KEY, slots);
}

// ── Slot accessors ────────────────────────────────────────────────────────────

/** Return a read-only snapshot of the current slot array. */
export function getHotbarSlots(): readonly (HotbarSlot | null)[] {
    return slots;
}

/** Assign a slot.  index must be 0–11. */
export function setHotbarSlot(index: number, slot: HotbarSlot): void {
    if (index < 0 || index >= HOTBAR_SLOT_COUNT) return;
    slots[index] = slot;
    saveHotbar();
    gameEvents.emit('hotbarUpdate');
}

/** Clear a slot. */
export function clearHotbarSlot(index: number): void {
    if (index < 0 || index >= HOTBAR_SLOT_COUNT) return;
    slots[index] = null;
    saveHotbar();
    gameEvents.emit('hotbarUpdate');
}

/** Execute the command stored in a slot, if any. */
export function activateHotbarSlot(index: number): void {
    if (index < 0 || index >= HOTBAR_SLOT_COUNT) return;
    const slot = slots[index];
    if (slot) {
        if (slot.tag !== undefined) {
            clientSendApply(slot.tag);
        } else {
            extendedCommand(slot.command);
        }
    }
}

// ── Gamepad radial-select API ─────────────────────────────────────────────────

/** Enter gamepad hotbar-select mode (called on the rising edge of the button). */
export function enterHotbarGamepadMode(): void {
    gamepadModeActive = true;
    gamepadHighlight = -1;
    gameEvents.emit('hotbarUpdate');
}

/**
 * Exit gamepad hotbar-select mode (called on the falling edge of the button).
 * If `slotIndex` ≥ 0 the corresponding slot is activated.
 */
export function exitHotbarGamepadMode(slotIndex: number): void {
    gamepadModeActive = false;
    gamepadHighlight = -1;
    gameEvents.emit('hotbarUpdate');
    if (slotIndex >= 0) {
        activateHotbarSlot(slotIndex);
    }
}

/** Update which slot the stick is pointing at (-1 = neutral / none). */
export function setHotbarGamepadHighlight(slot: number): void {
    if (gamepadHighlight === slot) return;
    gamepadHighlight = slot;
    gameEvents.emit('hotbarUpdate');
}

/** Return the currently highlighted slot index (-1 = none). */
export function getHotbarGamepadHighlight(): number {
    return gamepadHighlight;
}

/** True while the gamepad hotbar-select button is held. */
export function isHotbarGamepadMode(): boolean {
    return gamepadModeActive;
}
