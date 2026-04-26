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

// ── Character-creation types ───────────────────────────────────────────────

/** A single optional choice within a race or class (e.g. preferred deity). */
export interface ChargenChoice {
  /** Internal name returned to the server (e.g. "skin"). */
  name: string;
  /** Human-readable description shown in the UI. */
  desc: string;
  /** Available option values — arch is sent to server, desc shown in UI. */
  values: Array<{ arch: string; desc: string }>;
}

/** Race or class data received from `replyinfo race_info` / `replyinfo class_info`. */
export interface RaceClassEntry {
  archName: string;
  publicName: string;
  description: string;
  /** Signed adjustments to each stat (keyed by short name, e.g. "Str"). */
  statAdj: Record<string, number>;
  choices: ChargenChoice[];
}

/** Stat constraints and names from `replyinfo newcharinfo`. */
export interface NewCharInfo {
  /** Total attribute points the player may distribute. */
  statPoints: number;
  statMin: number;
  statMax: number;
  /** Short stat names in the order the server uses them (e.g. ["Str","Dex",…]). */
  statNames: string[];
  /** True when the server's newcharinfo included a `startingmap requestinfo` line. */
  wantsStartingMap: boolean;
}

/** A single starting-map option from `replyinfo startingmap`. */
export interface StartingMapEntry {
  /** Archetype name sent back in the `createplayer` packet. */
  archName: string;
  /** Human-readable name shown in the UI. */
  publicName: string;
  /** Longer description shown below the selector. */
  description: string;
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
  questUpdate:     [];
  knowledgeUpdate: [];
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
  /** Emitted when the server confirms heartbeat support via `setup beat 1`. */
  beatEnabled:     [];
  /** Emitted when the server confirms the negotiated login method via `setup loginmethod`. */
  loginMethodConfirmed: [method: number];

  // Character-creation events (loginmethod >= 2)
  /** Pipe-separated list of available race archetype names from `replyinfo race_list`. */
  raceListReceived:    [archNames: string[]];
  /** Pipe-separated list of available class archetype names from `replyinfo class_list`. */
  classListReceived:   [archNames: string[]];
  /** Detailed race data from `replyinfo race_info`. */
  raceInfoReceived:    [info: RaceClassEntry];
  /** Detailed class data from `replyinfo class_info`. */
  classInfoReceived:   [info: RaceClassEntry];
  /** Stat constraints from `replyinfo newcharinfo`. */
  newCharInfoReceived: [info: NewCharInfo];
  /** Starting map choices from `replyinfo startingmap`. */
  startingMapReceived: [maps: StartingMapEntry[]];

  // UI-internal events (component-to-component communication)
  /** Ask the InfoPanel to focus its command input field. */
  focusCommandInput: [prefill?: string];
  /** Ask the MenuBar to start a "bind last command to key" flow. */
  openKeyBind:       [];
  /** Ask the MenuBar to start a "bind last command to gamepad button" flow. */
  openGamepadBind:   [];
  /** Hotbar slot data or gamepad-select state changed. */
  hotbarUpdate:      [];
  /** Increase the tile zoom level by one step. */
  zoomIn:            [];
  /** Decrease the tile zoom level by one step. */
  zoomOut:           [];

  // Debug events
  /** Request the GameMap to enter "pick a tile" mode for debugging. */
  debugPickTile:     [mode: 'bigface' | 'tile'];
  /** Fired by the GameMap when the user clicks a tile in debug-pick mode.
   *  Coordinates are absolute (virtual-map) positions. */
  debugTileClicked:  [ax: number, ay: number, mode: 'bigface' | 'tile'];
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
