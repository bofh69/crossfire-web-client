<script lang="ts">
  import { onMount } from 'svelte';
  import { skillNames, playerStats, expBarPercent } from '../lib/commands';
  import type { Stats } from '../lib/protocol';
  import { extendedCommand } from '../lib/p_cmd';
  import { gameEvents } from '../lib/events';

  interface SkillEntry {
    index: number;
    name: string;
    level: number;
    exp: bigint;
  }

  let skills: SkillEntry[] = $state([]);
  let contextMenu = $state<{ x: number; y: number; skill: SkillEntry } | null>(null);

  function updateSkills(stats: Stats) {
    const entries: SkillEntry[] = [];
    for (let i = 0; i < stats.skillLevel.length; i++) {
      if (stats.skillLevel[i] > 0) {
        entries.push({
          index: i,
          name: skillNames[i] || `skill_${i}`,
          level: stats.skillLevel[i],
          exp: stats.skillExp[i],
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
    contextMenu = { x: e.clientX, y: e.clientY, skill };
  }

  function closeContextMenu() {
    contextMenu = null;
  }

  function useSkill(skill: SkillEntry) {
    extendedCommand(`use_skill ${skill.name}`);
    closeContextMenu();
  }

  function readySkill(skill: SkillEntry) {
    extendedCommand(`ready_skill ${skill.name}`);
    closeContextMenu();
  }
</script>

<svelte:window onclick={closeContextMenu} />

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
            <tr oncontextmenu={(e: MouseEvent) => handleContextMenu(e, skill)}>
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
  <div
    class="context-menu"
    style:left="{contextMenu.x}px"
    style:top="{contextMenu.y}px"
    role="menu"
  >
    <button onclick={() => contextMenu && useSkill(contextMenu.skill)}>
      Use skill: {contextMenu.skill.name}
    </button>
    <button onclick={() => contextMenu && readySkill(contextMenu.skill)}>
      Ready skill: {contextMenu.skill.name}
    </button>
  </div>
{/if}

<style>
  .skill-list {
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

  .context-menu {
    position: fixed;
    background: #333;
    border: 1px solid #555;
    border-radius: 4px;
    display: flex;
    flex-direction: column;
    z-index: 100;
    box-shadow: 0 2px 8px rgba(0,0,0,0.5);
  }

  .context-menu button {
    padding: 0.4rem 1rem;
    border: none;
    background: none;
    color: #ddd;
    text-align: left;
    cursor: pointer;
    font-size: 0.8rem;
  }

  .context-menu button:hover {
    background: #444;
  }
</style>

