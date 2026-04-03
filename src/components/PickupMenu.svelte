<script lang="ts">
  import { untrack } from 'svelte';
  import { sendCommand } from '../lib/player';

  // ── Pickup flag constants (from old/gtk-v2/src/pickup.c) ──────────

  const PU_NOTHING      = 0x00000000;
  const PU_DEBUG         = 0x10000000;
  const PU_INHIBIT       = 0x20000000;
  const PU_STOP          = 0x40000000;
  const PU_NEWMODE       = 0x80000000;
  const PU_RATIO         = 0x0000000F;

  const PU_FOOD          = 0x00000010;
  const PU_DRINK         = 0x00000020;
  const PU_VALUABLES     = 0x00000040;
  const PU_BOW           = 0x00000080;

  const PU_ARROW         = 0x00000100;
  const PU_HELMET        = 0x00000200;
  const PU_SHIELD        = 0x00000400;
  const PU_ARMOUR        = 0x00000800;

  const PU_BOOTS         = 0x00001000;
  const PU_GLOVES        = 0x00002000;
  const PU_CLOAK         = 0x00004000;
  const PU_KEY           = 0x00008000;

  const PU_MISSILEWEAPON = 0x00010000;
  const PU_ALLWEAPON     = 0x00020000;
  const PU_MAGICAL       = 0x00040000;
  const PU_POTION        = 0x00080000;

  const PU_SPELLBOOK     = 0x00100000;
  const PU_SKILLSCROLL   = 0x00200000;
  const PU_READABLES     = 0x00400000;
  const PU_MAGIC_DEVICE  = 0x00800000;

  const PU_NOT_CURSED    = 0x01000000;
  const PU_JEWELS        = 0x02000000;
  const PU_FLESH         = 0x04000000;
  const PU_CONTAINERS    = 0x08000000;

  // ── Menu structure ────────────────────────────────────────────────

  interface PickupItem {
    label: string;
    flag: number;
  }

  interface PickupGroup {
    label: string;
    items: PickupItem[];
  }

  const controlItems: PickupItem[] = [
    { label: "Don't Pickup",        flag: PU_INHIBIT },
    { label: 'Stop Before Pickup',  flag: PU_STOP },
    { label: 'Not Cursed',          flag: PU_NOT_CURSED },
  ];

  const groups: PickupGroup[] = [
    {
      label: 'Armor',
      items: [
        { label: 'Body Armor', flag: PU_ARMOUR },
        { label: 'Boots',      flag: PU_BOOTS },
        { label: 'Cloaks',     flag: PU_CLOAK },
        { label: 'Gloves',     flag: PU_GLOVES },
        { label: 'Helmets',    flag: PU_HELMET },
        { label: 'Shields',    flag: PU_SHIELD },
      ],
    },
    {
      label: 'Weapons',
      items: [
        { label: 'All Weapons',      flag: PU_ALLWEAPON },
        { label: 'Missile Weapons',   flag: PU_MISSILEWEAPON },
        { label: 'Bows',             flag: PU_BOW },
        { label: 'Arrows',           flag: PU_ARROW },
      ],
    },
    {
      label: 'Books',
      items: [
        { label: 'Spellbooks',     flag: PU_SPELLBOOK },
        { label: 'Skill Scrolls',  flag: PU_SKILLSCROLL },
        { label: 'Readables',      flag: PU_READABLES },
      ],
    },
    {
      label: 'Consumables',
      items: [
        { label: 'Food',     flag: PU_FOOD },
        { label: 'Drinks',   flag: PU_DRINK },
        { label: 'Flesh',    flag: PU_FLESH },
        { label: 'Potions',  flag: PU_POTION },
      ],
    },
    {
      label: 'Miscellaneous',
      items: [
        { label: 'Keys',           flag: PU_KEY },
        { label: 'Magical Items',  flag: PU_MAGICAL },
        { label: 'Valuables',      flag: PU_VALUABLES },
        { label: 'Jewels',         flag: PU_JEWELS },
        { label: 'Wands/Rods/Horns', flag: PU_MAGIC_DEVICE },
        { label: 'Containers',     flag: PU_CONTAINERS },
      ],
    },
  ];

  const ratioItems = [
    { label: 'Off',  value: 0 },
    { label: '5:1',  value: 1 },
    { label: '10:1', value: 2 },
    { label: '15:1', value: 3 },
    { label: '20:1', value: 4 },
    { label: '25:1', value: 5 },
    { label: '30:1', value: 6 },
    { label: '35:1', value: 7 },
    { label: '40:1', value: 8 },
    { label: '45:1', value: 9 },
    { label: '50:1', value: 10 },
  ];

  // ── State ─────────────────────────────────────────────────────────

  interface Props {
    initialMode?: number;
  }
  let { initialMode = PU_NEWMODE >>> 0 }: Props = $props();

  /** Current pickup mode bitmask (unsigned 32-bit). Initialised from parent. */
  let pmode = $state(untrack(() => initialMode >>> 0));

  /** Which submenu is expanded (null = none). */
  let expandedGroup = $state<string | null>(null);

  /** Update mode from the server (also called by parent on re-mount). */
  export function setPickupMode(mode: number) {
    pmode = mode >>> 0;
  }

  // ── Helpers ───────────────────────────────────────────────────────

  function isSet(flag: number): boolean {
    return (pmode & flag) !== 0;
  }

  function currentRatio(): number {
    return pmode & PU_RATIO;
  }

  function toggleFlag(flag: number) {
    if (pmode & flag) {
      pmode = ((pmode & ~flag) | PU_NEWMODE) >>> 0;
    } else {
      pmode = (pmode | flag | PU_NEWMODE) >>> 0;
    }
    sendPickup();
  }

  function setRatio(value: number) {
    pmode = ((pmode & ~PU_RATIO) | (value & PU_RATIO) | PU_NEWMODE) >>> 0;
    sendPickup();
  }

  function sendPickup() {
    sendCommand(`pickup ${pmode >>> 0}`, -1, 0);
  }

  function toggleGroup(label: string) {
    expandedGroup = expandedGroup === label ? null : label;
  }
