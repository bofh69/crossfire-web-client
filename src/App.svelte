<script lang="ts">
  import { onMount } from 'svelte';
  import { clientInit } from './lib/init';
  import { initCommands } from './lib/p_cmd';
  import { callbacks, playerStats, spells } from './lib/commands';
  import { locateItem, cpl } from './lib/item';
  import type { Stats } from './lib/protocol';
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

  let gameMap: GameMap | undefined = $state();
  let infoPanel: InfoPanel | undefined = $state();
  let statsPanel: StatsPanel | undefined = $state();
  let inventory: Inventory | undefined = $state();
  let spellList: SpellList | undefined = $state();
  let skillList: SkillList | undefined = $state();

  onMount(() => {
    clientInit();
    initCommands();
  });

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
    callbacks.onTick = undefined;
    callbacks.onGoodbye = undefined;
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

    callbacks.onTick = (_tickNo: number) => {
      refreshInventory();
    };

    callbacks.onGoodbye = () => {
      handleDisconnect();
    };
  }

  function refreshInventory() {
    const playerRoot = cpl?.ob ?? null;
    const groundRoot = locateItem(0);
    inventory?.updateInventory(playerRoot, groundRoot);
  }
</script>

{#if appState === 'login'}
  <Login onLoggedIn={handleLoggedIn} />
{:else}
  <div class="game-layout">
    <div class="menu-area">
      <MenuBar onDisconnect={handleDisconnect} />
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
        {#if activeTab === 'inventory'}
          <Inventory bind:this={inventory} />
        {:else if activeTab === 'spells'}
          <SpellList bind:this={spellList} />
        {:else}
          <SkillList bind:this={skillList} />
        {/if}
      </div>
    </div>
    <div class="info-area">
      <InfoPanel bind:this={infoPanel} />
    </div>
  </div>
{/if}
