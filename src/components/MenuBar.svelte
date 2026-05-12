<script lang="ts">
  import { onMount } from "svelte";
  import PickupMenu from "./PickupMenu.svelte";
  import ConnectionMenu from "./menus/ConnectionMenu.svelte";
  import ConfigMenu from "./menus/ConfigMenu.svelte";
  import InfoMenu from "./menus/InfoMenu.svelte";
  import KeyboardMenu from "./menus/KeyboardMenu.svelte";
  import GamepadMenu from "./menus/GamepadMenu.svelte";
  import { gameEvents } from "../lib/events";

  interface Props {
    onDisconnect: () => void;
  }
  let { onDisconnect }: Props = $props();

  let activeMenu = $state<string | null>(null);
  let pickupMenu: PickupMenu | undefined = $state();
  let keyboardMenu: KeyboardMenu | undefined = $state();
  let gamepadMenu: GamepadMenu | undefined = $state();
  let configMenu: ConfigMenu | undefined = $state();
  let infoMenu: InfoMenu | undefined = $state();

  /** Milliseconds after the cursor leaves the menu-bar before the open dropdown closes. */
  const MENU_FADE_MS = 2000;

  let menuFading = $state(false);
  let menuFadeTimer: ReturnType<typeof setTimeout> | null = null;

  /** Persist the pickup mode so it survives menu close/reopen. */
  let currentPickupMode = $state(0x80000000 >>> 0); // PU_NEWMODE

  /** The currently readied range item (spell, skill, bow, etc.). */
  let currentRange = $state("");

  onMount(() => {
    const cleanups = [
      gameEvents.on("pickupUpdate", setPickupMode),
      gameEvents.on("statsUpdate", (stats) => {
        if (stats.range !== undefined) currentRange = stats.range;
      }),
    ];
    return () => {
      for (const unsub of cleanups) unsub();
    };
  });

  function setPickupMode(mode: number) {
    currentPickupMode = mode >>> 0;
    pickupMenu?.setPickupMode(currentPickupMode);
  }

  function toggleMenu(menu: string) {
    clearMenuFadeTimer();
    menuFading = false;
    activeMenu = activeMenu === menu ? null : menu;
  }

  function closeMenu() {
    clearMenuFadeTimer();
    menuFading = false;
    activeMenu = null;
  }

  function clearMenuFadeTimer() {
    if (menuFadeTimer !== null) {
      clearTimeout(menuFadeTimer);
      menuFadeTimer = null;
    }
  }

  function handleMenuBarMouseLeave() {
    if (activeMenu === null) return;
    menuFading = true;
    menuFadeTimer = setTimeout(() => {
      activeMenu = null;
      menuFading = false;
      menuFadeTimer = null;
    }, MENU_FADE_MS);
  }

  function handleMenuBarMouseEnter() {
    clearMenuFadeTimer();
    menuFading = false;
  }

  /** Returns true while any key-capture, confirm, or about dialog is showing. */
  export function isDialogActive(): boolean {
    return (
      (keyboardMenu?.isDialogActive() ?? false) ||
      (gamepadMenu?.isDialogActive() ?? false) ||
      (configMenu?.isDialogActive() ?? false) ||
      (infoMenu?.isDialogActive() ?? false)
    );
  }
</script>

<svelte:window onclick={closeMenu} />

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div
  class="menu-bar"
  onclick={(e: MouseEvent) => e.stopPropagation()}
  onmouseenter={handleMenuBarMouseEnter}
  onmouseleave={handleMenuBarMouseLeave}
>
  <ConnectionMenu
    fading={menuFading}
    isOpen={activeMenu === "file"}
    onToggle={() => toggleMenu("file")}
    onClose={closeMenu}
    {onDisconnect}
  />

  <div class="menu-item">
    <button
      class="menu-button"
      onclick={() => toggleMenu("pickup")}
      oncontextmenu={(e) => {
        e.preventDefault();
        toggleMenu("pickup");
      }}>Pickup</button
    >
    {#if activeMenu === "pickup"}
      <!-- svelte-ignore a11y_click_events_have_key_events -->
      <!-- svelte-ignore a11y_no_static_element_interactions -->
      <div
        class="dropdown"
        class:fading={menuFading}
        onclick={(e: MouseEvent) => e.stopPropagation()}
      >
        <PickupMenu bind:this={pickupMenu} initialMode={currentPickupMode} />
      </div>
    {/if}
  </div>

  <KeyboardMenu
    bind:this={keyboardMenu}
    fading={menuFading}
    isOpen={activeMenu === "keyboard"}
    onToggle={() => toggleMenu("keyboard")}
    onClose={closeMenu}
  />
  <GamepadMenu
    bind:this={gamepadMenu}
    fading={menuFading}
    isOpen={activeMenu === "gamepad"}
    onToggle={() => toggleMenu("gamepad")}
    onClose={closeMenu}
  />
  <ConfigMenu
    bind:this={configMenu}
    fading={menuFading}
    isOpen={activeMenu === "config"}
    onToggle={() => toggleMenu("config")}
    onClose={closeMenu}
  />
  <InfoMenu
    bind:this={infoMenu}
    fading={menuFading}
    isOpen={activeMenu === "help"}
    onToggle={() => toggleMenu("help")}
    onClose={closeMenu}
  />

  <div class="spacer"></div>
  {#if currentRange}
    <span class="range-label">{currentRange}</span>
  {/if}
  <span class="title">Crossfire</span>
</div>

<style>
  .menu-bar {
    display: flex;
    align-items: center;
    background: var(--bg-mid);
    border-bottom: 1px solid var(--border);
    height: 28px;
    padding: 0 0.25rem;
    font-size: 0.8rem;
    user-select: none;
  }

  .spacer {
    flex: 1;
  }

  .range-label {
    color: #a0b0c0;
    font-size: 0.75rem;
    padding-right: 0.75rem;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    max-width: 200px;
  }

  .title {
    color: var(--accent);
    font-size: 0.75rem;
    padding-right: 0.5rem;
  }
</style>
