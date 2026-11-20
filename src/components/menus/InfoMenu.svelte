<script lang="ts">
  import { onMount } from "svelte";
  import {
    requestMenuHiscore,
    cancelPendingMenuHiscore,
  } from "../../lib/commands";
  import { clientTour } from "../../lib/client_tour.svelte";
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
  let showHelpDialog = $state(false);
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

  function showHelp() {
    showHelpDialog = true;
    onClose();
  }

  function closeHelp() {
    showHelpDialog = false;
  }

  function showTour() {
    onClose();
    clientTour.start();
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
    return (
      showAboutDialog || showHelpDialog || showHiscoreDialog || hiscorePending
    );
  }

  /**
   * Closes the open About, Help or hiscore dialog (Escape key).
   *
   * @returns True if a dialog was closed.
   */
  export function cancelDialog(): boolean {
    if (showAboutDialog) {
      closeAbout();
    } else if (showHelpDialog) {
      closeHelp();
    } else if (showHiscoreDialog || hiscorePending) {
      if (hiscorePending) cancelPendingMenuHiscore();
      closeHiscore();
    } else {
      return false;
    }
    return true;
  }
</script>

<div class="menu-item">
  <button
    class="menu-button"
    data-ui-nav-group="menubar"
    data-ui-nav-group-policy="horizontal"
    data-ui-nav-id="ui-menu-info"
    data-ui-nav-panel="menubar"
    onclick={onToggle}
    oncontextmenu={(e) => {
      e.preventDefault();
      onToggle();
    }}>Info</button
  >
  {#if isOpen}
    <div class="dropdown" class:fading data-ui-nav-scope="ui-menu-info">
      <button
        data-ui-nav-id="ui-menu-info-hiscore"
        data-ui-nav-panel="menubar"
        onclick={showHiscore}
        oncontextmenu={(e) => {
          e.preventDefault();
          showHiscore();
        }}>Hiscore</button
      >
      <button
        data-ui-nav-id="ui-menu-info-help"
        data-ui-nav-panel="menubar"
        onclick={showHelp}
        oncontextmenu={(e) => {
          e.preventDefault();
          showHelp();
        }}>Help</button
      >
      <button
        data-ui-nav-id="ui-menu-info-tour"
        data-ui-nav-panel="menubar"
        onclick={showTour}
        oncontextmenu={(e) => {
          e.preventDefault();
          showTour();
        }}>Client Tour</button
      >
      <button
        data-ui-nav-id="ui-menu-info-about"
        data-ui-nav-panel="menubar"
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
        Antiqua'.
      </p>
      <p>
        Other bundled fonts used by the client include Caveat, Fira Code, Fira
        Mono, Noto Sans Runic, and Uncial Antiqua.
      </p>
      <p>
        These fonts are distributed via Fontsource and licensed under the SIL
        Open Font License, Version 1.1.
      </p>
      <p>
        This license is available with a FAQ at:
        <a href="http://scripts.sil.org/OFL">OFL</a>.
      </p>
      <p>
        Gamepad button images are from
        <a
          href="https://kenney.nl/assets/input-prompts"
          target="_blank"
          rel="noopener noreferrer">Input Prompts</a
        >
        by Kenney (<a
          href="https://www.kenney.nl/"
          target="_blank"
          rel="noopener noreferrer">www.kenney.nl</a
        >), licensed under
        <a
          href="https://creativecommons.org/publicdomain/zero/1.0/"
          target="_blank"
          rel="noopener noreferrer">CC0</a
        >.
      </p>
      <div class="dialog-buttons">
        <button onclick={closeAbout}>Close</button>
      </div>
    </div>
  </div>
{/if}

{#if showHelpDialog}
  <div class="dialog-overlay">
    <div class="dialog dialog-wide">
      <p class="dialog-title">Help</p>
      <div class="help-text">
        <h4>Moving and fighting</h4>
        <ul>
          <li>
            Move with the <strong>arrow keys</strong>, the
            <strong>numpad</strong> or <strong>y u h j k l b n</strong>. Walk
            into a monster to attack it.
          </li>
          <li>
            Hold <strong>Alt</strong> with a direction to run, and
            <strong>Shift</strong> to fire the readied spell, skill or ranged
            weapon.
            <strong>+</strong> and <strong>-</strong> change what is readied.
          </li>
          <li>
            <strong>a</strong> applies (uses) what you stand on,
            <strong>,</strong> picks it up and <strong>s</strong> searches.
          </li>
        </ul>
        <h4>Mouse</h4>
        <ul>
          <li>
            On the map, click a square to look at it; right-click to walk there.
          </li>
          <li>
            Click an item, spell or skill to use it (items need a second click
            unless turned off in <strong>Config</strong>); right-click it for
            more actions, such as <strong>Drop N…</strong> or
            <strong>Add to hotbar…</strong>.
          </li>
          <li>
            <strong>Ctrl</strong>+wheel over the map zooms;
            <strong>Config</strong> → <strong>Automatic Zoom</strong> fits it to the
            window again.
          </li>
        </ul>
        <h4>Commands and messages</h4>
        <ul>
          <li>
            <strong>'</strong> opens the command line, <strong>Enter</strong>
            starts a chat message and <strong>"</strong> says something to the people
            next to you. Up and Down go through earlier commands; Tab completes them.
          </li>
          <li>
            Type <strong>help</strong> for the server's help, or
            <strong>help</strong> and a command name for that command.
          </li>
          <li>
            When talking to someone, their reply choices appear above the
            command line; press <strong>1</strong>–<strong>9</strong> to pick one.
          </li>
          <li>
            Right-click a message to copy it or to filter what is shown; 🔍
            searches the messages, and Page Up and Page Down scroll them from
            the command line.
          </li>
        </ul>
        <h4>Keyboard only</h4>
        <ul>
          <li>
            <strong>Tab</strong> moves the focus to the menus, lists and
            buttons; then use the arrow keys, <strong>Enter</strong> to choose,
            <strong>m</strong> for a list entry's actions and
            <strong>Esc</strong> to go back to the game.
          </li>
          <li>
            <strong>F1</strong>–<strong>F12</strong> use the hotbar slots.
          </li>
          <li>
            All key bindings are in <strong>Keyboard</strong> →
            <strong>Show key bindings</strong>.
          </li>
          <li>
            A key or gamepad button can run several commands in a row: when
            binding one that is already bound, check
            <strong>Add after current commands</strong>. Typing
            <strong>bind -a</strong> and a command does the same from the command
            line.
          </li>
        </ul>
      </div>
      <div class="dialog-buttons">
        <button onclick={closeHelp}>Close</button>
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
  .help-text {
    max-height: 60vh;
    overflow-y: auto;
  }

  .help-text h4 {
    margin: 0.6rem 0 0.2rem;
    color: var(--text-warm);
  }

  .help-text h4:first-child {
    margin-top: 0;
  }

  .help-text ul {
    margin: 0;
    padding-left: 1.2rem;
  }

  .help-text li {
    margin: 0.15rem 0;
  }

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
