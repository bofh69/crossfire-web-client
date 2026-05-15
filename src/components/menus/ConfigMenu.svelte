<script lang="ts">
  import { tick } from "svelte";
  import {
    getMusicVolume,
    getSfxVolume,
    setMusicVolume,
    setSfxVolume,
  } from "../../lib/sound";
  import { gameEvents } from "../../lib/events";
  import { useConfig, wantConfig, saveCurrentConfig } from "../../lib/init";
  import { exportConfigBackup, importConfigBackup } from "../../lib/storage";
  import {
    clientIsConnected,
    configureCurrentServerFaceset,
    getConfiguredFacesetForCurrentServer,
  } from "../../lib/client";
  import {
    getAvailableFacesets,
    getCurrentFaceset,
    hasFacesetInfo,
  } from "../../lib/image";
  import {
    getConfiguredNdiColors,
    getDefaultNdiColor,
    getInfoPanelBackgroundColor,
    INFO_PANEL_BACKGROUND_DEFAULT,
    NDI_COLOR_DEFINITIONS,
    setInfoPanelColors,
  } from "../../lib/markup";
  import type { FaceSet } from "../../lib/protocol";

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

  let musicVolume = $state(getMusicVolume());
  let sfxVolume = $state(getSfxVolume());
  let fogGrayscale = $state(useConfig.fogGrayscale);
  let darknessInterpolation = $state(useConfig.darknessInterpolation);
  let showColorsDialog = $state(false);
  let editableBackgroundColor = $state(INFO_PANEL_BACKGROUND_DEFAULT);
  let editableNdiColors = $state<Record<number, string>>({});
  let selectedColorTarget = $state("background");
  let selectedColorInput: HTMLInputElement | undefined = $state();
  let restoreFileInput: HTMLInputElement | undefined = $state();
  let showRestoreConfirmDialog = $state(false);
  let pendingRestoreData: unknown = $state(null);
  let availableFacesets = $state<FaceSet[]>([]);
  let selectedFaceset = $state("0");

  function updateMusicVolume(event: Event) {
    const input = event.currentTarget as HTMLInputElement;
    const volume = Number(input.value);
    setMusicVolume(volume);
    musicVolume = getMusicVolume();
  }

  function updateSfxVolume(event: Event) {
    const input = event.currentTarget as HTMLInputElement;
    const volume = Number(input.value);
    setSfxVolume(volume);
    sfxVolume = getSfxVolume();
  }

  function toggleFogGrayscale() {
    fogGrayscale = !fogGrayscale;
    useConfig.fogGrayscale = fogGrayscale;
    wantConfig.fogGrayscale = fogGrayscale;
    saveCurrentConfig();
    onClose();
  }

  function toggleDarknessInterpolation() {
    darknessInterpolation = !darknessInterpolation;
    useConfig.darknessInterpolation = darknessInterpolation;
    wantConfig.darknessInterpolation = darknessInterpolation;
    saveCurrentConfig();
    onClose();
  }

  function handleZoomIn() {
    gameEvents.emit("zoomIn");
    onClose();
  }

  function handleZoomOut() {
    gameEvents.emit("zoomOut");
    onClose();
  }

  function facesetLabel(faceset: FaceSet): string {
    if (faceset.fullname) return faceset.fullname;
    if (faceset.comment) return faceset.comment;
    if (faceset.prefix) return faceset.prefix;
    return `Faceset ${faceset.setnum}`;
  }

  function refreshFacesets() {
    const facesets = getAvailableFacesets();
    let preferredFaceset =
      getConfiguredFacesetForCurrentServer() ?? getCurrentFaceset();
    const availableSetnums = new Set(facesets.map((f) => f.setnum));
    if (facesets.length > 0 && !availableSetnums.has(preferredFaceset)) {
      preferredFaceset = facesets[0]!.setnum;
    }
    availableFacesets = facesets;
    selectedFaceset = String(preferredFaceset);
  }

  function changeFaceset(event: Event) {
    const nextFaceset = Number(
      (event.currentTarget as HTMLSelectElement).value,
    );
    if (!Number.isFinite(nextFaceset)) return;
    selectedFaceset = String(nextFaceset);
    configureCurrentServerFaceset(nextFaceset);
  }

  function backupConfig() {
    const backup = exportConfigBackup();
    const blob = new Blob([JSON.stringify(backup, null, 2)], {
      type: "application/json",
    });
    const formattedDateTime = new Date()
      .toISOString()
      .substring(0, 19)
      .replace(/:/g, "-");
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `crossfire-config-backup-v${backup.version}-${formattedDateTime}.json`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
    onClose();
  }

  function restoreConfig() {
    restoreFileInput?.click();
    onClose();
  }

  async function handleRestoreFileSelected(event: Event) {
    const input = event.currentTarget as HTMLInputElement;
    const file = input.files?.[0];
    if (!file) return;

    try {
      const text = await file.text();
      let parsed: unknown;
      try {
        parsed = JSON.parse(text) as unknown;
      } catch {
        throw new Error(
          "The selected file is not a valid JSON configuration backup.",
        );
      }
      pendingRestoreData = parsed;
      showRestoreConfirmDialog = true;
    } catch (error) {
      showRestoreError(error);
    } finally {
      input.value = "";
    }
  }

  function confirmRestoreConfig() {
    if (pendingRestoreData === null || pendingRestoreData === undefined) {
      showRestoreConfirmDialog = false;
      return;
    }
    try {
      importConfigBackup(pendingRestoreData);
      window.location.reload();
    } catch (error) {
      showRestoreError(error);
      pendingRestoreData = null;
      showRestoreConfirmDialog = false;
    }
  }

  function showRestoreError(error: unknown) {
    const message =
      error instanceof Error ? error.message : "Unknown restore error.";
    alert(`Failed to restore configuration: ${message}`);
  }

  function cancelRestoreConfig() {
    pendingRestoreData = null;
    showRestoreConfirmDialog = false;
  }

  async function openColorsDialog() {
    editableBackgroundColor = getInfoPanelBackgroundColor();
    editableNdiColors = getConfiguredNdiColors();
    selectedColorTarget = "background";
    showColorsDialog = true;
    await tick();
    selectedColorInput?.focus();
    onClose();
  }

  function closeColorsDialog() {
    showColorsDialog = false;
  }

  function handleDialogKeydown(event: KeyboardEvent) {
    if (
      (showColorsDialog || showRestoreConfirmDialog) &&
      event.key === "Escape"
    ) {
      event.preventDefault();
      event.stopPropagation();
      if (showColorsDialog) closeColorsDialog();
      if (showRestoreConfirmDialog) cancelRestoreConfig();
    }
  }

  function saveColors() {
    setInfoPanelColors(editableBackgroundColor, editableNdiColors);
    showColorsDialog = false;
  }

  function resetAllEditableColorsToDefaults() {
    const next: Record<number, string> = {};
    for (const def of NDI_COLOR_DEFINITIONS) {
      next[def.id] = def.defaultColor;
    }
    editableNdiColors = next;
    editableBackgroundColor = INFO_PANEL_BACKGROUND_DEFAULT;
  }

  function resetEditableBackgroundToDefault() {
    editableBackgroundColor = INFO_PANEL_BACKGROUND_DEFAULT;
  }

  function resetEditableColorToDefault(ndi: number) {
    editableNdiColors = {
      ...editableNdiColors,
      [ndi]: getDefaultNdiColor(ndi),
    };
  }

  function getSelectedNdiId(): number | null {
    if (!selectedColorTarget.startsWith("ndi:")) return null;
    return Number(selectedColorTarget.substring(4));
  }

  function getSelectedColorValue(): string {
    const selectedNdiId = getSelectedNdiId();
    if (selectedNdiId === null) return editableBackgroundColor;
    return (
      editableNdiColors[selectedNdiId] ?? getDefaultNdiColor(selectedNdiId)
    );
  }

  function getSelectedColorName(): string {
    const selectedNdiId = getSelectedNdiId();
    if (selectedNdiId === null) return "BACKGROUND";
    const def = NDI_COLOR_DEFINITIONS.find(
      (entry) => entry.id === selectedNdiId,
    );
    return def?.name ?? "UNKNOWN";
  }

  function getSelectedExampleTextColor(): string {
    const selectedNdiId = getSelectedNdiId();
    if (selectedNdiId === null) return "#e0e0e0";
    return (
      editableNdiColors[selectedNdiId] ?? getDefaultNdiColor(selectedNdiId)
    );
  }

  function updateSelectedColor(event: Event) {
    const value = (event.currentTarget as HTMLInputElement).value;
    const selectedNdiId = getSelectedNdiId();
    if (selectedNdiId === null) {
      editableBackgroundColor = value;
      return;
    }
    editableNdiColors = {
      ...editableNdiColors,
      [selectedNdiId]: value,
    };
  }

  function resetSelectedColorToDefault() {
    const selectedNdiId = getSelectedNdiId();
    if (selectedNdiId === null) {
      resetEditableBackgroundToDefault();
      return;
    }
    resetEditableColorToDefault(selectedNdiId);
  }

  /** Returns true while the colors configuration dialog is open. */
  export function isDialogActive(): boolean {
    return showColorsDialog || showRestoreConfirmDialog;
  }

  $effect(() => {
    if (isOpen) refreshFacesets();
  });

  $effect(() => {
    const unsub = gameEvents.on("facesetInfoUpdated", refreshFacesets);
    return unsub;
  });

  $effect(() => {
    const unsub = gameEvents.on("disconnect", refreshFacesets);
    return unsub;
  });
