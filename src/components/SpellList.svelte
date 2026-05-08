<script lang="ts">
  import { onMount } from "svelte";
  import { sendCommand } from "../lib/player";
  import { spells } from "../lib/commands";
  import type { Spell } from "../lib/protocol";
  import { gameEvents } from "../lib/events";
  import { setHotbarSlot } from "../lib/hotbar";
  import { getFaceUrl } from "../lib/image";
  import HotbarSlotPicker from "./HotbarSlotPicker.svelte";
  import ContextMenu from "./ContextMenu.svelte";
  import { capitalizeFirstLetter } from "../lib/misc";

  let spellList: Spell[] = $state([]);
  let contextMenu = $state<{ x: number; y: number; spell: Spell } | null>(null);
  let showSlotPicker = $state(false);

  function updateSpells() {
    spellList = [...spells];
  }

  onMount(() => {
    const unsub = gameEvents.on("spellUpdate", updateSpells);
    // Snapshot spells that arrived before this component mounted (e.g. with
    // loginmethod >= 2 the server sends addspell commands before addme_success,
    // so spells[] is already populated by the time this component is created).
    updateSpells();
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
        face: contextMenu.spell.face,
      });
    }
    closeContextMenu();
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
            <th class="col-face"></th>
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
              title={spell.message || undefined}
              class="spell-row"
            >
              <td class="col-face">
                {#if getFaceUrl(spell.face)}
                  <img src={getFaceUrl(spell.face)} alt="" class="spell-icon" />
                {:else}
                  <span class="spell-icon-placeholder"></span>
                {/if}
              </td>
              <td class="spell-name">{capitalizeFirstLetter(spell.name)}</td>
              <td>{spell.level}</td>
              <td>{spell.sp > 0 ? spell.sp : "-"}</td>
              <td>{spell.grace > 0 ? spell.grace : "-"}</td>
              <td>{spell.dam > 0 ? spell.dam : "-"}</td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </div>
</div>

{#if contextMenu}
  {@const spell = contextMenu.spell}
  <ContextMenu x={contextMenu.x} y={contextMenu.y} onClose={closeContextMenu}>
    <button
      onclick={() => {
        castSpell(spell);
        closeContextMenu();
      }}
    >
      Cast {spell.name}
    </button>
    <button onclick={() => handleAddToHotbar(spell)}> Add to hotbar… </button>
    {#if showSlotPicker}
      <HotbarSlotPicker
        onSelect={handleSlotSelected}
        onCancel={closeContextMenu}
      />
    {/if}
  </ContextMenu>
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

  .col-face {
    width: 28px;
    padding: 0.15rem 0.2rem;
  }

  .spell-icon {
    width: 24px;
    height: 24px;
    image-rendering: pixelated;
    display: block;
  }

  .spell-icon-placeholder {
    width: 24px;
    height: 24px;
    display: block;
  }
</style>
