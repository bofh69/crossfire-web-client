<script lang="ts">
  import { getMusicMuted, getSfxMuted, setMusicMuted, setSfxMuted } from '../../lib/sound';
  import { gameEvents } from '../../lib/events';
  import { useConfig, wantConfig, saveCurrentConfig } from '../../lib/init';

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

  let musicMuted = $state(getMusicMuted());
  let sfxMuted = $state(getSfxMuted());
  let fogGrayscale = $state(useConfig.fogGrayscale);

  function toggleMusicMute() {
    musicMuted = !musicMuted;
    setMusicMuted(musicMuted);
    onClose();
  }

  function toggleSfxMute() {
    sfxMuted = !sfxMuted;
    setSfxMuted(sfxMuted);
    onClose();
  }

  function toggleFogGrayscale() {
    fogGrayscale = !fogGrayscale;
    useConfig.fogGrayscale = fogGrayscale;
    wantConfig.fogGrayscale = fogGrayscale;
    saveCurrentConfig();
    onClose();
  }

  function handleZoomIn() {
    gameEvents.emit('zoomIn');
    onClose();
  }

  function handleZoomOut() {
    gameEvents.emit('zoomOut');
    onClose();
  }
</script>

<div class="menu-item">
  <button class="menu-button" onclick={onToggle} oncontextmenu={(e) => { e.preventDefault(); onToggle(); }}>Config</button>
  {#if isOpen}
    <div class="dropdown" class:fading>
      <button
        onclick={toggleMusicMute}
        oncontextmenu={(e) => { e.preventDefault(); toggleMusicMute(); }}
      >{musicMuted ? 'Unmute Music' : 'Mute Music'}</button>
      <button
        onclick={toggleSfxMute}
        oncontextmenu={(e) => { e.preventDefault(); toggleSfxMute(); }}
      >{sfxMuted ? 'Unmute Sound Effects' : 'Mute Sound Effects'}</button>
      <div class="separator"></div>
      <button
        onclick={toggleFogGrayscale}
        oncontextmenu={(e) => { e.preventDefault(); toggleFogGrayscale(); }}
      >{fogGrayscale ? 'Disable Grayscale Fog of War' : 'Enable Grayscale Fog of War'}</button>
      <div class="separator"></div>
      <button
        onclick={handleZoomIn}
        oncontextmenu={(e) => { e.preventDefault(); handleZoomIn(); }}
      >Zoom In</button>
      <button
        onclick={handleZoomOut}
        oncontextmenu={(e) => { e.preventDefault(); handleZoomOut(); }}
      >Zoom Out</button>
    </div>
  {/if}
</div>

<style>
  .separator {
    height: 1px;
    background: var(--border-mid);
    margin: 0.25rem 0;
  }
</style>
