<script lang="ts">
  import { onMount } from 'svelte';
  import { clientInit, getCpl } from './lib/init';
  import { initCommands } from './lib/p_cmd';
  import { callbacks, playerStats, spells } from './lib/commands';
  import { locateItem, animateObjects } from './lib/item';
  import { sendReply } from './lib/player';
  import {
    keybindingsInit, setKeyCallbacks, parseKey, parseKeyRelease,
    configureKeys, handleFocusLost,
  } from './lib/keys';
  import type { Stats } from './lib/protocol';
  import { InputState, CS_QUERY_HIDEINPUT, CS_QUERY_SINGLECHAR, CS_QUERY_YESNO, CONFIG_SERVER_TICKS } from './lib/protocol';
  import { useConfig } from './lib/init';
  import { mapdata_animation } from './lib/mapdata';
  import { initSound, stopAll as stopAllSound } from './lib/sound';
  import Login from './components/Login.svelte';
  import GameMap from './components/GameMap.svelte';
  import InfoPanel from './components/InfoPanel.svelte';
  import StatsPanel from './components/StatsPanel.svelte';
  import Inventory from './components/Inventory.svelte';
  import SpellList from './components/SpellList.svelte';
  import SkillList from './components/SkillList.svelte';
  import MenuBar from './components/MenuBar.svelte';

  type AppState = 'login' | 'playing';
  let appState = $state<AppState>('login');
  let activeTab = $state<'inventory' | 'spells' | 'skills'>('inventory');

  /** Self-tick timer (fallback when the server doesn't send ticks). */
  let selfTickTimer: ReturnType<typeof setInterval> | null = null;

  /** Set to true when the server disconnects while we're in playing state. */
  let serverDisconnected = $state(false);

  /** Query prompt sent by the server while in the playing state (e.g. character
   *  name prompt that the server sends after addme_success on some servers). */
  let gameQueryPrompt = $state('');
  let lastGameQueryPrompt = '';
  let gameQueryHidden = $state(false);
  let gameQuerySingleChar = $state(false);
  let gameQueryYesNo = $state(false);
  let gameQueryInput = $state('');
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
    gameQueryInput = '';
    gameQueryPrompt = '';
    gameQuerySingleChar = false;
    gameQueryYesNo = false;
  }

  function sendGameQueryReply(answer: string) {
    sendReply(answer);
    clearGameQuery();
  }

  function handleGameQuerySubmit() {
    if (gameQueryInput.trim() === '') return;
    sendGameQueryReply(gameQueryInput);
  }

  function handleGameQueryKeydown(e: KeyboardEvent) {
    if (gameQuerySingleChar && e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
      // For single-character queries, send the reply immediately on keypress
      // without requiring Enter. The typed character becomes the reply.
      sendGameQueryReply(e.key);
      e.preventDefault();
      // Stop propagation so the global window handler doesn't re-focus the chat input.
      e.stopPropagation();
    } else if (e.key === 'Enter') {
      // Stop propagation so the global window handler doesn't re-focus the chat input.
      e.stopPropagation();
      handleGameQuerySubmit();
    }
  }

  let gameMap: GameMap | undefined = $state();
  let infoPanel: InfoPanel | undefined = $state();
  let statsPanel: StatsPanel | undefined = $state();
  let inventory: Inventory | undefined = $state();
  let spellList: SpellList | undefined = $state();
  let skillList: SkillList | undefined = $state();
  let menuBar: MenuBar | undefined = $state();

  onMount(() => {
    clientInit();
    initCommands();
    keybindingsInit();

    // Wire key-system callbacks so keys.ts can interact with the UI.
    setKeyCallbacks({
      drawInfo: (message: string) => {
        infoPanel?.addMessage(0, message);
      },
      focusCommandInput: (prefill?: string) => {
        infoPanel?.focusInput(prefill);
      },
      getCpl: () => getCpl(),
    });

    // Listen for keyboard events on the window.
    window.addEventListener('keydown', handleGlobalKeyDown);
    window.addEventListener('keyup', handleGlobalKeyUp);
    window.addEventListener('blur', handleWindowBlur);

    return () => {
      window.removeEventListener('keydown', handleGlobalKeyDown);
      window.removeEventListener('keyup', handleGlobalKeyUp);
      window.removeEventListener('blur', handleWindowBlur);
    };
  });

  // ── Keyboard event routing ───────────────────────────────────────

  function isInputFocused(): boolean {
    const el = document.activeElement;
    if (!el) return false;
    const tag = el.tagName;
    return tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT' ||
           (el as HTMLElement).isContentEditable;
  }

  function handleGlobalKeyDown(e: KeyboardEvent) {
    // Only handle game keys when in the playing state.
    if (appState !== 'playing') return;

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
        if ((key === 'y' || key === 'n') && !e.ctrlKey && !e.altKey && !e.metaKey) {
          sendGameQueryReply(key);
          e.preventDefault();
        }
      }
      return;
    }

    // MenuBar key-capture dialog active: let MenuBar handle the key.
    if (menuBar?.isDialogActive()) return;

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
        infoPanel?.focusInput();
        break;
    }
  }

  function handleGlobalKeyUp(e: KeyboardEvent) {
    if (appState !== 'playing') return;
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
    appState = 'playing';
    serverDisconnected = false;
    wireCallbacks();
    initSound();
  }

  function handleDisconnect() {
    // Stop self-tick timer
    if (selfTickTimer !== null) {
      clearInterval(selfTickTimer);
      selfTickTimer = null;
    }

    // Stop all sound/music
    stopAllSound();

    // Clear callbacks to avoid stale references
    callbacks.onDrawInfo = undefined;
    callbacks.onDrawExtInfo = undefined;
    callbacks.onStatsUpdate = undefined;
    callbacks.onMapUpdate = undefined;
    callbacks.onNewMap = undefined;
    callbacks.onSpellUpdate = undefined;
    callbacks.onPlayerUpdate = undefined;
    callbacks.onPickupUpdate = undefined;
    callbacks.onTick = undefined;
    callbacks.onGoodbye = undefined;
    callbacks.onQuery = undefined;
    gameQueryPrompt = '';
    gameQuerySingleChar = false;
    gameQueryYesNo = false;
    appState = 'login';
  }

  function wireCallbacks() {
    callbacks.onDrawInfo = (color: number, message: string) => {
      infoPanel?.addMessage(color, message);
    };

    callbacks.onDrawExtInfo = (color: number, _type: number, _subtype: number, message: string) => {
      infoPanel?.addMessage(color, message);
    };

    callbacks.onStatsUpdate = (stats: Partial<Stats>) => {
      statsPanel?.updateStats(stats);
      skillList?.updateSkills(playerStats);
      if (stats.range !== undefined) {
        menuBar?.setRange(stats.range);
      }
    };

    callbacks.onQuery = (flags: number, prompt: string) => {
      // If the server sends an empty prompt, reuse the previous prompt text.
      if (prompt) {
        lastGameQueryPrompt = prompt;
      }
      gameQueryPrompt = lastGameQueryPrompt;
      gameQueryHidden = (flags & CS_QUERY_HIDEINPUT) !== 0;
      gameQuerySingleChar = (flags & CS_QUERY_SINGLECHAR) !== 0;
      gameQueryYesNo = (flags & CS_QUERY_YESNO) !== 0;
      gameQueryInput = '';
    };

    callbacks.onMapUpdate = () => {
      gameMap?.redrawMap();
    };

    callbacks.onNewMap = () => {
      gameMap?.redrawMap();
    };

    callbacks.onSpellUpdate = () => {
      spellList?.updateSpells(spells);
    };

    callbacks.onPlayerUpdate = () => {
      refreshInventory();
    };

    callbacks.onPickupUpdate = (mode: number) => {
      menuBar?.setPickupMode(mode);
    };

    callbacks.onTick = (_tickNo: number) => {
      mapdata_animation();
      animateObjects();
      gameMap?.redrawMap();
      refreshInventory();
    };

    // Self-tick fallback: if the server doesn't send ticks, drive
    // animations with a local 8 fps timer (matching the old C client).
    if (!useConfig[CONFIG_SERVER_TICKS]) {
      selfTickTimer = setInterval(() => {
        mapdata_animation();
        animateObjects();
        gameMap?.redrawMap();
        refreshInventory();
      }, 125);
    }

    callbacks.onGoodbye = () => {
      handleDisconnect();
    };

    callbacks.onDisconnect = () => {
      // Server closed the connection unexpectedly while we're in the game.
      serverDisconnected = true;
    };

    // Wire addme_success for in-session reconnects (e.g. after the server
    // asks the user to reconnect when leaving the game).
    callbacks.onAddMeSuccess = () => {
      serverDisconnected = false;
      gameQueryPrompt = '';
      gameQuerySingleChar = false;
      gameQueryYesNo = false;
    };
  }

  function refreshInventory() {
    const playerRoot = getCpl()?.ob ?? null;
    const groundRoot = locateItem(0);
    inventory?.updateInventory(playerRoot, groundRoot);
  }
