/**
 * events.ts — Typed event bus for server→UI communication.
 *
 * Replaces the mutable `callbacks` object that was shared between commands.ts,
 * App.svelte, and Login.svelte.  Components subscribe/unsubscribe cleanly via
 * on()/off() and there is no global mutable state to race on.
 */

import type { Stats } from './protocol.js';

// ── Event payload map ──────────────────────────────────────────────────────

export interface AccountPlayer {
  name: string;
  charClass: string;
  race: string;
  face: string;
  party: string;
  map: string;
  level: number;
  faceNum: number;
}

/** Maps event name → handler signature. */
export interface GameEventMap {
  drawInfo:        [color: number, message: string];
  drawExtInfo:     [color: number, type: number, subtype: number, message: string];
  statsUpdate:     [stats: Partial<Stats>];
  query:           [flags: number, prompt: string];
  newMap:          [];
  mapUpdate:       [];
  playerUpdate:    [];
  spellUpdate:     [];
  pickupUpdate:    [mode: number];
  accountPlayers:  [players: AccountPlayer[]];
  failure:         [command: string, message: string];
  magicMap:        [];
  tick:            [tickNo: number];
  goodbye:         [];
  addMeSuccess:    [];
  addMeFail:       [];
  version:         [csVersion: number, scVersion: number, versionString: string];
  disconnect:      [];
  replyInfo:       [infoType: string, text: string];

  // UI-internal events (component-to-component communication)
  /** Ask the InfoPanel to focus its command input field. */
  focusCommandInput: [prefill?: string];
  /** Ask the MenuBar to start a "bind last command to key" flow. */
  openKeyBind:       [];
  /** Ask the MenuBar to start a "bind last command to gamepad button" flow. */
  openGamepadBind:   [];
}

// ── Event bus implementation ───────────────────────────────────────────────

type Listener<Args extends unknown[]> = (...args: Args) => void;

class GameEventBus {
  private listeners = new Map<string, Set<Listener<any>>>();

  /** Subscribe to an event.  Returns an unsubscribe function. */
  on<K extends keyof GameEventMap>(
    event: K,
    handler: Listener<GameEventMap[K]>,
  ): () => void {
    let set = this.listeners.get(event as string);
    if (!set) {
      set = new Set();
      this.listeners.set(event as string, set);
    }
    set.add(handler);
    return () => { set!.delete(handler); };
  }

  /** Unsubscribe from an event. */
  off<K extends keyof GameEventMap>(
    event: K,
    handler: Listener<GameEventMap[K]>,
  ): void {
    this.listeners.get(event as string)?.delete(handler);
  }

  /** Emit an event to all current subscribers. */
  emit<K extends keyof GameEventMap>(
    event: K,
    ...args: GameEventMap[K]
  ): void {
    const set = this.listeners.get(event as string);
    if (!set) return;
    for (const fn of set) {
      fn(...args);
    }
  }

  /** Remove all listeners (useful on disconnect). */
  clear(): void {
    this.listeners.clear();
  }
}

/** Singleton event bus shared across the application. */
export const gameEvents = new GameEventBus();
