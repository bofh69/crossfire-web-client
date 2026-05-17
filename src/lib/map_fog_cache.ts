/**
 * map_fog_cache.ts — LRU cache for per-map fog-of-war snapshots.
 *
 * When the player leaves a map the currently-visible tiles are converted to
 * fog state and stored in a bounded LRU cache keyed by the server-supplied
 * map path.  When the player revisits the same map the fog snapshot is
 * restored into the virtual map so previously-explored areas are still shown
 * in fog instead of being blank.
 */

import type { MapCellLayer, MapCellTailLayer, MapLabel } from "./protocol.js";

// ──────────────────────────────────────────────────────────────────────────────
// Types
// ──────────────────────────────────────────────────────────────────────────────

/** Fog data for a single map tile, stored in map-relative coordinates. */
export interface FogCacheCell {
  /** Map-relative X: view_x + script_pos_x at the time of the snapshot. */
  x: number;
  /** Map-relative Y: view_y + script_pos_y at the time of the snapshot. */
  y: number;
  heads: MapCellLayer[];
  tails: MapCellTailLayer[];
  smooth: number[];
  darkness: number;
  labels: MapLabel[];
}

/**
 * Scroll-invariant virtual-map origin recorded at snapshot time.
 * Equals `pl_pos − script_pos` at the moment `mapdata_save_fog` was called,
 * i.e. the value `pl_pos` had when the player first entered this map visit.
 */
export interface FogSnapshotMeta {
  originX: number;
  originY: number;
}

/** All fog cells for one map visit, together with origin metadata. */
export interface FogSnapshot {
  meta: FogSnapshotMeta;
  cells: FogCacheCell[];
}

// ──────────────────────────────────────────────────────────────────────────────
// LRU cache
// ──────────────────────────────────────────────────────────────────────────────

/** Maximum number of map fog snapshots to retain. */
const FOG_CACHE_MAX_SIZE = 20;

/**
 * LRU cache: a JS Map whose insertion order provides O(1) LRU eviction.
 * The most-recently used entry is moved to the end on access.
 */
const fogCache = new Map<string, FogSnapshot>();

/**
 * Store a fog snapshot for the given map key.
 * Evicts the least-recently-used entry when the cache is full.
 *
 * A snapshot with no cells is not stored — if the player entered and
 * immediately left without exploring, the existing cached entry (if any) is
 * retained so the next visit still benefits from older fog data.
 */
export function cacheSaveFog(key: string, snapshot: FogSnapshot): void {
  if (snapshot.cells.length === 0) {
    return;
  }
  // Remove any existing entry so re-insertion puts it at the end (most recent).
  fogCache.delete(key);
  fogCache.set(key, snapshot);
  // Evict oldest entry if over capacity.
  if (fogCache.size > FOG_CACHE_MAX_SIZE) {
    const oldest = fogCache.keys().next().value;
    if (oldest !== undefined) {
      fogCache.delete(oldest);
    }
  }
}

/**
 * Retrieve the fog snapshot for the given map key, promoting it to
 * most-recently-used.  Returns undefined if not cached.
 */
export function cacheGetFog(key: string): FogSnapshot | undefined {
  const snapshot = fogCache.get(key);
  if (snapshot !== undefined) {
    // Move to end (most recently used).
    fogCache.delete(key);
    fogCache.set(key, snapshot);
  }
  return snapshot;
}

/** Discard all cached fog snapshots (e.g. on disconnect or new character). */
export function cacheClearFog(): void {
  fogCache.clear();
}

/**
 * Return summary statistics about the fog cache for the `debug mem`
 * subcommand.
 */
export function fogCacheStats(): {
  entries: number;
  maxEntries: number;
  totalCells: number;
} {
  let totalCells = 0;
  for (const snap of fogCache.values()) {
    totalCells += snap.cells.length;
  }
  return { entries: fogCache.size, maxEntries: FOG_CACHE_MAX_SIZE, totalCells };
}
