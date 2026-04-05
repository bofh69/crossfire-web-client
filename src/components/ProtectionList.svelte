<script lang="ts">
  import { onMount } from 'svelte';
  import { playerStats } from '../lib/commands';
  import type { Stats } from '../lib/protocol';

  // Names for each resistance slot, matching the server's CS_STAT_RESIST_START
  // index order (index 0 = physical/armor, 1 = magic, …, 17 = blind).
  const RESIST_NAMES: string[] = [
    'Physical', 'Magic', 'Fire', 'Electric',
    'Cold', 'Confusion', 'Acid', 'Drain',
    'Ghost Hit', 'Poison', 'Slow', 'Paralysis',
    'Turn Undead', 'Fear', 'Deplete', 'Death',
    'Holy Word', 'Blind',
  ];

  interface ProtectionEntry {
    index: number;
    name: string;
    value: number;
  }

  let protections: ProtectionEntry[] = $state([]);

  export function updateProtections(stats: Stats) {
    const entries: ProtectionEntry[] = [];
    for (let i = 0; i < RESIST_NAMES.length; i++) {
      entries.push({ index: i, name: RESIST_NAMES[i], value: stats.resists[i] ?? 0 });
    }
    protections = entries;
  }

  // Initialize with whatever data has already arrived before this component mounted.
  onMount(() => updateProtections(playerStats));
</script>

<div class="protection-list">
  <h3>Protections</h3>
  <div class="protections-scroll">
    <table>
      <thead>
        <tr>
          <th>Protection</th>
          <th>%</th>
        </tr>
      </thead>
      <tbody>
        {#each protections as prot (prot.index)}
          <tr>
            <td class="prot-name">{prot.name}</td>
            <td class="prot-value" class:positive={prot.value > 0} class:negative={prot.value < 0}>
              {prot.value > 0 ? '+' : ''}{prot.value}
            </td>
          </tr>
        {/each}
      </tbody>
    </table>
  </div>
</div>

<style>
  .protection-list {
    background: #1a1a1a;
    border: 1px solid #333;
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow: hidden;
  }

  h3 {
    margin: 0;
    padding: 0.4rem 0.5rem;
    color: #e0d0b0;
    font-size: 0.8rem;
    background: #252525;
  }

  .protections-scroll {
    flex: 1;
    overflow-y: auto;
  }

  table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.75rem;
  }

  thead th {
    position: sticky;
    top: 0;
    background: #252525;
    color: #999;
    text-align: left;
    padding: 0.3rem 0.4rem;
    border-bottom: 1px solid #333;
    font-weight: normal;
  }

  tr {
    color: #c0c0c0;
  }

  tr:hover {
    background: #2a2a2a;
  }

  td {
    padding: 0.25rem 0.4rem;
  }

  .prot-name {
    color: #aaccff;
  }

  .prot-value {
    text-align: right;
    font-variant-numeric: tabular-nums;
  }

  .positive {
    color: #88ff88;
  }

  .negative {
    color: #ff8888;
  }
</style>
