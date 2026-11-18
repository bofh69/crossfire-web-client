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
  import { loadConfig, saveConfig } from "../lib/storage";
  import { recordUiInteraction } from "../lib/websocket-recording";
  import ContextMenu from "./ContextMenu.svelte";

  let { inputDisabled = false }: { inputDisabled?: boolean } = $props();

  interface Message {
    spans: MessageSpan[];
    rawText: string;
    /** The text without markup, for search and copying. */
    plainText: string;
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

  /** Config key of the saved filter and split-view settings. */
  const FILTERS_KEY = "info_filters";
  /** Config key of the saved command history. */
  const HISTORY_KEY = "info_commandHistory";
  /** How many commands the saved history keeps. */
  const MAX_SAVED_HISTORY = 100;
  /** Commands that take a password, which are never saved in the history. */
  const UNSAVED_COMMANDS = new Set(["dm", "passwd"]);

  /** Whether `cmd` may be saved in the browser's command history. */
  function isSavableCommand(cmd: string): boolean {
    const name = cmd.trim().split(/\s+/)[0]?.toLowerCase() ?? "";
    return !UNSAVED_COMMANDS.has(name);
  }

  /** Filter settings of one view, as saved. */
  interface SavedViewFilters {
    showAll: boolean;
    categories: string[];
  }

  /** Filter and split-view settings, as saved. */
  interface SavedFilters {
    split: boolean;
    single: SavedViewFilters;
    left: SavedViewFilters;
    right: SavedViewFilters;
  }

  /** Saved filters of one view, or "all messages" when missing or invalid. */
  function loadViewFilters(saved: unknown): {
    showAll: boolean;
    enabled: Set<string>;
  } {
    const view = saved as Partial<SavedViewFilters> | null;
    if (!view || !Array.isArray(view.categories)) {
      return { showAll: true, enabled: new Set(ALL_CATEGORY_IDS) };
    }
    return {
      showAll: view.showAll !== false,
      enabled: new Set(
        view.categories.filter((id) => ALL_CATEGORY_IDS.has(id)),
      ),
    };
  }

  const savedFilters = loadConfig<Partial<SavedFilters> | null>(
    FILTERS_KEY,
    null,
  );
  const savedSingle = loadViewFilters(savedFilters?.single);
  const savedLeft = loadViewFilters(savedFilters?.left);
  const savedRight = loadViewFilters(savedFilters?.right);

  let messages: Message[] = $state([]);
  /** Currently active NPC dialog reply options.  Empty array = none shown. */
  let dialogOptions: Array<{ key: string; value: string }> = $state([]);

  /** Single-view filter state */
  let showAll = $state(savedSingle.showAll);
  let enabledCategories = $state<Set<string>>(savedSingle.enabled);

  /** Split-view filter state */
  let isSplit = $state(savedFilters?.split === true);
  let leftShowAll = $state(savedLeft.showAll);
  let rightShowAll = $state(savedRight.showAll);
  let leftEnabledCategories = $state<Set<string>>(savedLeft.enabled);
  let rightEnabledCategories = $state<Set<string>>(savedRight.enabled);

  // Save the filter and split-view settings whenever they change.
  $effect(() => {
    const saved: SavedFilters = {
      split: isSplit,
      single: { showAll, categories: [...enabledCategories] },
      left: { showAll: leftShowAll, categories: [...leftEnabledCategories] },
      right: { showAll: rightShowAll, categories: [...rightEnabledCategories] },
    };
    saveConfig(FILTERS_KEY, saved);
  });

  /** Whether the search bar is shown, and the text searched for. */
  let searchOpen = $state(false);
  let searchText = $state("");
  let searchEl: HTMLInputElement | undefined = $state();
  /** The search text in lower case, or "" when not searching. */
  let searchQuery = $derived(searchOpen ? searchText.trim().toLowerCase() : "");

  let commandInput = $state("");
  let singleMessagesDiv: HTMLDivElement | undefined = $state();
  let leftMessagesDiv: HTMLDivElement | undefined = $state();
  let rightMessagesDiv: HTMLDivElement | undefined = $state();

  /** Per view: whether the reader has scrolled up away from the newest text. */
  let scrolledUp = $state<Record<PanelViewId, boolean>>({
    left: false,
    right: false,
    single: false,
  });
  /** Per view: whether new text arrived while the reader was scrolled up. */
  let hasUnread = $state<Record<PanelViewId, boolean>>({
    left: false,
    right: false,
    single: false,
  });
  /** Per view: bumped on each unseen message to replay the flare animation. */
  let unreadFlare = $state<Record<PanelViewId, number>>({
    left: 0,
    right: 0,
    single: 0,
  });
  let inputEl: HTMLInputElement | undefined = $state();

  // Command history: oldest entry at index 0, newest at the end.  It is
  // saved, so it survives a reload.
  let commandHistory: string[] = $state(
    loadConfig<unknown[]>(HISTORY_KEY, [])
      .filter((cmd): cmd is string => typeof cmd === "string")
      .slice(-MAX_SAVED_HISTORY),
  );
  // -1 = not browsing history (live input); counts from newest (0) to oldest (length-1).
  let historyIndex = $state(-1);
  // Current input saved before browsing so we can restore it on ArrowDown.
  let savedInput = $state("");
  let infoPanelBackgroundColor = $state(getInfoPanelBackgroundColor());

  let contextMenu = $state<{
    x: number;
    y: number;
    message: Message;
    selectedText: string | null;
    view: PanelViewId;
  } | null>(null);

  /** Whether `message` contains the search text (always true when not searching). */
  function matchesSearch(message: Message): boolean {
    return (
      searchQuery === "" ||
      message.plainText.toLowerCase().includes(searchQuery)
    );
  }

  /** The messages a view shows with these filter settings and the search. */
  function filterMessages(all: boolean, enabled: Set<string>): Message[] {
    if (all && searchQuery === "") return messages;
    return messages.filter(
      (m) =>
        (all ||
          CATEGORY_FILTERS.some(
            (f) => enabled.has(f.id) && f.match(m.msgType),
          )) &&
        matchesSearch(m),
    );
  }

  let displayMessages = $derived(filterMessages(showAll, enabledCategories));
  let leftDisplayMessages = $derived(
    filterMessages(leftShowAll, leftEnabledCategories),
  );
  let rightDisplayMessages = $derived(
    filterMessages(rightShowAll, rightEnabledCategories),
  );
  /** Number of messages that contain the search text. */
  let searchMatchCount = $derived(
    searchQuery === "" ? 0 : messages.filter(matchesSearch).length,
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

  /** Max distance in px from the bottom still counted as "at the bottom". */
  const AUTO_SCROLL_SLACK_PX = 24;

  /**
   * Whether a view is scrolled to (or very near) its bottom, so new messages
   * should keep it pinned there instead of interrupting the reader.
   */
  function isViewNearBottom(view: PanelViewId): boolean {
    const container = viewContainerFor(view);
    if (!container) return true;
    return (
      container.scrollHeight - container.scrollTop - container.clientHeight <=
      AUTO_SCROLL_SLACK_PX
    );
  }

  /** Tracks whether a view is scrolled up, clearing unread state at the bottom. */
  function handleMessagesScroll(view: PanelViewId) {
    const up = !isViewNearBottom(view);
    scrolledUp[view] = up;
    if (!up) hasUnread[view] = false;
  }

  /** Marks that a message arrived in a view the reader has scrolled up in. */
  function markUnread(view: PanelViewId) {
    hasUnread[view] = true;
    unreadFlare[view]++;
  }

  /** The message views currently shown. */
  function shownViews(): PanelViewId[] {
    return isSplit ? ["left", "right"] : ["single"];
  }

  function scrollAllViewsToBottom() {
    for (const view of shownViews()) scrollViewToBottom(view);
  }

  /** Fraction of a view's height that Page Up/Down scroll it by. */
  const PAGE_SCROLL_FRACTION = 0.9;

  /** Scroll every shown view one page up (-1) or down (1). */
  function pageAllViews(direction: -1 | 1) {
    for (const view of shownViews()) {
      const container = viewContainerFor(view);
      container?.scrollBy({
        top: direction * container.clientHeight * PAGE_SCROLL_FRACTION,
      });
    }
  }

  /** The data-ui-nav-id of the `id` filter button ("all" or a category). */
  function filterNavId(view: PanelViewId, id: string): string {
    return view === "single"
      ? `ui-info-filter-${id}`
      : `ui-info-${view}-filter-${id}`;
  }

  function toggleAll(view: PanelViewId) {
    recordUiInteraction("filterClick", {
      selector: `[data-ui-nav-id="${filterNavId(view, "all")}"]`,
      group: "info",
      view,
      id: "all",
    });
    setViewFilters(view, !viewShowAll(view), new Set(viewEnabled(view)));
    scrollViewToBottom(view);
  }

  function toggleCategory(view: PanelViewId, id: string) {
    recordUiInteraction("filterClick", {
      selector: `[data-ui-nav-id="${filterNavId(view, id)}"]`,
      group: "info",
      view,
      id,
    });
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

  /**
   * Returns whether the provided DOM node belongs to the info panel tree.
   *
   * @param node A node to test.
   * @returns True when the node resolves inside `.info-panel`.
   */
  function isNodeInInfoPanel(node: Node | null): boolean {
    if (!node) return false;
    const element = node instanceof Element ? node : node.parentElement;
    return !!element?.closest(".info-panel");
  }

  /**
   * Returns the currently selected text when the active selection is fully
   * inside the info panel.
   *
   * @returns The trimmed selected text, or `null` when no valid info panel
   * selection exists.
   */
  function selectedInfoPanelText(): string | null {
    const selection = window.getSelection();
    if (!selection || selection.isCollapsed) return null;
    if (
      !isNodeInInfoPanel(selection.anchorNode) ||
      !isNodeInInfoPanel(selection.focusNode)
    ) {
      return null;
    }
    const text = selection.toString().trim();
    return text.length > 0 ? text : null;
  }

  /**
   * Copies text to clipboard using the modern Clipboard API, with a fallback
   * to the legacy `document.execCommand("copy")` path when needed.
   *
   * @param text The text to copy.
   * @returns Resolves when copy succeeds.
   * @throws When both modern and fallback copy mechanisms fail.
   */
  async function copyTextToClipboard(text: string): Promise<void> {
    if (navigator.clipboard?.writeText) {
      try {
        await navigator.clipboard.writeText(text);
        return;
      } catch {
        // Fall back to the legacy copy method when modern clipboard API fails.
      }
    }
    const textArea = document.createElement("textarea");
    textArea.setAttribute("aria-hidden", "true");
    textArea.setAttribute("readonly", "");
    textArea.setAttribute("tabindex", "-1");
    textArea.value = text;
    textArea.style.position = "fixed";
    textArea.style.opacity = "0";
    document.body.appendChild(textArea);
    try {
      textArea.focus();
      textArea.select();
      const copied =
        typeof document.execCommand === "function" &&
        document.execCommand("copy");
      if (!copied) {
        throw new Error("Clipboard copy command was not successful.");
      }
    } finally {
      document.body.removeChild(textArea);
    }
  }

  /**
   * Opens the info panel context menu for a specific message and records any
   * currently selected info panel text.
   *
   * @param e The right-click mouse event.
   * @param message The clicked message.
   * @param view The view in which the message appears.
   */
  function handleMessageContextMenu(
    e: MouseEvent,
    message: Message,
    view: PanelViewId,
  ) {
    e.preventDefault();
    contextMenu = {
      x: e.clientX - 8,
      y: e.clientY - 8,
      message,
      selectedText: selectedInfoPanelText(),
      view,
    };
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

  /** Copies the text of the right-clicked message. */
  async function handleCopyMessageFromMenu() {
    if (!contextMenu) return;
    try {
      await copyTextToClipboard(contextMenu.message.plainText);
    } catch (error) {
      console.error("Failed to copy message from info panel:", error);
    } finally {
      closeContextMenu();
    }
  }

  /**
   * Copies currently selected info panel text using the context menu action.
   *
   * @returns Resolves after copy has been attempted and the menu is closed.
   */
  async function handleCopyFromMenu() {
    if (!contextMenu?.selectedText) return;
    try {
      await copyTextToClipboard(contextMenu.selectedText);
    } catch (error) {
      console.error("Failed to copy selected text from info panel:", error);
    } finally {
      closeContextMenu();
    }
  }

  function addMessage(
    color: number,
    text: string,
    msgType: number | null = null,
    msgSubtype: number | null = null,
  ) {
    const baseColor = colorForNdi(color);
    // Sample scroll positions before the DOM grows, so readers scrolled up
    // in the history aren't yanked down by new messages.
    const stickLeft = isSplit && isViewNearBottom("left");
    const stickRight = isSplit && isViewNearBottom("right");
    const stickSingle = !isSplit && isViewNearBottom("single");
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
        plainText: spans.map((span) => span.text).join(""),
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

    const updateView = (view: PanelViewId, stick: boolean) => {
      if (!isMessageVisibleInView(view, msgType)) return;
      if (stick) scrollViewToBottom(view);
      else markUnread(view);
    };
    if (isSplit) {
      updateView("left", stickLeft);
      updateView("right", stickRight);
    } else {
      updateView("single", stickSingle);
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

  /**
   * Pick an NPC reply with the number keys 1–9 of the main keyboard (not the
   * keypad, which is used for moving) while reply buttons are shown.
   * @returns Whether `e` picked a reply.
   */
  export function handleDialogShortcut(e: KeyboardEvent): boolean {
    if (dialogOptions.length === 0) return false;
    if (e.ctrlKey || e.altKey || e.metaKey || e.shiftKey) return false;
    const match = /^Digit([1-9])$/.exec(e.code);
    if (!match) return false;
    const option = dialogOptions[Number(match[1]) - 1];
    if (!option) return false;
    handleDialogOptionClick(option.key, option.value);
    return true;
  }

  /** Show or hide the search bar; hiding it clears the search. */
  function toggleSearch() {
    if (searchOpen) {
      closeSearch();
      return;
    }
    searchOpen = true;
    requestAnimationFrame(() => searchEl?.focus());
  }

  function closeSearch() {
    searchOpen = false;
    searchText = "";
    scrollAllViewsToBottom();
  }

  function handleSearchKeydown(e: KeyboardEvent) {
    if (e.key === "Escape") {
      e.stopPropagation();
      closeSearch();
      searchEl?.blur();
    }
  }

  // Show the newest matches whenever the search changes.
  $effect(() => {
    void searchQuery;
    scrollAllViewsToBottom();
  });

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
    } else if (e.key === "PageUp" || e.key === "PageDown") {
      e.preventDefault();
      pageAllViews(e.key === "PageUp" ? -1 : 1);
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

  /**
   * Set while the pointer is pressed in the message log, which also takes
   * focus from the input; the log then keeps its position so text can be
   * selected.
   */
  let pressingInLog = false;

  function handleBlur() {
    // When the input loses focus, return to Playing state.
    const cpl = getCpl();
    if (cpl && cpl.inputState === InputState.CommandMode) {
      cpl.inputState = InputState.Playing;
    }
    // Leaving the input ends paging through the log with Page Up/Down.
    if (!pressingInLog) scrollAllViewsToBottom();
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
      saveConfig(
        HISTORY_KEY,
        commandHistory.filter(isSavableCommand).slice(-MAX_SAVED_HISTORY),
      );
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
    // Jump back to the newest messages so the command's response is visible.
    scrollAllViewsToBottom();
  }
</script>

{#snippet scrollToBottomButton(view: PanelViewId)}
  {#if scrolledUp[view]}
    <button
      class="scroll-bottom-btn"
      class:unread={hasUnread[view]}
      data-ui-nav-skip="true"
      title={hasUnread[view]
        ? "New messages: jump to newest"
        : "Jump to newest"}
      aria-label={hasUnread[view]
        ? "New messages, jump to newest"
        : "Jump to newest messages"}
      onclick={() => scrollViewToBottom(view)}
    >
      {#key unreadFlare[view]}
        <span class="scroll-bottom-icon" class:flare={hasUnread[view]}>
          <svg viewBox="0 0 16 16" width="14" height="14" aria-hidden="true">
            <path
              d="M3 3l5 5 5-5M3 8l5 5 5-5"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
            />
          </svg>
        </span>
      {/key}
    </button>
  {/if}
{/snippet}

<div class="info-panel" style:--info-panel-bg={infoPanelBackgroundColor}>
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div
    class="message-panels"
    class:split={isSplit}
    onpointerdown={() => {
      pressingInLog = true;
      setTimeout(() => (pressingInLog = false));
    }}
  >
    {#if isSplit}
      <div class="message-pane">
        <div class="message-pane-header">
          <button
            class="close-view-btn"
            data-ui-nav-skip="true"
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
              data-ui-nav-id="ui-info-left-filter-all"
              data-ui-nav-group="info-left-filters"
              data-ui-nav-group-policy="vertical"
              data-ui-nav-panel="info"
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
                data-ui-nav-id={`ui-info-left-filter-${filter.id}`}
                data-ui-nav-group="info-left-filters"
                data-ui-nav-group-policy="vertical"
                data-ui-nav-panel="info"
                title={filter.label}
                onclick={() => toggleCategory("left", filter.id)}
                aria-label={filter.label}
                aria-pressed={leftEnabledCategories.has(filter.id)}
                >{filter.icon}</button
              >
            {/each}
          </div>
          <div class="main-content">
            <div
              class="messages"
              bind:this={leftMessagesDiv}
              onscroll={() => handleMessagesScroll("left")}
            >
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
                        data-ui-nav-skip="true"
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
            {@render scrollToBottomButton("left")}
          </div>
        </div>
      </div>

      <div class="message-pane">
        <div class="message-pane-header">
          <button
            class="close-view-btn"
            data-ui-nav-skip="true"
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
              data-ui-nav-id="ui-info-right-filter-all"
              data-ui-nav-group="info-right-filters"
              data-ui-nav-group-policy="vertical"
              data-ui-nav-panel="info"
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
                data-ui-nav-id={`ui-info-right-filter-${filter.id}`}
                data-ui-nav-group="info-right-filters"
                data-ui-nav-group-policy="vertical"
                data-ui-nav-panel="info"
                title={filter.label}
                onclick={() => toggleCategory("right", filter.id)}
                aria-label={filter.label}
                aria-pressed={rightEnabledCategories.has(filter.id)}
                >{filter.icon}</button
              >
            {/each}
          </div>
          <div class="main-content">
            <div
              class="messages"
              bind:this={rightMessagesDiv}
              onscroll={() => handleMessagesScroll("right")}
            >
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
                        data-ui-nav-skip="true"
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
            {@render scrollToBottomButton("right")}
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
              data-ui-nav-id="ui-info-filter-all"
              data-ui-nav-group="info-filters"
              data-ui-nav-group-policy="vertical"
              data-ui-nav-panel="info"
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
                data-ui-nav-id={`ui-info-filter-${filter.id}`}
                data-ui-nav-group="info-filters"
                data-ui-nav-group-policy="vertical"
                data-ui-nav-panel="info"
                title={filter.label}
                onclick={() => toggleCategory("single", filter.id)}
                aria-label={filter.label}
                aria-pressed={enabledCategories.has(filter.id)}
                >{filter.icon}</button
              >
            {/each}
          </div>
          <div class="main-content">
            <div
              class="messages"
              bind:this={singleMessagesDiv}
              onscroll={() => handleMessagesScroll("single")}
            >
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
                        data-ui-nav-skip="true"
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
            {@render scrollToBottomButton("single")}
          </div>
        </div>
      </div>
    {/if}
  </div>

  {#if dialogOptions.length > 0}
    <div class="dialog-options">
      {#each dialogOptions as option, i}
        <button
          class="dialog-option-btn"
          data-ui-nav-id={`ui-info-dialog-option-${option.key}`}
          data-ui-nav-panel="info"
          aria-keyshortcuts={i < 9 ? String(i + 1) : undefined}
          onclick={() => handleDialogOptionClick(option.key, option.value)}
          >{#if i < 9}<span class="dialog-option-key" aria-hidden="true"
              >{i + 1}</span
            >{/if}{option.value}</button
        >
      {/each}
    </div>
  {/if}

  {#if searchOpen}
    <div class="search-row">
      <input
        type="search"
        bind:value={searchText}
        bind:this={searchEl}
        onkeydown={handleSearchKeydown}
        placeholder="Search messages..."
        aria-label="Search messages"
      />
      {#if searchQuery !== ""}
        <span class="search-count" aria-live="polite"
          >{searchMatchCount} found</span
        >
      {/if}
      <button
        data-ui-nav-skip="true"
        title="Close search"
        aria-label="Close search"
        onclick={closeSearch}>✕</button
      >
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
    <button
      class="search-btn"
      class:active={searchOpen}
      data-ui-nav-skip="true"
      title="Search messages"
      aria-label="Search messages"
      aria-pressed={searchOpen}
      onclick={toggleSearch}>🔍</button
    >
    <button
      data-ui-nav-skip="true"
      onclick={submitCommand}
      disabled={inputDisabled}>Send</button
    >
  </div>

  {#if contextMenu}
    {@const menuContext = contextMenu}
    {@const menuHasType =
      matchingCategoryIds(menuContext.message.msgType).length > 0}
    {#snippet contextMenuContent()}
      {#if menuContext.selectedText}
        <button
          aria-label="Copy selected text"
          type="button"
          onclick={handleCopyFromMenu}
          oncontextmenu={(e) => {
            e.preventDefault();
            void handleCopyFromMenu();
          }}>Copy selection</button
        >
      {/if}
      <button
        type="button"
        onclick={handleCopyMessageFromMenu}
        oncontextmenu={(e) => {
          e.preventDefault();
          void handleCopyMessageFromMenu();
        }}>Copy message</button
      >
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
    position: relative;
    display: flex;
    flex-direction: column;
    flex: 1;
    min-width: 0;
    min-height: 0;
  }

  .messages {
    /* The rest of the UI turns text selection off (see app.css). */
    user-select: text;
    -webkit-user-select: text;
    flex: 1;
    overflow-y: auto;
    padding: 0.5rem;
    font-family: var(--mono);
    font-size: 0.85rem;
    line-height: 1.3;
  }

  .scroll-bottom-btn {
    position: absolute;
    right: 14px;
    bottom: 6px;
    width: 26px;
    height: 26px;
    padding: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    border: 1px solid var(--border-light);
    border-radius: 50%;
    background: var(--bg-mid);
    color: var(--text-dim);
    cursor: pointer;
    opacity: 0.75;
    box-shadow: 0 1px 4px rgba(0, 0, 0, 0.5);
    transition:
      opacity 0.15s,
      color 0.15s,
      border-color 0.15s;
  }

  .scroll-bottom-btn:hover,
  .scroll-bottom-btn:focus-visible {
    opacity: 1;
    color: var(--text-bright);
  }

  .scroll-bottom-btn.unread {
    opacity: 1;
    color: var(--text-warm);
    border-color: var(--text-warm-dim);
  }

  .scroll-bottom-icon {
    display: flex;
    border-radius: 50%;
  }

  .scroll-bottom-icon.flare {
    animation: scroll-bottom-flare 0.9s ease-out;
  }

  @keyframes scroll-bottom-flare {
    0% {
      transform: scale(1.5);
      color: #fff2c0;
      filter: drop-shadow(0 0 4px #ffd070);
    }
    100% {
      transform: scale(1);
      filter: drop-shadow(0 0 0 transparent);
    }
  }

  @media (prefers-reduced-motion: reduce) {
    .scroll-bottom-icon.flare {
      animation: none;
    }
  }

  .message {
    padding: 1px 0;
    word-wrap: break-word;
    white-space: pre-wrap;
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

  .dialog-option-key {
    display: inline-block;
    min-width: 1.2em;
    margin-right: 0.4rem;
    color: var(--text-warm-dim);
    font-family: var(--mono);
  }

  .search-row {
    display: flex;
    align-items: center;
    border-top: 1px solid var(--border);
    flex-shrink: 0;
  }

  .search-count {
    padding: 0 0.5rem;
    color: var(--text-dim);
    font-size: 0.75rem;
    white-space: nowrap;
  }

  .search-btn.active {
    background: var(--border-mid);
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
