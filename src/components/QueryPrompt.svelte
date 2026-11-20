<script module lang="ts">
  import { gameEvents } from "../lib/events";
  import { sendReply } from "../lib/player";
  import {
    CS_QUERY_HIDEINPUT,
    CS_QUERY_SINGLECHAR,
    CS_QUERY_YESNO,
  } from "../lib/protocol";

  /** Whether no modifier key (Alt, Ctrl, Meta) is held for `e`. */
  function hasNoModifiers(e: KeyboardEvent): boolean {
    return !e.ctrlKey && !e.altKey && !e.metaKey;
  }

  /**
   * The state of a `query` command from the server, shown by
   * {@link QueryPrompt} until the user replies.
   */
  export class ServerQuery {
    /** The prompt text; empty when no query is pending. */
    prompt = $state("");
    /** Whether the input should be masked (CS_QUERY_HIDEINPUT). */
    hidden = $state(false);
    /** Whether a single key press is the reply (CS_QUERY_SINGLECHAR). */
    singleChar = $state(false);
    /** Whether the reply is "y" or "n" (CS_QUERY_YESNO). */
    yesNo = $state(false);
    /** The text the user has typed so far. */
    input = $state("");
    /** The last non-empty prompt, reused when the server sends an empty one. */
    #lastPrompt = "";

    /** Whether a query is waiting for a reply. */
    get active(): boolean {
      return this.prompt !== "";
    }

    /**
     * Subscribe to the server's `query` events.
     * @returns A function that unsubscribes.
     */
    listen(): () => void {
      return gameEvents.on("query", (flags, prompt) =>
        this.show(flags, prompt),
      );
    }

    /** Show a query with the `CS_QUERY_*` `flags` and `prompt` text. */
    show(flags: number, prompt: string): void {
      if (prompt) {
        this.#lastPrompt = prompt;
      }
      this.prompt = this.#lastPrompt;
      this.hidden = (flags & CS_QUERY_HIDEINPUT) !== 0;
      this.singleChar = (flags & CS_QUERY_SINGLECHAR) !== 0;
      this.yesNo = (flags & CS_QUERY_YESNO) !== 0;
      this.input = "";
    }

    /** Hide the query without replying. */
    clear(): void {
      this.prompt = "";
      this.singleChar = false;
      this.yesNo = false;
      this.input = "";
    }

    /** Send `answer` to the server and hide the query. */
    reply(answer: string): void {
      sendReply(answer);
      this.clear();
    }

    /**
     * Reply to a yes/no query when `e` is an unmodified "y" or "n" key press.
     * @returns Whether `e` was used as the reply.
     */
    handleYesNoKey(e: KeyboardEvent): boolean {
      if (!this.yesNo || !hasNoModifiers(e)) return false;
      const key = e.key.toLowerCase();
      if (key !== "y" && key !== "n") return false;
      this.reply(key);
      return true;
    }
  }
</script>

<script lang="ts">
  interface Props {
    /** The query to show. */
    query: ServerQuery;
    /** Called after a reply has been sent. */
    onReplied?: () => void;
  }

  let { query, onReplied }: Props = $props();

  let inputEl: HTMLInputElement | undefined = $state();

  // Focus the input whenever it (re-)appears in the DOM.
  $effect(() => {
    inputEl?.focus();
  });

  function reply(answer: string): void {
    query.reply(answer);
    onReplied?.();
  }

  function handleSubmit(e: SubmitEvent): void {
    e.preventDefault();
    const answer = query.input.trim();
    if (answer !== "") reply(answer);
  }

  function handleKeydown(e: KeyboardEvent): void {
    if (query.singleChar && e.key.length === 1 && hasNoModifiers(e)) {
      // The typed character is the reply; no Enter is needed.
      reply(e.key);
      e.preventDefault();
      // Keep the window handler from moving focus to the command input.
      e.stopPropagation();
    } else if (e.key === "Enter") {
      e.stopPropagation();
    }
  }
</script>

<form class="query-form" onsubmit={handleSubmit}>
  <p class="query-text" aria-live="polite">{query.prompt}</p>
  {#if query.yesNo}
    <div class="yesno-buttons">
      <button type="button" aria-keyshortcuts="y" onclick={() => reply("y")}>
        Yes
      </button>
      <button type="button" aria-keyshortcuts="n" onclick={() => reply("n")}>
        No
      </button>
    </div>
  {:else}
    <label>
      <input
        type={query.hidden ? "password" : "text"}
        bind:value={query.input}
        bind:this={inputEl}
        onkeydown={handleKeydown}
      />
    </label>
    {#if !query.singleChar}
      <button type="submit">Submit</button>
    {/if}
  {/if}
</form>

<style>
  .query-form {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .query-text {
    color: var(--text-warm-dim);
    font-size: 0.9rem;
    margin: 0;
  }

  .yesno-buttons {
    display: flex;
    gap: 0.5rem;
  }

  .yesno-buttons button {
    flex: 1;
  }

  label {
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
  }

  input {
    padding: 0.5rem;
    border: 1px solid var(--border-light);
    border-radius: 4px;
    background: var(--bg-lighter);
    color: var(--text-bright);
    font-size: 1rem;
  }

  input:focus {
    outline: none;
    border-color: var(--accent);
  }

  button {
    padding: 0.6rem 1rem;
    border: 1px solid var(--accent);
    border-radius: 4px;
    background: var(--bg-warm);
    color: var(--text-warm);
    font-size: 1rem;
    cursor: pointer;
  }

  button:hover {
    background: var(--bg-warm-hover);
  }
</style>
