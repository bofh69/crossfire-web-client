<script lang="ts">
  import { tick, onMount } from 'svelte';
  import { clientSendApply, clientSendExamine, clientSendMove } from '../lib/player';
  import { toggleLocked, locateItem, sendMarkObj, sendInscribe } from '../lib/item';
  import { spells } from '../lib/commands';
  import type { Spell } from '../lib/protocol';
  import { getFaceUrl } from '../lib/image';
  import { getCpl } from '../lib/init';
  import type { Item } from '../lib/protocol';
  import { gameEvents } from '../lib/events';
  import { setHotbarSlot } from '../lib/hotbar';
  import HotbarSlotPicker from './HotbarSlotPicker.svelte';

  interface FlatItem {
    tag: number;
    name: string;
    dName: string;
    sName: string;
    weight: number;
    nrof: number;
    face: number;
    locked: boolean;
    applied: boolean;
    magical: boolean;
    cursed: boolean;
    unpaid: boolean;
    open: boolean;
    depth: number;
  }

  let playerItems: FlatItem[] = $state([]);
  let groundItems: FlatItem[] = $state([]);
  let contextMenu = $state<{ x: number; y: number; item: FlatItem; isGround: boolean } | null>(null);
  let menuFading = $state(false);
  let fadeTimer: ReturnType<typeof setTimeout> | null = null;
  let itemCount = $state(0);
  let showSlotPicker = $state(false);
  let showInscribeMenu = $state(false);

  /** Milliseconds after the cursor leaves the context menu before it closes. */
  const MENU_FADE_MS = 2000;

  /** Element refs for preserving scroll positions across inventory updates. */
  let playerListEl: HTMLElement | null = null;
  let groundListEl: HTMLElement | null = null;

  function flattenItems(root: Item | null, depth = 0): FlatItem[] {
    const result: FlatItem[] = [];
    let item = root?.inv ?? null;
    while (item) {
      result.push({
        tag: item.tag,
        name: item.nrof > 1 ? `${item.nrof} ${item.pName || item.dName}` : item.dName,
        dName: item.dName,
        sName: item.sName,
        weight: item.weight,
        nrof: item.nrof,
        face: item.face,
        locked: item.locked,
        applied: item.applied,
        magical: item.magical,
        cursed: item.cursed,
        unpaid: item.unpaid,
        open: item.open,
        depth,
      });
      // If the container is open, show its contents indented below it.
      if (item.open && item.inv) {
        result.push(...flattenItems(item, depth + 1));
      }
      item = item.next;
    }
    return result;
  }

  async function refreshInventory() {
    const playerRoot = getCpl()?.ob ?? null;
    const groundRoot = locateItem(0);

    const playerScrollTop = playerListEl?.scrollTop ?? 0;
    const groundScrollTop = groundListEl?.scrollTop ?? 0;

    playerItems = flattenItems(playerRoot);
    groundItems = flattenItems(groundRoot);
    itemCount = getCpl()?.count ?? 0;

    // Restore scroll positions after Svelte flushes DOM changes, so that
    // inserting/removing items (e.g. moving out of a container) doesn't jump
    // the list to an unexpected position.
    await tick();
    if (playerListEl) playerListEl.scrollTop = playerScrollTop;
    if (groundListEl) groundListEl.scrollTop = groundScrollTop;
  }

  onMount(() => {
    const cleanups = [
      gameEvents.on('playerUpdate', refreshInventory),
      gameEvents.on('tick', refreshInventory),
    ];
    return () => { for (const unsub of cleanups) unsub(); };
  });

  function handleApply(tag: number) {
    clientSendApply(tag);
  }

  function handleExamine(item: FlatItem) {
    clientSendExamine(item.tag);
    contextMenu = null;
  }

  function handleDrop(item: FlatItem) {
    // Move item to ground (loc = 0)
    clientSendMove(0, item.tag, getMoveCount());
    clearCount();
    contextMenu = null;
  }

  function handlePickup(item: FlatItem) {
    // Move item from ground to player (loc = player tag)
    const cpl = getCpl();
    const playerTag = cpl?.ob?.tag ?? 0;
    clientSendMove(playerTag, item.tag, getMoveCount());
    clearCount();
    contextMenu = null;
  }

  function handleLock(item: FlatItem) {
    const realItem = locateItem(item.tag);
    if (realItem) {
      toggleLocked(realItem);
    }
    contextMenu = null;
  }

  function handleMark(item: FlatItem) {
    const realItem = locateItem(item.tag);
    if (realItem) {
      sendMarkObj(realItem);
    }
    contextMenu = null;
  }

  /** Move an item between the player inventory and the open container. */
  function handleMoveToContainer(item: FlatItem) {
    const cpl = getCpl();
    const container = cpl?.container;
    if (!container) { contextMenu = null; return; }
    const count = getMoveCount();
    // If the item is currently in the container, move it to the player inventory.
    // Otherwise move it into the container.
    const realItem = locateItem(item.tag);
    if (realItem?.env?.tag === container.tag) {
      // Move to player inventory
      const playerTag = cpl?.ob?.tag ?? 0;
      clientSendMove(playerTag, item.tag, count);
    } else {
      // Move to container
      clientSendMove(container.tag, item.tag, count);
    }
    clearCount();
    contextMenu = null;
  }

  function handleContextMenu(e: MouseEvent, item: FlatItem, isGround: boolean) {
    e.preventDefault();
    // Cancel any pending fade from a previously open menu.
    clearFadeTimer();
    menuFading = false;
    showSlotPicker = false;
    showInscribeMenu = false;
    // Place the menu so the cursor sits slightly inside the top-left corner.
    contextMenu = { x: e.clientX - 8, y: e.clientY - 8, item, isGround };
  }

  function clearFadeTimer() {
    if (fadeTimer !== null) {
      clearTimeout(fadeTimer);
      fadeTimer = null;
    }
  }

  function closeContextMenu() {
    clearFadeTimer();
    menuFading = false;
    contextMenu = null;
    showSlotPicker = false;
    showInscribeMenu = false;
  }

  function handleAddToHotbar(_item: FlatItem) {
    showSlotPicker = true;
  }

  /** Show the spell submenu for inscribing onto this scroll. */
  function handleInscribe(_item: FlatItem) {
    showSlotPicker = false;
    showInscribeMenu = true;
  }

  /** Send the inscribe command for the chosen spell onto the current item. */
  function handleInscribeSpell(spell: Spell) {
    if (!contextMenu) return;
    sendInscribe(spell.tag, contextMenu.item.tag);
    closeContextMenu();
  }

  function handleSlotSelected(index: number) {
    if (contextMenu) {
      const item = contextMenu.item;
      setHotbarSlot(index, {
        label: item.sName,
        command: `apply ${item.sName}`,
        face: item.face,
        tag: item.tag,
      });
    }
    closeContextMenu();
  }

  function handleMenuMouseLeave() {
    menuFading = true;
    fadeTimer = setTimeout(() => {
      contextMenu = null;
      menuFading = false;
      fadeTimer = null;
    }, MENU_FADE_MS);
  }

  function handleMenuMouseEnter() {
    clearFadeTimer();
    menuFading = false;
  }

  function formatWeight(w: number): string {
    if (w < 0) return ``;
    if (w < 1) return `${(w * 1000).toFixed(0)}g`;
    return `${w.toFixed(1)}kg`;
  }

  /** Return the number of items to move: use cpl.count if set, otherwise 0 (all). */
  function getMoveCount(): number {
    const cpl = getCpl();
    return cpl && cpl.count > 0 ? cpl.count : 0;
  }

  /** Reset the typed item count to zero. */
  function clearCount() {
    const cpl = getCpl();
    if (cpl) cpl.count = 0;
    itemCount = 0;
  }
