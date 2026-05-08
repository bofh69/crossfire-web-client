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
  /** Optional face number used to show a sprite icon.  Not persisted — refreshed each session. */
  face?: number;
  /**
   * Item tag used to apply the item; not persisted since tags change between
   * logins.  Refreshed each session by matching on `itemName`.
   */
  tag?: number;
  /**
   * Singular item name used to re-match the item across logins (tags are
   * session-specific).  Persisted to localStorage.
   */
  itemName?: string;
}

// ── Module state ─────────────────────────────────────────────────────────────

const HOTBAR_CHAR_STORAGE_KEY_PREFIX = "hotbar_slots_char_";
const HOTBAR_SLOT_COUNT = 12;

/** Sanitise a character name for use as a localStorage key segment. */
function sanitiseName(name: string): string {
  return name.replace(/[^a-zA-Z0-9_-]/g, "_").substring(0, 80);
}

/** The 12 hotbar slots.  null = empty. */
let slots: (HotbarSlot | null)[] = Array(HOTBAR_SLOT_COUNT).fill(null);

/** The name of the currently logged-in character (empty = no character). */
let currentCharName = "";

// ── Gamepad radial-select state ──────────────────────────────────────────────

/** Whether the gamepad hotbar-select mode is currently active. */
let gamepadModeActive = false;
/** The slot index (0–11) currently highlighted by the gamepad stick; -1 = none. */
let gamepadHighlight = -1;

// ── Persistence ──────────────────────────────────────────────────────────────

/** Strip session-only fields (`face`, `tag`) from a slot before persisting to localStorage. */
function stripSessionData(slot: HotbarSlot): Omit<HotbarSlot, "face" | "tag"> {
  const { label, command, itemName } = slot;
  return itemName !== undefined
    ? { label, command, itemName }
    : { label, command };
}

/** Load hotbar slots for the given character name from localStorage. */
function loadHotbarForChar(charName: string): void {
  const key = HOTBAR_CHAR_STORAGE_KEY_PREFIX + sanitiseName(charName);
  const saved = loadConfig<(HotbarSlot | null)[]>(key, []);
  slots = Array(HOTBAR_SLOT_COUNT).fill(null);
  let loaded = 0;
  for (let i = 0; i < Math.min(saved.length, HOTBAR_SLOT_COUNT); i++) {
    const slot = saved[i] ?? null;
    if (slot !== null) {
      // face and tag are session-specific and must not be loaded from
      // config; they are refreshed when addspell / item2 commands arrive.
      slots[i] = stripSessionData(slot);
      loaded++;
    }
  }
}

/**
 * Initialise the hotbar module.  Slots start empty until a character logs in
 * and setCurrentCharacter() is called.
 */
export function loadHotbar(): void {
  slots = Array(HOTBAR_SLOT_COUNT).fill(null);
  currentCharName = "";
}

/**
 * Reset the current-character guard so that the next setCurrentCharacter()
 * call always reloads slots from storage even if the character name is the
 * same as last session.  Call this at the start of each new player login
 * (i.e. from PlayerCmd) before emitting playerUpdate.
 */
export function resetHotbarSession(): void {
  currentCharName = "";
}

function saveHotbar(): void {
  if (!currentCharName) return;
  const key = HOTBAR_CHAR_STORAGE_KEY_PREFIX + sanitiseName(currentCharName);
  // Strip face before saving — face numbers are session-specific and are
  // refreshed from addspell / item2 commands on each login.
  const toSave = slots.map((slot) =>
    slot !== null ? stripSessionData(slot) : null,
  );
  saveConfig(key, toSave);
}

/**
 * Called when a character logs in.  Loads character-specific hotbar slots and
 * emits a hotbarUpdate event so the UI refreshes.
 * Skips the reload if the character name is unchanged (prevents repeated
 * playerUpdate events from Item2Cmd etc. from wiping in-memory face/tag values).
 */
export function setCurrentCharacter(charName: string): void {
  if (charName === currentCharName) {
    return;
  }
  currentCharName = charName;
  loadHotbarForChar(charName);
  gameEvents.emit("hotbarUpdate");
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
  gameEvents.emit("hotbarUpdate");
}

/** Clear a slot. */
export function clearHotbarSlot(index: number): void {
  if (index < 0 || index >= HOTBAR_SLOT_COUNT) return;
  slots[index] = null;
  saveHotbar();
  gameEvents.emit("hotbarUpdate");
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
  gameEvents.emit("hotbarUpdate");
}

/**
 * Exit gamepad hotbar-select mode (called on the falling edge of the button).
 * If `slotIndex` ≥ 0 the corresponding slot is activated.
 */
export function exitHotbarGamepadMode(slotIndex: number): void {
  gamepadModeActive = false;
  gamepadHighlight = -1;
  gameEvents.emit("hotbarUpdate");
  if (slotIndex >= 0) {
    activateHotbarSlot(slotIndex);
  }
}

/** Update which slot the stick is pointing at (-1 = neutral / none). */
export function setHotbarGamepadHighlight(slot: number): void {
  if (gamepadHighlight === slot) return;
  gamepadHighlight = slot;
  gameEvents.emit("hotbarUpdate");
}

/** Return the currently highlighted slot index (-1 = none). */
export function getHotbarGamepadHighlight(): number {
  return gamepadHighlight;
}

/** True while the gamepad hotbar-select button is held. */
export function isHotbarGamepadMode(): boolean {
  return gamepadModeActive;
}

/**
 * Update the in-memory face for every hotbar slot whose command is
 * `cast <name>`.  Returns true if any slot was changed.
 * Called when an addspell packet arrives so the hotbar shows the correct
 * sprite for the current session without persisting the face number.
 *
 * Slots are replaced with new objects (not mutated in-place) so that
 * Svelte's fine-grained reactivity detects the change in the {#each} block.
 */
export function updateHotbarFacesFromSpell(
  name: string,
  face: number,
): boolean {
  const command = `cast ${name}`;
  let changed = false;
  for (let i = 0; i < slots.length; i++) {
    const slot = slots[i];
    if (slot && slot.command === command) {
      if (slot.face !== face) {
        slots[i] = { ...slot, face };
        changed = true;
      }
    }
  }
  return changed;
}

/**
 * Update the in-memory tag and face for every hotbar slot whose `itemName`
 * matches `sName`.  Returns true if any slot was changed.
 * Called when an item2 packet arrives so the hotbar shows the correct
 * sprite and uses the correct (session-specific) tag without persisting
 * either value.
 *
 * Slots are replaced with new objects (not mutated in-place) so that
 * Svelte's fine-grained reactivity detects the change in the {#each} block.
 */
export function updateHotbarSlotFromItem(
  sName: string,
  tag: number,
  face: number,
): boolean {
  let changed = false;
  for (let i = 0; i < slots.length; i++) {
    const slot = slots[i];
    if (slot && slot.itemName === sName) {
      if (slot.tag !== tag || slot.face !== face) {
        slots[i] = { ...slot, tag, face };
        changed = true;
      }
    }
  }
  return changed;
}
