<script lang="ts">
  import "@fontsource/caveat";
  import "@fontsource/fira-code";
  import "@fontsource/fira-mono";
  import "@fontsource/modern-antiqua";
  import "@fontsource/noto-sans-runic/runic.css";
  import "@fontsource/uncial-antiqua";
  import { onMount, tick } from "svelte";
  import { clientInit, getCpl } from "./lib/init";
  import { initCommands } from "./lib/p_cmd";
  import { playerStats } from "./lib/commands";
  import { gameEvents } from "./lib/events";
  import type { AccountPlayer } from "./lib/events";
  import { animateObjects } from "./lib/item";
  import { walkDir, runDir, stopRun } from "./lib/player";
  import {
    setWalkDir,
    setRunDir,
    setStopRun,
    setGetMapImageSize,
  } from "./lib/mapdata";
  import { getFaceTileSize } from "./lib/image";
  import {
    keybindingsInit,
    parseKey,
    parseKeyRelease,
    configureKeys,
    handleFocusLost,
  } from "./lib/keys";
  import {
    gamepadInit,
    gamepadShutdown,
    notifyHpUpdate,
    resetHpTracking,
  } from "./lib/gamepad";
  import type { Stats } from "./lib/protocol";
  import { InputState, SHOWMAGIC_FLASH_BIT } from "./lib/protocol";
  import { useConfig } from "./lib/init";
  import { mapdata_animation, run_move_to } from "./lib/mapdata";
  import { initSound, stopAll as stopAllSound } from "./lib/sound";
  import { SELF_TICK_INTERVAL_MS } from "./lib/constants";
  import { getReconnectLogin } from "./lib/client";
  import { clientTour } from "./lib/client_tour.svelte";
  import type { ReconnectLogin } from "./lib/client";
  import { disconnectAndReload, isReloadRequested } from "./lib/disconnect";
  import ClientTour from "./components/ClientTour.svelte";
  import Login from "./components/Login.svelte";
  import GameMap from "./components/GameMap.svelte";
  import InfoPanel from "./components/InfoPanel.svelte";
  import StatsPanel from "./components/StatsPanel.svelte";
  import Inventory from "./components/Inventory.svelte";
  import SpellList from "./components/SpellList.svelte";
  import SkillList from "./components/SkillList.svelte";
  import ProtectionList from "./components/ProtectionList.svelte";
  import QuestList from "./components/QuestList.svelte";
  import QueryPrompt, { ServerQuery } from "./components/QueryPrompt.svelte";
  import KnowledgeList from "./components/KnowledgeList.svelte";
  import MenuBar from "./components/MenuBar.svelte";
  import MagicMap from "./components/MagicMap.svelte";
  import Hotbar from "./components/Hotbar.svelte";
  import {
    loadHotbar,
    activateHotbarSlot,
    getHotbarSlots,
    isHotbarGamepadMode,
  } from "./lib/hotbar";
  import { exportConfigBackup } from "./lib/storage";
  import { PanelSplit } from "./lib/panel_split.svelte";
  import {
    recordBrowserSize,
    recordConfigSnapshot,
    recordKeyInput,
  } from "./lib/websocket-recording";
  import {
    exitUiNavMode,
    firstVisibleInGroup,
    getUiNavActiveTargetId,
    handleUiNavKeyDown,
    isUiNavEnabled,
    lastVisibleInGroup,
    setUiNavCallbacks,
  } from "./lib/ui_nav";

  // ── Layout resize ────────────────────────────────────────────────
  let gameLayoutEl = $state<HTMLDivElement | undefined>();
  const sideSplit = new PanelSplit({
    axis: "x",
    configKey: "layout_sideWidthFrac",
    min: 180,
    maxFrac: 0.6,
    panelSelector: ".side-area",
  });
  const infoSplit = new PanelSplit({
    axis: "y",
    configKey: "layout_infoHeightFrac",
    min: 60,
    maxFrac: 0.5,
    panelSelector: ".info-area",
  });

  type AppState = "login" | "playing";
  let appState = $state<AppState>("login");
  let hotbarVisible = $state(false);

  /** Character list received via `accountplayers` while in the playing state
   *  (e.g. after "bed to reality").  Passed to Login so it shows the
   *  character-select screen immediately on re-mount. */
  let pendingCharacterList = $state<AccountPlayer[] | null>(null);
  type ListTab = "knowledge" | "protections" | "quests" | "skills" | "spells";
  type Tab = "inventory" | ListTab;
  type SecondaryTab = "protections" | "quests" | "knowledge";

  /** UI-nav group of the list shown in each tab, other than the inventory. */
  const LIST_TAB_GROUPS: Record<ListTab, string> = {
    knowledge: "knowledge-list",
    protections: "protection-list",
    quests: "quest-list",
    skills: "skill-list",
    spells: "spell-list",
  };
  /** UI-nav groups of the lists in {@link LIST_TAB_GROUPS}. */
  const LIST_GROUPS: ReadonlySet<string> = new Set(
    Object.values(LIST_TAB_GROUPS),
  );
  /** UI-nav groups of the info filters, one per pane configuration. */
  const INFO_FILTER_GROUPS: readonly string[] = [
    "info-filters",
    "info-left-filters",
    "info-right-filters",
  ];

  function isListTab(tab: string): tab is ListTab {
    return Object.hasOwn(LIST_TAB_GROUPS, tab);
  }

  function isSecondaryTab(tab: string): tab is SecondaryTab {
    return tab === "protections" || tab === "quests" || tab === "knowledge";
  }

  let activeTab = $state<Tab>("inventory");
  const secondaryTabs: { id: SecondaryTab; label: string }[] = [
    { id: "protections", label: "Protect" },
    { id: "quests", label: "Quests" },
    { id: "knowledge", label: "Know" },
  ];

  /** Whether the secondary tabs have been collapsed into the overflow button. */
  let tabOverflow = $state(false);
  /** Whether the overflow dropdown is open. */
  let tabOverflowOpen = $state(false);
  /** Position for the fixed-positioned overflow dropdown. */
  let tabOverflowPos = $state<{ x: number; y: number } | null>(null);
  /** Last-selected secondary tab (shown as overflow button label when a primary tab is active). */
  let lastSecondaryTab = $state<SecondaryTab>("protections");
  /** Bound to the tab-bar element for ResizeObserver. */
  let tabBarEl = $state<HTMLDivElement | undefined>(undefined);

  const isSecondaryActive = $derived(isSecondaryTab(activeTab));
  const overflowTabLabel = $derived(
    secondaryTabs.find((t) => t.id === currentSecondaryTab())!.label,
  );

  /** Show `tab`, remembering it if it is a secondary tab. */
  function showTab(tab: Tab) {
    activeTab = tab;
    if (isSecondaryTab(tab)) lastSecondaryTab = tab;
  }

  function selectSecondaryTab(tab: SecondaryTab) {
    showTab(tab);
    closeTabOverflowMenu();
  }

  function closeTabOverflowMenu() {
    tabOverflowOpen = false;
    tabOverflowPos = null;
  }

  function isOverflowTabTargetId(targetId: string | null): boolean {
    return (
      targetId === "ui-tab-overflow" ||
      targetId?.startsWith("ui-tab-overflow-") === true
    );
  }

  function openTabOverflowMenu(): boolean {
    if (!tabOverflow) return false;
    const btn = document.querySelector<HTMLElement>(
      '[data-ui-nav-id="ui-tab-overflow"]',
    );
    if (!btn) return false;
    const rect = btn.getBoundingClientRect();
    tabOverflowPos = { x: rect.left, y: rect.bottom };
    tabOverflowOpen = true;
    return true;
  }

  function closeUiNavMenus(
    previousTargetId: string | null,
    nextTargetId: string | null,
  ): void {
    if (
      menuBar?.hasOpenMenu() &&
      nextTargetId !== previousTargetId &&
      isNavTargetInGroup(nextTargetId, "menubar")
    ) {
      menuBar.closeOpenMenu();
    }
    if (
      menuBar?.hasOpenMenu() &&
      !isNavTargetInPanel(nextTargetId, "menubar")
    ) {
      menuBar.closeOpenMenu();
    }
    if (tabOverflowOpen && !isOverflowTabTargetId(nextTargetId)) {
      closeTabOverflowMenu();
    }
  }

  function isNavTargetInPanel(targetId: string | null, panel: string): boolean {
    if (!targetId) return false;
    const el = document.querySelector<HTMLElement>(
      `[data-ui-nav-id="${CSS.escape(targetId)}"]`,
    );
    return el?.dataset.uiNavPanel === panel;
  }

  function isNavTargetInGroup(targetId: string | null, group: string): boolean {
    if (!targetId) return false;
    const el = document.querySelector<HTMLElement>(
      `[data-ui-nav-id="${CSS.escape(targetId)}"]`,
    );
    return el?.dataset.uiNavGroup === group;
  }

  function firstVisibleMenubarDropdownTarget(): string | null {
    for (const el of document.querySelectorAll<HTMLElement>(
      '[data-ui-nav-panel="menubar"]',
    )) {
      if (el.dataset.uiNavGroup === "menubar") continue;
      if (el.hidden) continue;
      const rect = el.getBoundingClientRect();
      if (rect.width <= 0 || rect.height <= 0) continue;
      return el.dataset.uiNavId ?? null;
    }
    return null;
  }

  function handleOverflowBtnClick(e: MouseEvent) {
    e.stopPropagation();
    if (tabOverflowOpen) {
      closeTabOverflowMenu();
    } else {
      openTabOverflowMenu();
    }
  }

  /** Use a hidden measurement div to check if all 6 tab buttons fit. */
  $effect(() => {
    const el = tabBarEl;
    if (!el) return;

    function checkTabOverflow() {
      const measure = el!.querySelector<HTMLElement>(".tab-bar-measure");
      if (!measure) return;
      tabOverflow = measure.scrollWidth > el!.clientWidth;
      if (!tabOverflow) {
        tabOverflowOpen = false;
        tabOverflowPos = null;
      }
    }

    const ro = new ResizeObserver(checkTabOverflow);
    ro.observe(el);
    checkTabOverflow();
    return () => ro.disconnect();
  });

  /** Self-tick timer (fallback when the server doesn't send ticks). */
  let selfTickTimer: ReturnType<typeof setInterval> | null = null;

  /** Set to true when the server disconnects while we're in playing state. */
  let serverDisconnected = $state(false);
  /** Whether the lost session can be logged back in to automatically. */
  let canReconnect = $state(false);
  /** Login details handed to the login screen when reconnecting. */
  let reconnectLogin = $state<ReconnectLogin | null>(null);
  let disconnectButton: HTMLButtonElement | undefined = $state();

  // Focus the overlay's button so Enter/Space acts on it right away.
  $effect(() => {
    if (serverDisconnected) disconnectButton?.focus();
  });

  /** Go back to the login screen, which reconnects and logs in again. */
  function handleReconnect() {
    reconnectLogin = getReconnectLogin();
    handleDisconnect();
  }

  /** Query prompt sent by the server while in the playing state (e.g. character
   *  name prompt that the server sends after addme_success on some servers). */
  const gameQuery = new ServerQuery();

  /** Fire / Run indicator state for display */
  let fireOn = $state(false);
  let runOn = $state(false);

  let infoPanel: InfoPanel | undefined = $state();
  let menuBar: MenuBar | undefined = $state();
  let showMagicMap = $state(false);

  /** Returns the first navigable hotbar slot, falling back to the first menubar button. */
  function hotbarOrMenubarTarget(): string | null {
    return firstVisibleInGroup("hotbar") ?? firstVisibleInGroup("menubar");
  }

  /** Returns the first visible info filter ID across all pane configurations. */
  function firstInfoFilterTarget(): string | null {
    for (const group of INFO_FILTER_GROUPS) {
      const target = firstVisibleInGroup(group);
      if (target !== null) return target;
    }
    return null;
  }

  /** Returns the first navigable target in the currently active tab's content. */
  function activeTabFirstTarget(): string | null {
    if (activeTab === "inventory")
      return firstVisibleInGroup("inventory-filters");
    return firstVisibleInGroup(LIST_TAB_GROUPS[activeTab]);
  }

  /** Returns the first navigable list entry in the currently active tab's content. */
  function activeTabFirstListTarget(): string | null {
    if (activeTab === "inventory") {
      return (
        firstVisibleInGroup("ground-items") ??
        firstVisibleInGroup("inventory-items")
      );
    }
    return firstVisibleInGroup(LIST_TAB_GROUPS[activeTab]);
  }

  function uiNavComponentKey(
    panel: string | null,
    group: string | null,
  ): string | null {
    if (panel === "menubar") return "menubar";
    if (group === "hotbar") return "hotbar";
    if (group === "tabs" || group === "tab-overflow-options") return "tabs";
    if (group !== null && INFO_FILTER_GROUPS.includes(group)) {
      return "info-filters";
    }
    if (group === "inventory-filters") return "inventory-filters";
    if (group === "inventory-items") return "inventory-items";
    if (group === "ground-items") return "ground-items";
    if (group !== null && LIST_GROUPS.has(group)) return "active-list";
    if (panel !== null && isListTab(panel)) return "active-list";
    return null;
  }

  function uiNavComponentOrder(): string[] {
    const order = ["menubar"];
    if (hotbarVisible) {
      order.push("hotbar");
    }
    order.push("tabs");
    if (activeTab === "inventory") {
      order.push("inventory-filters", "inventory-items", "ground-items");
    } else {
      order.push("active-list");
    }
    order.push("info-filters");
    return order;
  }

  function uiNavComponentTarget(component: string): string | null {
    switch (component) {
      case "active-list":
        return activeTabFirstListTarget();
      case "ground-items":
        return firstVisibleInGroup("ground-items");
      case "hotbar":
        return firstVisibleInGroup("hotbar");
      case "info-filters":
        return firstInfoFilterTarget();
      case "inventory-filters":
        return firstVisibleInGroup("inventory-filters");
      case "inventory-items":
        return firstVisibleInGroup("inventory-items");
      case "menubar":
        return firstVisibleInGroup("menubar");
      case "tabs":
        return firstVisibleInGroup("tabs");
      default:
        return null;
    }
  }

  function resolveSequentialComponentTarget(
    panel: string | null,
    group: string | null,
    direction: "next" | "prev",
  ): string | null {
    const components = uiNavComponentOrder()
      .map((component) => {
        return {
          component,
          target: uiNavComponentTarget(component),
        };
      })
      .filter((entry): entry is { component: string; target: string } => {
        return entry.target !== null;
      });
    if (components.length === 0) return null;

    const currentComponent = uiNavComponentKey(panel, group);
    const currentIndex = components.findIndex((entry) => {
      return entry.component === currentComponent;
    });
    if (currentIndex < 0) {
      return direction === "next"
        ? components[0]!.target
        : components[components.length - 1]!.target;
    }
    const delta = direction === "next" ? 1 : -1;
    const nextIndex =
      (currentIndex + delta + components.length) % components.length;
    return components[nextIndex]!.target;
  }

  /** Returns the currently selected secondary tab used by the overflow tab button/menu. */
  function currentSecondaryTab(): SecondaryTab {
    return isSecondaryTab(activeTab) ? activeTab : lastSecondaryTab;
  }

  onMount(() => {
    clientInit();
    initCommands();
    keybindingsInit();
    gamepadInit();
    loadHotbar();

    recordBrowserSize(window.innerWidth, window.innerHeight);
    const handleResize = () => {
      recordBrowserSize(window.innerWidth, window.innerHeight);
    };
    window.addEventListener("resize", handleResize);

    setUiNavCallbacks({
      resolveTargetId: async (target: string) => {
        switch (target) {
          case "hotbar":
            return hotbarVisible ? "ui-panel-hotbar" : null;
          case "info":
            return "ui-panel-info";
          case "menubar":
            return "ui-menu-connection";
        }
        // The ground panel is in the inventory tab; other panels are tabs.
        const tab = target === "ground" ? "inventory" : target;
        if (tab === "inventory" || isListTab(tab)) {
          showTab(tab);
          await tick();
          return `ui-panel-${target}`;
        }
        return target.startsWith("ui-") ? target : null;
      },
      onEscape: () => {
        if (tabOverflowOpen) {
          closeTabOverflowMenu();
          return true;
        }
        if (menuBar?.hasOpenMenu()) {
          menuBar.closeOpenMenu();
          return true;
        }
        return false;
      },
      onExit: () => {
        closeTabOverflowMenu();
        menuBar?.closeOpenMenu();
      },
      onTargetChanged: (previousTargetId, nextTargetId) => {
        gameEvents.emit("uiNavTargetChanged", nextTargetId);
        closeUiNavMenus(previousTargetId, nextTargetId);
      },
      resolveSequentialTarget: (panel, group, direction) => {
        return resolveSequentialComponentTarget(panel, group, direction);
      },
      resolveNavBoundary: (_panel, group, direction) => {
        // Menubar buttons: down → open dropdown entry (if open) else hotbar/info;
        // boundary left → info; boundary right → tabs
        if (group === "menubar") {
          if (direction === "move-down") {
            if (menuBar?.hasOpenMenu()) {
              return firstVisibleMenubarDropdownTarget();
            }
            return firstVisibleInGroup("hotbar") ?? firstInfoFilterTarget();
          }
          if (direction === "move-left") return firstInfoFilterTarget();
          if (direction === "move-right") return firstVisibleInGroup("tabs");
        }

        // Hotbar slots: up → menubar; down → info; boundary left → info; boundary right → tabs
        if (group === "hotbar") {
          if (direction === "move-up") return firstVisibleInGroup("menubar");
          if (direction === "move-down") return firstInfoFilterTarget();
          if (direction === "move-left") return firstInfoFilterTarget();
          if (direction === "move-right") return firstVisibleInGroup("tabs");
        }

        // Tab headers: up → hotbar or menubar; down → first content in active tab
        if (group === "tabs") {
          if (
            direction === "move-down" &&
            getUiNavActiveTargetId() === "ui-tab-overflow" &&
            !tabOverflowOpen &&
            openTabOverflowMenu()
          ) {
            return "ui-tab-overflow";
          }
          if (
            tabOverflowOpen &&
            (direction === "move-up" || direction === "move-down")
          ) {
            return `ui-tab-overflow-${currentSecondaryTab()}`;
          }
          if (direction === "move-up") return hotbarOrMenubarTarget();
          if (direction === "move-down") return activeTabFirstTarget();
        }

        // Overflow secondary tabs: up/down stay in list; down from bottom enters selected list.
        if (group === "tab-overflow-options") {
          if (direction === "move-up") return "ui-tab-overflow";
          if (direction === "move-down") return activeTabFirstListTarget();
        }

        // Info filter sidebar: up → hotbar or menubar; right → active tab's first list entry
        if (group !== null && INFO_FILTER_GROUPS.includes(group)) {
          if (direction === "move-up") return hotbarOrMenubarTarget();
          if (direction === "move-right") return activeTabFirstListTarget();
        }

        // Inventory filter bar: up → first tab; down → first inventory item
        if (group === "inventory-filters") {
          if (direction === "move-up") return firstVisibleInGroup("tabs");
          if (direction === "move-down")
            return firstVisibleInGroup("inventory-items");
        }

        // Inventory items: up → inventory filters; left → info filters; right → first ground item
        if (group === "inventory-items") {
          if (direction === "move-up")
            return firstVisibleInGroup("inventory-filters");
          if (direction === "move-left") return firstInfoFilterTarget();
          if (direction === "move-right")
            return firstVisibleInGroup("ground-items");
        }

        // Ground items: up → last inventory item; left → info filters
        if (group === "ground-items") {
          if (direction === "move-up")
            return lastVisibleInGroup("inventory-items");
          if (direction === "move-left") return firstInfoFilterTarget();
        }

        // Spell, skill, protection, quest and knowledge lists:
        // up → tabs; left → info filters; right → last entry
        if (group !== null && LIST_GROUPS.has(group)) {
          if (direction === "move-up") return firstVisibleInGroup("tabs");
          if (direction === "move-left") return firstInfoFilterTarget();
          if (direction === "move-right") return lastVisibleInGroup(group);
        }

        return null;
      },
    });

    // Wire movement callbacks so run_move_to() can issue walk/run commands.
    setWalkDir(walkDir);
    setRunDir(runDir);
    setStopRun(stopRun);

    // Wire the image-cache size function so mapdata correctly tracks multi-tile
    // (bigface) objects.  Without this, all faces are treated as 1×1 tiles and
    // tail cells are never populated.
    setGetMapImageSize(getFaceTileSize);

    // Listen for keyboard events on the window.
    window.addEventListener("keydown", handleGlobalKeyDown);
    window.addEventListener("keyup", handleGlobalKeyUp);
    window.addEventListener("blur", handleWindowBlur);
    window.addEventListener("beforeunload", handleBeforeUnload);

    return () => {
      window.removeEventListener("keydown", handleGlobalKeyDown);
      window.removeEventListener("keyup", handleGlobalKeyUp);
      window.removeEventListener("beforeunload", handleBeforeUnload);
      window.removeEventListener("blur", handleWindowBlur);
      window.removeEventListener("resize", handleResize);
      gamepadShutdown();
      exitUiNavMode(false);
    };
  });

  // ── Keyboard event routing ───────────────────────────────────────

  function isInputFocused(): boolean {
    const el = document.activeElement;
    if (!el) return false;
    const tag = el.tagName;
    return (
      tag === "INPUT" ||
      tag === "TEXTAREA" ||
      tag === "SELECT" ||
      (el as HTMLElement).isContentEditable
    );
  }

  function handleGlobalKeyDown(e: KeyboardEvent) {
    // Only handle game keys when in the playing state. While the disconnect
    // overlay is up, leave keys to the browser so they reach its buttons.
    if (appState !== "playing" || serverDisconnected) return;
    // The tour's balloon handles the keys while it is shown.
    if (clientTour.active) return;

    const cpl = getCpl();
    if (!cpl) return;

    if (isUiNavEnabled()) {
      if (handleUiNavKeyDown(e)) {
        e.preventDefault();
      }
      return;
    }

    // If a text input has focus, let it handle keys normally.
    // The modifier reality-check in the Playing handler will correct
    // any stale fire/run state when focus returns to the game.
    if (isInputFocused()) {
      return;
    }

    // Query prompt active: don't dispatch game keys.
    if (gameQuery.active) {
      // For yes/no queries, handle y/n key presses without needing focus on a button.
      if (gameQuery.handleYesNoKey(e)) {
        e.preventDefault();
      }
      return;
    }

    // Escape closes an open menu-bar dialog or dropdown, unless a dialog
    // already consumed it (e.g. capturing Escape as a key to bind).
    if (e.key === "Escape" && !e.defaultPrevented && menuBar?.handleEscape()) {
      e.preventDefault();
      return;
    }

    // MenuBar key-capture dialog active: let MenuBar handle the key.
    if (menuBar?.isDialogActive()) return;

    // If the magic map is displayed, Escape or Enter dismisses it.
    if (showMagicMap && (e.key === "Escape" || e.key === "Enter")) {
      const cpl2 = getCpl();
      if (cpl2) cpl2.showmagic = 0;
      showMagicMap = false;
      e.preventDefault();
      return;
    }

    // Route based on input state.
    switch (cpl.inputState) {
      case InputState.Playing:
        // Verify that modifier key state matches reality
        // (handles missed key-ups from focus changes).
        // Missed key-ups are recorded so a replay releases the keys too.
        if (cpl.runOn && !e.altKey) {
          cpl.runOn = false;
          runOn = false;
          recordKeyInput("Alt", "up", "");
        }
        if (cpl.fireOn && !e.shiftKey) {
          cpl.fireOn = false;
          fireOn = false;
          recordKeyInput("Shift", "up", "");
        }
        if (cpl.altOn && !e.ctrlKey) {
          cpl.altOn = false;
          recordKeyInput("Control", "up", "");
        }

        // 1–9 pick an NPC reply while reply buttons are shown.
        if (infoPanel?.handleDialogShortcut(e)) {
          e.preventDefault();
          break;
        }

        // F1–F12 activate hotbar slots before keybinding lookup.
        if (e.key.startsWith("F") && e.key.length <= 3) {
          const fNum = parseInt(e.key.slice(1), 10);
          if (
            fNum >= 1 &&
            fNum <= 12 &&
            !e.shiftKey &&
            !e.ctrlKey &&
            !e.altKey &&
            !e.metaKey
          ) {
            recordKeyInput(e.key, "press", "");
            activateHotbarSlot(fNum - 1);
            e.preventDefault();
            break;
          }
        }

        parseKey(e);

        // Update indicator state.
        fireOn = cpl.fireOn;
        runOn = cpl.runOn;

        // Prevent default browser actions for game keys.
        e.preventDefault();
        break;

      case InputState.ConfigureKeys:
        configureKeys(e);
        e.preventDefault();
        break;

      case InputState.CommandMode:
        // Focus should go to the command input.
        gameEvents.emit("focusCommandInput");
        break;
    }
  }

  function handleGlobalKeyUp(e: KeyboardEvent) {
    if (appState !== "playing" || serverDisconnected) return;
    if (isInputFocused()) return;

    parseKeyRelease(e);
    const cpl = getCpl();
    if (cpl) {
      fireOn = cpl.fireOn;
      runOn = cpl.runOn;
    }
  }

  function handleWindowBlur() {
    handleFocusLost();
    fireOn = false;
    runOn = false;
  }

  /** Ask before closing or reloading the tab while a game is in progress. */
  function handleBeforeUnload(e: BeforeUnloadEvent) {
    if (appState !== "playing" || serverDisconnected || isReloadRequested()) {
      return;
    }
    e.preventDefault();
    // Legacy browsers need returnValue set to show the prompt.
    e.returnValue = "";
  }

  function handleLoggedIn() {
    appState = "playing";
    pendingCharacterList = null;
    reconnectLogin = null;
    serverDisconnected = false;
    resetHpTracking();
    wireCallbacks();
    initSound();
    recordConfigSnapshot(exportConfigBackup());
    clientTour.startIfRequested();
  }

  /** Unsubscribe functions for game event subscriptions. */
  let eventCleanups: (() => void)[] = [];

  function handleDisconnect() {
    // Stop self-tick timer
    if (selfTickTimer !== null) {
      clearInterval(selfTickTimer);
      selfTickTimer = null;
    }

    // Stop all sound/music
    stopAllSound();

    // Unsubscribe all event handlers to avoid stale references
    for (const unsub of eventCleanups) unsub();
    eventCleanups = [];

    gameQuery.clear();
    clientTour.stop();
    showMagicMap = false;
    hotbarVisible = false;
    exitUiNavMode(false);
    appState = "login";
  }

  function refreshHotbarVisibility() {
    hotbarVisible =
      !useConfig.hideEmptyHotbar ||
      getHotbarSlots().some((s) => s !== null) ||
      isHotbarGamepadMode();
  }

  function wireCallbacks() {
    refreshHotbarVisibility();

    eventCleanups.push(
      gameEvents.on("hotbarUpdate", () => {
        refreshHotbarVisibility();
      }),

      gameEvents.on("statsUpdate", (stats: Partial<Stats>) => {
        if (stats.hp !== undefined) {
          notifyHpUpdate(playerStats.hp, playerStats.maxhp);
        }
      }),

      gameQuery.listen(),

      gameEvents.on("showMagicMap", () => {
        showMagicMap = true;
      }),

      gameEvents.on("newMap", () => {
        // Switching to a new map hides the magic map overlay.
        showMagicMap = false;
      }),

      gameEvents.on("tick", (_tickNo: number) => {
        // Only ask for redraws when an animation actually changed something.
        if (mapdata_animation()) gameEvents.emit("mapAnimated");
        if (animateObjects()) gameEvents.emit("playerUpdate");
        run_move_to();

        // Flash player position on the magic map.
        const cpl = getCpl();
        if (cpl && cpl.showmagic && showMagicMap) {
          cpl.showmagic ^= SHOWMAGIC_FLASH_BIT;
        } else if (showMagicMap && cpl && !cpl.showmagic) {
          // User closed via the MagicMap component's close button.
          showMagicMap = false;
        }
      }),

      gameEvents.on("goodbye", () => {
        handleDisconnect();
      }),

      gameEvents.on("disconnect", () => {
        // Server closed the connection unexpectedly while we're in the game.
        canReconnect = getReconnectLogin() !== null;
        serverDisconnected = true;
      }),

      // When the server sends accountplayers while in the playing state (e.g.
      // after the player applies a "bed to reality" object), transition back to
      // the login/character-select screen without closing the WebSocket.
      gameEvents.on("accountPlayers", (players: AccountPlayer[]) => {
        pendingCharacterList = players;
        handleDisconnect();
      }),

      // Wire addme_success for in-session reconnects (e.g. after the server
      // asks the user to reconnect when leaving the game).
      gameEvents.on("addMeSuccess", () => {
        serverDisconnected = false;
        gameQuery.clear();
      }),
    );

    // Self-tick fallback: if the server doesn't send ticks, emit them locally
    // at 8 fps (matching the old C client) so the "tick" handlers above and
    // in other components run the same way as with server ticks.
    if (!useConfig.serverTicks) {
      let selfTickNo = 0;
      selfTickTimer = setInterval(() => {
        gameEvents.emit("tick", selfTickNo++);
      }, SELF_TICK_INTERVAL_MS);
    }
  }
