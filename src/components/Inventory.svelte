<script lang="ts">
  import { clientSendApply, clientSendExamine, clientSendMove } from '../lib/player';
  import { toggleLocked, locateItem } from '../lib/item';
  import { getFaceUrl } from '../lib/image';
  import { getCpl } from '../lib/init';
  import type { Item } from '../lib/protocol';

  interface FlatItem {
    tag: number;
    name: string;
    weight: number;
    nrof: number;
    face: number;
    locked: boolean;
    applied: boolean;
    magical: boolean;
    cursed: boolean;
    unpaid: boolean;
  }

  let playerItems: FlatItem[] = $state([]);
  let groundItems: FlatItem[] = $state([]);
  let contextMenu = $state<{ x: number; y: number; item: FlatItem; isGround: boolean } | null>(null);

  function flattenItems(root: Item | null): FlatItem[] {
    const result: FlatItem[] = [];
    let item = root?.inv ?? null;
    while (item) {
      result.push({
        tag: item.tag,
        name: item.nrof > 1 ? `${item.nrof} ${item.pName || item.dName}` : item.dName,
        weight: item.weight,
        nrof: item.nrof,
        face: item.face,
        locked: item.locked,
        applied: item.applied,
        magical: item.magical,
        cursed: item.cursed,
        unpaid: item.unpaid,
      });
      item = item.next;
    }
    return result;
  }

  export function updateInventory(playerRoot: Item | null, groundRoot: Item | null) {
    playerItems = flattenItems(playerRoot);
    groundItems = flattenItems(groundRoot);
  }

  function handleApply(tag: number) {
    clientSendApply(tag);
  }

  function handleExamine(item: FlatItem) {
    clientSendExamine(item.tag);
    contextMenu = null;
  }

  function handleDrop(item: FlatItem) {
    // Move item to ground (loc = 0)
    clientSendMove(0, item.tag, item.nrof);
    contextMenu = null;
  }

  function handlePickup(item: FlatItem) {
    // Move item from ground to player (loc = player tag)
    const cpl = getCpl();
    const playerTag = cpl?.ob?.tag ?? 0;
    clientSendMove(playerTag, item.tag, item.nrof);
    contextMenu = null;
  }

  function handleLock(item: FlatItem) {
    const realItem = locateItem(item.tag);
    if (realItem) {
      toggleLocked(realItem);
    }
    contextMenu = null;
  }

  function handleContextMenu(e: MouseEvent, item: FlatItem, isGround: boolean) {
    e.preventDefault();
    contextMenu = { x: e.clientX, y: e.clientY, item, isGround };
  }

  function closeContextMenu() {
    contextMenu = null;
  }

  function formatWeight(w: number): string {
    if (w < 1) return `${(w * 1000).toFixed(0)}g`;
    return `${w.toFixed(1)}kg`;
  }
</script>

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="inventory" onclick={closeContextMenu}>
  <div class="inv-section">
    <h3>Inventory ({playerItems.length})</h3>
    <div class="item-list">
      {#each playerItems as item (item.tag)}
        <div
          class="item-row"
          class:applied={item.applied}
          class:cursed={item.cursed}
          class:magical={item.magical}
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
    <div class="item-list">
      {#each groundItems as item (item.tag)}
        <div
          class="item-row"
          onclick={() => handleApply(item.tag)}
          oncontextmenu={(e: MouseEvent) => handleContextMenu(e, item, true)}
        >
          {#if getFaceUrl(item.face)}
            <img src={getFaceUrl(item.face)} alt="" class="item-icon" />
          {:else}
            <span class="item-icon-placeholder">?</span>
          {/if}
          <span class="item-name">{item.name}</span>
          <span class="item-weight">{formatWeight(item.weight)}</span>
        </div>
      {/each}
    </div>
  </div>

  {#if contextMenu}
    <div class="context-menu" style:left="{contextMenu.x}px" style:top="{contextMenu.y}px">
      <button onclick={() => contextMenu && handleExamine(contextMenu.item)}>Examine</button>
      {#if contextMenu.isGround}
        <button onclick={() => contextMenu && handlePickup(contextMenu.item)}>Pickup</button>
      {:else}
        <button onclick={() => contextMenu && handleDrop(contextMenu.item)}>Drop</button>
        <button onclick={() => contextMenu && handleLock(contextMenu.item)}>
          {contextMenu.item.locked ? 'Unlock' : 'Lock'}
        </button>
      {/if}
    </div>
  {/if}
</div>

<style>
  .inventory {
    background: #1a1a1a;
    border: 1px solid #333;
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
    border-top: 1px solid #444;
  }

  h3 {
    margin: 0;
    padding: 0.4rem 0.5rem;
    color: #e0d0b0;
    font-size: 0.8rem;
    background: #252525;
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
    color: #c0c0c0;
  }

  .item-row:hover {
    background: #2a2a2a;
  }

  .item-row.applied {
    color: #aaccff;
  }

  .item-row.cursed {
    color: #ff8888;
  }

  .item-row.magical {
    color: #88ccff;
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
    background: #333;
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
    color: #888;
    font-size: 0.7rem;
    flex-shrink: 0;
  }

  .context-menu {
    position: fixed;
    background: #333;
    border: 1px solid #555;
    border-radius: 4px;
    display: flex;
    flex-direction: column;
    z-index: 100;
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
    background: #444;
  }
</style>
