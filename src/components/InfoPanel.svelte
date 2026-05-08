<script lang="ts">
  import { onMount } from "svelte";
  import {
    extendedCommand,
    completeCommand,
    getCompletionMatches,
  } from "../lib/p_cmd";
  import {
    InputState,
    MSG_TYPE_COMMUNICATION,
    MSG_TYPE_ATTACK,
    MSG_TYPE_VICTIM,
    MSG_TYPE_SKILL,
    MSG_TYPE_SPELL,
    MSG_TYPE_APPLY,
    MSG_TYPE_ITEM,
    MSG_TYPE_SHOP,
    MSG_TYPE_ATTRIBUTE,
    MSG_TYPE_BOOK,
    MSG_TYPE_CARD,
    MSG_TYPE_PAPER,
    MSG_TYPE_SIGN,
    MSG_TYPE_MONUMENT,
    MSG_TYPE_DIALOG,
    MSG_TYPE_ADMIN,
    MSG_TYPE_COMMAND,
    MSG_TYPE_CLIENT,
    MSG_TYPE_MISC,
    MSG_TYPE_MOTD,
    SC_ALWAYS,
  } from "../lib/protocol";
  import { getCpl } from "../lib/init";
  import { type MessageSpan, colorForNdi, parseMarkup } from "../lib/markup";
  import { gameEvents } from "../lib/events";
  import { MSG_BUFFER_MAX, MSG_BUFFER_TRIM } from "../lib/constants";
  import { sendCommand } from "../lib/player";

  let { inputDisabled = false }: { inputDisabled?: boolean } = $props();

  interface Message {
    spans: MessageSpan[];
    rawText: string;
    rawColor: number;
    count: number;
    /** MSG_TYPE_* value from drawExtInfo, or null for plain drawInfo messages. */
    msgType: number | null;
    msgSubtype: number | null;
  }

  // ── Filter definitions ──────────────────────────────────────────────────────

  interface FilterDef {
    id: string;
    icon: string;
    label: string;
    match: (msgType: number | null) => boolean;
  }

  /** The "All messages" toggle — not a category, handled separately. */
  const ALL_FILTER = { id: "all", icon: "📋", label: "All messages" };

  /** Per-category toggles. Each one can be independently enabled/disabled. */
  const CATEGORY_FILTERS: FilterDef[] = [
    {
      id: "communication",
      icon: "💬",
      label: "Communication (chat, tells, party)",
      match: (t) => t === MSG_TYPE_COMMUNICATION,
    },
    {
      id: "combat",
      icon: "⚔️",
      label: "Combat (attacks, victim notifications)",
      match: (t) => t === MSG_TYPE_ATTACK || t === MSG_TYPE_VICTIM,
    },
    {
      id: "skills",
      icon: "🎯",
      label: "Skills & Spells",
      match: (t) => t === MSG_TYPE_SKILL || t === MSG_TYPE_SPELL,
    },
    {
      id: "items",
      icon: "🎒",
      label: "Items, Apply & Shop",
      match: (t) =>
        t === MSG_TYPE_APPLY || t === MSG_TYPE_ITEM || t === MSG_TYPE_SHOP,
    },
    {
      id: "attributes",
      icon: "📊",
      label: "Attribute & stat changes",
      match: (t) => t === MSG_TYPE_ATTRIBUTE,
    },
    {
      id: "reading",
      icon: "📖",
      label: "Reading (books, signs, dialogs)",
      match: (t) =>
        t === MSG_TYPE_BOOK ||
        t === MSG_TYPE_CARD ||
        t === MSG_TYPE_PAPER ||
        t === MSG_TYPE_SIGN ||
        t === MSG_TYPE_MONUMENT ||
        t === MSG_TYPE_DIALOG,
    },
    {
      id: "system",
      icon: "⚙️",
      label: "System & Admin messages",
      match: (t) =>
        t === MSG_TYPE_ADMIN ||
        t === MSG_TYPE_COMMAND ||
        t === MSG_TYPE_CLIENT ||
        t === MSG_TYPE_MISC ||
        t === MSG_TYPE_MOTD,
    },
  ];

  const ALL_CATEGORY_IDS = new Set(CATEGORY_FILTERS.map((f) => f.id));

  let messages: Message[] = $state([]);
  /** Currently active NPC dialog reply options.  Empty array = none shown. */
  let dialogOptions: Array<{ key: string; value: string }> = $state([]);
  /**
   * When true, all messages are shown regardless of `enabledCategories`.
   * Starts true so the panel shows everything by default.
   */
  let showAll = $state(true);
  /**
   * Which category filters are currently toggled on.
   * All categories start enabled so that switching off "All" is non-disruptive.
   */
  let enabledCategories = $state<Set<string>>(new Set(ALL_CATEGORY_IDS));
  let commandInput = $state("");
  let messagesDiv: HTMLDivElement | undefined = $state();
  let inputEl: HTMLInputElement | undefined = $state();

  // Command history: oldest entry at index 0, newest at the end.
  let commandHistory: string[] = $state([]);
  // -1 = not browsing history (live input); counts from newest (0) to oldest (length-1).
  let historyIndex = $state(-1);
  // Current input saved before browsing so we can restore it on ArrowDown.
  let savedInput = $state("");

  let displayMessages = $derived(
    showAll
      ? messages
      : messages.filter((m) =>
          CATEGORY_FILTERS.some(
            (f) => enabledCategories.has(f.id) && f.match(m.msgType),
          ),
        ),
  );

  function toggleAll() {
    showAll = !showAll;
    scrollToBottom();
  }

  function toggleCategory(id: string) {
    if (showAll) {
      // Transition from "show everything" to filter mode.
      // Restore all categories first so nothing unexpectedly disappears, then
      // toggle the clicked one (i.e. the user is "clicking it off").
      showAll = false;
      enabledCategories = new Set(ALL_CATEGORY_IDS);
    }
    const next = new Set(enabledCategories);
    if (next.has(id)) {
      next.delete(id);
    } else {
      next.add(id);
    }
    enabledCategories = next;
    scrollToBottom();
  }

  function addMessage(
    color: number,
    text: string,
    msgType: number | null = null,
    msgSubtype: number | null = null,
  ) {
    const baseColor = colorForNdi(color);
    const lastMsg = messages[messages.length - 1];
    if (
      lastMsg &&
      lastMsg.rawText === text &&
      lastMsg.rawColor === color &&
      lastMsg.msgType === msgType
    ) {
      messages = [
        ...messages.slice(0, -1),
        { ...lastMsg, count: lastMsg.count + 1 },
      ];
    } else {
      const spans = parseMarkup(text, baseColor);
      const newMsg: Message = {
        spans,
        rawText: text,
        rawColor: color,
        count: 1,
        msgType,
        msgSubtype,
      };
      messages = [...messages, newMsg];
      // Keep a reasonable buffer
      if (messages.length > MSG_BUFFER_MAX) {
        messages = messages.slice(-MSG_BUFFER_TRIM);
      }
    }
    // Only scroll if the new/updated message is visible under the current filter.
    const isVisible =
      showAll ||
      CATEGORY_FILTERS.some(
        (f) => enabledCategories.has(f.id) && f.match(msgType),
      );
    if (isVisible) {
      scrollToBottom();
    }
  }

  /**
   * Focus the command input field, optionally pre-filling it with text.
   * Used by the keybinding system when entering command mode.
   */
  function focusInput(prefill?: string) {
    if (prefill !== undefined) {
      commandInput = prefill;
    }
    requestAnimationFrame(() => {
      inputEl?.focus();
      // Place cursor at end of prefilled text.
      if (inputEl && prefill) {
        inputEl.setSelectionRange(prefill.length, prefill.length);
      }
    });
  }

  /**
   * Handle a click on a dialog-option button.
   * Sends the key as a "say" command, records the user-visible option text in
   * the InfoPanel, and removes all option buttons.
   */
  function handleDialogOptionClick(key: string, value: string) {
    // Sanitize: only allow word characters and hyphens (matching the server's
    // key format) so that no unexpected tokens are injected into the command.
    const safeKey = key.replace(/[^\w-]/g, "");
    if (safeKey.length === 0) return;
    sendCommand(`say ${safeKey}`, 0, SC_ALWAYS);
    addMessage(0, ` - ${value}`, MSG_TYPE_COMMUNICATION, null);
    dialogOptions = [];
  }

  onMount(() => {
    const cleanups = [
      gameEvents.on("drawInfo", (color, message) =>
        addMessage(color, message, null, null),
      ),
      gameEvents.on("drawExtInfo", (color, type, subtype, message) =>
        addMessage(color, message, type, subtype),
      ),
      gameEvents.on("focusCommandInput", (prefill) => focusInput(prefill)),
      gameEvents.on("clearMessages", () => {
        messages = [];
      }),
      gameEvents.on("dialogOptions", (options) => {
        dialogOptions = options;
        scrollToBottom();
      }),
      gameEvents.on("clearDialogOptions", () => {
        dialogOptions = [];
      }),
    ];
    return () => {
      for (const unsub of cleanups) unsub();
    };
  });

  function scrollToBottom() {
    requestAnimationFrame(() => {
      if (messagesDiv) {
        messagesDiv.scrollTop = messagesDiv.scrollHeight;
      }
    });
  }

  /** The prefix that Enter/focusInput inserts by default. */
  const CHAT_PREFIX = "chat ";

  function handleKeydown(e: KeyboardEvent) {
    if (e.key === "Enter") {
      // Stop propagation so the global window handler doesn't re-focus us
      // (the Enter binding would call focusCommandInput('chat ') again).
      e.stopPropagation();
      submitCommand();
    } else if (e.key === "Escape") {
      // Leave command mode and return focus to the game.
      commandInput = "";
      historyIndex = -1;
      blurInput();
      e.stopPropagation();
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      if (commandHistory.length === 0) return;
      if (historyIndex === -1) {
        // Start browsing: save live input and jump to the newest entry.
        savedInput = commandInput;
        historyIndex = 0;
      } else if (historyIndex < commandHistory.length - 1) {
        historyIndex++;
      }
      commandInput = commandHistory[commandHistory.length - 1 - historyIndex]!;
      // Move cursor to end after value update.
      requestAnimationFrame(() => {
        inputEl?.setSelectionRange(commandInput.length, commandInput.length);
      });
    } else if (e.key === "ArrowDown") {
      e.preventDefault();
      if (historyIndex === -1) return;
      if (historyIndex > 0) {
        historyIndex--;
        commandInput =
          commandHistory[commandHistory.length - 1 - historyIndex]!;
      } else {
        // Back to live input.
        historyIndex = -1;
        commandInput = savedInput;
      }
      requestAnimationFrame(() => {
        inputEl?.setSelectionRange(commandInput.length, commandInput.length);
      });
    } else if (e.key === "Tab") {
      e.preventDefault();
      const completed = completeCommand(commandInput, commandHistory);
      if (completed !== commandInput) {
        commandInput = completed;
      } else {
        // Input already equals the longest common prefix — show all matches.
        const matches = getCompletionMatches(commandInput, commandHistory);
        if (matches.length > 1) {
          addMessage(0, matches.join("  "));
        }
      }
    }
  }

  function handleBlur() {
    // When the input loses focus, return to Playing state.
    const cpl = getCpl();
    if (cpl && cpl.inputState === InputState.CommandMode) {
      cpl.inputState = InputState.Playing;
    }
  }

  function blurInput() {
    const cpl = getCpl();
    if (cpl) {
      cpl.inputState = InputState.Playing;
    }
    inputEl?.blur();
  }

  function submitCommand() {
    const cmd = commandInput.trim();

    // Empty input or bare chat prefix → just unfocus, don't send anything.
    if (cmd.length === 0 || cmd === CHAT_PREFIX.trim()) {
      commandInput = "";
      historyIndex = -1;
      blurInput();
      return;
    }

    // Append to history, avoiding consecutive duplicates.
    if (
      commandHistory.length === 0 ||
      commandHistory[commandHistory.length - 1] !== cmd
    ) {
      commandHistory = [...commandHistory, cmd];
    }
    historyIndex = -1;
    savedInput = "";

    // Any user command dismisses pending dialog option buttons.
    dialogOptions = [];

    extendedCommand(cmd);
    commandInput = "";
    blurInput();
  }
