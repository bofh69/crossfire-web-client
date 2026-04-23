<script lang="ts">
  import { onMount } from 'svelte';
  import { expBarPercent } from '../lib/commands';
  import type { Stats } from '../lib/protocol';
  import { gameEvents } from '../lib/events';
  import { MAX_FOOD } from '../lib/constants';

  let stats: Stats = $state({
    Str: 0, Dex: 0, Con: 0, Wis: 0, Cha: 0, Int: 0, Pow: 0,
    wc: 0, ac: 0, level: 0, hp: 0, maxhp: 0, sp: 0, maxsp: 0,
    grace: 0, maxgrace: 0, exp: BigInt(0), food: 0, dam: 0,
    speed: 0, weaponSp: 0, attuned: 0, repelled: 0, denied: 0,
    flags: 0, resists: new Array(30).fill(0), resistChange: false,
    skillLevel: new Array(50).fill(0), skillExp: new Array(50).fill(BigInt(0)),
    weightLimit: 0, golemHp: 0, golemMaxhp: 0,
    range: '',
    title: '',
    raceStr: 0, raceInt: 0, raceWis: 0, raceDex: 0, raceCon: 0, raceCha: 0, racePow: 0,
    baseStr: 0, baseInt: 0, baseWis: 0, baseDex: 0, baseCon: 0, baseCha: 0, basePow: 0,
    appliedStr: 0, appliedInt: 0, appliedWis: 0, appliedDex: 0, appliedCon: 0, appliedCha: 0, appliedPow: 0,
  });

  function updateStats(newStats: Partial<Stats>) {
    stats = { ...stats, ...newStats };
  }

  onMount(() => {
    const unsub = gameEvents.on('statsUpdate', updateStats);
    return unsub;
  });

  function barPercent(current: number, max: number): number {
    if (max <= 0) return 0;
    return Math.max(0, Math.min(100, (current / max) * 100));
  }

  function barColor(percent: number): string {
    if (percent > 50) return '#44cc44';
    if (percent > 25) return '#ccaa00';
    return '#cc4444';
  }

  let hpPercent = $derived(barPercent(stats.hp, stats.maxhp));
  let spPercent = $derived(barPercent(stats.sp, stats.maxsp));
  let gracePercent = $derived(barPercent(stats.grace, stats.maxgrace));
  let foodPercent = $derived(barPercent(stats.food, MAX_FOOD));
  // expTable is a plain array mutated on login; re-evaluated whenever stats updates.
  let expPercent = $derived(expBarPercent(stats.exp, stats.level));
</script>

