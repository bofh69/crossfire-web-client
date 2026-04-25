<script lang="ts">
  import { onMount } from 'svelte';
  import { knowledgeItems, knowledgeTypeInfos } from '../lib/commands';
  import type { KnowledgeItem } from '../lib/commands';
  import { gameEvents } from '../lib/events';
  import { getFaceUrl } from '../lib/image';
  import { sendCommand } from '../lib/player';
  import { NDI_NAVY } from '../lib/protocol';
  import ContextMenu from './ContextMenu.svelte';

  let items: KnowledgeItem[] = $state([]);
  let contextMenu = $state<{ x: number; y: number; item: KnowledgeItem } | null>(null);

  function updateKnowledge() {
    items = [...knowledgeItems.values()].sort((a, b) => {
      const tc = a.type.localeCompare(b.type);
      if (tc !== 0) return tc;
      return a.title.localeCompare(b.title);
    });
  }

  function typeLabel(type: string): string {
    return knowledgeTypeInfos.get(type)?.displayName || type;
  }

  function canAttempt(type: string): boolean {
    return knowledgeTypeInfos.get(type)?.attempt ?? false;
  }

  function showInfo(item: KnowledgeItem) {
    gameEvents.emit('drawInfo', NDI_NAVY, item.title);
    sendCommand(`knowledge show ${item.code}`, 0, 1);
  }

  function attemptKnowledge(item: KnowledgeItem) {
    sendCommand(`knowledge attempt ${item.code}`, 0, 1);
  }

  function handleContextMenu(e: MouseEvent, item: KnowledgeItem) {
    e.preventDefault();
    contextMenu = { x: e.clientX - 8, y: e.clientY - 8, item };
  }

  function closeContextMenu() {
    contextMenu = null;
  }

  onMount(() => {
    updateKnowledge();
    const unsub = gameEvents.on('knowledgeUpdate', updateKnowledge);
    return unsub;
  });
</script>

<div class="knowledge-list">
  <h3>Knowledge ({items.length})</h3>
  <div class="knowledge-scroll">
    {#if items.length === 0}
      <p class="empty">No knowledge</p>
    {:else}
      <table>
        <thead>
          <tr>
            <th class="col-face"></th>
            <th>Title</th>
            <th>Type</th>
          </tr>
        </thead>
        <tbody>
          {#each items as item (item.code)}
            <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
            <tr
              onclick={() => showInfo(item)}
              oncontextmenu={(e: MouseEvent) => handleContextMenu(e, item)}
              class="knowledge-row"
            >
              <td class="col-face">
                {#if getFaceUrl(item.face)}
                  <img src={getFaceUrl(item.face)} alt="" class="knowledge-icon" />
                {/if}
              </td>
              <td class="knowledge-title">{item.title}</td>
              <td class="knowledge-type">{typeLabel(item.type)}</td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </div>
</div>

{#if contextMenu}
  {@const item = contextMenu.item}
  <ContextMenu x={contextMenu.x} y={contextMenu.y} onClose={closeContextMenu}>
    <button
      onclick={() => { showInfo(item); closeContextMenu(); }}
      oncontextmenu={(e) => { e.preventDefault(); showInfo(item); closeContextMenu(); }}
    >Info</button>
    {#if canAttempt(item.type)}
      <button
        onclick={() => { attemptKnowledge(item); closeContextMenu(); }}
        oncontextmenu={(e) => { e.preventDefault(); attemptKnowledge(item); closeContextMenu(); }}
      >Attempt</button>
    {/if}
  </ContextMenu>
{/if}

<style>
  .knowledge-list {
    background: var(--bg-panel);
    border: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow: hidden;
  }

  h3 {
    margin: 0;
    padding: 0.4rem 0.5rem;
    color: var(--text-warm);
    font-size: 0.8rem;
    background: var(--bg-mid);
  }

  .knowledge-scroll {
    flex: 1;
    overflow-y: auto;
  }

  .empty {
    color: #666;
    font-size: 0.8rem;
    padding: 0.5rem;
    text-align: center;
  }

  table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.75rem;
  }

  thead th {
    position: sticky;
    top: 0;
    background: var(--bg-mid);
    color: #999;
    text-align: left;
    padding: 0.3rem 0.4rem;
    border-bottom: 1px solid var(--border);
    font-weight: normal;
  }

  .knowledge-row {
    cursor: pointer;
    color: var(--text);
  }

  .knowledge-row:hover {
    background: var(--bg-lighter);
  }

  td {
    padding: 0.25rem 0.4rem;
  }

  .col-face {
    width: 1px;
    padding: 0.1rem 0.2rem;
    white-space: nowrap;
  }

  .knowledge-icon {
    width: 32px;
    height: 32px;
    display: block;
    image-rendering: pixelated;
  }

  .knowledge-title {
    color: #aaddff;
  }

  .knowledge-type {
    color: #999;
    font-size: 0.7rem;
  }
</style>
