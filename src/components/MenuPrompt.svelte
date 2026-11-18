<script lang="ts">
  /**
   * MenuPrompt — inline prompt used inside context menus to ask for a value:
   * how many items of a stack to act on (e.g. "Drop N…") when `max` is given,
   * otherwise a line of text (e.g. a spell argument).
   *
   * Enter or "OK" calls onConfirm with the value (a count is between 1 and
   * `max`); Escape or "Cancel" calls onCancel.  Keys typed in the input are
   * kept from the game and UI-navigation key handlers.
   */
  interface Props {
    /** The question shown above the input, e.g. "Drop how many?". */
    label: string;
    /** Ask for a count up to this (e.g. the stack size); omit to ask for text. */
    max?: number;
    /** Whether text must be entered (counts are always required). */
    required?: boolean;
    /** Called with the entered value. */
    onConfirm: (value: string) => void;
    /** Called when the prompt is dismissed without a value. */
    onCancel: () => void;
  }

  let { label, max, required = false, onConfirm, onCancel }: Props = $props();

  // svelte-ignore state_referenced_locally
  let value = $state(max === undefined ? "" : "1");
  let inputEl: HTMLInputElement | undefined = $state();

  // Focus and select the input so typing replaces the default.  A menu that
  // opens together with the prompt stays hidden for a frame while it is
  // placed, and hidden inputs can't take focus, so try again then.
  $effect(() => {
    if (!inputEl) return;
    const el = inputEl;
    const focusInput = (triesLeft: number) =>
      requestAnimationFrame(() => {
        el.focus();
        el.select();
        if (document.activeElement !== el && triesLeft > 0) {
          focusInput(triesLeft - 1);
        }
      });
    focusInput(2);
  });

  /** The value to confirm, or null if it is not valid. */
  function validValue(): string | null {
    if (max === undefined) {
      const text = String(value).trim();
      return required && text === "" ? null : text;
    }
    // A whole number limited to 1..max.
    const n = Math.floor(Number(value));
    if (!Number.isFinite(n) || n < 1) return null;
    return String(Math.min(n, max));
  }

  function confirm(): void {
    const valid = validValue();
    if (valid !== null) onConfirm(valid);
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
<div class="menu-prompt" onclick={(e) => e.stopPropagation()}>
  <label>
    {label}
    <span class="value-row">
      {#if max === undefined}
        <input
          type="text"
          class="text-input"
          bind:value
          bind:this={inputEl}
          onkeydown={handleKeydown}
        />
      {:else}
        <input
          type="number"
          min="1"
          {max}
          step="1"
          bind:value
          bind:this={inputEl}
          onkeydown={handleKeydown}
        />
        <span class="value-max">of {max}</span>
      {/if}
    </span>
  </label>
  <div class="prompt-buttons">
    <button type="button" onclick={confirm} disabled={validValue() === null}
      >OK</button
    >
    <button type="button" onclick={onCancel}>Cancel</button>
  </div>
</div>

<style>
  .menu-prompt {
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

  .value-row {
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

  .text-input {
    width: 12rem;
  }

  input:focus {
    outline: none;
    border-color: var(--accent);
  }

  .value-max {
    color: var(--text-dim);
  }

  .prompt-buttons {
    display: flex;
    gap: 0.3rem;
  }

  .prompt-buttons button {
    flex: 1;
  }
</style>