</script>

<div class="pickup-menu">
  <!-- Control items -->
  {#each controlItems as item}
    <label class="pickup-item">
      <input type="checkbox" checked={isSet(item.flag)} onchange={() => toggleFlag(item.flag)} />
      {item.label}
    </label>
  {/each}

  <div class="separator"></div>

  <!-- Category groups -->
  {#each groups as group}
    <button class="group-header" onclick={() => toggleGroup(group.label)}>
      <span class="arrow">{expandedGroup === group.label ? '▾' : '▸'}</span>
      {group.label}
    </button>
    {#if expandedGroup === group.label}
      <div class="group-items">
        {#each group.items as item}
          <label class="pickup-item sub-item">
            <input type="checkbox" checked={isSet(item.flag)} onchange={() => toggleFlag(item.flag)} />
            {item.label}
          </label>
        {/each}
      </div>
    {/if}
  {/each}

  <div class="separator"></div>

  <!-- Weight/Value Ratio -->
  <button class="group-header" onclick={() => toggleGroup('ratio')}>
    <span class="arrow">{expandedGroup === 'ratio' ? '▾' : '▸'}</span>
    Weight/Value Ratio
  </button>
  {#if expandedGroup === 'ratio'}
    <div class="group-items">
      {#each ratioItems as r}
        <label class="pickup-item sub-item">
          <input type="radio" name="ratio" checked={currentRatio() === r.value} onchange={() => setRatio(r.value)} />
          {r.label}
        </label>
      {/each}
    </div>
  {/if}
</div>

<style>
  .pickup-menu {
    max-height: 400px;
    overflow-y: auto;
    min-width: 180px;
  }

  .pickup-item {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    padding: 0.25rem 0.5rem;
    color: #c0c0c0;
    font-size: 0.8rem;
    cursor: pointer;
    user-select: none;
  }

  .pickup-item:hover {
    background: #3a3a3a;
  }

  .sub-item {
    padding-left: 1.2rem;
  }

  .group-header {
    display: flex;
    align-items: center;
    gap: 0.3rem;
    width: 100%;
    padding: 0.25rem 0.5rem;
    border: none;
    background: none;
    color: #d0c0a0;
    font-size: 0.8rem;
    font-weight: 600;
    cursor: pointer;
    text-align: left;
  }

  .group-header:hover {
    background: #3a3a3a;
  }

  .arrow {
    font-size: 0.7rem;
    width: 0.8rem;
    text-align: center;
  }

  .group-items {
    border-left: 2px solid #444;
    margin-left: 0.5rem;
  }

  .separator {
    height: 1px;
    background: #444;
    margin: 0.25rem 0;
  }

  input[type="checkbox"],
  input[type="radio"] {
    accent-color: #7a6a4a;
    margin: 0;
  }
</style>
