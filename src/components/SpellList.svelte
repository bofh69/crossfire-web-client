<script lang="ts">
  import { onMount } from 'svelte';
  import { sendCommand } from '../lib/player';
  import { spells } from '../lib/commands';
  import type { Spell } from '../lib/protocol';
  import { gameEvents } from '../lib/events';
  import { setHotbarSlot } from '../lib/hotbar';
  import HotbarSlotPicker from './HotbarSlotPicker.svelte';

  let spellList: Spell[] = $state([]);
  let contextMenu = $state<{ x: number; y: number; spell: Spell } | null>(null);
  let showSlotPicker = $state(false);

  function updateSpells() {
    spellList = [...spells];
  }

  onMount(() => {
    const unsub = gameEvents.on('spellUpdate', updateSpells);
    return unsub;
  });

  function castSpell(spell: Spell) {
    sendCommand(`cast ${spell.name}`, 0, 1);
  }

  function handleContextMenu(e: MouseEvent, spell: Spell) {
    e.preventDefault();
    showSlotPicker = false;
    contextMenu = { x: e.clientX - 8, y: e.clientY - 8, spell };
  }

  function closeContextMenu() {
    contextMenu = null;
    showSlotPicker = false;
  }

  function handleAddToHotbar(_spell: Spell) {
    showSlotPicker = true;
  }

  function handleSlotSelected(index: number) {
    if (contextMenu) {
      setHotbarSlot(index, {
        label: contextMenu.spell.name,
        command: `cast ${contextMenu.spell.name}`,
      });
    }
    closeContextMenu();
  }
</script>

<svelte:window onclick={closeContextMenu} />

<div class="spell-list">
  <h3>Spells ({spellList.length})</h3>
  <div class="spells-scroll">
    {#if spellList.length === 0}
      <p class="empty">No spells known</p>
    {:else}
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Lv</th>
            <th>Mana</th>
            <th>Grace</th>
            <th>Dam</th>
          </tr>
        </thead>
        <tbody>
          {#each spellList as spell (spell.tag)}
            <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
            <tr
              onclick={() => castSpell(spell)}
              oncontextmenu={(e: MouseEvent) => handleContextMenu(e, spell)}
              class="spell-row"
            >
              <td class="spell-name">{spell.name}</td>
              <td>{spell.level}</td>
              <td>{spell.sp > 0 ? spell.sp : '-'}</td>
              <td>{spell.grace > 0 ? spell.grace : '-'}</td>
              <td>{spell.dam > 0 ? spell.dam : '-'}</td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </div>
</div>

{#if contextMenu}
  {@const spell = contextMenu.spell}
  <!-- svelte-ignore a11y_click_events_have_key_events -->
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div
    class="context-menu"
    style:left="{contextMenu.x}px"
    style:top="{contextMenu.y}px"
    role="menu"
    tabindex="-1"
    onclick={(e) => e.stopPropagation()}
  >
    <button onclick={() => { castSpell(spell); closeContextMenu(); }}>
      Cast {spell.name}
    </button>
    <button onclick={() => handleAddToHotbar(spell)}>
      Add to hotbar…
    </button>
    {#if showSlotPicker}
      <HotbarSlotPicker
        onSelect={handleSlotSelected}
        onCancel={closeContextMenu}
      />
    {/if}
  </div>
{/if}

<style>
  .spell-list {
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

  .spells-scroll {
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

  .spell-row {
    cursor: pointer;
    color: var(--text);
  }

  .spell-row:hover {
    background: var(--bg-lighter);
  }

  td {
    padding: 0.25rem 0.4rem;
  }

  .spell-name {
    color: #aaccff;
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
</style>
