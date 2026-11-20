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
  import { stripMarkupTags } from "../lib/markup";
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

  /** Lowest total a stat may have after the race and class bonuses
   *  (MIN_STAT in the server's check_race_and_class). */
  const MIN_STAT_TOTAL = 1;

  /** Total that suggestStats raises every stat to before favouring any. */
  const SUGGEST_FLOOR = 8;

  /** Points a shift-click on a stat's − or + button moves. */
  const BIG_STEP = 5;

  /** Long names and short explanations of the standard stats. */
  const STAT_INFO: Readonly<Record<string, { name: string; help: string }>> = {
    Cha: { name: "Charisma", help: "Better prices in shops." },
    Con: { name: "Constitution", help: "More health, and faster healing." },
    Dex: {
      name: "Dexterity",
      help: "Hit more often, dodge better and act faster.",
    },
    Int: {
      name: "Intelligence",
      help: "Learn spells, and more mana for spells.",
    },
    Pow: { name: "Power", help: "More mana, and faster mana regeneration." },
    Str: {
      name: "Strength",
      help: "More damage in melee, and carry more.",
    },
    Wis: { name: "Wisdom", help: "More grace for prayers, and faster grace." },
  };

  /** The characters a name may contain (playername_ok in the server). */
  const NAME_PATTERN = /^[A-Za-z0-9][A-Za-z0-9_-]*$/;

  /** The page shown: the character form, or the starting-map choice. */
  let step = $state<"form" | "startingMap">("form");

  /** True once the race/class/newcharinfo requests have been sent. */
  let charInfoRequested = false;

  /** Race and class lists received from the server, sorted by name. */
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
  /** Arch name of the selected race; the first race when not found. */
  let selectedRaceArch = $state("");
  /** Arch name of the selected class; the first class when not found. */
  let selectedClassArch = $state("");
  /** User's per-stat allocation (stat short name → points allocated). */
  let statAlloc = $state<Record<string, number>>({});
  /** True once the user has changed the stats by hand.  Until then they
   *  follow the race and class (see suggestStats). */
  let statsTouched = $state(false);
  /** Selected choice value indices for the chosen race's choices. */
  let raceChoiceSel = $state<number[]>([]);
  /** Selected choice value indices for the chosen class's choices. */
  let classChoiceSel = $state<number[]>([]);

  const selectedRace = $derived(
    availableRaces.find((r) => r.archName === selectedRaceArch) ??
      availableRaces[0],
  );
  const selectedClass = $derived(
    availableClasses.find((c) => c.archName === selectedClassArch) ??
      availableClasses[0],
  );

  /** True when the server lets the player pick race, class and stats. */
  const hasChoices = $derived(loginMethod >= 2 && availableRaces.length > 0);
  /** True while the race/class/stat data hasn't arrived yet. */
  const loading = $derived(loginMethod >= 2 && newCharStatPoints === 0);

  /** The race plus class bonus for the stat `sn`. */
  function bonus(sn: string): number {
    return (selectedRace?.statAdj[sn] ?? 0) + (selectedClass?.statAdj[sn] ?? 0);
  }

  /** The points allocated to the stat `sn`. */
  function allocOf(sn: string): number {
    return statAlloc[sn] ?? newCharStatMin;
  }

  const ccSpent = $derived(
    newCharStatNames.reduce((s, n) => s + allocOf(n), 0),
  );
  const ccRemaining = $derived(newCharStatPoints - ccSpent);
  /** The first stat whose total is too low, if any. */
  const ccLowStat = $derived(
    newCharStatNames.find((sn) => bonus(sn) + allocOf(sn) < MIN_STAT_TOTAL),
  );
  const nameError = $derived.by(() => {
    const name = newCharName.trim();
    if (name === "") return "";
    if (!NAME_PATTERN.test(name)) {
      return "Use only letters, digits, - and _, and start with a letter or digit.";
    }
    return "";
  });
  /** Why the character can't be created yet, or "" if it can. */
  const ccBlocker = $derived.by(() => {
    if (loading && availableRaces.length > 0) {
      return "Waiting for the character options…";
    }
    if (newCharName.trim() === "") return "Choose a name for your character.";
    if (nameError) return nameError;
    if (ccRemaining < 0) {
      return `You have spent ${-ccRemaining} points too many.`;
    }
    if (ccLowStat) {
      return `${statLongName(ccLowStat)} must be at least ${MIN_STAT_TOTAL}.`;
    }
    return "";
  });
  /** Scale of the stat bars: a total this high fills the bar. */
  const statBarMax = $derived(newCharStatMax + 6);

  /** The long name of the stat `sn`, e.g. "Strength" for "Str". */
  function statLongName(sn: string): string {
    return STAT_INFO[sn]?.name ?? sn;
  }

  /** A stat bonus as text, e.g. "+2", "−1" or "0". */
  function formatBonus(value: number): string {
    if (value > 0) return `+${value}`;
    if (value < 0) return `−${-value}`;
    return "0";
  }

  /** The non-zero stat bonuses of a race or class, in stat order. */
  function bonusList(entry: RaceClassEntry | undefined) {
    if (!entry) return [];
    return newCharStatNames
      .map((sn) => ({ sn, value: entry.statAdj[sn] ?? 0 }))
      .filter((b) => b.value !== 0);
  }

  /**
   * Split a race or class description into its "Key: value" facts (the
   * lines before a `---` line) and its paragraphs of text.
   */
  function splitDescription(desc: string): {
    facts: { label: string; value: string }[];
    paragraphs: string[];
  } {
    const lines = desc.split("\n");
    const sep = lines.findIndex((l) => l.trim() === "---");
    const factLines = sep >= 0 ? lines.slice(0, sep) : [];
    const facts: { label: string; value: string }[] = [];
    for (const line of factLines) {
      const m = /^\s*([^:]{1,20}):\s*(.*)$/.exec(line);
      if (m) facts.push({ label: m[1]!, value: m[2]! });
    }
    const paragraphs = lines
      .slice(sep + 1)
      .map((l) => l.trim())
      .filter((l) => l.length > 0);
    return { facts, paragraphs };
  }

  /**
   * Called when the user clicks "Next" on the character-creation form.
   * If starting maps are available, show the starting-map page; otherwise
   * submit the character immediately.
   */
  function handleProceedToStartingMap() {
    onError("");
    if (ccBlocker) {
      onError(ccBlocker);
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
   * - Increasing is also limited to the points the player has left.
   */
  function adjustStat(statName: string, delta: number) {
    const current = allocOf(statName);
    const room = delta > 0 ? Math.min(delta, ccRemaining) : delta;
    const newVal = Math.max(
      newCharStatMin,
      Math.min(newCharStatMax, current + room),
    );
    if (newVal === current) return;
    statAlloc = { ...statAlloc, [statName]: newVal };
    statsTouched = true;
  }

  /** Put every stat back to the lowest value. */
  function resetStats() {
    statAlloc = Object.fromEntries(
      newCharStatNames.map((n) => [n, newCharStatMin]),
    );
    statsTouched = true;
  }

  /**
   * Spend all the points, one at a time.  First every stat is raised to a
   * total of {@link SUGGEST_FLOOR}, lowest first, so no stat is left far
   * behind.  The rest go to the stat with the lowest allocation minus its race
   * and class bonus, so the stats the race and class are good at get the most.
   */
  function suggestStats() {
    const alloc: Record<string, number> = Object.fromEntries(
      newCharStatNames.map((n) => [n, newCharStatMin]),
    );
    /** Sort key of a stat: the one with the smallest key gets the point. */
    const key = (sn: string): [number, number] => {
      const total = alloc[sn]! + bonus(sn);
      return total < SUGGEST_FLOOR ? [0, total] : [1, alloc[sn]! - bonus(sn)];
    };
    let left = newCharStatPoints - newCharStatMin * newCharStatNames.length;
    while (left > 0) {
      let pick: string | undefined;
      for (const sn of newCharStatNames) {
        if (alloc[sn]! >= newCharStatMax) continue;
        if (pick === undefined) {
          pick = sn;
          continue;
        }
        const [a0, a1] = key(sn);
        const [b0, b1] = key(pick);
        if (a0 < b0 || (a0 === b0 && a1 < b1)) pick = sn;
      }
      if (pick === undefined) break;
      alloc[pick]!++;
      left--;
    }
    statAlloc = alloc;
    statsTouched = false;
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
    if (loginMethod >= 2 && selectedRace && selectedClass) {
      const race = selectedRace;
      const cls = selectedClass;
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

  // Until the user changes the stats, spend the points to suit the race and
  // class, and again whenever they change.
  $effect(() => {
    if (newCharStatPoints === 0 || newCharStatNames.length === 0) return;
    void selectedRace?.statAdj;
    void selectedClass?.statAdj;
    if (!statsTouched) suggestStats();
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
        statsTouched = false;
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

<!-- A race or class picker: a button per entry, then the chosen one's details. -->
{#snippet picker(
  label: string,
  entries: RaceClassEntry[],
  selected: RaceClassEntry | undefined,
  select: (arch: string) => void,
  choiceSel: number[],
)}
  <section class="cc-section">
    <h4 class="cc-heading">{label}</h4>
    <div class="cc-picker" role="radiogroup" aria-label={label}>
      {#each entries as entry (entry.archName)}
        <button
          type="button"
          role="radio"
          aria-checked={entry === selected}
          class="cc-option"
          class:selected={entry === selected}
          onclick={() => select(entry.archName)}
        >
          {entry.publicName}
        </button>
      {/each}
    </div>
    {#if selected}
      {@const desc = splitDescription(selected.description)}
      {@const bonuses = bonusList(selected)}
      <div class="cc-details">
        <div class="cc-details-head">
          <span class="cc-details-name">{selected.publicName}</span>
          {#if bonuses.length > 0}
            <span class="cc-chips">
              {#each bonuses as b}
                <span
                  class="cc-chip"
                  class:pos={b.value > 0}
                  class:neg={b.value < 0}
                  title={statLongName(b.sn)}
                >
                  {b.sn}
                  {formatBonus(b.value)}
                </span>
              {/each}
            </span>
          {/if}
        </div>
        {#if desc.facts.length > 0}
          <dl class="cc-facts">
            {#each desc.facts as fact}
              <dt>{fact.label}</dt>
              <dd>{fact.value}</dd>
            {/each}
          </dl>
        {/if}
        {#if desc.paragraphs.length > 0}
          <div class="cc-desc">
            {#each desc.paragraphs as paragraph}
              <p>{paragraph}</p>
            {/each}
          </div>
        {/if}
        {#each selected.choices as choice, ci}
          <label class="cc-choice">
            {choice.desc || choice.name}
            <select bind:value={choiceSel[ci]}>
              {#each choice.values as val, vi}
                <option value={vi}>{val.desc || val.arch}</option>
              {/each}
            </select>
          </label>
        {/each}
      </div>
    {/if}
  </section>
{/snippet}

{#if visible}
  {#if step === "form"}
    <form
      class="login-form create-char-form"
      class:has-choices={hasChoices}
      onsubmit={(e) => {
        e.preventDefault();
        handleProceedToStartingMap();
      }}
    >
      <div class="cc-title">
        <h3 class="panel-title">Create Your Character</h3>
        {#if hasChoices}
          <p class="cc-intro">
            Choose a name, a race and a class, and spend your points on your
            abilities. The race and class descriptions tell you what each is
            good at.
          </p>
        {/if}
      </div>

      <div class="cc-columns">
        <div class="cc-main">
          <section class="cc-section">
            <label class="cc-name">
              <span class="cc-heading">Name</span>
              <!-- svelte-ignore a11y_autofocus -->
              <input
                type="text"
                bind:value={newCharName}
                autocomplete="off"
                placeholder="What are you called?"
                aria-invalid={nameError !== ""}
                autofocus
              />
            </label>
            {#if nameError}
              <p class="cc-field-error">{nameError}</p>
            {/if}
          </section>

          {#if hasChoices}
            {@render picker(
              "Race",
              availableRaces,
              selectedRace,
              (arch) => {
                selectedRaceArch = arch;
                raceChoiceSel = [];
              },
              raceChoiceSel,
            )}
            {#if availableClasses.length > 0}
              {@render picker(
                "Class",
                availableClasses,
                selectedClass,
                (arch) => {
                  selectedClassArch = arch;
                  classChoiceSel = [];
                },
                classChoiceSel,
              )}
            {/if}
          {:else if loading}
            <p class="status">Loading character options…</p>
          {/if}
        </div>

        {#if hasChoices && newCharStatPoints > 0}
          <section class="cc-section cc-stats">
            <div class="cc-stats-head">
              <h4 class="cc-heading">Abilities</h4>
              <span
                class="cc-points"
                class:done={ccRemaining === 0}
                class:over={ccRemaining < 0}
              >
                {ccRemaining} point{ccRemaining === 1 ? "" : "s"} left
              </span>
            </div>
            <div class="stat-table">
              <span class="stat-col-head"></span>
              <span class="stat-col-head">Points</span>
              <span class="stat-col-head" title="Race and class bonus">
                Bonus
              </span>
              <span class="stat-col-head">Total</span>
              {#each newCharStatNames as sn}
                {@const b = bonus(sn)}
                {@const alloc = allocOf(sn)}
                {@const total = b + alloc}
                <span class="stat-name" title={STAT_INFO[sn]?.help}>
                  <span class="stat-long">{statLongName(sn)}</span>
                  {#if STAT_INFO[sn]}
                    <span class="stat-help">{STAT_INFO[sn]?.help}</span>
                  {/if}
                </span>
                <span class="stat-spin">
                  <button
                    type="button"
                    class="spin-btn"
                    aria-label="Less {statLongName(sn)}"
                    onclick={(e) => adjustStat(sn, e.shiftKey ? -BIG_STEP : -1)}
                    disabled={alloc <= newCharStatMin}
                  >
                    −
                  </button>
                  <span class="spin-val">{alloc}</span>
                  <button
                    type="button"
                    class="spin-btn"
                    aria-label="More {statLongName(sn)}"
                    onclick={(e) => adjustStat(sn, e.shiftKey ? BIG_STEP : 1)}
                    disabled={ccRemaining <= 0 || alloc >= newCharStatMax}
                  >
                    +
                  </button>
                </span>
                <span class="stat-bonus" class:pos={b > 0} class:neg={b < 0}>
                  {formatBonus(b)}
                </span>
                <span class="stat-total" class:bad={total < MIN_STAT_TOTAL}>
                  <span class="stat-total-val">{total}</span>
                  <span class="stat-bar">
                    <span
                      class="stat-bar-fill"
                      style:width="{Math.max(
                        0,
                        Math.min(100, (100 * total) / statBarMax),
                      )}%"
                    ></span>
                  </span>
                </span>
              {/each}
            </div>
            <div class="cc-stat-actions">
              <button
                type="button"
                class="cc-small-btn"
                onclick={suggestStats}
                title="Spend the points to suit the race and class"
              >
                Suggest
              </button>
              <button type="button" class="cc-small-btn" onclick={resetStats}>
                Reset
              </button>
              <span class="cc-hint">Shift-click − or + to move 5 points.</span>
            </div>
          </section>
        {/if}
      </div>

      <div class="cc-footer">
        {#if ccBlocker}
          <p class="cc-blocker">{ccBlocker}</p>
        {:else if hasChoices}
          <p class="cc-summary">
            <strong>{newCharName.trim()}</strong>, the {selectedRace?.publicName}
            {selectedClass?.publicName}{#if ccRemaining > 0}<span
                class="cc-unspent"
              >
                — {ccRemaining} point{ccRemaining === 1 ? "" : "s"} not spent
              </span>{/if}
          </p>
        {/if}
        <div class="cc-buttons">
          <button type="button" class="back-btn" onclick={onCancel}>
            ← Back
          </button>
          <button type="submit" class="cc-create" disabled={ccBlocker !== ""}>
            {availableStartingMaps.length > 0 ? "Next →" : "Create Character"}
          </button>
        </div>
      </div>
    </form>
  {:else}
    <!-- Starting map selection page -->
    <form
      class="login-form create-char-form maps"
      onsubmit={(e) => {
        e.preventDefault();
        handleCreateCharacter();
      }}
    >
      <h3 class="panel-title">Where do you want to start?</h3>
      <div class="cc-maps" role="radiogroup" aria-label="Starting map">
        {#each availableStartingMaps as map, i}
          <button
            type="button"
            role="radio"
            aria-checked={i === selectedStartingMapIdx}
            class="cc-map"
            class:selected={i === selectedStartingMapIdx}
            onclick={() => (selectedStartingMapIdx = i)}
          >
            <span class="cc-map-name">{map.publicName}</span>
            {#if map.description}
              <span class="cc-map-desc">
                {stripMarkupTags(map.description)}
              </span>
            {/if}
          </button>
        {/each}
      </div>
      <div class="cc-buttons">
        <button type="button" class="back-btn" onclick={handleBackToCreateChar}>
          ← Back
        </button>
        <button type="submit" class="cc-create">Create Character</button>
      </div>
    </form>
  {/if}
{/if}

<style>
  .create-char-form {
    max-width: 24rem;
    max-height: 80vh;
    overflow-y: auto;
    /* Room for the focus outlines, which the scroll box would clip. */
    padding: 0 4px;
  }

  .create-char-form.has-choices {
    max-width: 60rem;
  }

  .cc-title .panel-title {
    font-size: 1.3rem;
  }

  .cc-intro {
    margin: 0;
    font-size: 0.85rem;
    color: var(--text-warm-dim);
  }

  .cc-columns {
    display: flex;
    gap: 1.5rem;
    align-items: flex-start;
  }

  .cc-main {
    flex: 1 1 0;
    min-width: 0;
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }

  .cc-section {
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
  }

  .cc-heading {
    margin: 0;
    color: var(--text-warm);
    font-size: 0.95rem;
    font-weight: bold;
  }

  .cc-name input {
    font-size: 1.1rem;
  }

  .cc-name input[aria-invalid="true"] {
    border-color: #e06060;
  }

  .cc-field-error {
    margin: 0;
    color: #e06060;
    font-size: 0.8rem;
  }

  /* ── Race and class pickers ─────────────────────────────────── */

  .cc-picker {
    display: flex;
    flex-wrap: wrap;
    gap: 0.3rem;
  }

  .login-form .cc-option {
    font-family: inherit;
    padding: 0.3rem 0.65rem;
    border: 1px solid var(--border-mid);
    border-radius: 999px;
    background: var(--bg-darker);
    color: var(--text-warm-dim);
    font-size: 0.85rem;
  }

  .login-form .cc-option:hover {
    border-color: var(--accent);
    color: var(--text-warm);
  }

  .login-form .cc-option.selected {
    border-color: var(--text-warm-dim);
    background: var(--bg-warm);
    color: var(--text-warm);
    box-shadow: 0 0 0 1px var(--text-warm-dim);
  }

  .cc-details {
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
    padding: 0.6rem 0.75rem;
    border: 1px solid var(--border);
    border-left: 3px solid var(--accent);
    border-radius: 4px;
    background: var(--bg-darker);
  }

  .cc-details-head {
    display: flex;
    flex-wrap: wrap;
    align-items: baseline;
    gap: 0.5rem;
  }

  .cc-details-name {
    color: var(--text-warm);
    font-size: 1.05rem;
    font-weight: bold;
  }

  .cc-chips {
    display: flex;
    flex-wrap: wrap;
    gap: 0.25rem;
  }

  .cc-chip {
    padding: 0 0.4rem;
    border-radius: 3px;
    font-size: 0.75rem;
    font-variant-numeric: tabular-nums;
    background: var(--bg-lighter);
  }

  .cc-facts {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 0.1rem 0.6rem;
    margin: 0;
    font-size: 0.8rem;
  }

  .cc-facts dt {
    color: var(--text-warm-dim);
  }

  .cc-facts dd {
    margin: 0;
    color: var(--text-bright);
  }

  .cc-desc {
    max-height: 9rem;
    overflow-y: auto;
    font-size: 0.8rem;
    line-height: 1.45;
    color: var(--text-warm-dim);
  }

  .cc-desc p {
    margin: 0 0 0.4rem;
  }

  .cc-choice {
    font-size: 0.85rem;
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

  .pos {
    color: #80d080;
  }

  .neg,
  .bad {
    color: #e06060;
  }

  /* ── Abilities ──────────────────────────────────────────────── */

  .cc-stats {
    flex: 0 0 26rem;
    padding: 0.75rem;
    border: 1px solid var(--border);
    border-radius: 6px;
    background: var(--bg-darker);
    position: sticky;
    top: 0;
  }

  .cc-stats-head {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
  }

  .cc-points {
    padding: 0.1rem 0.5rem;
    border-radius: 999px;
    background: var(--bg-warm);
    color: var(--text-warm);
    font-size: 0.8rem;
    font-variant-numeric: tabular-nums;
  }

  .cc-points.done {
    background: #2e4a2e;
    color: #b0e0b0;
  }

  .cc-points.over {
    background: var(--danger-bg);
    color: #ffd0d0;
  }

  .stat-table {
    display: grid;
    grid-template-columns: 1fr auto 2.5rem 5.5rem;
    align-items: center;
    gap: 0.35rem 0.6rem;
    font-size: 0.85rem;
  }

  .stat-col-head {
    color: var(--text-dim);
    font-size: 0.7rem;
    text-align: center;
  }

  .stat-name {
    display: flex;
    flex-direction: column;
    line-height: 1.2;
    min-width: 0;
  }

  .stat-long {
    color: var(--text-warm);
  }

  .stat-help {
    color: var(--text-dim);
    font-size: 0.68rem;
  }

  .stat-spin {
    display: flex;
    align-items: center;
    gap: 2px;
  }

  .login-form .spin-btn {
    padding: 0;
    width: 1.5rem;
    height: 1.5rem;
    font-size: 0.9rem;
    line-height: 1;
    border: 1px solid var(--border-mid);
    background: var(--bg-lighter);
    color: var(--text-warm);
    border-radius: 3px;
  }

  .spin-val {
    min-width: 1.6rem;
    text-align: center;
    font-variant-numeric: tabular-nums;
  }

  .stat-bonus {
    text-align: center;
    font-variant-numeric: tabular-nums;
  }

  .stat-total {
    display: flex;
    align-items: center;
    gap: 0.35rem;
  }

  .stat-total-val {
    min-width: 1.4rem;
    text-align: right;
    font-weight: bold;
    font-variant-numeric: tabular-nums;
    color: var(--text-bright);
  }

  .stat-total.bad .stat-total-val {
    color: #e06060;
  }

  .stat-bar {
    flex: 1;
    height: 0.4rem;
    border-radius: 2px;
    background: var(--bg-lighter);
    overflow: hidden;
  }

  .stat-bar-fill {
    display: block;
    height: 100%;
    background: linear-gradient(90deg, var(--accent), var(--text-warm-dim));
    transition: width 0.15s ease;
  }

  .cc-stat-actions {
    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: 0.4rem;
    margin-top: 0.3rem;
    padding-top: 0.5rem;
    border-top: 1px solid var(--border);
  }

  .login-form .cc-small-btn {
    padding: 0.2rem 0.6rem;
    font-size: 0.8rem;
  }

  .cc-hint {
    color: var(--text-dim);
    font-size: 0.7rem;
  }

  /* ── Footer ─────────────────────────────────────────────────── */

  .cc-footer {
    position: sticky;
    bottom: 0;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    padding: 0.75rem 0 0.25rem;
    border-top: 1px solid var(--border);
    background: var(--bg);
  }

  .cc-footer p {
    margin: 0;
    font-size: 0.85rem;
  }

  .cc-blocker {
    color: var(--text-warm-dim);
    font-style: italic;
  }

  .cc-summary {
    color: var(--text-warm);
  }

  .cc-unspent {
    color: #e0b060;
  }

  .cc-buttons {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 0.5rem;
  }

  .login-form .cc-create {
    min-width: 12rem;
    font-weight: bold;
  }

  /* ── Starting maps ──────────────────────────────────────────── */

  .cc-maps {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }

  .create-char-form.maps {
    max-width: 36rem;
  }

  .login-form .cc-map {
    font-family: inherit;
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 0.2rem;
    padding: 0.6rem 0.75rem;
    border: 1px solid var(--border-mid);
    background: var(--bg-darker);
    text-align: left;
  }

  .login-form .cc-map.selected {
    border-color: var(--text-warm-dim);
    background: var(--bg-warm);
    box-shadow: 0 0 0 1px var(--text-warm-dim);
  }

  .cc-map-name {
    color: var(--text-warm);
    font-weight: bold;
  }

  .cc-map-desc {
    color: var(--text-warm-dim);
    font-size: 0.8rem;
  }

  @media (max-width: 720px) {
    .create-char-form,
    .create-char-form.has-choices {
      max-width: none;
    }

    .cc-columns {
      flex-direction: column;
      align-items: stretch;
    }

    .cc-stats {
      flex-basis: auto;
      position: static;
    }
  }
</style>
