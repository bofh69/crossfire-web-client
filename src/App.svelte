<script lang="ts">
  import "@fontsource/caveat";
  import "@fontsource/fira-code";
  import "@fontsource/fira-mono";
  import "@fontsource/modern-antiqua";
  import "@fontsource/noto-sans-runic/runic.css";
  import "@fontsource/uncial-antiqua";
  import { onMount } from "svelte";
  import { clientInit, getCpl } from "./lib/init";
  import { initCommands, setPCmdCallbacks } from "./lib/p_cmd";
  import { playerStats } from "./lib/commands";
  import { gameEvents } from "./lib/events";
  import type { AccountPlayer } from "./lib/events";
  import { animateObjects } from "./lib/item";
  import { sendReply, walkDir, runDir, stopRun } from "./lib/player";
  import {
    setWalkDir,
    setRunDir,
    setStopRun,
    setGetMapImageSize,
  } from "./lib/mapdata";
  import { getFaceTileSize } from "./lib/image";
  import {
    keybindingsInit,
    setKeyCallbacks,
    parseKey,
    parseKeyRelease,
    configureKeys,
    handleFocusLost,
    setCurrentCharacter as setKeyCurrentCharacter,
  } from "./lib/keys";
  import {
    gamepadInit,
    gamepadShutdown,
    setGamepadCallbacks,
    notifyHpUpdate,
    resetHpTracking,
    setCurrentCharacter as setGamepadCurrentCharacter,
  } from "./lib/gamepad";
  import type { Stats } from "./lib/protocol";
  import {
    InputState,
    CS_QUERY_HIDEINPUT,
    CS_QUERY_SINGLECHAR,
    CS_QUERY_YESNO,
    SHOWMAGIC_FLASH_BIT,
  } from "./lib/protocol";
  import { useConfig } from "./lib/init";
  import { mapdata_animation, run_move_to } from "./lib/mapdata";
  import { initSound, stopAll as stopAllSound } from "./lib/sound";
  import { SELF_TICK_INTERVAL_MS } from "./lib/constants";
  import Login from "./components/Login.svelte";
  import GameMap from "./components/GameMap.svelte";
  import InfoPanel from "./components/InfoPanel.svelte";
  import StatsPanel from "./components/StatsPanel.svelte";
  import Inventory from "./components/Inventory.svelte";
  import SpellList from "./components/SpellList.svelte";
  import SkillList from "./components/SkillList.svelte";
  import ProtectionList from "./components/ProtectionList.svelte";
  import QuestList from "./components/QuestList.svelte";
  import KnowledgeList from "./components/KnowledgeList.svelte";
  import MenuBar from "./components/MenuBar.svelte";
  import MagicMap from "./components/MagicMap.svelte";
  import Hotbar from "./components/Hotbar.svelte";
  import {
    loadHotbar,
    activateHotbarSlot,
    setCurrentCharacter as setHotbarCurrentCharacter,
    getHotbarSlots,
    isHotbarGamepadMode,
  } from "./lib/hotbar";
  import { loadConfig, saveConfig } from "./lib/storage";

  // ── Layout resize ────────────────────────────────────────────────
  const MIN_SIDE_WIDTH = 180;
  const MAX_SIDE_WIDTH_FRAC = 0.6;
  const MIN_INFO_HEIGHT = 60;
  const MAX_INFO_HEIGHT_FRAC = 0.5;

  function loadLayoutSize(
    fracKey: string,
    viewportSize: number,
    min: number,
    maxFrac: number,
  ): number | null {
    const frac = loadConfig<number | null>(fracKey, null);
    if (frac === null) return null;
    return Math.max(
      min,
      Math.min(
        Math.floor(maxFrac * viewportSize),
        Math.round(frac * viewportSize),
      ),
    );
  }

  let gameLayoutEl = $state<HTMLDivElement | undefined>();
  let sideWidthPx = $state(
    loadLayoutSize(
      "layout_sideWidthFrac",
      window.innerWidth,
      MIN_SIDE_WIDTH,
      MAX_SIDE_WIDTH_FRAC,
    ),
  );
  let infoHeightPx = $state(
    loadLayoutSize(
      "layout_infoHeightFrac",
      window.innerHeight,
      MIN_INFO_HEIGHT,
      MAX_INFO_HEIGHT_FRAC,
    ),
  );
  let isDraggingCol = $state(false);
  let isDraggingRow = $state(false);

  function handleColResizeStart(e: MouseEvent) {
    e.preventDefault();
    const layout = gameLayoutEl!;
    const startSideWidth = layout
      .querySelector<HTMLElement>(".side-area")!
      .getBoundingClientRect().width;
    const startX = e.clientX;
    const maxSide = Math.floor(
      layout.getBoundingClientRect().width * MAX_SIDE_WIDTH_FRAC,
    );

    isDraggingCol = true;
    document.body.style.cursor = "col-resize";
    document.body.style.userSelect = "none";

    function onMove(me: MouseEvent) {
      const delta = startX - me.clientX;
      sideWidthPx = Math.max(
        MIN_SIDE_WIDTH,
        Math.min(maxSide, Math.round(startSideWidth + delta)),
      );
    }

    function onUp() {
      isDraggingCol = false;
      document.body.style.cursor = "";
      document.body.style.userSelect = "";
      if (sideWidthPx !== null) {
        saveConfig(
          "layout_sideWidthFrac",
          sideWidthPx / layout.getBoundingClientRect().width,
        );
      }
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    }

    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup", onUp);
  }

  function handleRowResizeStart(e: MouseEvent) {
    e.preventDefault();
    const layout = gameLayoutEl!;
    const startInfoHeight = layout
      .querySelector<HTMLElement>(".info-area")!
      .getBoundingClientRect().height;
    const startY = e.clientY;
    const maxInfo = Math.floor(
      layout.getBoundingClientRect().height * MAX_INFO_HEIGHT_FRAC,
    );

    isDraggingRow = true;
    document.body.style.cursor = "row-resize";
    document.body.style.userSelect = "none";

    function onMove(me: MouseEvent) {
      const delta = startY - me.clientY;
      infoHeightPx = Math.max(
        MIN_INFO_HEIGHT,
        Math.min(maxInfo, Math.round(startInfoHeight + delta)),
      );
    }

    function onUp() {
      isDraggingRow = false;
      document.body.style.cursor = "";
      document.body.style.userSelect = "";
      if (infoHeightPx !== null) {
        saveConfig(
          "layout_infoHeightFrac",
          infoHeightPx / layout.getBoundingClientRect().height,
        );
      }
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    }

    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup", onUp);
  }

  type AppState = "login" | "playing";
  let appState = $state<AppState>("login");
  let hotbarVisible = $state(false);

  /** Character list received via `accountplayers` while in the playing state
   *  (e.g. after "bed to reality").  Passed to Login so it shows the
   *  character-select screen immediately on re-mount. */
  let pendingCharacterList = $state<AccountPlayer[] | null>(null);
  let activeTab = $state<
    "inventory" | "spells" | "skills" | "protections" | "quests" | "knowledge"
  >("inventory");

  type SecondaryTab = "protections" | "quests" | "knowledge";
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

  const isSecondaryActive = $derived(
    activeTab === "protections" ||
      activeTab === "quests" ||
      activeTab === "knowledge",
  );
  const overflowTabLabel = $derived(
    isSecondaryActive
      ? secondaryTabs.find((t) => t.id === activeTab)!.label
      : secondaryTabs.find((t) => t.id === lastSecondaryTab)!.label,
  );

  function selectSecondaryTab(tab: SecondaryTab) {
    activeTab = tab;
    lastSecondaryTab = tab;
    tabOverflowOpen = false;
    tabOverflowPos = null;
  }

  function handleOverflowBtnClick(e: MouseEvent) {
    e.stopPropagation();
    if (tabOverflowOpen) {
      tabOverflowOpen = false;
      tabOverflowPos = null;
    } else {
      const rect = (e.currentTarget as HTMLElement).getBoundingClientRect();
      tabOverflowPos = { x: rect.left, y: rect.bottom };
      tabOverflowOpen = true;
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

  /** Query prompt sent by the server while in the playing state (e.g. character
   *  name prompt that the server sends after addme_success on some servers). */
  let gameQueryPrompt = $state("");
  let lastGameQueryPrompt = "";
  let gameQueryHidden = $state(false);
  let gameQuerySingleChar = $state(false);
  let gameQueryYesNo = $state(false);
  let gameQueryInput = $state("");
  let gameQueryInputEl: HTMLInputElement | undefined = $state();

  /** Fire / Run indicator state for display */
  let fireOn = $state(false);
  let runOn = $state(false);

  $effect(() => {
    if (gameQueryInputEl) {
      gameQueryInputEl.focus();
    }
  });

  function clearGameQuery() {
    gameQueryInput = "";
    gameQueryPrompt = "";
    gameQuerySingleChar = false;
    gameQueryYesNo = false;
  }

  function sendGameQueryReply(answer: string) {
    sendReply(answer);
    clearGameQuery();
  }

  function handleGameQuerySubmit() {
    if (gameQueryInput.trim() === "") return;
    sendGameQueryReply(gameQueryInput);
  }

  function handleGameQueryKeydown(e: KeyboardEvent) {
    if (
      gameQuerySingleChar &&
      e.key.length === 1 &&
      !e.ctrlKey &&
      !e.altKey &&
      !e.metaKey
    ) {
      // For single-character queries, send the reply immediately on keypress
      // without requiring Enter. The typed character becomes the reply.
      sendGameQueryReply(e.key);
      e.preventDefault();
      // Stop propagation so the global window handler doesn't re-focus the chat input.
      e.stopPropagation();
    } else if (e.key === "Enter") {
      // Stop propagation so the global window handler doesn't re-focus the chat input.
      e.stopPropagation();
      handleGameQuerySubmit();
    }
  }

  let menuBar: MenuBar | undefined = $state();
  let showMagicMap = $state(false);

  onMount(() => {
    clientInit();
    initCommands();
    keybindingsInit();
    gamepadInit();
    loadHotbar();

    // Wire key-system callbacks so keys.ts can interact with the UI.
    setKeyCallbacks({
      drawInfo: (message: string) => {
        gameEvents.emit("drawInfo", 0, message);
      },
      focusCommandInput: (prefill?: string) => {
        gameEvents.emit("focusCommandInput", prefill);
      },
      getCpl: () => getCpl(),
    });

    // Wire gamepad callbacks.
    setGamepadCallbacks({
      drawInfo: (message: string) => {
        gameEvents.emit("drawInfo", 0, message);
      },
    });

    // Wire p_cmd callbacks so bind/gamepad_bind commands can open dialogs.
    setPCmdCallbacks({
      drawInfo: (message: string) => {
        gameEvents.emit("drawInfo", 0, message);
      },
      openKeyBind: () => gameEvents.emit("openKeyBind"),
      openGamepadBind: () => gameEvents.emit("openGamepadBind"),
      showMagicMap: () => {
        showMagicMap = true;
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

    return () => {
      window.removeEventListener("keydown", handleGlobalKeyDown);
      window.removeEventListener("keyup", handleGlobalKeyUp);
      window.removeEventListener("blur", handleWindowBlur);
      gamepadShutdown();
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
    // Only handle game keys when in the playing state.
    if (appState !== "playing") return;

    const cpl = getCpl();
    if (!cpl) return;

    // If a text input has focus, let it handle keys normally.
    // The modifier reality-check in the Playing handler will correct
    // any stale fire/run state when focus returns to the game.
    if (isInputFocused()) {
      return;
    }

    // Query prompt active: don't dispatch game keys.
    if (gameQueryPrompt) {
      // For yes/no queries, handle y/n key presses without needing focus on a button.
      if (gameQueryYesNo) {
        const key = e.key.toLowerCase();
        if (
          (key === "y" || key === "n") &&
          !e.ctrlKey &&
          !e.altKey &&
          !e.metaKey
        ) {
          sendGameQueryReply(key);
          e.preventDefault();
        }
      }
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
        if (cpl.runOn && !e.altKey) {
          cpl.runOn = false;
          runOn = false;
        }
        if (cpl.fireOn && !e.shiftKey) {
          cpl.fireOn = false;
          fireOn = false;
        }
        if (cpl.altOn && !e.ctrlKey) {
          cpl.altOn = false;
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
    if (appState !== "playing") return;
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

  function handleLoggedIn() {
    appState = "playing";
    pendingCharacterList = null;
    serverDisconnected = false;
    resetHpTracking();
    wireCallbacks();
    initSound();
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

    gameQueryPrompt = "";
    gameQuerySingleChar = false;
    gameQueryYesNo = false;
    showMagicMap = false;
    hotbarVisible = false;
    appState = "login";
  }

  function refreshHotbarVisibility() {
    hotbarVisible =
      getHotbarSlots().some((s) => s !== null) || isHotbarGamepadMode();
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

      gameEvents.on("playerUpdate", () => {
        // When the player object is received, load character-specific bindings.
        const cpl = getCpl();
        const charName = cpl?.ob?.dName ?? "";
        if (charName) {
          setKeyCurrentCharacter(charName);
          setGamepadCurrentCharacter(charName);
          setHotbarCurrentCharacter(charName);
        }
      }),

      gameEvents.on("query", (flags: number, prompt: string) => {
        // If the server sends an empty prompt, reuse the previous prompt text.
        if (prompt) {
          lastGameQueryPrompt = prompt;
        }
        gameQueryPrompt = lastGameQueryPrompt;
        gameQueryHidden = (flags & CS_QUERY_HIDEINPUT) !== 0;
        gameQuerySingleChar = (flags & CS_QUERY_SINGLECHAR) !== 0;
        gameQueryYesNo = (flags & CS_QUERY_YESNO) !== 0;
        gameQueryInput = "";
      }),

      gameEvents.on("newMap", () => {
        // Switching to a new map hides the magic map overlay.
        showMagicMap = false;
      }),

      gameEvents.on("tick", (_tickNo: number) => {
        mapdata_animation();
        animateObjects();
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
        gameQueryPrompt = "";
        gameQuerySingleChar = false;
        gameQueryYesNo = false;
      }),
    );

    // Self-tick fallback: if the server doesn't send ticks, drive
    // animations with a local 8 fps timer (matching the old C client).
    if (!useConfig.serverTicks) {
      selfTickTimer = setInterval(() => {
        mapdata_animation();
        animateObjects();
        run_move_to();
        // Emit mapUpdate and playerUpdate so child components refresh
        // (same as tick handler would in child components).
        gameEvents.emit("mapUpdate");
        gameEvents.emit("playerUpdate");

        const cpl = getCpl();
        if (cpl && cpl.showmagic && showMagicMap) {
          cpl.showmagic ^= SHOWMAGIC_FLASH_BIT;
        } else if (showMagicMap && cpl && !cpl.showmagic) {
          showMagicMap = false;
        }
      }, SELF_TICK_INTERVAL_MS);
    }
  }
</script>

<svelte:window
  onclick={() => {
    tabOverflowOpen = false;
    tabOverflowPos = null;
  }}
/>

{#if appState === "login"}
  <Login onLoggedIn={handleLoggedIn} initialCharacters={pendingCharacterList} />
{:else}
  <div
    class="game-layout"
    bind:this={gameLayoutEl}
    class:hotbar-hidden={!hotbarVisible}
    style:--side-width={sideWidthPx !== null ? `${sideWidthPx}px` : undefined}
    style:--info-height={infoHeightPx !== null
      ? `${infoHeightPx}px`
      : undefined}
  >
    <div class="menu-area">
      <MenuBar bind:this={menuBar} />
      <div class="status-indicators">
        {#if fireOn}<span class="indicator fire">Fire</span>{/if}
        {#if runOn}<span class="indicator run">Run</span>{/if}
      </div>
    </div>
    <div class="hotbar-area">
      <Hotbar />
    </div>
    <div class="map-area">
      <GameMap />
      {#if showMagicMap}
        <MagicMap />
      {/if}
      {#if gameQueryPrompt}
        <div class="query-overlay">
          <div class="query-box">
            <p class="query-text" aria-live="polite">{gameQueryPrompt}</p>
            {#if gameQueryYesNo}
              <div class="yesno-buttons">
                <button
                  aria-keyshortcuts="y"
                  onclick={() => sendGameQueryReply("y")}>Yes</button
                >
                <button
                  aria-keyshortcuts="n"
                  onclick={() => sendGameQueryReply("n")}>No</button
                >
              </div>
            {:else}
              <label>
                <input
                  type={gameQueryHidden ? "password" : "text"}
                  bind:value={gameQueryInput}
                  bind:this={gameQueryInputEl}
                  onkeydown={handleGameQueryKeydown}
                />
              </label>
              {#if !gameQuerySingleChar}
                <button onclick={handleGameQuerySubmit}>Submit</button>
              {/if}
            {/if}
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
          onclick={() => (activeTab = "inventory")}>Items</button
        >
        <button
          class:active={activeTab === "spells"}
          onclick={() => (activeTab = "spells")}>Spells</button
        >
        <button
          class:active={activeTab === "skills"}
          onclick={() => (activeTab = "skills")}>Skills</button
        >
        {#if !tabOverflow}
          <button
            class:active={activeTab === "protections"}
            onclick={() => (activeTab = "protections")}>Protect</button
          >
          <button
            class:active={activeTab === "quests"}
            onclick={() => (activeTab = "quests")}>Quests</button
          >
          <button
            class:active={activeTab === "knowledge"}
            onclick={() => (activeTab = "knowledge")}>Know</button
          >
        {:else}
          <button
            class:active={isSecondaryActive}
            onclick={handleOverflowBtnClick}>{overflowTabLabel} ▾</button
          >
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
    <div class="info-area" class:query-active={!!gameQueryPrompt}>
      <InfoPanel inputDisabled={!!gameQueryPrompt} />
    </div>
    <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
    <div
      class="resize-handle-col"
      class:dragging={isDraggingCol}
      role="separator"
      aria-label="Resize side panel"
      aria-orientation="vertical"
      onmousedown={handleColResizeStart}
    ></div>
    <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
    <div
      class="resize-handle-row"
      class:dragging={isDraggingRow}
      role="separator"
      aria-label="Resize info panel"
      aria-orientation="horizontal"
      onmousedown={handleRowResizeStart}
    ></div>
  </div>

  {#if serverDisconnected}
    <div class="disconnect-overlay">
      <div class="disconnect-box">
        <p>⚠ Disconnected from server</p>
        <button onclick={handleDisconnect}>Back to Login</button>
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
          role="menuitem"
          onclick={() => selectSecondaryTab(tab.id)}>{tab.label}</button
        >
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
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
    width: 340px;
    padding: 1.5rem;
    background: var(--bg);
    border: 1px solid var(--accent);
    border-radius: 6px;
  }

  .query-box .query-text {
    color: var(--text-warm-dim);
    font-size: 0.9rem;
    margin: 0;
  }

  .query-box .yesno-buttons {
    display: flex;
    gap: 0.5rem;
  }

  .query-box .yesno-buttons button {
    flex: 1;
  }

  .query-box label {
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
    color: var(--text-warm-dim);
    font-size: 0.9rem;
  }

  .query-box input {
    padding: 0.5rem;
    border: 1px solid var(--border-light);
    border-radius: 4px;
    background: var(--bg-lighter);
    color: var(--text-bright);
    font-size: 1rem;
  }

  .query-box input:focus {
    outline: none;
    border-color: var(--accent);
  }

  .query-box button {
    padding: 0.6rem 1rem;
    border: 1px solid var(--accent);
    border-radius: 4px;
    background: var(--bg-warm);
    color: var(--text-warm);
    font-size: 1rem;
    cursor: pointer;
  }

  .query-box button:hover {
    background: var(--bg-warm-hover);
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
</style>
