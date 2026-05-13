<script lang="ts">
  import type { Snippet } from "svelte";

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

  let menuEl: HTMLDivElement | undefined = $state();
  let adjustedX = $state(0);
  let adjustedY = $state(0);
  /** Hidden until we've clamped the position, to avoid a visible flash. */
  let menuVisible = $state(false);
  let rafId = 0;

  $effect(() => {
    // Track x and y reactively so we re-run when the caller moves the menu.
    const nx = x;
    const ny = y;
    adjustedX = nx;
    adjustedY = ny;
    menuVisible = false;

    // Cancel any previous pending frame so only the latest values are used.
    cancelAnimationFrame(rafId);
    rafId = requestAnimationFrame(() => {
      const el = menuEl;
      if (!el) {
        menuVisible = true;
        return;
      }
      const rect = el.getBoundingClientRect();
      const vw = window.innerWidth;
      const vh = window.innerHeight;
      let cx = nx;
      let cy = ny;
      // Shift left if menu overflows right edge.
      if (rect.right > vw) cx = Math.max(0, cx - (rect.right - vw));
      // Shift up if menu overflows bottom edge.
      if (rect.bottom > vh) cy = Math.max(0, cy - (rect.bottom - vh));
      adjustedX = cx;
      adjustedY = cy;
      menuVisible = true;
    });
  });

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

  function handleKeydown(e: KeyboardEvent) {
    if (e.key === "Escape") {
      e.stopPropagation();
      onClose();
    }
  }
</script>

<svelte:window onclick={onClose} onkeydown={handleKeydown} />

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div
  bind:this={menuEl}
  class="context-menu"
  class:fading={menuFading}
  style:left="{adjustedX}px"
  style:top="{adjustedY}px"
  style:visibility={menuVisible ? "visible" : "hidden"}
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
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.5);
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
  }

  .context-menu :global(button:hover) {
    background: var(--border-mid);
  }
</style>