</script>

<svelte:window onkeydown={handleDialogKeydown} />

<div class="menu-item">
  <button
    class="menu-button"
    onclick={onToggle}
    oncontextmenu={(e) => {
      e.preventDefault();
      onToggle();
    }}>Config</button
  >
  {#if isOpen}
    <div class="dropdown" class:fading>
      <div class="slider-row">
        <label for="music-volume">Music</label>
        <input
          id="music-volume"
          type="range"
          min="0"
          max="100"
          step="1"
          value={musicVolume}
          oninput={updateMusicVolume}
          onclick={(e) => e.stopPropagation()}
        />
        <span>{musicVolume}%</span>
      </div>
      <div class="slider-row">
        <label for="sfx-volume">Sound Effects</label>
        <input
          id="sfx-volume"
          type="range"
          min="0"
          max="100"
          step="1"
          value={sfxVolume}
          oninput={updateSfxVolume}
          onclick={(e) => e.stopPropagation()}
        />
        <span>{sfxVolume}%</span>
      </div>
      {#if clientIsConnected() && hasFacesetInfo() && availableFacesets.length > 0}
        <div class="faceset-row">
          <label for="faceset-select">Faceset</label>
          <select
            id="faceset-select"
            value={selectedFaceset}
            onchange={changeFaceset}
          >
            {#each availableFacesets as faceset}
              <option value={String(faceset.setnum)}>
                {facesetLabel(faceset)}
              </option>
            {/each}
          </select>
        </div>
      {/if}
      <div class="separator"></div>
      <button
        onclick={openColorsDialog}
        oncontextmenu={(e) => {
          e.preventDefault();
          openColorsDialog();
        }}>Colors</button
      >
      <div class="separator"></div>
      <button
        onclick={toggleFogGrayscale}
        oncontextmenu={(e) => {
          e.preventDefault();
          toggleFogGrayscale();
        }}
        >{fogGrayscale
          ? "Disable Grayscale Fog of War"
          : "Enable Grayscale Fog of War"}</button
      >
      <button
        onclick={toggleDarknessInterpolation}
        oncontextmenu={(e) => {
          e.preventDefault();
          toggleDarknessInterpolation();
        }}
        >{darknessInterpolation
          ? "Disable Darkness Interpolation"
          : "Enable Darkness Interpolation"}</button
      >
      <div class="separator"></div>
      <button
        onclick={handleZoomIn}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleZoomIn();
        }}>Zoom In</button
      >
      <button
        onclick={handleZoomOut}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleZoomOut();
        }}>Zoom Out</button
      >
      <div class="separator"></div>
      <button
        onclick={backupConfig}
        oncontextmenu={(e) => {
          e.preventDefault();
          backupConfig();
        }}>Backup Config…</button
      >
      <button
        onclick={restoreConfig}
        oncontextmenu={(e) => {
          e.preventDefault();
          restoreConfig();
        }}>Restore Config…</button
      >
      <input
        type="file"
        accept="application/json,.json"
        bind:this={restoreFileInput}
        onchange={handleRestoreFileSelected}
        style="display: none"
      />
    </div>
  {/if}
