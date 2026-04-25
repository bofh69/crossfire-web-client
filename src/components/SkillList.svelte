<script lang="ts">
  import { onMount } from 'svelte';
  import { skillNames, playerStats, expBarPercent, skillDescriptions } from '../lib/commands';
  import type { Stats } from '../lib/protocol';
  import { extendedCommand } from '../lib/p_cmd';
  import { gameEvents } from '../lib/events';
  import { setHotbarSlot } from '../lib/hotbar';
  import HotbarSlotPicker from './HotbarSlotPicker.svelte';
  import ContextMenu from './ContextMenu.svelte';

  interface SkillEntry {
    index: number;
    name: string;
    level: number;
    exp: bigint;
    description: string;
  }

  let skills: SkillEntry[] = $state([]);
  let contextMenu = $state<{ x: number; y: number; skill: SkillEntry } | null>(null);
  let showSlotPicker = $state(false);

  function updateSkills(stats: Stats) {
    const entries: SkillEntry[] = [];
    for (let i = 0; i < stats.skillLevel.length; i++) {
      if (stats.skillLevel[i]! > 0) {
        entries.push({
          index: i,
          name: skillNames[i] || `skill_${i}`,
          level: stats.skillLevel[i]!,
          exp: stats.skillExp[i]!,
          description: skillDescriptions[i] || '',
        });
      }
    }
    skills = entries.sort((a, b) => a.name.localeCompare(b.name));
  }

  // Initialize with whatever data has already arrived before this component mounted.
  // skill_info and stats packets often arrive during login, before wireCallbacks() is
  // set up in App.svelte, so we must load on mount to catch the pre-login data.
  onMount(() => {
    updateSkills(playerStats);
    const unsub = gameEvents.on('statsUpdate', () => updateSkills(playerStats));
    return unsub;
  });

  function handleContextMenu(e: MouseEvent, skill: SkillEntry) {
    e.preventDefault();
    showSlotPicker = false;
    contextMenu = { x: e.clientX, y: e.clientY, skill };
  }

  function closeContextMenu() {
    contextMenu = null;
    showSlotPicker = false;
  }

  function useSkill(skill: SkillEntry) {
    extendedCommand(`use_skill ${skill.name}`);
    closeContextMenu();
  }

  function readySkill(skill: SkillEntry) {
    extendedCommand(`ready_skill ${skill.name}`);
    closeContextMenu();
  }

  function handleAddToHotbar(_skill: SkillEntry) {
    showSlotPicker = true;
  }

  function handleSlotSelected(index: number) {
    if (contextMenu) {
      setHotbarSlot(index, {
        label: contextMenu.skill.name,
        command: `use_skill ${contextMenu.skill.name}`,
      });
    }
    closeContextMenu();
  }
</script>

<div class="skill-list">
  <h3>Skills ({skills.length})</h3>
  <div class="skills-scroll">
    {#if skills.length === 0}
      <p class="empty">No skills</p>
    {:else}
      <table>
        <thead>
          <tr>
            <th>Skill</th>
            <th>Lv</th>
            <th>Exp</th>
          </tr>
        </thead>
        <tbody>
          {#each skills as skill (skill.index)}
            <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
            <tr
              oncontextmenu={(e: MouseEvent) => handleContextMenu(e, skill)}
              title={skill.description || undefined}
            >
              <td class="skill-name">{skill.name}</td>
              <td>{skill.level}</td>
              <td class="exp-cell">
                <div class="exp-bar" style:width="{expBarPercent(skill.exp, skill.level)}%"></div>
                <span class="exp-text">{skill.exp.toString()}</span>
              </td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </div>
</div>

{#if contextMenu}
  <ContextMenu x={contextMenu.x} y={contextMenu.y} onClose={closeContextMenu}>
    <button onclick={() => contextMenu && useSkill(contextMenu.skill)}>
      Use skill: {contextMenu.skill.name}
    </button>
    <button onclick={() => contextMenu && readySkill(contextMenu.skill)}>
      Ready skill: {contextMenu.skill.name}
    </button>
    <button onclick={() => contextMenu && handleAddToHotbar(contextMenu.skill)}>
      Add to hotbar…
    </button>
    {#if showSlotPicker}
      <HotbarSlotPicker
        onSelect={handleSlotSelected}
        onCancel={closeContextMenu}
      />
    {/if}
  </ContextMenu>
{/if}

<style>
  .skill-list {
    background: var(--bg-panel);
    border: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow: hidden;
  }

  h3 {
    margin: 0;
    padding: 0.4rem 0.5rem;
    color: var(--text-warm);
    font-size: 0.8rem;
    background: var(--bg-mid);
  }

  .skills-scroll {
    flex: 1;
    overflow-y: auto;
  }

  .empty {
    color: #666;
    font-size: 0.8rem;
    padding: 0.5rem;
    text-align: center;
  }

  table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.75rem;
  }

  thead th {
    position: sticky;
    top: 0;
    background: var(--bg-mid);
    color: #999;
    text-align: left;
    padding: 0.3rem 0.4rem;
    border-bottom: 1px solid var(--border);
    font-weight: normal;
  }

  tr {
    color: var(--text);
  }

  tr:hover {
    background: var(--bg-lighter);
  }

  td {
    padding: 0.25rem 0.4rem;
  }

  .skill-name {
    color: #aaffaa;
  }

  .exp-cell {
    position: relative;
    min-width: 60px;
  }

  .exp-bar {
    position: absolute;
    top: 0;
    bottom: 0;
    left: 0;
    background: rgba(180, 140, 40, 0.3);
    pointer-events: none;
  }

  .exp-text {
    position: relative;
    z-index: 1;
  }
</style>

