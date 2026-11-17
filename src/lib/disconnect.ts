import { clientDisconnect } from "./client";

/**
 * Snapshot of the `?server=` query parameter at page-load time.
 * Used to decide whether to preserve the parameter on disconnect:
 * if it was present when the page first loaded the user likely bookmarked
 * or shared the URL, so it should survive a disconnect/reload.  If it was
 * added dynamically by `handleConnect` it should be removed.
 */
const pageLoadServerParam = new URLSearchParams(window.location.search).get(
  "server",
);

/** Set once the client itself starts a page reload, to skip the unload prompt. */
let reloadRequested = false;

/**
 * Whether the client has started an intentional page reload.
 *
 * @returns True after {@link reloadPage} or {@link disconnectAndReload} ran.
 */
export function isReloadRequested(): boolean {
  return reloadRequested;
}

/**
 * Reload the page without triggering the "leave the game?" unload prompt.
 */
export function reloadPage(): void {
  reloadRequested = true;
  window.location.reload();
}

/**
 * Disconnect from the current server and hard-reload the page.
 * Removes the `?server=` query parameter from the URL unless it was already
 * present when the page was first loaded (e.g. a bookmarked URL).
 */
export function disconnectAndReload(): void {
  clientDisconnect();
  if (!pageLoadServerParam) {
    const params = new URLSearchParams(window.location.search);
    params.delete("server");
    const newSearch = params.toString();
    history.replaceState(
      null,
      "",
      newSearch ? "?" + newSearch : location.pathname,
    );
  }
  reloadPage();
}
