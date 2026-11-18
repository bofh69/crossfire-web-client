<script lang="ts">
  /**
   * CountPrompt — inline prompt used inside context menus to ask how many
   * items of a stack to act on (e.g. "Drop N…").
   *
   * Enter or "OK" calls onConfirm with a count between 1 and `max`;
   * Escape or "Cancel" calls onCancel.  Keys typed in the input are kept
   * from the game and UI-navigation key handlers.
   */
  interface Props {
    /** The question shown above the input, e.g. "Drop how many?". */
    label: string;
    /** The largest count allowed (the stack size). */
    max: number;
    /** Called with the chosen count. */
    onConfirm: (count: number) => void;
    /** Called when the prompt is dismissed without a count. */
    onCancel: () => void;
  }

  let { label, max, onConfirm, onCancel }: Props = $props();

  let value = $state(1);
  let inputEl: HTMLInputElement | undefined = $state();

  // Focus and select the input so a typed number replaces the default.
  $effect(() => {
    if (!inputEl) return;
    const el = inputEl;
    requestAnimationFrame(() => {
      el.focus();
      el.select();
    });
  });

  /** The typed value as a whole number limited to 1..max, or null if invalid. */
  function validCount(): number | null {
    const n = Math.floor(Number(value));
    if (!Number.isFinite(n) || n < 1) return null;
    return Math.min(n, max);
  }

  function confirm(): void {
    const count = validCount();
    if (count !== null) onConfirm(count);
  }

  function handleKeydown(e: KeyboardEvent): void {
    // The window handlers would treat these keys as game or UI-nav keys.
    e.stopPropagation();
    if (e.key === "Enter") {
      e.preventDefault();
      confirm();
    } else if (e.key === "Escape") {
      e.preventDefault();
      onCancel();
    }
  }
</script>

<!-- svelte-ignore a11y_no_static_element_interactions -->
<!-- svelte-ignore a11y_click_events_have_key_events -->
<div class="count-prompt" onclick={(e) => e.stopPropagation()}>
  <label>
    {label}
    <span class="count-row">
      <input
        type="number"
        min="1"
        {max}
        step="1"
        bind:value
        bind:this={inputEl}
        onkeydown={handleKeydown}
      />
      <span class="count-max">of {max}</span>
    </span>
  </label>
  <div class="count-buttons">
    <button type="button" onclick={confirm} disabled={validCount() === null}
      >OK</button
    >
    <button type="button" onclick={onCancel}>Cancel</button>
  </div>
</div>

<style>
  .count-prompt {
    padding: 0.4rem;
    background: var(--bg-mid);
    border-top: 1px solid var(--border-light);
    display: flex;
    flex-direction: column;
    gap: 0.3rem;
    font-size: 0.8rem;
    color: var(--text);
  }

  label {
    display: flex;
    flex-direction: column;
    gap: 0.2rem;
  }

  .count-row {
    display: flex;
    align-items: center;
    gap: 0.4rem;
  }

  input {
    width: 5rem;
    padding: 0.2rem 0.3rem;
    border: 1px solid var(--border-light);
    border-radius: 3px;
    background: var(--bg-lighter);
    color: var(--text-bright);
    font-size: 0.85rem;
  }

  input:focus {
    outline: none;
    border-color: var(--accent);
  }

  .count-max {
    color: var(--text-dim);
  }

  .count-buttons {
    display: flex;
    gap: 0.3rem;
  }

  .count-buttons button {
    flex: 1;
  }
</style>
