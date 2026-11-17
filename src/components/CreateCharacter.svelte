<script lang="ts">
  /**
   * CreateCharacter — the character-creation step of the login screen:
   * name, race, class and stat choices, then the starting map (if the server
   * offers a choice).  The race, class and stat data is requested from the
   * server the first time the form is shown.
   */
  import { onMount } from "svelte";
  import {
    getAccountPassword,
    sendCreatePlayer,
    sendRequestInfo,
  } from "../lib/client";
  import { gameEvents } from "../lib/events";
  import type {
    NewCharInfo,
    RaceClassEntry,
    StartingMapEntry,
  } from "../lib/events";

  interface Props {
    /** Whether the form is shown. */
    visible: boolean;
    /** Login method confirmed by the server (1 or 2). */
    loginMethod: number;
    /** Called when the user goes back to the character list. */
    onCancel: () => void;
    /** Show an error message; "" clears it. */
    onError: (text: string) => void;
    /** Show a status message; "" clears it. */
    onStatus: (text: string) => void;
  }

  let { visible, loginMethod, onCancel, onError, onStatus }: Props = $props();

  /** The page shown: the character form, or the starting-map choice. */
  let step = $state<"form" | "startingMap">("form");

  /** True once the race/class/newcharinfo requests have been sent. */
  let charInfoRequested = false;

  /** Race and class lists received from the server. */
  let availableRaces = $state<RaceClassEntry[]>([]);
  let availableClasses = $state<RaceClassEntry[]>([]);

  /** Starting map options received from the server (empty = no choice needed). */
  let availableStartingMaps = $state<StartingMapEntry[]>([]);
  /** Selected index in availableStartingMaps. */
  let selectedStartingMapIdx = $state(0);

  /** newcharinfo data from the server. */
  let newCharStatPoints = $state(0);
  let newCharStatMin = $state(3);
  let newCharStatMax = $state(18);
  let newCharStatNames = $state<string[]>([]);

  /** Character-creation form: name input. */
  let newCharName = $state("");
  /** Selected race index in availableRaces. */
  let selectedRaceIdx = $state(0);
  /** Selected class index in availableClasses. */
  let selectedClassIdx = $state(0);
  /** User's per-stat allocation (stat short name → extra points allocated). */
  let statAlloc = $state<Record<string, number>>({});
  /** Selected choice value indices for the chosen race's choices. */
  let raceChoiceSel = $state<number[]>([]);
  /** Selected choice value indices for the chosen class's choices. */
  let classChoiceSel = $state<number[]>([]);

  const ccSpent = $derived(
    newCharStatNames.reduce((s, n) => s + (statAlloc[n] ?? newCharStatMin), 0),
  );
  const ccRemaining = $derived(newCharStatPoints - ccSpent);
  const ccHasNegStat = $derived(
    newCharStatNames.some((sn) => {
      const rb = availableRaces[selectedRaceIdx]?.statAdj[sn] ?? 0;
      const cb = availableClasses[selectedClassIdx]?.statAdj[sn] ?? 0;
      return rb + cb + (statAlloc[sn] ?? newCharStatMin) < newCharStatMin;
    }),
  );
  const ccCanCreate = $derived(
    newCharName.trim().length > 0 &&
      ccRemaining >= 0 &&
      !ccHasNegStat &&
      // Allow creation when: loginmethod 1 (no stats needed), or loginmethod 2
      // data has arrived (statPoints set), or server didn't send any races.
      (loginMethod < 2 || newCharStatPoints > 0 || availableRaces.length === 0),
  );

  /**
   * Called when the user clicks "Next" on the character-creation form.
   * If starting maps are available, show the starting-map page; otherwise
   * submit the character immediately.
   */
  function handleProceedToStartingMap() {
    onError("");
    const name = newCharName.trim();
    if (!name) {
      onError("Please enter a character name.");
      return;
    }
    if (availableStartingMaps.length > 0) {
      step = "startingMap";
    } else {
      handleCreateCharacter();
    }
  }

  /** Return from the starting-map page to the character-creation form. */
  function handleBackToCreateChar() {
    step = "form";
    onError("");
    onStatus("");
  }

  /**
   * Increment or decrement a stat allocation by `delta`.
   * - Allocation is clamped to [statMin, statMax] (from newcharinfo; defaults 3–18).
   * - Increasing is also gated on the player having remaining points to spend.
   */
  function adjustStat(statName: string, delta: number) {
    const current = statAlloc[statName] ?? newCharStatMin;
    const newVal = current + delta;
    if (newVal < newCharStatMin) return;
    if (newVal > newCharStatMax) return;
    statAlloc = { ...statAlloc, [statName]: newVal };
  }

  /** Send the createplayer command to the server. */
  function handleCreateCharacter() {
    onError("");
    const name = newCharName.trim();
    if (!name) {
      onError("Please enter a character name.");
      return;
    }
    const password = getAccountPassword();
    if (
      loginMethod >= 2 &&
      availableRaces.length > 0 &&
      availableClasses.length > 0
    ) {
      const race = availableRaces[selectedRaceIdx]!;
      const cls = availableClasses[selectedClassIdx]!;
      const rChoices = race.choices
        .map((ch, i) => {
          const selIdx = raceChoiceSel[i] ?? 0;
          return {
            choiceName: ch.name,
            valueArch: ch.values[selIdx]?.arch ?? "",
          };
        })
        .filter((c) => c.valueArch !== "");
      const cChoices = cls.choices
        .map((ch, i) => {
          const selIdx = classChoiceSel[i] ?? 0;
          return {
            choiceName: ch.name,
            valueArch: ch.values[selIdx]?.arch ?? "",
          };
        })
        .filter((c) => c.valueArch !== "");
      const sAlloc = newCharStatNames.map((sn) => ({
        statName: sn,
        value: statAlloc[sn] ?? 0,
      }));
      const startingMapArch =
        availableStartingMaps[selectedStartingMapIdx]?.archName;
      sendCreatePlayer(
        name,
        password,
        race.archName,
        cls.archName,
        rChoices,
        cChoices,
        sAlloc,
        startingMapArch,
      );
    } else {
      // loginmethod 1 — just name + password.
      sendCreatePlayer(name, password);
    }
    onStatus("Creating character…");
  }

  $effect(() => {
    if (!visible) {
      // Start on the character form the next time it is shown.
      step = "form";
    } else if (!charInfoRequested) {
      charInfoRequested = true;
      sendRequestInfo("race_list");
      sendRequestInfo("class_list");
      sendRequestInfo("newcharinfo");
    }
  });

  onMount(() => {
    const cleanups = [
      // ── Character-creation events ──────────────────────────────────────────

      gameEvents.on("raceListReceived", (archNames: string[]) => {
        // Initialise with arch-name-only placeholders; detail arrives via race_info.
        availableRaces = archNames.map((archName) => ({
          archName,
          publicName: archName,
          description: "",
          statAdj: {},
          choices: [],
        }));
        if (selectedRaceIdx >= availableRaces.length) selectedRaceIdx = 0;
        // Request detailed info for every race so the UI can show public names,
        // descriptions, stat bonuses, and optional choices.
        for (const archName of archNames) {
          sendRequestInfo(`race_info ${archName}`);
        }
      }),

      gameEvents.on("classListReceived", (archNames: string[]) => {
        availableClasses = archNames.map((archName) => ({
          archName,
          publicName: archName,
          description: "",
          statAdj: {},
          choices: [],
        }));
        if (selectedClassIdx >= availableClasses.length) selectedClassIdx = 0;
        // Request detailed info for every class so the UI can show public names,
        // descriptions, stat bonuses, and optional choices.
        for (const archName of archNames) {
          sendRequestInfo(`class_info ${archName}`);
        }
      }),

      gameEvents.on("raceInfoReceived", (info: RaceClassEntry) => {
        const idx = availableRaces.findIndex(
          (r) => r.archName === info.archName,
        );
        const updated =
          idx >= 0 ? availableRaces.with(idx, info) : [...availableRaces, info];
        availableRaces = updated.toSorted((a, b) =>
          a.publicName.localeCompare(b.publicName),
        );
      }),

      gameEvents.on("classInfoReceived", (info: RaceClassEntry) => {
        const idx = availableClasses.findIndex(
          (c) => c.archName === info.archName,
        );
        const updated =
          idx >= 0
            ? availableClasses.with(idx, info)
            : [...availableClasses, info];
        availableClasses = updated.toSorted((a, b) =>
          a.publicName.localeCompare(b.publicName),
        );
      }),

      gameEvents.on("newCharInfoReceived", (info: NewCharInfo) => {
        newCharStatPoints = info.statPoints;
        newCharStatMin = info.statMin;
        newCharStatMax = info.statMax;
        newCharStatNames = info.statNames;
        // Reset per-stat allocations to statMin whenever the stat list changes.
        statAlloc = Object.fromEntries(
          info.statNames.map((n) => [n, info.statMin]),
        );
        // Server indicated starting-map selection is required.
        if (info.wantsStartingMap) {
          availableStartingMaps = [];
          selectedStartingMapIdx = 0;
          sendRequestInfo("startingmap");
        }
      }),

      gameEvents.on("startingMapReceived", (maps: StartingMapEntry[]) => {
        availableStartingMaps = maps;
        selectedStartingMapIdx = 0;
      }),
    ];
    return () => {
      for (const unsub of cleanups) unsub();
    };
  });