</script>

<svelte:window
  onclick={() => {
    closeTabOverflowMenu();
  }}
/>

{#if appState === "login"}
  <Login
    onLoggedIn={handleLoggedIn}
    initialCharacters={pendingCharacterList}
    {reconnectLogin}
  />
{:else}
  <div
    class="game-layout"
    bind:this={gameLayoutEl}
    class:hotbar-hidden={!hotbarVisible}
    style:--side-width={sideSplit.size !== null
      ? `${sideSplit.size}px`
      : undefined}
    style:--info-height={infoSplit.size !== null
      ? `${infoSplit.size}px`
      : undefined}
  >
    <div class="menu-area">
      <MenuBar bind:this={menuBar} />
      <div class="status-indicators">
        {#if fireOn}<span class="indicator fire">Fire</span>{/if}
        {#if runOn}<span class="indicator run">Run</span>{/if}
      </div>
    </div>
    <div
      class="hotbar-area"
      data-ui-nav-entry="hotbar"
      data-ui-nav-id="ui-panel-hotbar"
      data-ui-nav-panel="hotbar"
    >
      <Hotbar />
    </div>
    <div class="map-area">
      <GameMap />
      {#if showMagicMap}
        <MagicMap />
      {/if}
      {#if gameQuery.active}
        <div class="query-overlay">
          <div class="query-box">
            <QueryPrompt query={gameQuery} />
          </div>
        </div>
      {/if}
    </div>
    <div class="side-area">
      <div class="stats-section">
        <StatsPanel />
      </div>
      <div class="tab-bar" bind:this={tabBarEl}>
        <!--
          Hidden measurement div: all 6 labels at natural (non-flex) size.
          scrollWidth vs clientWidth tells us whether all tabs fit.
        -->
        <div class="tab-bar-measure" aria-hidden="true">
          <span>Items</span><span>Spells</span><span>Skills</span>
          <span>Protect</span><span>Quests</span><span>Know</span>
        </div>
        <button
          class:active={activeTab === "inventory"}
          data-ui-nav-id="ui-tab-inventory"
          data-ui-nav-group="tabs"
          data-ui-nav-group-policy="horizontal"
          data-ui-nav-panel="tabs"
          onclick={() => (activeTab = "inventory")}
        >
          Items
        </button>
        <button
          class:active={activeTab === "spells"}
          data-ui-nav-id="ui-tab-spells"
          data-ui-nav-group="tabs"
          data-ui-nav-group-policy="horizontal"
          data-ui-nav-panel="tabs"
          onclick={() => (activeTab = "spells")}
        >
          Spells
        </button>
        <button
          class:active={activeTab === "skills"}
          data-ui-nav-id="ui-tab-skills"
          data-ui-nav-group="tabs"
          data-ui-nav-group-policy="horizontal"
          data-ui-nav-panel="tabs"
          onclick={() => (activeTab = "skills")}
        >
          Skills
        </button>
        {#if !tabOverflow}
          <button
            class:active={activeTab === "protections"}
            data-ui-nav-id="ui-tab-protections"
            data-ui-nav-group="tabs"
            data-ui-nav-group-policy="horizontal"
            data-ui-nav-panel="tabs"
            onclick={() => (activeTab = "protections")}
          >
            Protect
          </button>
          <button
            class:active={activeTab === "quests"}
            data-ui-nav-id="ui-tab-quests"
            data-ui-nav-group="tabs"
            data-ui-nav-group-policy="horizontal"
            data-ui-nav-panel="tabs"
            onclick={() => (activeTab = "quests")}
          >
            Quests
          </button>
          <button
            class:active={activeTab === "knowledge"}
            data-ui-nav-id="ui-tab-knowledge"
            data-ui-nav-group="tabs"
            data-ui-nav-group-policy="horizontal"
            data-ui-nav-panel="tabs"
            onclick={() => (activeTab = "knowledge")}
          >
            Know
          </button>
        {:else}
          <button
            class:active={isSecondaryActive}
            data-ui-nav-id="ui-tab-overflow"
            data-ui-nav-group="tabs"
            data-ui-nav-group-policy="horizontal"
            data-ui-nav-panel="tabs"
            onclick={handleOverflowBtnClick}
          >
            {overflowTabLabel} ▾
          </button>
        {/if}
      </div>
      <div class="tab-content">
        <div hidden={activeTab !== "inventory"} class="tab-panel">
          <Inventory />
        </div>
        <div hidden={activeTab !== "spells"} class="tab-panel">
          <SpellList />
        </div>
        <div hidden={activeTab !== "skills"} class="tab-panel">
          <SkillList />
        </div>
        <div hidden={activeTab !== "protections"} class="tab-panel">
          <ProtectionList />
        </div>
        <div hidden={activeTab !== "quests"} class="tab-panel">
          <QuestList />
        </div>
        <div hidden={activeTab !== "knowledge"} class="tab-panel">
          <KnowledgeList />
        </div>
      </div>
    </div>
    <div
      class="info-area"
      class:query-active={gameQuery.active}
      data-ui-nav-entry="info"
      data-ui-nav-id="ui-panel-info"
      data-ui-nav-panel="info"
    >
      <InfoPanel bind:this={infoPanel} inputDisabled={gameQuery.active} />
    </div>
    <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
    <div
      class="resize-handle-col"
      class:dragging={sideSplit.dragging}
      role="separator"
      aria-label="Resize side panel"
      aria-orientation="vertical"
      onmousedown={(e) => sideSplit.startDrag(e, gameLayoutEl!)}
    ></div>
    <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
    <div
      class="resize-handle-row"
      class:dragging={infoSplit.dragging}
      role="separator"
      aria-label="Resize info panel"
      aria-orientation="horizontal"
      onmousedown={(e) => infoSplit.startDrag(e, gameLayoutEl!)}
    ></div>
  </div>

  {#if clientTour.active && !serverDisconnected}
    <ClientTour />
  {/if}

  {#if serverDisconnected}
    <div class="disconnect-overlay">
      <div
        class="disconnect-box"
        role="alertdialog"
        aria-modal="true"
        aria-labelledby="disconnect-title"
        aria-describedby="disconnect-detail"
      >
        <p id="disconnect-title">⚠ Disconnected from server</p>
        <p id="disconnect-detail" class="disconnect-detail">
          {canReconnect
            ? "The connection to the server was lost."
            : "The connection to the server was lost. Log in again from the start page."}
        </p>
        {#if canReconnect}
          <button bind:this={disconnectButton} onclick={handleReconnect}>
            Reconnect
          </button>
        {:else}
          <button bind:this={disconnectButton} onclick={disconnectAndReload}>
            Back to start page
          </button>
        {/if}
      </div>
    </div>
  {/if}

  {#if tabOverflow && tabOverflowOpen && tabOverflowPos}
    <!-- svelte-ignore a11y_click_events_have_key_events -->
    <!-- svelte-ignore a11y_no_static_element_interactions -->
    <div
      class="tab-overflow-menu"
      style:left="{tabOverflowPos.x}px"
      style:top="{tabOverflowPos.y}px"
      role="menu"
      tabindex="-1"
      onclick={(e) => e.stopPropagation()}
    >
      {#each secondaryTabs as tab}
        <button
          class:active={activeTab === tab.id}
          data-ui-nav-id={`ui-tab-overflow-${tab.id}`}
          data-ui-nav-group="tab-overflow-options"
          data-ui-nav-group-policy="vertical"
          data-ui-nav-panel="tabs"
          role="menuitem"
          onfocus={() => {
            activeTab = tab.id;
            lastSecondaryTab = tab.id;
          }}
          onclick={() => selectSecondaryTab(tab.id)}
        >
          {tab.label}
        </button>
      {/each}
    </div>
  {/if}
{/if}

<style>
  .query-overlay {
    position: absolute;
    inset: 0;
    background: rgba(0, 0, 0, 0.325);
    z-index: 100;
  }

  .query-box {
    position: absolute;
    top: calc(50% + 48px);
    left: 50%;
    transform: translateX(-50%);
    width: 340px;
    padding: 1.5rem;
    background: var(--bg);
    border: 1px solid var(--accent);
    border-radius: 6px;
  }

  .menu-area {
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }

  .status-indicators {
    display: flex;
    gap: 0.4rem;
  }

  .indicator {
    padding: 0.15rem 0.5rem;
    border-radius: 3px;
    font-size: 0.75rem;
    font-weight: bold;
    text-transform: uppercase;
  }

  .indicator.fire {
    background: var(--danger-bg);
    color: #ffcccc;
  }

  .indicator.run {
    background: #205080;
    color: #cce0ff;
  }

  /* Keep all tab panels mounted so data callbacks always work.
     Only one is visible at a time via the hidden attribute. */
  .tab-panel {
    height: 100%;
  }

  .tab-panel[hidden] {
    display: none;
  }

  .disconnect-overlay {
    position: fixed;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    background: rgba(0, 0, 0, 0.75);
    z-index: 200;
  }

  .disconnect-box {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 1rem;
    padding: 2rem;
    background: var(--bg);
    border: 1px solid var(--danger-bg);
    border-radius: 6px;
  }

  .disconnect-box p {
    color: var(--danger-text);
    font-size: 1.1rem;
    margin: 0;
  }

  .disconnect-box button {
    padding: 0.6rem 1.5rem;
    border: 1px solid var(--accent);
    border-radius: 4px;
    background: var(--bg-warm);
    color: var(--text-warm);
    font-size: 1rem;
    cursor: pointer;
  }

  .disconnect-box button:hover {
    background: var(--bg-warm-hover);
  }

  .disconnect-box .disconnect-detail {
    color: var(--text);
    font-size: 0.9rem;
  }
</style>