<div class="stats-panel">
  <h3>{stats.title ? stats.title : 'Stats'}</h3>

  <div class="bars">
    <div class="bar-row">
      <span class="bar-label">HP</span>
      <div class="bar-track">
        <div class="bar-fill" style:width="{hpPercent}%" style:background={barColor(hpPercent)}></div>
      </div>
      <span class="bar-value">{stats.hp}/{stats.maxhp}</span>
    </div>
    <div class="bar-row">
      <span class="bar-label">SP</span>
      <div class="bar-track">
        <div class="bar-fill" style:width="{spPercent}%" style:background="#4488ff"></div>
      </div>
      <span class="bar-value">{stats.sp}/{stats.maxsp}</span>
    </div>
    <div class="bar-row">
      <span class="bar-label">Grace</span>
      <div class="bar-track">
        <div class="bar-fill" style:width="{gracePercent}%" style:background="#cc88ff"></div>
      </div>
      <span class="bar-value">{stats.grace}/{stats.maxgrace}</span>
    </div>
    <div class="bar-row">
      <span class="bar-label">Food</span>
      <div class="bar-track">
        <div class="bar-fill" style:width="{foodPercent}%" style:background={barColor(foodPercent)}></div>
      </div>
      <span class="bar-value">{stats.food}</span>
    </div>
  </div>

  <div class="attributes">
    <div class="attr-header">
      <span></span>
      <span class="attr-col-label">Total</span>
      <span class="attr-col-label">Base</span>
      <span class="attr-col-label">Race</span>
      <span class="attr-col-label">Applied</span>
    </div>
    {#each [
      { name: 'Str', total: stats.Str, base: stats.baseStr, race: stats.raceStr, applied: stats.appliedStr },
      { name: 'Dex', total: stats.Dex, base: stats.baseDex, race: stats.raceDex, applied: stats.appliedDex },
      { name: 'Con', total: stats.Con, base: stats.baseCon, race: stats.raceCon, applied: stats.appliedCon },
      { name: 'Int', total: stats.Int, base: stats.baseInt, race: stats.raceInt, applied: stats.appliedInt },
      { name: 'Wis', total: stats.Wis, base: stats.baseWis, race: stats.raceWis, applied: stats.appliedWis },
      { name: 'Pow', total: stats.Pow, base: stats.basePow, race: stats.racePow, applied: stats.appliedPow },
      { name: 'Cha', total: stats.Cha, base: stats.baseCha, race: stats.raceCha, applied: stats.appliedCha },
    ] as attr}
      <div class="attr-row">
        <span class="attr-name">{attr.name}</span>
        <span class="attr-total">{attr.total}</span>
        {#if attr.base !== 0 || attr.race !== 0 || attr.applied !== 0}
          <span class="attr-sub">{attr.base}</span>
          <span class="attr-sub">{attr.race > 0 ? '+' : ''}{attr.race}</span>
          <span class="attr-sub">{attr.applied > 0 ? '+' : ''}{attr.applied}</span>
        {:else}
          <span class="attr-sub attr-unavail">—</span>
          <span class="attr-sub attr-unavail"></span>
          <span class="attr-sub attr-unavail"></span>
        {/if}
      </div>
    {/each}
  </div>

  <div class="combat-stats">
    <div class="stat-row">
      <span>Level</span><span>{stats.level}</span>
    </div>
    <div class="stat-row">
      <span>WC</span><span>{stats.wc}</span>
    </div>
    <div class="stat-row">
      <span>AC</span><span>{stats.ac}</span>
    </div>
    <div class="stat-row">
      <span>Dam</span><span>{stats.dam}</span>
    </div>
    <div class="stat-row">
      <span>Speed</span><span>{(stats.speed / 100000).toFixed(2)}</span>
    </div>
    <div class="stat-row exp-stat-row">
      <div class="exp-bg-bar" style:width="{expPercent}%"></div>
      <span>Exp</span><span>{stats.exp.toString()}</span>
    </div>
  </div>
</div>

<style>
  .stats-panel {
    background: var(--bg-panel);
    border: 1px solid var(--border);
    padding: 0.5rem;
    font-size: 0.8rem;
    color: var(--text);
    overflow-y: auto;
  }

  h3 {
    margin: 0 0 0.5rem;
    color: var(--text-warm);
    font-size: 0.9rem;
    border-bottom: 1px solid var(--border);
    padding-bottom: 0.25rem;
  }

  .bars {
    display: flex;
    flex-direction: column;
    gap: 0.3rem;
    margin-bottom: 0.75rem;
  }

  .bar-row {
    display: flex;
    align-items: center;
    gap: 0.4rem;
  }

  .bar-label {
    width: 40px;
    text-align: right;
    color: #999;
    font-size: 0.75rem;
  }

  .bar-track {
    flex: 1;
    height: 12px;
    background: #222;
    border-radius: 2px;
    overflow: hidden;
  }

  .bar-fill {
    height: 100%;
    transition: width 0.3s ease;
    border-radius: 2px;
  }

  .bar-value {
    width: 70px;
    text-align: right;
    font-size: 0.75rem;
    color: #aaa;
  }

  .attributes {
    margin-bottom: 0.75rem;
  }

  .attr-header {
    display: grid;
    grid-template-columns: 40px 1fr 1fr 1fr 1fr;
    gap: 0.1rem;
    color: #666;
    font-size: 0.7rem;
    padding: 0 0 0.2rem;
    border-bottom: 1px solid var(--border);
    margin-bottom: 0.15rem;
  }

  .attr-col-label {
    text-align: right;
  }

  .attr-row {
    display: grid;
    grid-template-columns: 40px 1fr 1fr 1fr 1fr;
    gap: 0.1rem;
    font-size: 0.75rem;
    line-height: 1.4;
  }

  .attr-name {
    color: #999;
  }

  .attr-total {
    text-align: right;
    color: #ddd;
    font-weight: bold;
  }

  .attr-sub {
    text-align: right;
    color: #778;
    font-size: 0.7rem;
  }

  .attr-unavail {
    color: #444;
  }

  .combat-stats {
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
  }

  .stat-row {
    display: flex;
    justify-content: space-between;
    font-size: 0.75rem;
  }

  .stat-row span:first-child {
    color: #999;
  }

  .stat-row span:last-child {
    color: #ddd;
  }

  .exp-stat-row {
    position: relative;
  }

  .exp-bg-bar {
    position: absolute;
    top: 0;
    bottom: 0;
    left: 0;
    background: rgba(180, 140, 40, 0.3);
    pointer-events: none;
  }</style>