</script>

{#if visible}
  {#if step === "form"}
    <!-- Character creation form (loginmethod >= 1) -->
    <div class="login-form create-char-form">
      <h3 class="panel-title">Create New Character</h3>

      {#if loginMethod >= 2 && newCharStatPoints === 0}
        <p class="status">Loading character options…</p>
      {/if}

      <label>
        Character Name
        <!-- svelte-ignore a11y_autofocus -->
        <input
          type="text"
          bind:value={newCharName}
          autocomplete="off"
          autofocus
        />
      </label>

      {#if loginMethod >= 2 && availableRaces.length > 0}
        <!-- Race selection -->
        <label>
          Race
          <select bind:value={selectedRaceIdx}>
            {#each availableRaces as race, i}
              <option value={i}>{race.publicName}</option>
            {/each}
          </select>
        </label>
        {#if availableRaces[selectedRaceIdx]?.description}
          <p class="rc-desc">
            {availableRaces[selectedRaceIdx]?.description}
          </p>
        {/if}
        {#each availableRaces[selectedRaceIdx]?.choices ?? [] as choice, ci}
          <label>
            {choice.desc || choice.name}
            <select bind:value={raceChoiceSel[ci]}>
              {#each choice.values as val, vi}
                <option value={vi}>{val.desc || val.arch}</option>
              {/each}
            </select>
          </label>
        {/each}

        <!-- Class selection -->
        {#if availableClasses.length > 0}
          <label>
            Class
            <select bind:value={selectedClassIdx}>
              {#each availableClasses as cls, i}
                <option value={i}>{cls.publicName}</option>
              {/each}
            </select>
          </label>
          {#if availableClasses[selectedClassIdx]?.description}
            <p class="rc-desc">
              {availableClasses[selectedClassIdx]?.description}
            </p>
          {/if}
          {#each availableClasses[selectedClassIdx]?.choices ?? [] as choice, ci}
            <label>
              {choice.desc || choice.name}
              <select bind:value={classChoiceSel[ci]}>
                {#each choice.values as val, vi}
                  <option value={vi}>{val.desc || val.arch}</option>
                {/each}
              </select>
            </label>
          {/each}
        {/if}

        <!-- Stat allocation table -->
        {#if newCharStatPoints > 0}
          <div class="stat-table">
            <div class="stat-header">
              <span>Stat</span><span title="Race bonus">Race</span>
              <span title="Class bonus">Class</span>
              <span>Pts</span><span>Total</span>
            </div>
            {#each newCharStatNames as sn}
              {@const rb = availableRaces[selectedRaceIdx]?.statAdj[sn] ?? 0}
              {@const cb = availableClasses[selectedClassIdx]?.statAdj[sn] ?? 0}
              {@const alloc = statAlloc[sn] ?? 0}
              {@const total = rb + cb + alloc}
              <div class="stat-row" class:stat-bad={total < 1}>
                <span class="stat-name">{sn}</span>
                <span class:bonus-pos={rb > 0} class:bonus-neg={rb < 0}
                  >{rb > 0 ? "+" : ""}{rb}</span
                >
                <span class:bonus-pos={cb > 0} class:bonus-neg={cb < 0}
                  >{cb > 0 ? "+" : ""}{cb}</span
                >
                <span class="stat-spin">
                  <button
                    class="spin-btn"
                    onclick={() => adjustStat(sn, -1)}
                    disabled={alloc <= newCharStatMin}>−</button
                  >
                  <span class="spin-val">{alloc}</span>
                  <button
                    class="spin-btn"
                    onclick={() => adjustStat(sn, +1)}
                    disabled={ccRemaining <= 0 || alloc >= newCharStatMax}
                    >+</button
                  >
                </span>
                <span class:stat-bad={total < newCharStatMin}>{total}</span>
              </div>
            {/each}
            <div class="stat-remaining" class:points-over={ccRemaining < 0}>
              Points remaining: {ccRemaining}
            </div>
          </div>
        {/if}
      {/if}

      <button onclick={handleProceedToStartingMap} disabled={!ccCanCreate}>
        {availableStartingMaps.length > 0 ? "Next →" : "Create Character"}
      </button>
      <button class="back-btn" onclick={onCancel}>← Back</button>
    </div>
  {:else}
    <!-- Starting map selection page -->
    <div class="login-form create-char-form">
      <h3 class="panel-title">Choose Starting Map</h3>

      <label>
        Starting Map
        <select bind:value={selectedStartingMapIdx}>
          {#each availableStartingMaps as map, i}
            <option value={i}>{map.publicName}</option>
          {/each}
        </select>
      </label>
      {#if availableStartingMaps[selectedStartingMapIdx]?.description}
        <p class="entry-desc">
          {availableStartingMaps[selectedStartingMapIdx]?.description}
        </p>
      {/if}

      <button onclick={handleCreateCharacter}>Create Character</button>
      <button class="back-btn" onclick={handleBackToCreateChar}>← Back</button>
    </div>
  {/if}
{/if}

<style>
  .create-char-form {
    max-width: 24rem;
    max-height: 80vh;
    overflow-y: auto;
  }

  select {
    padding: 0.4rem 0.5rem;
    border: 1px solid var(--border-light);
    border-radius: 4px;
    background: var(--bg-lighter);
    color: var(--text-bright);
    font-size: 0.95rem;
  }

  select:focus {
    outline: none;
    border-color: var(--accent);
  }

  .rc-desc {
    font-size: 0.78rem;
    color: var(--text-warm-dim);
    min-height: 4em;
    max-height: 8em;
    overflow-y: auto;
    margin: 0;
    padding: 0.25rem 0.4rem;
    border-left: 2px solid var(--border);
    line-height: 1.35;
  }

  .stat-table {
    display: grid;
    grid-template-columns: 2.5rem 2.5rem 2.5rem 1fr 2.5rem;
    gap: 2px 0;
    font-size: 0.85rem;
  }

  .stat-header {
    display: contents;
    font-weight: bold;
    color: var(--text-warm-dim);
  }

  .stat-header span {
    text-align: center;
    padding: 0.15rem 0;
    border-bottom: 1px solid var(--border);
  }

  .stat-row {
    display: contents;
  }

  .stat-row > span {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 0.15rem 0;
    font-variant-numeric: tabular-nums;
  }

  .stat-row .stat-name {
    justify-content: flex-start;
  }

  .bonus-pos {
    color: #80d080;
  }
  .bonus-neg {
    color: #e06060;
  }

  .stat-bad {
    color: #e06060;
  }

  .stat-spin {
    display: flex !important;
    align-items: center;
    gap: 2px;
  }

  .spin-btn {
    padding: 0 0.3rem;
    font-size: 0.85rem;
    border: 1px solid var(--border);
    background: var(--bg-darker);
    color: var(--text-warm);
    border-radius: 3px;
    line-height: 1.4;
    min-width: 1.4rem;
  }

  .spin-val {
    min-width: 1.5rem;
    text-align: center;
    font-variant-numeric: tabular-nums;
  }

  .stat-remaining {
    grid-column: 1 / -1;
    text-align: right;
    font-size: 0.8rem;
    color: var(--text-warm-dim);
    border-top: 1px solid var(--border);
    padding-top: 0.25rem;
    margin-top: 0.1rem;
  }

  .points-over {
    color: #e06060;
    font-weight: bold;
  }

  @media (max-width: 720px) {
    .create-char-form {
      max-width: none;
    }
  }
</style>