</script>

<div class="info-panel">
  <div class="filter-sidebar">
    <button
      class="filter-btn"
      class:active={showAll}
      title={ALL_FILTER.label}
      onclick={toggleAll}
      aria-label={ALL_FILTER.label}
      aria-pressed={showAll}>{ALL_FILTER.icon}</button
    >
    {#each CATEGORY_FILTERS as filter}
      <button
        class="filter-btn"
        class:active={enabledCategories.has(filter.id)}
        class:dimmed={showAll}
        title={filter.label}
        onclick={() => toggleCategory(filter.id)}
        aria-label={filter.label}
        aria-pressed={enabledCategories.has(filter.id)}>{filter.icon}</button
      >
    {/each}
  </div>
  <div class="main-content">
    <div class="messages" bind:this={messagesDiv}>
      {#each displayMessages as msg}
        <div class="message">
          {#if msg.count > 1}<span class="repeat-count">{msg.count}×</span
            >{/if}{#each msg.spans as span}<span
              style:color={span.color}
              style:font-weight={span.bold ? "bold" : "normal"}
              style:font-style={span.italic ? "italic" : "normal"}
              style:text-decoration={span.underline ? "underline" : "none"}
              >{span.text}</span
            >{/each}
        </div>
      {/each}
    </div>
    {#if dialogOptions.length > 0}
      <div class="dialog-options">
        {#each dialogOptions as option}
          <button
            class="dialog-option-btn"
            onclick={() => handleDialogOptionClick(option.key, option.value)}
            >{option.value}</button
          >
        {/each}
      </div>
    {/if}
    <div class="input-row" class:disabled={inputDisabled}>
      <input
        type="text"
        bind:value={commandInput}
        bind:this={inputEl}
        onkeydown={handleKeydown}
        onblur={handleBlur}
        placeholder="Type command..."
        disabled={inputDisabled}
      />
      <button onclick={submitCommand} disabled={inputDisabled}>Send</button>
    </div>
  </div>
</div>

<style>
  .info-panel {
    display: flex;
    flex-direction: row;
    height: 100%;
    background: var(--bg-panel);
    border: 1px solid var(--border);
  }

  /* ── Filter sidebar ──────────────────────────────────────────── */

  .filter-sidebar {
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 3px 2px;
    gap: 2px;
    border-right: 1px solid var(--border);
    background: var(--bg-panel);
    flex-shrink: 0;
  }

  .filter-btn {
    width: 28px;
    height: 28px;
    border: 1px solid transparent;
    border-radius: 4px;
    background: transparent;
    cursor: pointer;
    font-size: 14px;
    line-height: 1;
    padding: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    color: var(--text);
    /* Inactive: desaturate the emoji icon */
    filter: grayscale(1) opacity(0.55);
    transition:
      filter 0.1s,
      background 0.12s,
      border-color 0.1s;
  }

  .filter-btn:hover {
    filter: grayscale(0) opacity(1);
    border-color: var(--border-mid);
  }

  .filter-btn.active {
    filter: grayscale(0) opacity(1);
    background: rgba(100, 140, 255, 0.18);
    border-color: transparent;
  }

  .filter-btn.active:hover {
    border-color: var(--border-mid);
  }

  /* Category buttons are visually muted when "All" overrides them:
     keep the grayscale/dim look even though they are technically "enabled". */
  .filter-btn.dimmed:not(:hover) {
    filter: grayscale(1) opacity(0.55);
    background: transparent;
  }

  /* ── Main content (messages + input) ────────────────────────── */

  .main-content {
    display: flex;
    flex-direction: column;
    flex: 1;
    min-width: 0;
  }

  .messages {
    flex: 1;
    overflow-y: auto;
    padding: 0.5rem;
    font-family: var(--mono);
    font-size: 0.85rem;
    line-height: 1.3;
  }

  .message {
    padding: 1px 0;
    word-wrap: break-word;
  }

  .repeat-count {
    display: inline-block;
    font-weight: bold;
    color: #999;
    background: #2e2e2e;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 0 3px;
    font-size: 0.75em;
    margin-right: 0.35em;
    vertical-align: middle;
  }

  .input-row {
    display: flex;
    border-top: 1px solid var(--border);
  }

  .input-row.disabled {
    opacity: 0.4;
    pointer-events: none;
  }

  /* ── Dialog option buttons ───────────────────────────────────── */

  .dialog-options {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: 4px 6px;
    border-top: 1px solid var(--border);
    background: var(--bg-panel);
  }

  .dialog-option-btn {
    width: 100%;
    padding: 0.3rem 0.6rem;
    text-align: left;
    border-width: 1px 1px 1px 0;
    border-style: solid;
    border-color: var(--border-mid);
    border-radius: 3px;
    background: #1e2a1e;
    color: var(--text-bright);
    cursor: pointer;
    font-family: var(--mono);
    font-size: 0.82rem;
  }

  .dialog-option-btn:hover {
    background: #2a3e2a;
    border-color: #5a9a5a;
  }

  input {
    flex: 1;
    padding: 0.4rem 0.5rem;
    border: none;
    background: #222;
    color: var(--text-bright);
    font-family: var(--mono);
    font-size: 0.85rem;
  }

  input:focus {
    outline: none;
    background: #282828;
  }

  button {
    padding: 0.4rem 0.75rem;
    border: none;
    border-left: 1px solid var(--border);
    background: var(--border);
    color: var(--text);
    cursor: pointer;
    font-size: 0.85rem;
  }

  button:hover {
    background: var(--border-mid);
  }
</style>
