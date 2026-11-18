<script lang="ts">
  import type { Snippet } from "svelte";
  import { focusUiNavTarget, isUiNavEnabled } from "../lib/ui_nav";

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
  let menuId = $state(
    `ui-context-menu-${Math.random().toString(36).slice(2, 10)}`,
  );

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
      clampToViewport(el, nx, ny);
      menuVisible = true;
    });

    $effect(() => {
      const el = menuEl;
      if (!el) return;

      const buttons = Array.from(
        el.querySelectorAll<HTMLButtonElement>("button"),
      );
      buttons.forEach((button, index) => {
        button.dataset.uiNavGroup ||= `${menuId}-group`;
        button.dataset.uiNavGroupPolicy ||= "vertical";
        button.dataset.uiNavId ||= `${menuId}-item-${index}`;
        button.dataset.uiNavPanel ||= "menu";
      });
      if (isUiNavEnabled() && buttons.length > 0) {
        requestAnimationFrame(() => {
          const targetId = buttons[0]?.dataset.uiNavId;
          if (targetId) {
            focusUiNavTarget(targetId);
          }
        });
      }
    });
  });

  // A right-click outside the menu closes it.  The capture phase runs before
  // the clicked element's own handler, which may then open a new menu.
  $effect(() => {
    const closeOnOutsideContextMenu = (e: MouseEvent) => {
      if (menuEl && !menuEl.contains(e.target as Node)) onClose();
    };
    window.addEventListener("contextmenu", closeOnOutsideContextMenu, true);
    return () =>
      window.removeEventListener(
        "contextmenu",
        closeOnOutsideContextMenu,
        true,
      );
  });

  // The menu can grow after it is shown (e.g. when a hotbar slot picker is
  // added to it), so keep it inside the viewport whenever its size changes.
  $effect(() => {
    const el = menuEl;
    if (!el) return;
    const observer = new ResizeObserver(() => clampToViewport(el, x, y));
    observer.observe(el);
    return () => observer.disconnect();
  });

  /**
   * Place the menu at (`nx`, `ny`), moved left and up as needed so that `el`
   * fits inside the viewport.
   */
  function clampToViewport(el: HTMLElement, nx: number, ny: number): void {
    const { width, height } = el.getBoundingClientRect();
    adjustedX = Math.max(0, Math.min(nx, window.innerWidth - width));
    adjustedY = Math.max(0, Math.min(ny, window.innerHeight - height));
  }

  function clearFadeTimer() {
    if (fadeTimer !== null) {
      clearTimeout(fadeTimer);
      fadeTimer = null;
    }
  }

  /** Whether a field in the menu has focus, e.g. a count being typed. */
  function isTypingInMenu(): boolean {
    const el = document.activeElement;
    return !!el && !!menuEl?.contains(el) && el.matches("input, select");
  }

  function handleMouseLeave() {
    if (isTypingInMenu()) return;
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
  onfocusin={() => {
    if (isTypingInMenu()) handleMouseEnter();
  }}
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
