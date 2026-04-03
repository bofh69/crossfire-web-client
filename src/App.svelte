<script lang="ts">
  import { onMount } from 'svelte';
  import { clientInit, getCpl } from './lib/init';
  import { initCommands } from './lib/p_cmd';
  import { callbacks, playerStats, spells } from './lib/commands';
  import { locateItem } from './lib/item';
  import { sendReply } from './lib/player';
  import {
    keybindingsInit, setKeyCallbacks, parseKey, parseKeyRelease,
    configureKeys, handleFocusLost,
  } from './lib/keys';
  import type { Stats } from './lib/protocol';
  import { InputState, CS_QUERY_HIDEINPUT } from './lib/protocol';
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

  /** Query prompt sent by the server while in the playing state (e.g. character
   *  name prompt that the server sends after addme_success on some servers). */
  let gameQueryPrompt = $state('');
  let gameQueryHidden = $state(false);
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

  function handleGameQuerySubmit() {
    sendReply(gameQueryInput);
    gameQueryInput = '';
    gameQueryPrompt = '';
  }

  function handleGameQueryKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
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
    if (gameQueryPrompt) return;

    // Route based on input state.
    switch (cpl.inputState) {
      case InputState.Playing:
        // Verify that modifier key state matches reality
        // (handles missed key-ups from focus changes).
        if (cpl.runOn && !e.ctrlKey) {
          cpl.runOn = false;
          runOn = false;
        }
        if (cpl.fireOn && !e.shiftKey) {
          cpl.fireOn = false;
          fireOn = false;
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
    wireCallbacks();
  }

  function handleDisconnect() {
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
    };

    callbacks.onQuery = (flags: number, prompt: string) => {
      gameQueryPrompt = prompt;
      gameQueryHidden = (flags & CS_QUERY_HIDEINPUT) !== 0;
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
      refreshInventory();
    };

    callbacks.onGoodbye = () => {
      handleDisconnect();
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
    <div class="info-area">
      <InfoPanel bind:this={infoPanel} />
    </div>
  </div>

  {#if gameQueryPrompt}
    <div class="query-overlay">
      <div class="query-box">
        <label>
          {gameQueryPrompt}
          <input
            type={gameQueryHidden ? 'password' : 'text'}
            bind:value={gameQueryInput}
            bind:this={gameQueryInputEl}
            onkeydown={handleGameQueryKeydown}
          />
        </label>
        <button onclick={handleGameQuerySubmit}>Submit</button>
      </div>
    </div>
  {/if}
{/if}

<style>
  .query-overlay {
    position: fixed;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    background: rgba(0, 0, 0, 0.65);
    z-index: 100;
  }

  .query-box {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
    width: 340px;
    padding: 1.5rem;
    background: #1e1e1e;
    border: 1px solid #7a6a4a;
    border-radius: 6px;
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
</style>
