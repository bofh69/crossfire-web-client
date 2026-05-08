<script lang="ts">
  import {
    getMusicVolume,
    getSfxVolume,
    setMusicVolume,
    setSfxVolume,
  } from "../../lib/sound";
  import { gameEvents } from "../../lib/events";
  import { useConfig, wantConfig, saveCurrentConfig } from "../../lib/init";

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
</script>

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
</style>
