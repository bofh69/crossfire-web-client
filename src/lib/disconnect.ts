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
  window.location.reload();
}
