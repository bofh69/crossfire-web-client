<script lang="ts">
  /**
   * HotbarSlotPicker — inline slot-picker used inside context menus.
   *
   * Displays a 2×6 grid of all 12 hotbar slots.  Each cell shows the F-key
   * label and, if the slot is occupied, a short truncated label of the current
   * occupant.  Clicking a cell calls onSelect(index); pressing Escape or
   * clicking the "Cancel" button calls onCancel().
   */
  import { onMount } from "svelte";
  import { getHotbarSlots } from "../lib/hotbar";
  import { gameEvents } from "../lib/events";

  interface Props {
    onSelect: (index: number) => void;
    onCancel: () => void;
  }

  let { onSelect, onCancel }: Props = $props();

  let currentSlots = $state([...getHotbarSlots()]);

  onMount(() => {
    const unsub = gameEvents.on("hotbarUpdate", () => {
      currentSlots = [...getHotbarSlots()];
    });
    return unsub;
  });
</script>

<!-- svelte-ignore a11y_no_static_element_interactions -->
<!-- svelte-ignore a11y_click_events_have_key_events -->
<div class="slot-picker" onclick={(e) => e.stopPropagation()}>
  <div class="picker-grid">
    {#each currentSlots as slot, i}
      <button
        class="picker-cell"
        class:occupied={slot !== null}
        onclick={() => onSelect(i)}
        title={slot ? slot.command : `F${i + 1} (empty)`}
      >
        <span class="fkey">F{i + 1}</span>
        {#if slot}
          <span class="slot-label">{slot.label}</span>
        {:else}
          <span class="slot-empty">—</span>
        {/if}
      </button>
    {/each}
  </div>
  <button class="cancel-btn" onclick={onCancel}>Cancel</button>
</div>

<style>
  .slot-picker {
    padding: 0.4rem;
    background: var(--bg-mid);
    border-top: 1px solid var(--border-light);
  }

  .picker-grid {
    display: grid;
    grid-template-columns: repeat(6, 1fr);
    gap: 2px;
    margin-bottom: 0.3rem;
  }

  .picker-cell {
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 0.2rem 0.1rem;
    border: 1px solid var(--border);
    border-radius: 3px;
    background: var(--bg);
    color: var(--text-dim);
    cursor: pointer;
    font-size: 0.65rem;
    min-width: 0;
    overflow: hidden;
  }

  .picker-cell:hover {
    background: var(--bg-lighter);
    border-color: var(--accent);
    color: var(--text-bright);
  }

  .picker-cell.occupied {
    border-color: var(--accent);
    color: var(--text-warm);
  }

  .fkey {
    font-weight: bold;
    font-size: 0.6rem;
    color: #999;
  }

  .slot-label {
    width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    text-align: center;
    font-size: 0.6rem;
    color: var(--text-warm-dim);
  }

  .slot-empty {
    color: var(--border-light);
    font-size: 0.65rem;
  }

  .cancel-btn {
    width: 100%;
    padding: 0.2rem;
    border: 1px solid var(--border);
    border-radius: 3px;
    background: none;
    color: var(--text-dim);
    font-size: 0.7rem;
    cursor: pointer;
  }

  .cancel-btn:hover {
    background: var(--bg-lighter);
    color: var(--danger-text);
  }
</style>
