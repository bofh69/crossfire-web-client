<script lang="ts">
  import { onMount, tick } from "svelte";
  import GameMap from "./components/GameMap.svelte";
  import { dispatchPacket } from "./lib/commands";
  import { SELF_TICK_INTERVAL_MS } from "./lib/constants";
  import { setWatchedCell, clearWatchedCell } from "./lib/debug";
  import { gameEvents } from "./lib/events";
  import { getFaceTileSize } from "./lib/image";
  import { clientInit } from "./lib/init";
  import { animateObjects } from "./lib/item";
  import {
    mapdata_animation,
    mapdata_debug_bigface,
    mapdata_debug_tile,
    run_move_to,
    setGetMapImageSize,
  } from "./lib/mapdata";
  import { initCommands } from "./lib/p_cmd";
  import {
    decodeReplayMap2Payload,
    parseReplayLogFile,
    describeReplayEntry,
    resetReplaySandboxState,
    toPacketBuffer,
    type ReplayEntry,
    type ReplayMark,
  } from "./lib/replay";
  import { captureReplayMapdataSnapshot } from "./lib/replay_mapdata_state";

  interface ReplayLogMessage {
    id: number;
    kind: "error" | "info" | "mark" | "rx" | "tx" | "watch";
    text: string;
    lineNumber: number | null;
    timestamp: number | null;
    commandName: string | null;
    payload: Uint8Array | null;
  }

  interface SelectedTile {
    ax: number;
    ay: number;
  }

  const MAX_LOG_MESSAGES = 500;
  const TICK_RATE = 1000 / SELF_TICK_INTERVAL_MS;
  const TICK_RATE_LABEL = Number.isInteger(TICK_RATE)
    ? String(TICK_RATE)
    : TICK_RATE.toFixed(2).replace(/\.?0+$/, "");

  let entries = $state<ReplayEntry[]>([]);
  let marks = $state<ReplayMark[]>([]);
  let fileName = $state("");
  let parseError = $state("");
  let replayLog = $state<ReplayLogMessage[]>([]);
  let currentEntryIndex = $state(-1);
  let nextEntryIndex = $state(0);
  let appliedRxCount = $state(0);
  let selectedTile = $state<SelectedTile | null>(null);
  let selectedTileLines = $state<string[]>([
    "Click a tile on the map to inspect its mapdata.",
  ]);
  let replayLogContainer = $state<HTMLDivElement | null>(null);
  let replayTickEnabled = $state(false);
  let map2DialogText = $state<string | null>(null);
  let map2DialogTitle = $state("");
  let logId = 0;

  function addLog(
    kind: ReplayLogMessage["kind"],
    text: string,
    entry?: ReplayEntry,
  ): void {
    replayLog = [
      ...replayLog,
      {
        id: ++logId,
        kind,
        text,
        lineNumber: entry?.lineNumber ?? null,
        timestamp: entry?.timestamp ?? null,
        commandName: entry?.commandName ?? null,
        payload: entry?.payload ?? null,
      },
    ];
    if (replayLog.length > MAX_LOG_MESSAGES) {
      replayLog = replayLog.slice(-MAX_LOG_MESSAGES);
    }
  }

  function refreshSelectedTileInspection(): void {
    if (selectedTile === null) {
      selectedTileLines = ["Click a tile on the map to inspect its mapdata."];
      return;
    }
    selectedTileLines = [
      ...mapdata_debug_tile(selectedTile.ax, selectedTile.ay),
      "",
      ...mapdata_debug_bigface(selectedTile.ax, selectedTile.ay),
    ];
  }

  function reapplySelectedTileWatch(): void {
    clearWatchedCell();
    if (selectedTile === null) {
      refreshSelectedTileInspection();
      return;
    }
    const { ax, ay } = selectedTile;
    setWatchedCell({ ax, ay }, (event) => {
      addLog("watch", `watch (${ax}, ${ay}) ${event}`);
    });
    refreshSelectedTileInspection();
    gameEvents.emit("mapUpdate");
  }

  function armTileInspection(): void {
    gameEvents.emit("debugPickTile", "tile");
  }

  function resetReplay(logMessage = true): void {
    resetReplaySandboxState();
    nextEntryIndex = 0;
    currentEntryIndex = -1;
    appliedRxCount = 0;
    replayLog = [];
    reapplySelectedTileWatch();
    armTileInspection();
    if (logMessage) {
      addLog("info", "Replay reset to the beginning.");
    }
  }

  function applyEntry(entry: ReplayEntry): void {
    if (entry.type === "MARK") {
      addLog("mark", `MARK ${entry.markerText ?? ""}`, entry);
      return;
    }
    addLog(
      entry.type === "RX" ? "rx" : "tx",
      describeReplayEntry(entry),
      entry,
    );
    if (entry.type === "RX" && entry.payload !== null) {
      dispatchPacket(toPacketBuffer(entry.payload));
      appliedRxCount += 1;
      refreshSelectedTileInspection();
    }
  }

  function stepForward(): void {
    parseError = "";
    if (entries.length === 0) {
      addLog("error", "Load a replay log before stepping.");
      return;
    }
    while (nextEntryIndex < entries.length) {
      const entry = entries[nextEntryIndex]!;
      nextEntryIndex += 1;
      currentEntryIndex = nextEntryIndex - 1;
      applyEntry(entry);
      if (entry.type === "RX") {
        return;
      }
    }
    addLog("info", "Reached the end of the replay.");
  }

  function stepToNextMark(): void {
    parseError = "";
    if (entries.length === 0) {
      addLog("error", "Load a replay log before stepping to the next mark.");
      return;
    }
    while (nextEntryIndex < entries.length) {
      const entry = entries[nextEntryIndex]!;
      nextEntryIndex += 1;
      currentEntryIndex = nextEntryIndex - 1;
      applyEntry(entry);
      if (entry.type === "MARK") {
        return;
      }
    }
    addLog("info", "Reached the end of the replay.");
  }

  function replayToMark(mark: ReplayMark): void {
    parseError = "";
    if (entries.length === 0) {
      addLog("error", "Load a replay log before replaying to a mark.");
      return;
    }
    resetReplay(false);
    while (
      nextEntryIndex <= mark.entryIndex &&
      nextEntryIndex < entries.length
    ) {
      const entry = entries[nextEntryIndex]!;
      nextEntryIndex += 1;
      currentEntryIndex = nextEntryIndex - 1;
      applyEntry(entry);
    }
  }

  async function handleFileChange(event: Event): Promise<void> {
    const input = event.currentTarget as HTMLInputElement | null;
    const file = input?.files?.[0];
    if (!file) {
      return;
    }

    try {
      const parsed = parseReplayLogFile(await file.text());
      entries = parsed.entries;
      marks = parsed.marks;
      fileName = file.name;
      parseError = "";
      resetReplay(false);
      addLog(
        "info",
        `Loaded ${parsed.entries.length} entries and ${parsed.marks.length} MARK entries from ${file.name}.`,
      );
    } catch (error) {
      entries = [];
      marks = [];
      fileName = "";
      parseError = error instanceof Error ? error.message : String(error);
      replayLog = [];
      addLog("error", parseError);
    } finally {
      if (input) {
        input.value = "";
      }
    }
  }

  function handlePick(ax: number, ay: number): void {
    selectedTile = { ax, ay };
    reapplySelectedTileWatch();
    armTileInspection();
  }

  function clearReplayLog(): void {
    replayLog = [];
  }

  function closeMap2Dialog(): void {
    map2DialogText = null;
    map2DialogTitle = "";
  }

  function openMap2Dialog(message: ReplayLogMessage): void {
    if (
      message.kind !== "rx" ||
      message.commandName !== "map2" ||
      message.payload === null
    ) {
      return;
    }
    const decoded = decodeReplayMap2Payload(message.payload);
    if (!decoded) {
      return;
    }
    map2DialogTitle =
      message.lineNumber === null
        ? "Decoded map2 command"
        : `Decoded map2 command (line ${message.lineNumber})`;
    map2DialogText = decoded;
  }

  function sanitizeNamePart(value: string): string {
    const trimmed = value.trim();
    if (trimmed.length === 0) {
      return "snapshot";
    }
    const sanitized = trimmed
      .replace(/[^a-z0-9._-]+/gi, "-")
      .replace(/^-+|-+$/g, "");
    return sanitized.length > 0 ? sanitized : "snapshot";
  }

  function exportMapdataSnapshot(): void {
    if (typeof document === "undefined" || typeof URL === "undefined") {
      addLog("error", "Snapshot export is only available in a browser.");
      return;
    }
    const sourceMark = currentMarkLabel();
    const snapshot = captureReplayMapdataSnapshot({
      source: {
        replayFileName: fileName || undefined,
        markLabel: sourceMark,
        entryIndex: currentEntryIndex,
      },
    });
    const json = JSON.stringify(snapshot, null, 2);
    const blob = new Blob([json], { type: "application/json" });
    const objectUrl = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    const entryLabel =
      currentEntryIndex >= 0 ? `entry-${currentEntryIndex + 1}` : "entry-0";
    anchor.href = objectUrl;
    anchor.download = `${sanitizeNamePart(fileName || "replay")}-${sanitizeNamePart(sourceMark)}-${entryLabel}.state.json`;
    anchor.click();
    URL.revokeObjectURL(objectUrl);
    addLog(
      "info",
      `Exported state with ${snapshot.cells.length} cells at ${sourceMark}.`,
    );
  }

  function exportSelectedCellMapdataSnapshot(): void {
    if (selectedTile === null) {
      addLog("error", "Select a tile before exporting its state.");
      return;
    }
    if (typeof document === "undefined" || typeof URL === "undefined") {
      addLog("error", "Snapshot export is only available in a browser.");
      return;
    }
    const sourceMark = currentMarkLabel();
    const snapshot = captureReplayMapdataSnapshot({
      includeEmptyCells: true,
      source: {
        replayFileName: fileName || undefined,
        markLabel: sourceMark,
        entryIndex: currentEntryIndex,
      },
    });
    const selectedDx = selectedTile.ax - snapshot.playerAbsolute.x;
    const selectedDy = selectedTile.ay - snapshot.playerAbsolute.y;
    snapshot.cells = snapshot.cells.filter(
      (cell) => cell.dx === selectedDx && cell.dy === selectedDy,
    );
    const json = JSON.stringify(snapshot, null, 2);
    const blob = new Blob([json], { type: "application/json" });
    const objectUrl = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    const entryLabel =
      currentEntryIndex >= 0 ? `entry-${currentEntryIndex + 1}` : "entry-0";
    anchor.href = objectUrl;
    anchor.download =
      `${sanitizeNamePart(fileName || "replay")}-${sanitizeNamePart(sourceMark)}-${entryLabel}` +
      `-selected-${selectedTile.ax}-${selectedTile.ay}.state.json`;
    anchor.click();
    URL.revokeObjectURL(objectUrl);
    addLog(
      "info",
      `Exported selected-cell state with ${snapshot.cells.length} cells at ${sourceMark}.`,
    );
  }

  function formatPosition(): string {
    if (entries.length === 0) {
      return "No replay loaded";
    }
    return `${Math.max(0, currentEntryIndex + 1)} / ${entries.length} entries, ${appliedRxCount} RX applied`;
  }

  function currentMarkLabel(): string {
    if (currentEntryIndex < 0) {
      return "Before first MARK";
    }
    for (let index = marks.length - 1; index >= 0; index--) {
      const mark = marks[index]!;
      if (mark.entryIndex <= currentEntryIndex) {
        return mark.label;
      }
    }
    return "Before first MARK";
  }

  function latestReachedMarkEntryIndex(): number | null {
    for (let index = marks.length - 1; index >= 0; index--) {
      const mark = marks[index]!;
      if (mark.entryIndex <= currentEntryIndex) {
        return mark.entryIndex;
      }
    }
    return null;
  }

  const latestReachedMarkEntry = $derived(latestReachedMarkEntryIndex());

  $effect(() => {
    void replayLog.length;
    tick().then(() => {
      replayLogContainer?.scrollTo({
        top: replayLogContainer.scrollHeight,
      });
    });
  });

  $effect(() => {
    if (!replayTickEnabled) {
      return;
    }
    const intervalId = setInterval(() => {
      mapdata_animation();
      animateObjects();
      run_move_to();
      gameEvents.emit("mapUpdate");
      gameEvents.emit("playerUpdate");
    }, SELF_TICK_INTERVAL_MS);
    return () => {
      clearInterval(intervalId);
    };
  });

  onMount(() => {
    clientInit();
    initCommands();
    setGetMapImageSize(getFaceTileSize);
    resetReplay(false);
    addLog("info", "Replay sandbox ready. Load a ws-recording log to begin.");
    const unsubscribe = gameEvents.on("debugTileClicked", (ax, ay) => {
      handlePick(ax, ay);
    });
    armTileInspection();
    return () => {
      clearWatchedCell();
      unsubscribe();
    };
  });
