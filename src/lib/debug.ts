/**
 * Debug state and helpers for the Crossfire web client.
 *
 * The `/debug` command toggles flags here; other modules read them to
 * control optional diagnostic output.
 */

/** When true, periodic map-draw performance stats are logged to the console. */
export let perfLogging = false;

/** Toggle performance logging on or off. */
export function setPerfLogging(on: boolean): void {
    perfLogging = on;
}

// ---------------------------------------------------------------------------
// Watched-cell tracking — used by `debug watch`
// ---------------------------------------------------------------------------

export interface WatchedCell {
    ax: number;
    ay: number;
}

let watchedCell: WatchedCell | null = null;
let watchCallback: ((event: string) => void) | null = null;

/**
 * Start watching a cell.  Every update to that absolute map coordinate is
 * forwarded to `cb`.  Pass `null` for both args to clear the watch.
 */
export function setWatchedCell(
    cell: WatchedCell | null,
    cb: ((event: string) => void) | null,
): void {
    watchedCell = cell;
    watchCallback = cb;
}

/** Return the currently watched cell, or null if none. */
export function getWatchedCell(): WatchedCell | null {
    return watchedCell;
}

/** Stop watching the current cell. */
export function clearWatchedCell(): void {
    watchedCell = null;
    watchCallback = null;
}

/**
 * Called by map-data setters whenever a cell is modified.
 * If the given absolute coordinates match the watched cell the event string
 * is forwarded to the registered callback.
 */
export function notifyWatchedCell(ax: number, ay: number, event: string): void {
    if (watchedCell && watchedCell.ax === ax && watchedCell.ay === ay && watchCallback) {
        watchCallback(event);
    }
}