</script>

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="inventory" onclick={closeContextMenu}>
  <div class="inv-section">
    <h3>
      Inventory ({playerItems.length})
      {#if itemCount > 0}
        <span class="item-count">
          {itemCount}
          <!-- svelte-ignore a11y_click_events_have_key_events -->
          <!-- svelte-ignore a11y_no_static_element_interactions -->
          <span class="count-clear" onclick={clearCount}>✕</span>
        </span>
      {/if}
    </h3>
    <div class="item-list" bind:this={playerListEl}>
      {#each playerItems as item (item.tag)}
        <div
          class="item-row"
          class:applied={item.applied}
          class:cursed={item.cursed}
          class:magical={item.magical}
          style:padding-left="{0.4 + item.depth * 1}rem"
          onclick={() => handleApply(item.tag)}
          oncontextmenu={(e: MouseEvent) => handleContextMenu(e, item, false)}
        >
          {#if getFaceUrl(item.face)}
            <img src={getFaceUrl(item.face)} alt="" class="item-icon" />
          {:else}
            <span class="item-icon-placeholder">?</span>
          {/if}
          <span class="item-name">
            {item.name}
            {#if item.open}📂{/if}
            {#if item.locked}🔒{/if}
            {#if item.applied}*{/if}
            {#if item.unpaid}(unpaid){/if}
          </span>
          <span class="item-weight">{formatWeight(item.weight)}</span>
        </div>
      {/each}
    </div>
  </div>

  <div class="inv-section">
    <h3>Ground ({groundItems.length})</h3>
    <div class="item-list" bind:this={groundListEl}>
      {#each groundItems as item (item.tag)}
        <div
          class="item-row"
          class:cursed={item.cursed}
          class:magical={item.magical}
          style:padding-left="{0.4 + item.depth * 1}rem"
          onclick={() => handleApply(item.tag)}
          oncontextmenu={(e: MouseEvent) => handleContextMenu(e, item, true)}
        >
          {#if getFaceUrl(item.face)}
            <img src={getFaceUrl(item.face)} alt="" class="item-icon" />
          {:else}
            <span class="item-icon-placeholder">?</span>
          {/if}
          <span class="item-name">
            {item.name}
            {#if item.open}📂{/if}
          </span>
          <span class="item-weight">{formatWeight(item.weight)}</span>
        </div>
      {/each}
    </div>
  </div>

  {#if contextMenu}
    {#snippet contextMenuContent()}
      {@const item = contextMenu!.item}
      {@const isGround = contextMenu!.isGround}
      {@const openContainer = getCpl()?.container ?? null}
      {@const realItem = locateItem(item.tag)}
      {@const inContainer = openContainer !== null && realItem?.env?.tag === openContainer.tag}
      {#if isGround}
        <button
          onclick={() => contextMenu && handleExamine(contextMenu.item)}
          oncontextmenu={(e) => { e.preventDefault(); contextMenu && handleExamine(contextMenu.item); }}
        >Examine</button>
        <button
          onclick={() => contextMenu && handlePickup(contextMenu.item)}
          oncontextmenu={(e) => { e.preventDefault(); contextMenu && handlePickup(contextMenu.item); }}
        >Pickup</button>
      {:else}
        {#if openContainer !== null}
          <button
            onclick={() => contextMenu && handleMoveToContainer(contextMenu.item)}
            oncontextmenu={(e) => { e.preventDefault(); contextMenu && handleMoveToContainer(contextMenu.item); }}
          >
            {inContainer ? 'Move to inventory' : 'Move to container'}
          </button>
        {/if}
        <button
          onclick={() => contextMenu && handleDrop(contextMenu.item)}
          oncontextmenu={(e) => { e.preventDefault(); contextMenu && handleDrop(contextMenu.item); }}
        >Drop</button>
        <button
          onclick={() => contextMenu && handleExamine(contextMenu.item)}
          oncontextmenu={(e) => { e.preventDefault(); contextMenu && handleExamine(contextMenu.item); }}
        >Examine</button>
        <button
          onclick={() => contextMenu && handleLock(contextMenu.item)}
          oncontextmenu={(e) => { e.preventDefault(); contextMenu && handleLock(contextMenu.item); }}
        >
          {item.locked ? 'Unlock' : 'Lock'}
        </button>
        <button
          onclick={() => contextMenu && handleMark(contextMenu.item)}
          oncontextmenu={(e) => { e.preventDefault(); contextMenu && handleMark(contextMenu.item); }}
        >Mark</button>
        {#if spells.length > 0}
          <button
            onclick={() => contextMenu && handleInscribe(contextMenu.item)}
            oncontextmenu={(e) => { e.preventDefault(); contextMenu && handleInscribe(contextMenu.item); }}
          >Inscribe…</button>
          {#if showInscribeMenu}
            <div class="submenu">
              {#each spells as spell (spell.tag)}
                <button
                  onclick={() => handleInscribeSpell(spell)}
                  oncontextmenu={(e) => { e.preventDefault(); handleInscribeSpell(spell); }}
                >{spell.name}</button>
              {/each}
            </div>
          {/if}
        {/if}
        <button
          onclick={() => contextMenu && handleAddToHotbar(contextMenu.item)}
          oncontextmenu={(e) => { e.preventDefault(); contextMenu && handleAddToHotbar(contextMenu.item); }}
        >Add to hotbar…</button>
        {#if showSlotPicker}
          <HotbarSlotPicker
            onSelect={handleSlotSelected}
            onCancel={closeContextMenu}
          />
        {/if}
      {/if}
    {/snippet}
    <div
      class="context-menu"
      class:fading={menuFading}
      style:left="{contextMenu.x}px"
      style:top="{contextMenu.y}px"
      onmouseenter={handleMenuMouseEnter}
      onmouseleave={handleMenuMouseLeave}
      onclick={(e) => e.stopPropagation()}
    >
      {@render contextMenuContent()}
    </div>
  {/if}
</div>

<style>
  .inventory {
    background: var(--bg-panel);
    border: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow: hidden;
    position: relative;
  }

  .inv-section {
    flex: 1;
    display: flex;
    flex-direction: column;
    min-height: 0;
  }

  .inv-section + .inv-section {
    border-top: 1px solid var(--border-mid);
  }

  h3 {
    margin: 0;
    padding: 0.4rem 0.5rem;
    color: var(--text-warm);
    font-size: 0.8rem;
    background: var(--bg-mid);
    display: flex;
    align-items: center;
  }

  .item-count {
    margin-left: auto;
    display: flex;
    align-items: center;
    gap: 0.2rem;
    color: #ffcc66;
    font-size: 0.75rem;
    font-weight: normal;
  }

  .count-clear {
    cursor: pointer;
    color: var(--text-dim);
    font-size: 0.7rem;
    padding: 0 0.15rem;
    border-radius: 2px;
  }

  .count-clear:hover {
    color: var(--danger-text);
    background: var(--bg-card);
  }

  .item-list {
    flex: 1;
    overflow-y: auto;
    font-size: 0.78rem;
  }

  .item-row {
    display: flex;
    align-items: center;
    gap: 0.3rem;
    padding: 0.2rem 0.4rem;
    cursor: pointer;
    color: var(--text);
  }

  .item-row:hover {
    background: var(--bg-lighter);
  }

  .item-row.applied {
    color: #aaccff;
  }

  .item-row.magical {
    color: #88ccff;
    background: #1a2535;
  }

  .item-row.cursed {
    color: var(--danger-text);
    background: #351a1a;
  }

  .item-icon {
    width: 24px;
    height: 24px;
    image-rendering: pixelated;
    flex-shrink: 0;
  }

  .item-icon-placeholder {
    width: 24px;
    height: 24px;
    display: flex;
    align-items: center;
    justify-content: center;
    background: var(--border);
    color: #666;
    font-size: 0.7rem;
    flex-shrink: 0;
  }

  .item-name {
    flex: 1;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .item-weight {
    color: var(--text-dim);
    font-size: 0.7rem;
    flex-shrink: 0;
  }

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
  }

  .submenu {
    display: flex;
    flex-direction: column;
    border-top: 1px solid var(--border-mid);
    max-height: 14rem;
    overflow-y: auto;
  }

  .submenu button {
    padding-left: 1.6rem;
    font-style: italic;
  }
</style>
