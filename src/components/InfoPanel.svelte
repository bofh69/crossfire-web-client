<script lang="ts">
  import { onMount } from "svelte";
  import { capitalizeFirstLetter } from "../lib/misc";
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
    MSG_TYPE_CLIENT_NOTICE,
    MSG_TYPE_MISC,
    MSG_TYPE_MOTD,
    NDI_WHITE,
    SC_ALWAYS,
  } from "../lib/protocol";
  import { getCpl } from "../lib/init";
  import {
    type MessageSpan,
    colorForNdi,
    getInfoPanelBackgroundColor,
    parseMarkup,
  } from "../lib/markup";
  import { gameEvents } from "../lib/events";
  import { MSG_BUFFER_MAX, MSG_BUFFER_TRIM } from "../lib/constants";
  import { sendCommand } from "../lib/player";
  import ContextMenu from "./ContextMenu.svelte";

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

  interface FilterDef {
    id: string;
    icon: string;
    label: string;
    match: (msgType: number | null) => boolean;
  }

  type PanelViewId = "single" | "left" | "right";

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

  /** Single-view filter state */
  let showAll = $state(true);
  let enabledCategories = $state<Set<string>>(new Set(ALL_CATEGORY_IDS));

  /** Split-view filter state */
  let isSplit = $state(false);
  let leftShowAll = $state(true);
  let rightShowAll = $state(true);
  let leftEnabledCategories = $state<Set<string>>(new Set(ALL_CATEGORY_IDS));
  let rightEnabledCategories = $state<Set<string>>(new Set(ALL_CATEGORY_IDS));

  let commandInput = $state("");
  let singleMessagesDiv: HTMLDivElement | undefined = $state();
  let leftMessagesDiv: HTMLDivElement | undefined = $state();
  let rightMessagesDiv: HTMLDivElement | undefined = $state();
  let inputEl: HTMLInputElement | undefined = $state();

  // Command history: oldest entry at index 0, newest at the end.
  let commandHistory: string[] = $state([]);
  // -1 = not browsing history (live input); counts from newest (0) to oldest (length-1).
  let historyIndex = $state(-1);
  // Current input saved before browsing so we can restore it on ArrowDown.
  let savedInput = $state("");
  let infoPanelBackgroundColor = $state(getInfoPanelBackgroundColor());

  let contextMenu = $state<{
    x: number;
    y: number;
    message: Message;
    view: PanelViewId;
  } | null>(null);

  let displayMessages = $derived(
    showAll
      ? messages
      : messages.filter((m) =>
          CATEGORY_FILTERS.some(
            (f) => enabledCategories.has(f.id) && f.match(m.msgType),
          ),
        ),
  );

  let leftDisplayMessages = $derived(
    leftShowAll
      ? messages
      : messages.filter((m) =>
          CATEGORY_FILTERS.some(
            (f) => leftEnabledCategories.has(f.id) && f.match(m.msgType),
          ),
        ),
  );

  let rightDisplayMessages = $derived(
    rightShowAll
      ? messages
      : messages.filter((m) =>
          CATEGORY_FILTERS.some(
            (f) => rightEnabledCategories.has(f.id) && f.match(m.msgType),
          ),
        ),
  );

  function viewShowAll(view: PanelViewId): boolean {
    if (view === "left") return leftShowAll;
    if (view === "right") return rightShowAll;
    return showAll;
  }

  function viewEnabled(view: PanelViewId): Set<string> {
    if (view === "left") return leftEnabledCategories;
    if (view === "right") return rightEnabledCategories;
    return enabledCategories;
  }

  function setViewFilters(
    view: PanelViewId,
    nextShowAll: boolean,
    nextEnabled: Set<string>,
  ) {
    if (view === "left") {
      leftShowAll = nextShowAll;
      leftEnabledCategories = nextEnabled;
      return;
    }
    if (view === "right") {
      rightShowAll = nextShowAll;
      rightEnabledCategories = nextEnabled;
      return;
    }
    showAll = nextShowAll;
    enabledCategories = nextEnabled;
  }

  function matchingCategoryIds(msgType: number | null): string[] {
    return CATEGORY_FILTERS.filter((f) => f.match(msgType)).map((f) => f.id);
  }

  function isMessageVisibleInView(
    view: PanelViewId,
    msgType: number | null,
  ): boolean {
    if (viewShowAll(view)) return true;
    const enabled = viewEnabled(view);
    return CATEGORY_FILTERS.some((f) => enabled.has(f.id) && f.match(msgType));
  }

  function messageTypeLabel(message: Message): string {
    const ids = matchingCategoryIds(message.msgType);
    if (ids.length === 0) return "this message type";
    return (
      CATEGORY_FILTERS.find((f) => f.id === ids[0])?.label ??
      "this message type"
    );
  }

  function viewContainerFor(view: PanelViewId): HTMLDivElement | undefined {
    if (view === "left") return leftMessagesDiv;
    if (view === "right") return rightMessagesDiv;
    return singleMessagesDiv;
  }

  function scrollViewToBottom(view: PanelViewId) {
    requestAnimationFrame(() => {
      const container = viewContainerFor(view);
      if (container) {
        container.scrollTop = container.scrollHeight;
      }
    });
  }

  function scrollAllViewsToBottom() {
    if (isSplit) {
      scrollViewToBottom("left");
      scrollViewToBottom("right");
    } else {
      scrollViewToBottom("single");
    }
  }

  function toggleAll(view: PanelViewId) {
    setViewFilters(view, !viewShowAll(view), new Set(viewEnabled(view)));
    scrollViewToBottom(view);
  }

  function toggleCategory(view: PanelViewId, id: string) {
    let nextShowAll = viewShowAll(view);
    let nextEnabled = new Set(viewEnabled(view));
    if (nextShowAll) {
      nextShowAll = false;
      nextEnabled = new Set(ALL_CATEGORY_IDS);
    }
    if (nextEnabled.has(id)) {
      nextEnabled.delete(id);
    } else {
      nextEnabled.add(id);
    }
    setViewFilters(view, nextShowAll, nextEnabled);
    scrollViewToBottom(view);
  }

  function stopListeningToMessageType(
    view: PanelViewId,
    msgType: number | null,
  ) {
    const ids = matchingCategoryIds(msgType);
    if (ids.length === 0) return;
    const nextEnabled = viewShowAll(view)
      ? new Set(ALL_CATEGORY_IDS)
      : new Set(viewEnabled(view));
    for (const id of ids) {
      nextEnabled.delete(id);
    }
    setViewFilters(view, false, nextEnabled);
    scrollViewToBottom(view);
  }

  function onlyListenToMessageType(view: PanelViewId, msgType: number | null) {
    const ids = matchingCategoryIds(msgType);
    if (ids.length === 0) return;
    setViewFilters(view, false, new Set(ids));
    scrollViewToBottom(view);
  }

  function splitToSide(side: "left" | "right", msgType: number | null) {
    if (isSplit) return;
    const ids = matchingCategoryIds(msgType);
    if (ids.length === 0) return;

    leftShowAll = showAll;
    rightShowAll = showAll;
    leftEnabledCategories = new Set(enabledCategories);
    rightEnabledCategories = new Set(enabledCategories);

    if (side === "left") {
      onlyListenToMessageType("left", msgType);
      stopListeningToMessageType("right", msgType);
    } else {
      onlyListenToMessageType("right", msgType);
      stopListeningToMessageType("left", msgType);
    }

    isSplit = true;
    requestAnimationFrame(() => {
      scrollViewToBottom("left");
      scrollViewToBottom("right");
    });
  }

  function closeView(view: PanelViewId) {
    if (!isSplit) return;
    if (view !== "left" && view !== "right") return;
    // Merge both panes' filters: show all if either pane showed all,
    // otherwise take the union of their enabled category sets.
    showAll = leftShowAll || rightShowAll;
    enabledCategories = new Set([
      ...leftEnabledCategories,
      ...rightEnabledCategories,
    ]);
    isSplit = false;
    requestAnimationFrame(() => {
      scrollViewToBottom("single");
    });
  }

  function handleMessageContextMenu(
    e: MouseEvent,
    message: Message,
    view: PanelViewId,
  ) {
    e.preventDefault();
    console.log(message);
    contextMenu = { x: e.clientX - 8, y: e.clientY - 8, message, view };
  }

  function closeContextMenu() {
    contextMenu = null;
  }

  function handleStopListeningFromMenu() {
    if (!contextMenu) return;
    stopListeningToMessageType(contextMenu.view, contextMenu.message.msgType);
    closeContextMenu();
  }

  function handleOnlyListeningFromMenu() {
    if (!contextMenu) return;
    onlyListenToMessageType(contextMenu.view, contextMenu.message.msgType);
    closeContextMenu();
  }

  function handleSplitFromMenu(side: "left" | "right") {
    if (!contextMenu) return;
    splitToSide(side, contextMenu.message.msgType);
    closeContextMenu();
  }

  function handleCloseViewFromMenu() {
    if (!contextMenu) return;
    closeView(contextMenu.view);
    closeContextMenu();
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

    if (isSplit) {
      if (isMessageVisibleInView("left", msgType)) scrollViewToBottom("left");
      if (isMessageVisibleInView("right", msgType)) scrollViewToBottom("right");
    } else if (isMessageVisibleInView("single", msgType)) {
      scrollViewToBottom("single");
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
        addMessage(color, message, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE),
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
        scrollAllViewsToBottom();
      }),
      gameEvents.on("clearDialogOptions", () => {
        dialogOptions = [];
      }),
      gameEvents.on("infoPanelColorsChanged", () => {
        infoPanelBackgroundColor = getInfoPanelBackgroundColor();
      }),
    ];
    return () => {
      for (const unsub of cleanups) unsub();
    };
  });

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

  function runUserCommand(cmd: string, blurAfter: boolean): void {
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
    if (blurAfter) {
      blurInput();
    }
  }

  function handleSpanCommandClick(
    command: string | undefined,
    commandKind: "cmd" | "help" | undefined,
    linkText: string,
  ): void {
    if (!command) return;
    const cmd = command.trim();
    if (cmd.length === 0 || cmd === CHAT_PREFIX.trim()) return;
    if (commandKind === "help") {
      const heading = linkText.trim();
      if (heading.length > 0) {
        addMessage(
          NDI_WHITE,
          "[cmd]Help[/cmd] " + capitalizeFirstLetter(heading),
          MSG_TYPE_CLIENT,
          MSG_TYPE_CLIENT_NOTICE,
        );
      }
    }
    runUserCommand(cmd, false);
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

    runUserCommand(cmd, true);
  }
