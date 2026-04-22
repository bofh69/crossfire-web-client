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