</script>

<div class="replay-page">
  <aside class="sidebar controls">
    <h1>Replay</h1>
    <label class="file-picker">
      <span>Replay log file</span>
      <input
        type="file"
        accept=".log,.txt,text/plain"
        onchange={handleFileChange}
      />
    </label>
    <div class="status-card">
      <div><strong>File:</strong> {fileName || "None loaded"}</div>
      <div><strong>Progress:</strong> {formatPosition()}</div>
      <div><strong>Current MARK:</strong> {currentMarkLabel()}</div>
    </div>
    <div class="button-row">
      <button onclick={() => resetReplay()} disabled={entries.length === 0}>
        Start over
      </button>
      <button onclick={stepForward} disabled={entries.length === 0}>
        Step cmd
      </button>
      <button onclick={stepToNextMark} disabled={entries.length === 0}>
        Next mark
      </button>
    </div>
    <div class="button-row">
      <button onclick={exportMapdataSnapshot} disabled={entries.length === 0}>
        Export mapdata state
      </button>
      <button
        onclick={exportSelectedCellMapdataSnapshot}
        disabled={entries.length === 0 || selectedTile === null}
      >
        Export selected cell state
      </button>
    </div>
    <label class="tick-toggle">
      <input type="checkbox" bind:checked={replayTickEnabled} />
      <span>Tick ({TICK_RATE_LABEL} fps)</span>
    </label>
    {#if parseError}
      <p class="error">{parseError}</p>
    {/if}
    <section class="marks">
      <h2>MARK entries</h2>
      {#if marks.length === 0}
        <p class="empty">No MARK entries loaded.</p>
      {:else}
        <div class="mark-list">
          {#each marks as mark}
            <button
              class:active={currentEntryIndex === mark.entryIndex}
              class:latest-reached={latestReachedMarkEntry === mark.entryIndex}
              onclick={() => replayToMark(mark)}
            >
              <span>{mark.label}</span>
              <small>{mark.timestamp} ms · line {mark.lineNumber}</small>
            </button>
          {/each}
        </div>
      {/if}
    </section>
  </aside>

  <main class="map-shell">
    <div class="map-header">
      <strong>GameMap replay</strong>
      <span>Click on cell to select and watch it.</span>
    </div>
    <div class="map-panel">
      <GameMap />
    </div>
  </main>

  <aside class="sidebar details">
    <section class="tile-panel">
      <h2>Selected tile</h2>
      <div class="tile-list">
        <pre>{selectedTileLines.join("\n")}</pre>
      </div>
    </section>
    <section class="log-panel">
      <div class="panel-header">
        <h2>Replay log</h2>
        <button
          class="clear-log"
          onclick={clearReplayLog}
          disabled={replayLog.length === 0}
          title="Clear replay log"
          aria-label="Clear replay log"
        >
          ×
        </button>
      </div>
      {#if replayLog.length === 0}
        <p class="empty">No replay output yet.</p>
      {:else}
        <div class="log-list" bind:this={replayLogContainer}>
          {#each replayLog as message}
            <div class={`log-entry ${message.kind}`}>
              <button
                class="log-entry-hitbox"
                type="button"
                ondblclick={() => openMap2Dialog(message)}
                title={
                  message.kind === "rx" && message.commandName === "map2"
                    ? "Double-click to decode map2 details"
                    : undefined
                }
              >
              <div class="log-meta">
                <span class="kind">{message.kind.toUpperCase()}</span>
                {#if message.timestamp !== null}
                  <span>{message.timestamp} ms</span>
                {/if}
                {#if message.lineNumber !== null}
                  <span>line {message.lineNumber}</span>
                {/if}
              </div>
              <pre>{message.text}</pre>
              </button>
            </div>
          {/each}
        </div>
      {/if}
    </section>
  </aside>
</div>

{#if map2DialogText !== null}
  <div class="map2-dialog-backdrop" role="dialog" aria-modal="true">
    <div class="map2-dialog">
      <div class="map2-dialog-header">
        <h3>{map2DialogTitle}</h3>
        <button
          class="map2-dialog-close"
          type="button"
          onclick={closeMap2Dialog}
          aria-label="Close map2 details"
        >
          ×
        </button>
      </div>
      <div class="map2-dialog-body">
        <pre>{map2DialogText}</pre>
      </div>
    </div>
  </div>
{/if}

<style>
  .replay-page {
    display: grid;
    grid-template-columns: minmax(260px, 320px) minmax(0, 1fr) minmax(
        320px,
        420px
      );
    gap: 0;
    width: 100vw;
    width: 100dvw;
    height: 100vh;
    height: 100dvh;
  }

  .sidebar,
  .map-shell {
    min-height: 0;
  }

  .sidebar {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
    padding: 1rem;
    background: var(--bg-panel);
    border-right: 1px solid var(--border);
    overflow: hidden;
  }

  .details {
    border-left: 1px solid var(--border);
    border-right: none;
  }

  h1,
  h2 {
    margin: 0;
    color: var(--text-warm);
  }

  h1 {
    font-size: 1.35rem;
  }

  h2 {
    font-size: 1rem;
  }

  .file-picker,
  .status-card,
  .tile-panel,
  .log-panel,
  .marks,
  .map-header {
    background: var(--bg-mid);
    border: 1px solid var(--border);
    border-radius: 0.35rem;
  }

  .file-picker,
  .status-card,
  .map-header {
    padding: 0.75rem;
  }

  .file-picker {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }

  .button-row {
    display: flex;
    gap: 0.5rem;
  }

  button,
  input::file-selector-button {
    background: var(--bg-warm);
    color: var(--text-warm);
    border: 1px solid var(--border-light);
    border-radius: 0.3rem;
    cursor: pointer;
    font: inherit;
    padding: 0.45rem 0.75rem;
  }

  button:hover,
  input::file-selector-button:hover {
    background: var(--bg-warm-hover);
  }

  button:disabled {
    cursor: not-allowed;
    opacity: 0.5;
  }

  .marks,
  .tile-panel,
  .tile-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    margin-top: 0.75rem;
    overflow: auto;
    min-height: 0;
  }

  .log-panel {
    display: flex;
    flex-direction: column;
    min-height: 0;
    padding: 0.75rem;
  }
  .mark-list,
  .log-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    margin-top: 0.75rem;
    overflow: auto;
    min-height: 0;
  }

  .mark-list button {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 0.1rem;
    width: 100%;
    text-align: left;
  }

  .mark-list button.active {
    outline: 2px solid var(--accent);
  }

  .mark-list button.latest-reached {
    border-color: var(--accent);
    background: color-mix(in srgb, var(--accent) 22%, var(--bg-warm));
  }

  .panel-header {
    align-items: center;
    display: flex;
    justify-content: space-between;
  }

  .map-shell {
    display: flex;
    flex-direction: column;
    min-width: 0;
    background: var(--bg);
  }

  .map-header {
    margin: 1rem 1rem 0;
    display: flex;
    justify-content: space-between;
    gap: 1rem;
    align-items: center;
  }

  .map-panel {
    flex: 1;
    min-height: 0;
    margin: 1rem;
    border: 1px solid var(--border);
    background: #111;
  }

  .tile-panel {
    flex: 0 0 auto;
    max-height: 40%;
  }

  .log-panel {
    flex: 1;
  }

  .error {
    color: var(--danger-text);
    margin: 0;
  }

  .empty {
    color: var(--text-dim);
    margin: 0.75rem 0 0;
  }

  .status-card {
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
  }

  .log-entry {
    border: 1px solid var(--border);
    border-radius: 0.3rem;
    background: var(--bg-panel);
  }

  .log-entry-hitbox {
    background: transparent;
    border: 0;
    color: inherit;
    cursor: inherit;
    display: block;
    padding: 0.5rem;
    text-align: left;
    width: 100%;
  }

  .log-entry-hitbox:hover {
    background: transparent;
  }

  .clear-log {
    font-size: 0.95rem;
    line-height: 1;
    min-height: 1.4rem;
    min-width: 1.4rem;
    padding: 0.15rem 0.35rem;
  }

  .tick-toggle {
    align-items: center;
    display: flex;
    gap: 0.45rem;
  }

  .log-entry.mark {
    border-color: var(--accent);
  }

  .log-entry.error {
    border-color: var(--danger-bg);
  }

  .log-entry.watch {
    border-color: #4f6b8a;
  }

  .log-meta {
    display: flex;
    gap: 0.5rem;
    flex-wrap: wrap;
    font-size: 0.75rem;
    color: var(--text-dim);
    margin-bottom: 0.35rem;
  }

  .kind {
    color: var(--text-warm);
  }

  pre {
    margin: 0;
    white-space: pre-wrap;
    word-break: break-word;
    font-family: var(--mono);
    font-size: 0.8rem;
  }

  .map2-dialog-backdrop {
    align-items: center;
    background: color-mix(in srgb, #000 70%, transparent);
    display: flex;
    inset: 0;
    justify-content: center;
    padding: 1rem;
    position: fixed;
    z-index: 20;
  }

  .map2-dialog {
    background: var(--bg-panel);
    border: 1px solid var(--border-light);
    border-radius: 0.35rem;
    display: flex;
    flex-direction: column;
    max-height: min(80vh, 900px);
    max-width: min(95vw, 900px);
    width: min(95vw, 900px);
  }

  .map2-dialog-header {
    align-items: center;
    border-bottom: 1px solid var(--border);
    display: flex;
    gap: 0.75rem;
    justify-content: space-between;
    padding: 0.75rem;
  }

  .map2-dialog-header h3 {
    font-size: 1rem;
    margin: 0;
  }

  .map2-dialog-close {
    font-size: 1rem;
    line-height: 1;
    min-height: 1.5rem;
    min-width: 1.5rem;
    padding: 0.15rem 0.35rem;
  }

  .map2-dialog-body {
    min-height: 0;
    overflow: auto;
    padding: 0.75rem;
  }

  @media (max-width: 1200px) {
    .replay-page {
      grid-template-columns: minmax(240px, 300px) minmax(0, 1fr);
      grid-template-rows: minmax(0, 1fr) minmax(260px, 35vh);
    }

    .details {
      grid-column: 1 / span 2;
      border-left: none;
      border-top: 1px solid var(--border);
    }
  }
</style>
