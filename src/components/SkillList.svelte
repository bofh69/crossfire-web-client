<script lang="ts">
  import type { Stats } from '../lib/protocol';

  interface SkillEntry {
    index: number;
    name: string;
    level: number;
    exp: string;
  }

  const SKILL_NAMES: Record<number, string> = {
    1: 'lockpicking', 2: 'hiding', 3: 'smithery', 4: 'bowyer',
    5: 'jeweler', 6: 'alchemy', 7: 'stealing', 8: 'literacy',
    9: 'bargaining', 10: 'jumping', 11: 'detect magic',
    12: 'oratory', 13: 'singing', 14: 'detect curse',
    15: 'find traps', 16: 'meditation', 17: 'punching',
    18: 'flame touch', 19: 'karate', 20: 'climbing',
    21: 'woodsman', 22: 'inscription', 23: 'one handed weapons',
    24: 'missile weapons', 25: 'throwing', 26: 'use magic item',
    27: 'praying', 28: 'clawing', 29: 'levitation',
    30: 'summoning', 31: 'pyromancy', 32: 'evocation',
    33: 'sorcery', 34: 'two handed weapons',
  };

  let skills: SkillEntry[] = $state([]);

  export function updateSkills(stats: Stats) {
    const entries: SkillEntry[] = [];
    for (let i = 0; i < stats.skillLevel.length; i++) {
      if (stats.skillLevel[i] > 0) {
        entries.push({
          index: i,
          name: SKILL_NAMES[i] ?? `skill_${i}`,
          level: stats.skillLevel[i],
          exp: stats.skillExp[i].toString(),
        });
      }
    }
    skills = entries.sort((a, b) => a.name.localeCompare(b.name));
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
            <tr>
              <td class="skill-name">{skill.name}</td>
              <td>{skill.level}</td>
              <td>{skill.exp}</td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </div>
</div>

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
</style>
