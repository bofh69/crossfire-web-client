<script lang="ts">
  import { onMount } from 'svelte';
  import { sendCommand } from '../lib/player';
  import { spells } from '../lib/commands';
  import type { Spell } from '../lib/protocol';
  import { gameEvents } from '../lib/events';

  let spellList: Spell[] = $state([]);

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
</script>

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
            <tr onclick={() => castSpell(spell)} class="spell-row">
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
</style>