</div>

{#if showColorsDialog}
  <div class="dialog-overlay">
    <div class="dialog dialog-widest colors-dialog">
      <p class="dialog-title">InfoPanel Colors</p>

      <div class="color-editor">
        <label class="editor-label" for="color-target">Color</label>
        <select id="color-target" bind:value={selectedColorTarget}>
          <option value="background">Background</option>
          {#each NDI_COLOR_DEFINITIONS as def}
            <option value={`ndi:${def.id}`}>{def.name}</option>
          {/each}
        </select>
        <input
          type="color"
          value={getSelectedColorValue()}
          oninput={updateSelectedColor}
          bind:this={selectedColorInput}
        />
        <button onclick={resetSelectedColorToDefault}>Default</button>
      </div>

      <div class="preview-area" style:background={editableBackgroundColor}>
        <p class="selected-preview-label">
          Selected color: <strong>{getSelectedColorName()}</strong>
        </p>
        <p
          class="selected-preview-text"
          style:color={getSelectedExampleTextColor()}
        >
          The quick brown fox jumps over the lazy dragon.
        </p>
        {#each NDI_COLOR_DEFINITIONS as def}
          <div class="preview-line">
            <span
              class="preview-name"
              style:color={editableNdiColors[def.id] ?? def.defaultColor}
              >{def.name}</span
            >
            <span style:color={editableNdiColors[def.id] ?? def.defaultColor}
              >The quick brown fox jumps over the lazy dragon.</span
            >
          </div>
        {/each}
      </div>

      <div class="dialog-buttons">
        <button onclick={resetAllEditableColorsToDefaults}
          >Reset All to Defaults</button
        >
        <button onclick={saveColors} class="btn-primary">Save</button>
        <button onclick={closeColorsDialog}>Cancel</button>
      </div>
    </div>
  </div>
{/if}

{#if showRestoreConfirmDialog}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Restore Config</p>
      <p class="dialog-prompt">
        Replace all saved configuration with the selected backup file?
      </p>
      <div class="dialog-buttons">
        <button class="btn-primary" onclick={confirmRestoreConfig}
          >Restore</button
        >
        <button onclick={cancelRestoreConfig}>Cancel</button>
      </div>
    </div>
  </div>
{/if}

<style>
  .separator {
    height: 1px;
    background: var(--border-mid);
    margin: 0.25rem 0;
  }

  .slider-row {
    display: grid;
    grid-template-columns: 1fr minmax(90px, 120px) 4ch;
    align-items: center;
    gap: 0.4rem;
    padding: 0.25rem 0.5rem;
    color: var(--text);
    font-size: 0.75rem;
    white-space: nowrap;
  }

  .slider-row label {
    cursor: default;
  }

  .slider-row input {
    width: 100%;
  }

  .slider-row span {
    text-align: right;
    font-variant-numeric: tabular-nums;
  }

  .faceset-row {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0.2rem;
    padding: 0.25rem 0.5rem;
    color: var(--text);
    font-size: 0.75rem;
  }

  .faceset-row label {
    cursor: default;
  }

  .faceset-row select {
    width: 100%;
    min-width: 14rem;
    padding: 0.2rem 0.35rem;
    border: 1px solid var(--border-light);
    border-radius: 3px;
    background: var(--bg-lighter);
    color: var(--text);
    font-family: var(--mono);
    font-size: 0.75rem;
  }

  .colors-dialog {
    max-width: min(760px, 95vw);
  }

  .color-editor {
    display: grid;
    grid-template-columns: auto minmax(170px, 1fr) auto auto;
    gap: 0.5rem;
    align-items: center;
    margin: 0.5rem 0 0.8rem;
    border: 1px solid var(--border-mid);
    border-radius: 3px;
    padding: 0.5rem;
    background: var(--bg-panel);
  }

  .editor-label {
    font-family: var(--mono);
    font-size: 0.78rem;
    color: var(--text);
  }

  .color-editor select {
    padding: 0.35rem 0.5rem;
    border: 1px solid var(--border-light);
    border-radius: 3px;
    background: var(--bg-lighter);
    color: var(--text);
    font-family: var(--mono);
    font-size: 0.78rem;
  }

  .color-editor input[type="color"] {
    width: 44px;
    height: 24px;
    padding: 0;
    border: 1px solid var(--border-mid);
    border-radius: 3px;
    background: transparent;
    cursor: pointer;
  }

  .color-editor button {
    padding: 0.2rem 0.5rem;
    border: 1px solid var(--border-light);
    border-radius: 3px;
    background: var(--border);
    color: var(--text);
    cursor: pointer;
    font-size: 0.75rem;
  }

  .color-editor button:hover {
    background: var(--border-mid);
  }

  .preview-area {
    border: 1px solid var(--border-mid);
    border-radius: 3px;
    padding: 0.6rem;
    font-family: var(--mono);
    font-size: 0.78rem;
    min-height: 180px;
  }

  .preview-line {
    display: flex;
    gap: 0.75rem;
    padding: 0.1rem 0;
    line-height: 1.35;
  }

  .preview-name {
    min-width: 112px;
    font-weight: bold;
  }

  .selected-preview-label {
    margin: 0 0 0.35rem;
    color: var(--text);
    font-size: 0.75rem;
  }

  .selected-preview-text {
    margin: 0 0 0.6rem;
    font-weight: bold;
  }
</style>
