<script lang="ts">
  import { onMount } from "svelte";
  import { sendCommand } from "../lib/player";
  import { skillNames, spells } from "../lib/commands";
  import type { Spell } from "../lib/protocol";
  import {
    CS_STAT_SKILLINFO,
    SPELL_USAGE_NONE,
    SPELL_USAGE_SPELL,
    SPELL_USAGE_TEXT,
  } from "../lib/protocol";
  import { gameEvents } from "../lib/events";
  import { setHotbarSlot } from "../lib/hotbar";
  import { getFaceUrl } from "../lib/image";
  import { loadConfig, saveConfig } from "../lib/storage";
  import ContextMenu from "./ContextMenu.svelte";
  import HotbarSlotPicker from "./HotbarSlotPicker.svelte";
  import MenuPrompt from "./MenuPrompt.svelte";
  import { capitalizeFirstLetter } from "../lib/misc";

  /** A sortable column of the spell list. */
  type SortKey = "name" | "school" | "level" | "sp" | "grace" | "dam";

  interface Column {
    key: SortKey;
    label: string;
    /** Value to sort by. */
    value: (spell: Spell) => string | number;
  }

  const COLUMNS: Column[] = [
    { key: "name", label: "Name", value: (s) => s.name },
    { key: "school", label: "School", value: (s) => spellSchool(s) },
    { key: "level", label: "Lv", value: (s) => s.level },
    { key: "sp", label: "Mana", value: (s) => s.sp },
    { key: "grace", label: "Grace", value: (s) => s.grace },
    { key: "dam", label: "Dam", value: (s) => s.dam },
  ];

  /** The sort buttons of each header cell; school shares the name's cell. */
  const HEADERS: SortKey[][] = [
    ["name", "school"],
    ["level"],
    ["sp"],
    ["grace"],
    ["dam"],
  ];

  /** Config key of the saved sort order. */
  const SORT_KEY = "spells_sort";

  let spellList: Spell[] = $state([]);
  let contextMenu = $state<{
    spell: Spell;
    targetId: string;
    x: number;
    y: number;
  } | null>(null);
  let showSlotPicker = $state(false);
  /** The argument prompt shown in the context menu, if any. */
  let argumentPrompt = $state<{ invoke: boolean } | null>(null);

  const savedSort = loadConfig<{ key?: string; descending?: boolean } | null>(
    SORT_KEY,
    null,
  );
  let sortKey = $state<SortKey>(
    COLUMNS.find((c) => c.key === savedSort?.key)?.key ?? "name",
  );
  let sortDescending = $state(savedSort?.descending === true);
  let searchText = $state("");

  /** The spells shown: matching the search, in the chosen order. */
  let shownSpells = $derived.by(() => {
    const query = searchText.trim().toLowerCase();
    const column = COLUMNS.find((c) => c.key === sortKey)!;
    const direction = sortDescending ? -1 : 1;
    return spellList
      .filter(
        (spell) =>
          query === "" ||
          spell.name.toLowerCase().includes(query) ||
          spellSchool(spell).toLowerCase().includes(query),
      )
      .sort((a, b) => {
        const va = column.value(a);
        const vb = column.value(b);
        const order =
          typeof va === "number" && typeof vb === "number"
            ? va - vb
            : String(va).localeCompare(String(vb));
        // Ties are listed by name.
        return order * direction || a.name.localeCompare(b.name);
      });
  });

  function updateSpells() {
    spellList = [...spells];
  }

  onMount(() => {
    const cleanups = [
      gameEvents.on("spellUpdate", updateSpells),
      gameEvents.on("uiNavTargetChanged", (targetId) => {
        if (
          contextMenu &&
          targetId !== contextMenu.targetId &&
          !targetId?.startsWith("ui-context-menu-")
        ) {
          closeContextMenu();
        }
      }),
    ];
    // Snapshot spells that arrived before this component mounted (e.g. with
    // loginmethod >= 2 the server sends addspell commands before addme_success,
    // so spells[] is already populated by the time this component is created).
    updateSpells();
    return () => {
      for (const unsub of cleanups) unsub();
    };
  });

  /** The skill a spell is cast with (its school), e.g. "evocation". */
  function spellSchool(spell: Spell): string {
    return skillNames[spell.skillNumber - CS_STAT_SKILLINFO] ?? "";
  }

  /** Whether the spell can't be cast without an argument. */
  function needsArgument(spell: Spell): boolean {
    return (
      spell.usage === SPELL_USAGE_SPELL || spell.usage === SPELL_USAGE_TEXT
    );
  }

  /**
   * Cast (ready) or invoke (cast at once) `spell`, with an optional argument.
   * The spell is named by its tag, as the protocol recommends.
   */
  function castSpell(spell: Spell, invoke = false, argument = "") {
    const command = `${invoke ? "invoke" : "cast"} ${spell.tag}`;
    sendCommand(argument ? `${command} ${argument}` : command, 0, 1);
  }

  /** A click on a spell casts it, or asks for its argument first. */
  function handleRowClick(e: MouseEvent, spell: Spell) {
    if (needsArgument(spell)) {
      openContextMenu(e, spell);
      argumentPrompt = { invoke: false };
    } else {
      castSpell(spell);
    }
  }

  function openContextMenu(e: MouseEvent, spell: Spell) {
    showSlotPicker = false;
    argumentPrompt = null;
    contextMenu = {
      spell,
      targetId: `ui-spell-${spell.tag}`,
      x: e.clientX - 8,
      y: e.clientY - 8,
    };
  }

  function handleContextMenu(e: MouseEvent, spell: Spell) {
    e.preventDefault();
    openContextMenu(e, spell);
  }

  function closeContextMenu() {
    contextMenu = null;
    showSlotPicker = false;
    argumentPrompt = null;
  }

  function handleAddToHotbar(_spell: Spell) {
    argumentPrompt = null;
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

  /** Sort by `key`; choosing the current column again reverses the order. */
  function sortBy(key: SortKey) {
    if (sortKey === key) {
      sortDescending = !sortDescending;
    } else {
      sortKey = key;
      sortDescending = false;
    }
    saveConfig(SORT_KEY, { key: sortKey, descending: sortDescending });
  }

  /** The tooltip of a spell: its description and what it needs. */
  function spellTooltip(spell: Spell): string | undefined {
    const parts = [spell.message];
    if (spell.requirements) parts.push(`Requires: ${spell.requirements}`);
    if (spell.usage === SPELL_USAGE_SPELL) parts.push("Needs a spell name.");
    else if (spell.usage === SPELL_USAGE_TEXT) parts.push("Needs a text.");
    return parts.filter((p) => p).join("\n") || undefined;
  }
</script>

<div class="spell-list">
  <h3
    data-ui-nav-entry="spells"
    data-ui-nav-id="ui-panel-spells"
    data-ui-nav-panel="spells"
  >
    Spells ({searchText.trim()
      ? `${shownSpells.length}/${spellList.length}`
      : spellList.length})
  </h3>
  {#if spellList.length > 0}
    <div class="search-row">
      <input
        type="search"
        bind:value={searchText}
        placeholder="Search spells..."
        aria-label="Search spells by name or school"
        onkeydown={(e) => {
          // Keep typed keys from the game's key handling.
          e.stopPropagation();
          if (e.key === "Escape") {
            searchText = "";
            (e.currentTarget as HTMLInputElement).blur();
          }
        }}
      />
    </div>
  {/if}
  <div class="spells-scroll">
    {#if spellList.length === 0}
      <p class="empty">No spells known</p>
    {:else}
      <table>
        <thead>
          <tr>
            <th class="col-face"></th>
            <!-- The school is shown under the name, to keep the list narrow. -->
            {#each HEADERS as keys (keys[0])}
              <th
                aria-sort={keys.includes(sortKey)
                  ? sortDescending
                    ? "descending"
                    : "ascending"
                  : undefined}
              >
                <!-- Several sort buttons wrap onto lines of their own when narrow. -->
                <span class="sort-group">
                  {#each keys as key (key)}
                    {@const column = COLUMNS.find((c) => c.key === key)!}
                    <button
                      class="sort-btn"
                      class:active={sortKey === key}
                      data-ui-nav-skip="true"
                      title={`Sort by ${column.label.toLowerCase()}`}
                      onclick={() => sortBy(key)}
                    >
                      {column.label}{#if sortKey === key}<span
                          class="sort-arrow"
                          aria-hidden="true"
                        >
                          {sortDescending ? "▾" : "▴"}
                        </span>{/if}
                    </button>
                  {/each}
                </span>
              </th>
            {/each}
          </tr>
        </thead>
        <tbody>
          {#each shownSpells as spell (spell.tag)}
            <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
            <tr
              data-ui-nav-id={`ui-spell-${spell.tag}`}
              data-ui-nav-group="spell-list"
              data-ui-nav-group-policy="vertical"
              data-ui-nav-menu="context"
              data-ui-nav-panel="spells"
              onclick={(e: MouseEvent) => handleRowClick(e, spell)}
              oncontextmenu={(e: MouseEvent) => handleContextMenu(e, spell)}
              title={spellTooltip(spell)}
              class="spell-row"
            >
              <td class="col-face">
                {#if getFaceUrl(spell.face)}
                  <img src={getFaceUrl(spell.face)} alt="" class="spell-icon" />
                {:else}
                  <span class="spell-icon-placeholder"></span>
                {/if}
              </td>
              <td class="spell-name">
                {capitalizeFirstLetter(
                  spell.name,
                )}{#if spell.usage !== SPELL_USAGE_NONE}<span
                    class="spell-arg"
                    aria-label={needsArgument(spell)
                      ? "needs an argument"
                      : "takes an argument"}
                  >
                    …
                  </span>{/if}{#if spellSchool(spell)}<span
                    class="spell-school"
                  >
                    {capitalizeFirstLetter(spellSchool(spell))}
                  </span>{/if}
              </td>
              <td>{spell.level}</td>
              <td>{spell.sp > 0 ? spell.sp : "-"}</td>
              <td>{spell.grace > 0 ? spell.grace : "-"}</td>
              <td>{spell.dam > 0 ? spell.dam : "-"}</td>
            </tr>
          {/each}
        </tbody>
      </table>
      {#if shownSpells.length === 0}
        <p class="empty">No spells match</p>
      {/if}
    {/if}
  </div>
</div>

{#if contextMenu}
  {@const spell = contextMenu.spell}
  {#snippet menuItem(label: string, action: () => void)}
    <button
      onclick={action}
      oncontextmenu={(e) => {
        e.preventDefault();
        action();
      }}
    >
      {label}
    </button>
  {/snippet}
  <ContextMenu x={contextMenu.x} y={contextMenu.y} onClose={closeContextMenu}>
    <!-- Same order as the skill menu: use (invoke) first, then ready. -->
    {#if !needsArgument(spell)}
      {@render menuItem(`Invoke ${spell.name}`, () => {
        castSpell(spell, true);
        closeContextMenu();
      })}
      {@render menuItem(`Ready ${spell.name}`, () => {
        castSpell(spell);
        closeContextMenu();
      })}
    {/if}
    {#if spell.usage !== SPELL_USAGE_NONE}
      {@render menuItem(`Invoke ${spell.name} with…`, () => {
        showSlotPicker = false;
        argumentPrompt = { invoke: true };
      })}
      {@render menuItem(`Ready ${spell.name} with…`, () => {
        showSlotPicker = false;
        argumentPrompt = { invoke: false };
      })}
    {/if}
    {@render menuItem("Add to hotbar…", () => handleAddToHotbar(spell))}
    {#if showSlotPicker}
      <HotbarSlotPicker
        onSelect={handleSlotSelected}
        onCancel={closeContextMenu}
      />
    {/if}
    {#if argumentPrompt}
      {@const invoke = argumentPrompt.invoke}
      <MenuPrompt
        label={`${invoke ? "Invoke" : "Ready"} ${spell.name} with${
          spell.usage === SPELL_USAGE_SPELL ? " which spell" : ""
        }:`}
        required={needsArgument(spell)}
        onConfirm={(argument) => {
          castSpell(spell, invoke, argument);
          closeContextMenu();
        }}
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

  .spell-arg {
    color: var(--text-dim);
    margin-left: 0.15rem;
  }

  .spell-school {
    display: block;
    color: var(--text-dim);
    font-size: 0.65rem;
    /* May break inside the word, so it never widens a narrow list. */
    overflow-wrap: anywhere;
  }

  .search-row {
    display: flex;
    border-bottom: 1px solid var(--border);
  }

  .search-row input {
    flex: 1;
    padding: 0.25rem 0.4rem;
    border: none;
    background: #222;
    color: var(--text-bright);
    font-size: 0.75rem;
  }

  .search-row input:focus {
    outline: none;
    background: #282828;
  }

  .sort-btn {
    padding: 0;
    border: none;
    background: none;
    color: inherit;
    font: inherit;
    cursor: pointer;
  }

  .sort-btn:hover {
    color: var(--text-warm);
  }

  .sort-btn.active {
    color: var(--text-warm);
  }

  .sort-group {
    display: flex;
    flex-wrap: wrap;
    column-gap: 0.5rem;
  }

  .sort-arrow {
    margin-left: 0.2rem;
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
