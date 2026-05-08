<script lang="ts">
  import { onMount } from "svelte";
  import { quests } from "../lib/commands";
  import type { Quest } from "../lib/commands";
  import { gameEvents } from "../lib/events";
  import { getFaceUrl } from "../lib/image";

  interface QuestNode {
    quest: Quest;
    depth: number;
  }

  let questNodes: QuestNode[] = $state([]);

  function buildTree(): QuestNode[] {
    const all = [...quests.values()];

    // Top-level quests: parent == 0
    const topLevel = all.filter((q) => q.parent === 0);

    // Sort helper: incomplete before complete, then by title
    function sortQuests(list: Quest[]): Quest[] {
      return list.slice().sort((a, b) => {
        if (a.end !== b.end) return a.end ? 1 : -1;
        return a.title.localeCompare(b.title);
      });
    }

    // Recursively build nodes; `visited` prevents infinite loops caused by
    // cycles (e.g. a buggy server that sends code=0 for every quest, making
    // every child appear to be a child of the single code=0 top-level entry).
    function buildNodes(
      list: Quest[],
      depth: number,
      visited: Set<number>,
    ): QuestNode[] {
      const result: QuestNode[] = [];
      for (const quest of sortQuests(list)) {
        if (visited.has(quest.code)) continue;
        visited.add(quest.code);
        result.push({ quest, depth });
        const children = all.filter(
          (q) => q.parent === quest.code && !visited.has(q.code),
        );
        if (children.length > 0) {
          result.push(...buildNodes(children, depth + 1, visited));
        }
      }
      return result;
    }

    return buildNodes(topLevel, 0, new Set());
  }

  function updateQuests() {
    questNodes = buildTree();
  }

  onMount(() => {
    updateQuests();
    const unsub = gameEvents.on("questUpdate", updateQuests);
    return unsub;
  });
</script>

<div class="quest-list">
  <h3>Quests ({[...quests.values()].length})</h3>
  <div class="quests-scroll">
    {#if questNodes.length === 0}
      <p class="empty">No quests</p>
    {:else}
      <table>
        <thead>
          <tr>
            <th class="col-face"></th>
            <th>Quest</th>
            <th>Status</th>
          </tr>
        </thead>
        <tbody>
          {#each questNodes as node (node.quest.code)}
            <tr
              class:completed={node.quest.end}
              title={node.quest.step || undefined}
            >
              <td class="col-face">
                {#if getFaceUrl(node.quest.face)}
                  <img
                    src={getFaceUrl(node.quest.face)}
                    alt=""
                    class="quest-icon"
                  />
                {/if}
              </td>
              <td
                class="quest-title"
                style:padding-left="{0.4 + Math.min(node.depth, 2) * 1.0}rem"
                >{node.quest.title}</td
              >
              <td class="quest-status">{node.quest.end ? "Done" : "Active"}</td>
            </tr>
          {/each}
        </tbody>
      </table>
    {/if}
  </div>
</div>

<style>
  .quest-list {
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

  .quests-scroll {
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

  tr.completed {
    color: #888;
    font-style: italic;
  }

  td {
    padding: 0.25rem 0.4rem;
  }

  .col-face {
    width: 1px;
    padding: 0.1rem 0.2rem;
    white-space: nowrap;
  }

  .quest-icon {
    width: 32px;
    height: 32px;
    display: block;
    image-rendering: pixelated;
  }

  .quest-title {
    color: #ffddaa;
  }

  tr.completed .quest-title {
    color: #887766;
  }

  .quest-status {
    white-space: nowrap;
    color: #aaffaa;
  }

  tr.completed .quest-status {
    color: #668866;
  }
</style>