</script>

{#if appState === 'login'}
  <Login onLoggedIn={handleLoggedIn} />
{:else}
  <div class="game-layout">
    <div class="menu-area">
      <MenuBar bind:this={menuBar} onDisconnect={handleDisconnect} />
      <div class="status-indicators">
        {#if fireOn}<span class="indicator fire">Fire</span>{/if}
        {#if runOn}<span class="indicator run">Run</span>{/if}
      </div>
    </div>
    <div class="map-area">
      <GameMap bind:this={gameMap} />
      {#if gameQueryPrompt}
        <div class="query-overlay">
          <div class="query-box">
            <p class="query-text" aria-live="polite">{gameQueryPrompt}</p>
            {#if gameQueryYesNo}
              <div class="yesno-buttons">
                <button aria-keyshortcuts="y" onclick={() => sendGameQueryReply('y')}>Yes</button>
                <button aria-keyshortcuts="n" onclick={() => sendGameQueryReply('n')}>No</button>
              </div>
            {:else}
              <label>
                <input
                  type={gameQueryHidden ? 'password' : 'text'}
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
        <StatsPanel bind:this={statsPanel} />
      </div>
      <div class="tab-bar">
        <button class:active={activeTab === 'inventory'} onclick={() => activeTab = 'inventory'}>Items</button>
        <button class:active={activeTab === 'spells'} onclick={() => activeTab = 'spells'}>Spells</button>
        <button class:active={activeTab === 'skills'} onclick={() => activeTab = 'skills'}>Skills</button>
      </div>
      <div class="tab-content">
        <div hidden={activeTab !== 'inventory'} class="tab-panel">
          <Inventory bind:this={inventory} />
        </div>
        <div hidden={activeTab !== 'spells'} class="tab-panel">
          <SpellList bind:this={spellList} />
        </div>
        <div hidden={activeTab !== 'skills'} class="tab-panel">
          <SkillList bind:this={skillList} />
        </div>
      </div>
    </div>
    <div class="info-area" class:query-active={!!gameQueryPrompt}>
      <InfoPanel bind:this={infoPanel} inputDisabled={!!gameQueryPrompt} />
    </div>
  </div>

  {#if serverDisconnected}
    <div class="disconnect-overlay">
      <div class="disconnect-box">
        <p>⚠ Disconnected from server</p>
        <button onclick={handleDisconnect}>Back to Login</button>
      </div>
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
    background: #1e1e1e;
    border: 1px solid #7a6a4a;
    border-radius: 6px;
  }

  .query-box .query-text {
    color: #c0b090;
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
    color: #c0b090;
    font-size: 0.9rem;
  }

  .query-box input {
    padding: 0.5rem;
    border: 1px solid #555;
    border-radius: 4px;
    background: #2a2a2a;
    color: #e0e0e0;
    font-size: 1rem;
  }

  .query-box input:focus {
    outline: none;
    border-color: #7a6a4a;
  }

  .query-box button {
    padding: 0.6rem 1rem;
    border: 1px solid #7a6a4a;
    border-radius: 4px;
    background: #4a3a2a;
    color: #e0d0b0;
    font-size: 1rem;
    cursor: pointer;
  }

  .query-box button:hover {
    background: #5a4a3a;
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
    background: #8b2020;
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
    background: #1e1e1e;
    border: 1px solid #8b2020;
    border-radius: 6px;
  }

  .disconnect-box p {
    color: #ff8888;
    font-size: 1.1rem;
    margin: 0;
  }

  .disconnect-box button {
    padding: 0.6rem 1.5rem;
    border: 1px solid #7a6a4a;
    border-radius: 4px;
    background: #4a3a2a;
    color: #e0d0b0;
    font-size: 1rem;
    cursor: pointer;
  }

  .disconnect-box button:hover {
    background: #5a4a3a;
  }
</style>