</script>

<div class="info-panel" style:--info-panel-bg={infoPanelBackgroundColor}>
  <div class="message-panels" class:split={isSplit}>
    {#if isSplit}
      <div class="message-pane">
        <div class="message-pane-header">
          <button
            class="close-view-btn"
            title="Close left view"
            aria-label="Close left view"
            onclick={() => closeView("left")}>✕</button
          >
        </div>
        <div class="pane-inner">
          <div class="filter-sidebar">
            <button
              class="filter-btn"
              class:active={leftShowAll}
              title={ALL_FILTER.label}
              onclick={() => toggleAll("left")}
              aria-label={ALL_FILTER.label}
              aria-pressed={leftShowAll}>{ALL_FILTER.icon}</button
            >
            {#each CATEGORY_FILTERS as filter}
              <button
                class="filter-btn"
                class:active={leftEnabledCategories.has(filter.id)}
                class:dimmed={leftShowAll}
                title={filter.label}
                onclick={() => toggleCategory("left", filter.id)}
                aria-label={filter.label}
                aria-pressed={leftEnabledCategories.has(filter.id)}
                >{filter.icon}</button
              >
            {/each}
          </div>
          <div class="main-content">
            <div class="messages" bind:this={leftMessagesDiv}>
              {#each leftDisplayMessages as msg}
                <!-- svelte-ignore a11y_no_static_element_interactions -->
                <div
                  class="message"
                  oncontextmenu={(e) =>
                    handleMessageContextMenu(e, msg, "left")}
                >
                  {#if msg.count > 1}<span class="repeat-count"
                      >{msg.count}×</span
                    >{/if}{#each msg.spans as span}{#if span.command}<button
                        class="message-link"
                        onclick={() =>
                          handleSpanCommandClick(
                            span.command,
                            span.commandKind,
                            span.text,
                          )}
                        style:color={span.color}
                        style:font-weight={span.bold ? "bold" : "normal"}
                        style:font-style={span.italic ? "italic" : "normal"}
                        style:font-family={span.fontFamily}>{span.text}</button
                      >{:else}<span
                        style:color={span.color}
                        style:font-weight={span.bold ? "bold" : "normal"}
                        style:font-style={span.italic ? "italic" : "normal"}
                        style:font-family={span.fontFamily}
                        style:text-decoration={span.underline
                          ? "underline"
                          : "none"}>{span.text}</span
                      >{/if}{/each}
                </div>
              {/each}
            </div>
          </div>
        </div>
      </div>

      <div class="message-pane">
        <div class="message-pane-header">
          <button
            class="close-view-btn"
            title="Close right view"
            aria-label="Close right view"
            onclick={() => closeView("right")}>✕</button
          >
        </div>
        <div class="pane-inner">
          <div class="filter-sidebar">
            <button
              class="filter-btn"
              class:active={rightShowAll}
              title={ALL_FILTER.label}
              onclick={() => toggleAll("right")}
              aria-label={ALL_FILTER.label}
              aria-pressed={rightShowAll}>{ALL_FILTER.icon}</button
            >
            {#each CATEGORY_FILTERS as filter}
              <button
                class="filter-btn"
                class:active={rightEnabledCategories.has(filter.id)}
                class:dimmed={rightShowAll}
                title={filter.label}
                onclick={() => toggleCategory("right", filter.id)}
                aria-label={filter.label}
                aria-pressed={rightEnabledCategories.has(filter.id)}
                >{filter.icon}</button
              >
            {/each}
          </div>
          <div class="main-content">
            <div class="messages" bind:this={rightMessagesDiv}>
              {#each rightDisplayMessages as msg}
                <!-- svelte-ignore a11y_no_static_element_interactions -->
                <div
                  class="message"
                  oncontextmenu={(e) =>
                    handleMessageContextMenu(e, msg, "right")}
                >
                  {#if msg.count > 1}<span class="repeat-count"
                      >{msg.count}×</span
                    >{/if}{#each msg.spans as span}{#if span.command}<button
                        class="message-link"
                        onclick={() =>
                          handleSpanCommandClick(
                            span.command,
                            span.commandKind,
                            span.text,
                          )}
                        style:color={span.color}
                        style:font-weight={span.bold ? "bold" : "normal"}
                        style:font-style={span.italic ? "italic" : "normal"}
                        style:font-family={span.fontFamily}>{span.text}</button
                      >{:else}<span
                        style:color={span.color}
                        style:font-weight={span.bold ? "bold" : "normal"}
                        style:font-style={span.italic ? "italic" : "normal"}
                        style:font-family={span.fontFamily}
                        style:text-decoration={span.underline
                          ? "underline"
                          : "none"}>{span.text}</span
                      >{/if}{/each}
                </div>
              {/each}
            </div>
          </div>
        </div>
      </div>
    {:else}
      <div class="message-pane">
        <div class="pane-inner">
          <div class="filter-sidebar">
            <button
              class="filter-btn"
              class:active={showAll}
              title={ALL_FILTER.label}
              onclick={() => toggleAll("single")}
              aria-label={ALL_FILTER.label}
              aria-pressed={showAll}>{ALL_FILTER.icon}</button
            >
            {#each CATEGORY_FILTERS as filter}
              <button
                class="filter-btn"
                class:active={enabledCategories.has(filter.id)}
                class:dimmed={showAll}
                title={filter.label}
                onclick={() => toggleCategory("single", filter.id)}
                aria-label={filter.label}
                aria-pressed={enabledCategories.has(filter.id)}
                >{filter.icon}</button
              >
            {/each}
          </div>
          <div class="main-content">
            <div class="messages" bind:this={singleMessagesDiv}>
              {#each displayMessages as msg}
                <!-- svelte-ignore a11y_no_static_element_interactions -->
                <div
                  class="message"
                  oncontextmenu={(e) =>
                    handleMessageContextMenu(e, msg, "single")}
                >
                  {#if msg.count > 1}<span class="repeat-count"
                      >{msg.count}×</span
                    >{/if}{#each msg.spans as span}{#if span.command}<button
                        class="message-link"
                        onclick={() =>
                          handleSpanCommandClick(
                            span.command,
                            span.commandKind,
                            span.text,
                          )}
                        style:color={span.color}
                        style:font-weight={span.bold ? "bold" : "normal"}
                        style:font-style={span.italic ? "italic" : "normal"}
                        style:font-family={span.fontFamily}>{span.text}</button
                      >{:else}<span
                        style:color={span.color}
                        style:font-weight={span.bold ? "bold" : "normal"}
                        style:font-style={span.italic ? "italic" : "normal"}
                        style:font-family={span.fontFamily}
                        style:text-decoration={span.underline
                          ? "underline"
                          : "none"}>{span.text}</span
                      >{/if}{/each}
                </div>
              {/each}
            </div>
          </div>
        </div>
      </div>
    {/if}
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

  {#if contextMenu}
    {@const menuContext = contextMenu}
    {@const menuHasType =
      matchingCategoryIds(menuContext.message.msgType).length > 0}
    {#snippet contextMenuContent()}
      <button
        disabled={!menuHasType}
        onclick={handleStopListeningFromMenu}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleStopListeningFromMenu();
        }}>Stop listening to {messageTypeLabel(menuContext.message)}</button
      >
      <button
        disabled={!menuHasType}
        onclick={handleOnlyListeningFromMenu}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleOnlyListeningFromMenu();
        }}>Only listen to {messageTypeLabel(menuContext.message)}</button
      >
      {#if !isSplit && menuHasType}
        <button
          onclick={() => handleSplitFromMenu("left")}
          oncontextmenu={(e) => {
            e.preventDefault();
            handleSplitFromMenu("left");
          }}>Split with this type on left</button
        >
        <button
          onclick={() => handleSplitFromMenu("right")}
          oncontextmenu={(e) => {
            e.preventDefault();
            handleSplitFromMenu("right");
          }}>Split with this type on right</button
        >
      {/if}
      {#if isSplit && menuContext.view !== "single"}
        <button
          onclick={handleCloseViewFromMenu}
          oncontextmenu={(e) => {
            e.preventDefault();
            handleCloseViewFromMenu();
          }}>Close view</button
        >
      {/if}
    {/snippet}
    <ContextMenu x={contextMenu.x} y={contextMenu.y} onClose={closeContextMenu}>
      {@render contextMenuContent()}
    </ContextMenu>
  {/if}
</div>

<style>
  .info-panel {
    --resolved-info-panel-bg: var(--info-panel-bg, var(--bg-panel));
    display: flex;
    flex-direction: column;
    height: 100%;
    background: var(--resolved-info-panel-bg);
    border: 1px solid var(--border);
  }

  .message-panels {
    flex: 1;
    min-height: 0;
    display: flex;
    flex-direction: row;
  }

  .message-pane {
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
  }

  .message-panels.split .message-pane + .message-pane {
    border-left: 1px solid var(--border);
  }

  .message-pane-header {
    height: 18px;
    display: flex;
    justify-content: flex-end;
    align-items: center;
    padding: 0 2px;
    border-bottom: 1px solid var(--border);
    background: var(--bg-mid);
    flex-shrink: 0;
  }

  .close-view-btn {
    border: none;
    border-radius: 2px;
    background: transparent;
    color: var(--text-dim);
    cursor: pointer;
    padding: 0 0.2rem;
    line-height: 1;
    font-size: 0.78rem;
  }

  .close-view-btn:hover {
    background: var(--bg-card);
    color: var(--danger-text);
  }

  .pane-inner {
    flex: 1;
    min-height: 0;
    display: flex;
    flex-direction: row;
  }

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

  .filter-btn.dimmed:not(:hover) {
    filter: grayscale(1) opacity(0.55);
    background: transparent;
  }

  .main-content {
    display: flex;
    flex-direction: column;
    flex: 1;
    min-width: 0;
    min-height: 0;
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

  .message:hover {
    background: rgba(255, 255, 255, 0.03);
  }

  .message .message-link {
    border: none;
    background: transparent;
    color: inherit;
    cursor: pointer;
    display: inline;
    font-family: inherit;
    font-size: inherit;
    line-height: inherit;
    padding: 0;
    text-decoration: underline;
  }

  .message .message-link:hover {
    color: #8fc1ff;
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
    flex-shrink: 0;
  }

  .input-row.disabled {
    opacity: 0.4;
    pointer-events: none;
  }

  .dialog-options {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: 4px 6px;
    border-top: 1px solid var(--border);
    background: var(--resolved-info-panel-bg);
    flex-shrink: 0;
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
