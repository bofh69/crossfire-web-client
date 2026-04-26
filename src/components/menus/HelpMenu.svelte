<script lang="ts">
  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

  let showAboutDialog = $state(false);

  function showAbout() {
    showAboutDialog = true;
    onClose();
  }

  function closeAbout() {
    showAboutDialog = false;
  }

  function registerWebCrossfireHandler() {
    try {
      navigator.registerProtocolHandler('web+crossfire', '/?server=%s');
    } catch (e) {
      console.warn('registerProtocolHandler failed:', e);
    }
    onClose();
  }

  export function isDialogActive(): boolean {
    return showAboutDialog;
  }
</script>

<div class="menu-item">
  <button class="menu-button" onclick={onToggle} oncontextmenu={(e) => { e.preventDefault(); onToggle(); }}>Help</button>
  {#if isOpen}
    <div class="dropdown" class:fading>
      <button
        onclick={showAbout}
        oncontextmenu={(e) => { e.preventDefault(); showAbout(); }}
      >About Crossfire Web Client</button>
      <button
        onclick={registerWebCrossfireHandler}
        oncontextmenu={(e) => { e.preventDefault(); registerWebCrossfireHandler(); }}
      >Register as web+crossfire handler</button>
    </div>
  {/if}
</div>

{#if showAboutDialog}
  <div class="dialog-overlay">
    <div class="dialog dialog-wide">
      <p class="dialog-title">About Crossfire Web Client</p>
      <p>
        A web-based client for <a href="http://crossfire.real-time.com/" target="_blank" rel="noopener noreferrer">Crossfire</a>,
        the cooperative multi-player graphical RPG and adventure game.
      </p>
      <p>
        This client is based on the original
        <a href="http://crossfire.real-time.com/" target="_blank" rel="noopener noreferrer">Crossfire GTK client</a>
        and reimplemented for the browser.
      </p>
      <p>
        Source code is available on
        <a href="https://github.com/bofh69/crossfire-web-client" target="_blank" rel="noopener noreferrer">GitHub</a>.
      </p>
      <p class="dialog-credits">
        Built with
        <a href="https://svelte.dev/" target="_blank" rel="noopener noreferrer">Svelte</a>,
        <a href="https://vite.dev/" target="_blank" rel="noopener noreferrer">Vite</a>, and
        <a href="https://www.typescriptlang.org/" target="_blank" rel="noopener noreferrer">TypeScript</a>.
      </p>
      <div class="dialog-buttons">
        <button onclick={closeAbout}>Close</button>
      </div>
    </div>
  </div>
{/if}
