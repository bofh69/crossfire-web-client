<script lang="ts">
  import type { Snippet } from 'svelte';

  interface Props {
    x: number;
    y: number;
    onClose: () => void;
    children: Snippet;
  }

  let { x, y, onClose, children }: Props = $props();

  const MENU_FADE_MS = 2000;
  let menuFading = $state(false);
  let fadeTimer: ReturnType<typeof setTimeout> | null = null;

  function clearFadeTimer() {
    if (fadeTimer !== null) {
      clearTimeout(fadeTimer);
      fadeTimer = null;
    }
  }

  function handleMouseLeave() {
    menuFading = true;
    fadeTimer = setTimeout(() => {
      menuFading = false;
      fadeTimer = null;
      onClose();
    }, MENU_FADE_MS);
  }

  function handleMouseEnter() {
    clearFadeTimer();
    menuFading = false;
  }
</script>

<svelte:window onclick={onClose} />

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div
  class="context-menu"
  class:fading={menuFading}
  style:left="{x}px"
  style:top="{y}px"
  onmouseenter={handleMouseEnter}
  onmouseleave={handleMouseLeave}
  onclick={(e) => e.stopPropagation()}
  role="menu"
  tabindex="-1"
>
  {@render children()}
</div>

<style>
  .context-menu {
    position: fixed;
    background: var(--border);
    border: 1px solid var(--border-light);
    border-radius: 4px;
    display: flex;
    flex-direction: column;
    z-index: 100;
    box-shadow: 0 2px 8px rgba(0,0,0,0.5);
    opacity: 1;
    transition: opacity 2s ease-out;
  }

  .context-menu.fading {
    opacity: 0;
  }

  /*
   * Style buttons rendered via the children snippet.
   * :global() is required because snippet content retains the
   * parent component's CSS scope, not this component's scope.
   */
  .context-menu :global(button) {
    padding: 0.4rem 1rem;
    border: none;
    background: none;
    color: #ddd;
    text-align: left;
    cursor: pointer;
    font-size: 0.8rem;
    display: block;
    width: 100%;
  }

  .context-menu :global(button:hover) {
    background: var(--border-mid);
  }
</style>
