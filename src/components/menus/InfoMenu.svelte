<script lang="ts">
  import { onMount } from "svelte";
  import {
    requestMenuHiscore,
    cancelPendingMenuHiscore,
  } from "../../lib/commands";
  import { gameEvents } from "../../lib/events";
  import { type HiscoreRow, parseHiscoreRows } from "../../lib/markup";

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

  let showAboutDialog = $state(false);
  let showHiscoreDialog = $state(false);
  let hiscoreRows = $state<HiscoreRow[]>([]);
  let hiscorePending = $state(false);

  function showAbout() {
    showAboutDialog = true;
    onClose();
  }

  function closeAbout() {
    showAboutDialog = false;
  }

  function showHiscore() {
    onClose();
    hiscorePending = true;
    requestMenuHiscore();
  }

  function closeHiscore() {
    showHiscoreDialog = false;
    hiscorePending = false;
  }

  onMount(() => {
    const unsubscribe = gameEvents.on("hiscoreResult", (message) => {
      hiscoreRows = parseHiscoreRows(message);
      hiscorePending = false;
      showHiscoreDialog = true;
    });
    return () => {
      unsubscribe();
      cancelPendingMenuHiscore();
    };
  });

  export function isDialogActive(): boolean {
    return showAboutDialog || showHiscoreDialog || hiscorePending;
  }
</script>

<div class="menu-item">
  <button
    class="menu-button"
    onclick={onToggle}
    oncontextmenu={(e) => {
      e.preventDefault();
      onToggle();
    }}>Info</button
  >
  {#if isOpen}
    <div class="dropdown" class:fading>
      <button
        onclick={showHiscore}
        oncontextmenu={(e) => {
          e.preventDefault();
          showHiscore();
        }}>Hiscore</button
      >
      <button
        onclick={showAbout}
        oncontextmenu={(e) => {
          e.preventDefault();
          showAbout();
        }}>About Crossfire Web Client</button
      >
    </div>
  {/if}
</div>

{#if showAboutDialog}
  <div class="dialog-overlay">
    <div class="dialog dialog-wide">
      <p class="dialog-title">About Crossfire Web Client</p>
      <p>
        A web-based client for <a
          href="http://crossfire.real-time.com/"
          target="_blank"
          rel="noopener noreferrer">Crossfire</a
        >, the cooperative multi-player graphical RPG and adventure game.
      </p>
      <p>
        This client is based on the original
        <a
          href="http://crossfire.real-time.com/"
          target="_blank"
          rel="noopener noreferrer">Crossfire GTK client</a
        >
        and reimplemented for the browser.
      </p>
      <p>
        Source code is available on
        <a
          href="https://github.com/bofh69/crossfire-web-client"
          target="_blank"
          rel="noopener noreferrer">GitHub</a
        >.
      </p>
      <p class="dialog-credits">
        Built with
        <a href="https://svelte.dev/" target="_blank" rel="noopener noreferrer"
          >Svelte</a
        >,
        <a href="https://vite.dev/" target="_blank" rel="noopener noreferrer"
          >Vite</a
        >, and
        <a
          href="https://www.typescriptlang.org/"
          target="_blank"
          rel="noopener noreferrer">TypeScript</a
        >.
      </p>
      <p>
        The font used is Modern Antiqua. Copyright (c) 2011, wmk69,
        (wmk69@o2.pl), with Reserved Font Names 'ModernAntiqua' and 'Modern
        Antiqua'. This Font Software is licensed under the SIL Open Font
        License, Version 1.1. This license is available with a FAQ at:
        <a href="http://scripts.sil.org/OFL">OFL</a>.
      </p>
      <div class="dialog-buttons">
        <button onclick={closeAbout}>Close</button>
      </div>
    </div>
  </div>
{/if}

{#if showHiscoreDialog}
  <div class="dialog-overlay">
    <div class="dialog dialog-widest">
      <p class="dialog-title">High Scores</p>
      <div class="bindings-table-wrapper">
        <table class="bindings-table hiscore-table">
          <thead>
            <tr>
              <th class="narrow-cell">Rank</th>
              <th class="narrow-cell">Score</th>
              <th>Who</th>
              <th class="narrow-cell">Max HP</th>
              <th class="narrow-cell">Max SP</th>
              <th class="narrow-cell">Max Grace</th>
            </tr>
          </thead>
          <tbody>
            {#each hiscoreRows as row}
              <tr>
                <td class="num-cell narrow-cell">{row.rank}</td>
                <td class="num-cell narrow-cell">{row.score}</td>
                <td>{row.who}</td>
                <td class="num-cell narrow-cell">{row.maxHp}</td>
                <td class="num-cell narrow-cell">{row.maxSp}</td>
                <td class="num-cell narrow-cell">{row.maxGrace}</td>
              </tr>
            {:else}
              <tr><td colspan="6" class="no-scores">No high scores yet.</td></tr
              >
            {/each}
          </tbody>
        </table>
      </div>
      <div class="dialog-buttons">
        <button onclick={closeHiscore}>Close</button>
      </div>
    </div>
  </div>
{/if}

<style>
  .hiscore-table {
    min-width: 100%;
    table-layout: auto;
  }

  .narrow-cell {
    /* Shrink column to content width; white-space: nowrap prevents wrapping. */
    width: 1%;
    white-space: nowrap;
  }

  .hiscore-table th.narrow-cell,
  .num-cell {
    text-align: right;
    font-variant-numeric: tabular-nums;
  }

  .no-scores {
    text-align: center;
    color: var(--text-dim);
    padding: 0.5rem;
  }
</style>
