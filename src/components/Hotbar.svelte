<script lang="ts">
  /**
   * Hotbar — 12-slot quick-action bar bound to F1–F12.
   *
   * • Left-click activates the slot.
   * • Right-click shows a small context menu to clear the slot.
   * • When a gamepad is in hotbar-select mode the bar shows an overlay
   *   and highlights the slot the stick is pointing at.
   */
  import { onMount } from 'svelte';
  import {
    getHotbarSlots,
    activateHotbarSlot,
    clearHotbarSlot,
    isHotbarGamepadMode,
    getHotbarGamepadHighlight,
  } from '../lib/hotbar';
  import { getFaceUrl } from '../lib/image';
  import { locateItem } from '../lib/item';
  import { gameEvents } from '../lib/events';

  let currentSlots = $state([...getHotbarSlots()]);
  let gamepadMode = $state(false);
  let gamepadHighlight = $state(-1);
  let contextMenu = $state<{ x: number; y: number; index: number } | null>(null);
  /** Incremented on every inventory change to force reactive face lookups. */
  let inventoryVersion = $state(0);

  onMount(() => {
    const unsubs = [
      gameEvents.on('hotbarUpdate', () => {
        currentSlots = [...getHotbarSlots()];
        gamepadMode = isHotbarGamepadMode();
        gamepadHighlight = getHotbarGamepadHighlight();
      }),
      gameEvents.on('playerUpdate', () => { inventoryVersion++; }),
      gameEvents.on('tick', () => { inventoryVersion++; }),
    ];
    return () => { for (const unsub of unsubs) unsub(); };
  });

  function getSlotFace(slot: { command: string; face?: number; tag?: number }): number | undefined {
    if (slot.tag !== undefined) {
      // inventoryVersion is read here so Svelte tracks it as a dependency.
      void inventoryVersion;
      // Fall back to the stored face when the item is not currently in inventory,
      // so the slot still shows a recognisable icon even when the item is unavailable.
      return locateItem(slot.tag)?.face ?? slot.face;
    }
    return slot.face;
  }

  function handleSlotClick(index: number) {
    activateHotbarSlot(index);
  }

  function handleContextMenu(e: MouseEvent, index: number) {
    e.preventDefault();
    contextMenu = { x: e.clientX - 8, y: e.clientY - 8, index };
  }

  function clearSlot(index: number) {
    clearHotbarSlot(index);
    contextMenu = null;
  }

  function closeContextMenu() {
    contextMenu = null;
  }
</script>

<svelte:window onclick={closeContextMenu} />

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="hotbar" class:gamepad-mode={gamepadMode}>
  {#if gamepadMode}
    <div class="gamepad-hint">🎮 Select slot with stick…</div>
  {/if}
  <div class="slot-row">
    {#each currentSlots as slot, i}
      <button
        class="slot"
        class:filled={slot !== null}
        class:gamepad-highlight={gamepadMode && gamepadHighlight === i}
        onclick={() => handleSlotClick(i)}
        oncontextmenu={(e: MouseEvent) => handleContextMenu(e, i)}
        title={slot ? `F${i + 1}: ${slot.command}` : `F${i + 1} (empty)`}
      >
        <span class="fkey">F{i + 1}</span>
        {#if slot}
          {#if getFaceUrl(getSlotFace(slot) ?? 0)}
            <img
              src={getFaceUrl(getSlotFace(slot) ?? 0)!}
              alt=""
              class="slot-icon"
            />
          {/if}
          <span class="slot-label">{slot.label}</span>
        {/if}
      </button>
    {/each}
  </div>
</div>

{#if contextMenu}
  {@const idx = contextMenu.index}
  <div
    class="context-menu"
    style:left="{contextMenu.x}px"
    style:top="{contextMenu.y}px"
    role="menu"
  >
    <button onclick={() => clearSlot(idx)}>
      Clear F{idx + 1}
    </button>
  </div>
{/if}

<style>
  .hotbar {
    display: flex;
    align-items: center;
    background: var(--bg-panel);
    border-top: 1px solid var(--border);
    padding: 0 0.2rem;
    gap: 0.2rem;
    overflow: hidden;
    height: 100%;
    position: relative;
  }

  .hotbar.gamepad-mode {
    background: #1a1a2e;
    border-top-color: #4466cc;
  }

  .gamepad-hint {
    font-size: 0.65rem;
    color: #7799ee;
    white-space: nowrap;
    padding: 0 0.3rem;
    flex-shrink: 0;
  }

  .slot-row {
    display: flex;
    gap: 2px;
    align-items: center;
    flex: 1;
    overflow: hidden;
    height: 100%;
    padding: 1px 0;
  }

  .slot {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    flex: 1;
    min-width: 0;
    height: 100%;
    padding: 1px 2px;
    border: 1px solid var(--border);
    border-radius: 3px;
    background: var(--bg);
    color: var(--text-dim);
    cursor: pointer;
    font-size: 0.65rem;
    overflow: hidden;
    gap: 1px;
  }

  .slot:hover {
    background: var(--bg-lighter);
    border-color: var(--border-light);
    color: var(--text);
  }

  .slot.filled {
    border-color: var(--accent);
    color: var(--text-warm);
  }

  .slot.filled:hover {
    border-color: var(--border-light);
    background: var(--bg-warm);
  }

  .slot.gamepad-highlight {
    border-color: #4466cc;
    background: #1e2a50;
    color: #aabbff;
    box-shadow: 0 0 4px #4466cc;
  }

  .fkey {
    font-size: 0.55rem;
    color: #777;
    line-height: 1;
    font-weight: bold;
  }

  .slot-icon {
    width: 16px;
    height: 16px;
    image-rendering: pixelated;
    flex-shrink: 0;
  }

  .slot-label {
    width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    text-align: center;
    font-size: 0.6rem;
    line-height: 1.1;
  }

  .context-menu {
    position: fixed;
    background: var(--border);
    border: 1px solid var(--border-light);
    border-radius: 4px;
    display: flex;
    flex-direction: column;
    z-index: 200;
    box-shadow: 0 2px 8px rgba(0,0,0,0.5);
  }

  .context-menu button {
    padding: 0.4rem 1rem;
    border: none;
    background: none;
    color: #ddd;
    text-align: left;
    cursor: pointer;
    font-size: 0.8rem;
  }

  .context-menu button:hover {
    background: var(--border-mid);
    color: var(--danger-text);
  }
</style>
