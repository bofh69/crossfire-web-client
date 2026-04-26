<script lang="ts">
  import { getMusicMuted, getSfxMuted, setMusicMuted, setSfxMuted } from '../../lib/sound';

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

  let musicMuted = $state(getMusicMuted());
  let sfxMuted = $state(getSfxMuted());

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
</script>

<div class="menu-item">
  <button class="menu-button" onclick={onToggle} oncontextmenu={(e) => { e.preventDefault(); onToggle(); }}>Sound</button>
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
    </div>
  {/if}
</div>
