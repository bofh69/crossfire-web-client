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
  import {
    getConfiguredNdiColors,
    getDefaultNdiColor,
    getInfoPanelBackgroundColor,
    INFO_PANEL_BACKGROUND_DEFAULT,
    NDI_COLOR_DEFINITIONS,
    setInfoPanelColors,
  } from "../../lib/markup";

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

  function handleColorsDialogKeydown(event: KeyboardEvent) {
    if (showColorsDialog && event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      closeColorsDialog();
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
    return showColorsDialog;
  }
</script>

<svelte:window onkeydown={handleColorsDialogKeydown} />

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
