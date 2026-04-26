<script module lang="ts">
  import type { InfoLine } from '../lib/markup';

  /** Cached server info sections (motd, news, rules) that survive component
   *  unmount/remount.  When the player returns to the login screen after a
   *  bed-to-reality logout the server won't resend these, so we keep the last
   *  known values here and restore them on the next mount. */
  interface InfoSection { type: string; lines: InfoLine[]; }
  let _cachedInfoSections: InfoSection[] = [];

  /** Cached login method confirmed by the server.  Like _cachedInfoSections,
   *  this survives component unmount/remount so that the character-creation
   *  form shows race/class/stat widgets correctly when the player returns to
   *  the login screen after bed-to-reality (the loginMethodConfirmed event is
   *  only emitted once, during the initial connection handshake). */
  let _cachedLoginMethod = 0;
</script>

<script lang="ts">
  import { onMount } from 'svelte';
  import { clientConnect, clientNegotiate, sendAddMe, sendAccountLogin, sendAccountNew, sendAccountPlay,
           sendAccountAddPlayer, sendRequestInfo, sendCreatePlayer, setAccountPassword, getAccountPassword } from '../lib/client';
  import { gameEvents } from '../lib/events';
  import { sendReply } from '../lib/player';
  import { CS_QUERY_HIDEINPUT, CS_QUERY_SINGLECHAR, CS_QUERY_YESNO, EPORT } from '../lib/protocol';
  import { parseMarkupLines } from '../lib/markup';
  import { wantConfig } from '../lib/init';
  import type { AccountPlayer, RaceClassEntry, NewCharInfo, StartingMapEntry } from '../lib/events';

  interface Props {
    onLoggedIn: () => void;
    /** When returning from gameplay (e.g. bed-to-reality), pre-populate the
     *  character list and show the character-select screen immediately. */
    initialCharacters?: AccountPlayer[] | null;
  }

  let { onLoggedIn, initialCharacters = null }: Props = $props();

  /** Server address provided via the `?server=` URL parameter, if any.
   *  Accepts `ws://`, `wss://`, and `web+crossfire:wss?://` (protocol-handler)
   *  schemes; other values are ignored.  The `web+crossfire:` prefix is stripped
   *  so the rest of the code always sees a plain `ws://` / `wss://` address. */
  const urlParamServer = (() => {
    const raw = new URLSearchParams(window.location.search).get('server') ?? '';
    const HANDLER_PREFIX = 'web+crossfire:';
    const addr = raw.startsWith(HANDLER_PREFIX) ? raw.slice(HANDLER_PREFIX.length) : raw;
    return (addr.startsWith('ws://') || addr.startsWith('wss://')) ? addr : '';
  })();

  /** Login method override from the `?loginmethod=` URL parameter (0, 1, or 2). */
  const urlParamLoginMethod = (() => {
    const raw = new URLSearchParams(window.location.search).get('loginmethod');
    if (raw === null) return null;
    const v = parseInt(raw, 10);
    return (!isNaN(v) && v >= 0 && v <= 2) ? v : null;
  })();

  /** True when the page was loaded on a standard HTTP/HTTPS port (80 or 443).
   *  In that case the server address is derived automatically and the input
   *  field is hidden. */
  const standardPort = (() => {
    const port = window.location.port;
    return port === '' || port === '80' || port === '443';
  })();

  /** True when the address input should be hidden (standard port or URL param). */
  const hideAddressInput = standardPort || urlParamServer !== '';

  function defaultServerAddress(): string {
    if (urlParamServer) {
      return urlParamServer;
    }
    if (standardPort) {
      const scheme = window.location.protocol === 'https:' ? 'wss' : 'ws';
      return `${scheme}://${window.location.host}/ws`;
    }
    return 'ws://' + window.location.hostname + ':' + EPORT;
  }

  let serverAddress = $state(defaultServerAddress());
  // Capture the initial prop value once — we only need it for initial state.
  // Using a local const avoids reactive-capture warnings from Svelte since we
  // deliberately want the snapshot at mount time, not a live derived value.
  // svelte-ignore state_referenced_locally
  const initChars = initialCharacters;
  // When initialCharacters is provided we're returning from gameplay with the
  // socket still open, so start in the "connected" state.
  let connected = $state(initChars !== null);
  let connecting = $state(false);

  // Auto-connect immediately when the server address is supplied via URL param.
  // A brief timeout ensures that all $effects (event subscriptions) have been
  // registered before the connection attempt begins.
  onMount(() => {
    if (urlParamServer) {
      setTimeout(() => handleConnect(), 0);
    }
  });
  let errorMessage = $state('');
  let queryPrompt = $state('');
  let lastQueryPrompt = '';
  let queryHidden = $state(false);
  let querySingleChar = $state(false);
  let queryYesNo = $state(false);
  let queryInput = $state('');
  let statusMessage = $state('');

  /** Sections received from replyinfo (motd, news, rules).
   *  Initialised from the module-level cache so they survive logout/remount. */
  let serverInfoSections = $state<InfoSection[]>(_cachedInfoSections);

  /** Set to true once the server sends addme_success. We only switch to the
   *  game screen when this is true AND no query prompt is pending, so that a
   *  server that sends addme_success quickly (before the user finishes
   *  answering login prompts) doesn't cause a premature state change. */
  let addMeSuccessReceived = $state(false);

  /** Element reference for the query input, used for programmatic focus. */
  let queryInputEl: HTMLInputElement | undefined = $state();

  // ── Account-based login state (loginmethod >= 1) ───────────────────────────

  /** True when the server has confirmed loginmethod >= 1 and we should show
   *  the account-based login form instead of the legacy query flow. */
  let accountLoginVisible = $state(false);
  /** True when showing "create new account" form instead of login form. */
  let showNewAccount = $state(false);
  /** Characters returned by the server's accountplayers command. */
  let characterList = $state<AccountPlayer[]>(initChars ?? []);
  /** True when the character selection panel is visible. */
  let characterSelectVisible = $state(initChars !== null);
  /** Account name input. */
  let accountName = $state('');
  /** Account password input (login form). */
  let accountPassword = $state('');
  /** Confirm-password field for new-account creation. */
  let accountPasswordConfirm = $state('');

  // ── Add-existing-character state (loginmethod >= 1) ────────────────────────

  /** True when the "add existing character" form is visible. */
  let addExistingVisible = $state(false);
  /** Character name input for add-existing flow. */
  let addExistingName = $state('');
  /** Character password input for add-existing flow. */
  let addExistingPassword = $state('');
  /** True when the server failure response said force=1 is allowed. */
  let addExistingCanForce = $state(false);
  /** The error text from the server when force is possible. */
  let addExistingForceMessage = $state('');

  // ── Character-creation state (loginmethod >= 2) ────────────────────────────

  /** Login method confirmed by the server (0 = legacy, 1 or 2 = account).
   *  Restored from the module-level cache when returning from gameplay so that
   *  the character-creation form renders correctly without a new handshake. */
  let confirmedLoginMethod = $state(initChars !== null ? _cachedLoginMethod : 0);

  /** True when the character-creation form is visible. */
  let createCharVisible = $state(false);
  /** True when the starting-map selection page is visible. */
  let startingMapVisible = $state(false);

  /** True once we have sent the race/class/newcharinfo requests for this session. */
  let charInfoRequested = $state(false);

  /** Race and class lists received from the server. */
  let availableRaces = $state<RaceClassEntry[]>([]);
  let availableClasses = $state<RaceClassEntry[]>([]);

  /** Starting map options received from the server (empty = no choice needed). */
  let availableStartingMaps = $state<StartingMapEntry[]>([]);
  /** Selected index in availableStartingMaps. */
  let selectedStartingMapIdx = $state(0);

  /** newcharinfo data from the server. */
  let newCharStatPoints = $state(0);
  let newCharStatMin = $state(1);
  let newCharStatMax = $state(20);
  let newCharStatNames = $state<string[]>([]);

  /** Character-creation form: name input. */
  let newCharName = $state('');
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

  // ── Derived values for character creation ──────────────────────────────────

  const ccSpent = $derived(
    newCharStatNames.reduce((s, n) => s + (statAlloc[n] ?? 0), 0),
  );
  const ccRemaining = $derived(newCharStatPoints - ccSpent);
  const ccHasNegStat = $derived(
    newCharStatNames.some(sn => {
      const rb = availableRaces[selectedRaceIdx]?.statAdj[sn] ?? 0;
      const cb = availableClasses[selectedClassIdx]?.statAdj[sn] ?? 0;
      return rb + cb + (statAlloc[sn] ?? 0) < newCharStatMin;
    }),
  );
  const ccCanCreate = $derived(
    newCharName.trim().length > 0
    && ccRemaining >= 0
    && !ccHasNegStat
    // Allow creation when: loginmethod 1 (no stats needed), or loginmethod 2
    // data has arrived (statPoints set), or server didn't send any races.
    && (confirmedLoginMethod < 2 || newCharStatPoints > 0 || availableRaces.length === 0),
  );

  // Focus the query input whenever it (re-)appears in the DOM.
  $effect(() => {
    if (queryInputEl) {
      queryInputEl.focus();
    }
  });

  function infoTypeLabel(type: string): string {
    switch (type) {
      case 'motd': return 'Message of the Day';
      case 'news': return 'News';
      case 'rules': return 'Rules';
      default: return type;
    }
  }

  function checkLoginComplete() {
    if (addMeSuccessReceived && !queryPrompt) {
      statusMessage = 'Login successful!';
      onLoggedIn();
    }
  }

  async function handleConnect() {
    errorMessage = '';
    serverInfoSections = [];
    connecting = true;
    statusMessage = 'Connecting...';

    // Apply URL-param login method override before negotiating.
    if (urlParamLoginMethod !== null) {
      wantConfig.loginMethod = urlParamLoginMethod;
    }

    try {
      await clientConnect(serverAddress);
      connected = true;
      statusMessage = 'Connected. Negotiating...';
      clientNegotiate();
    } catch (e) {
      errorMessage = `Connection failed: ${e instanceof Error ? e.message : String(e)}`;
      connecting = false;
      statusMessage = '';
    }
  }

  function clearQuery() {
    queryInput = '';
    queryPrompt = '';
    querySingleChar = false;
    queryYesNo = false;
  }

  function sendQueryReply(answer: string) {
    sendReply(answer);
    clearQuery();
    checkLoginComplete();
  }

  function handleQuerySubmit() {
    let trimmed = queryInput.trim();
    if (trimmed.length > 0) {
      sendQueryReply(trimmed);
    }
  }

  function handleQueryKeydown(e: KeyboardEvent) {
    if (querySingleChar && e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
      // For single-character queries, send the reply immediately on keypress
      // without requiring Enter. The typed character becomes the reply.
      sendQueryReply(e.key);
      e.preventDefault();
    } else if (e.key === 'Enter') {
      handleQuerySubmit();
      e.stopPropagation();
    }
  }

  function handleYesNoKeydown(e: KeyboardEvent) {
    if (!queryYesNo) return;
    const key = e.key.toLowerCase();
    if ((key === 'y' || key === 'n') && !e.ctrlKey && !e.altKey && !e.metaKey) {
      sendQueryReply(key);
      e.preventDefault();
    }
  }

  // ── Account login handlers ─────────────────────────────────────────────────

  function handleAccountLogin() {
    errorMessage = '';
    if (!accountName.trim() || !accountPassword) {
      errorMessage = 'Please enter both an account name and password.';
      return;
    }
    // Cache the password so it can be included in a future createplayer packet.
    setAccountPassword(accountPassword);
    sendAccountLogin(accountName.trim(), accountPassword);
    statusMessage = 'Logging in…';
  }

  function handleAccountNew() {
    errorMessage = '';
    if (!accountName.trim() || !accountPassword) {
      errorMessage = 'Please fill in all fields.';
      return;
    }
    if (accountPassword !== accountPasswordConfirm) {
      errorMessage = 'Passwords do not match.';
      return;
    }
    setAccountPassword(accountPassword);
    sendAccountNew(accountName.trim(), accountPassword);
    statusMessage = 'Creating account…';
  }

  function handlePlayCharacter(name: string) {
    sendAccountPlay(name);
    statusMessage = 'Starting game…';
  }

  // ── Add-existing-character helpers ─────────────────────────────────────────

  /** Show the add-existing-character form. */
  function handleShowAddExisting() {
    addExistingVisible = true;
    characterSelectVisible = false;
    addExistingName = '';
    addExistingPassword = '';
    addExistingCanForce = false;
    addExistingForceMessage = '';
    errorMessage = '';
    statusMessage = '';
  }

  /** Return from the add-existing form to the character-select panel. */
  function handleCancelAddExisting() {
    addExistingVisible = false;
    characterSelectVisible = true;
    addExistingName = '';
    addExistingPassword = '';
    addExistingCanForce = false;
    addExistingForceMessage = '';
    errorMessage = '';
    statusMessage = '';
  }

  /** Submit the add-existing request (normal, force=0). */
  function handleAddExisting() {
    errorMessage = '';
    const name = addExistingName.trim();
    if (!name || !addExistingPassword) {
      errorMessage = 'Please enter both a character name and password.';
      return;
    }
    addExistingCanForce = false;
    addExistingForceMessage = '';
    sendAccountAddPlayer(0, name, addExistingPassword);
    statusMessage = 'Adding character…';
  }

  /** Re-submit with force=1 when the server said it is allowed. */
  function handleForceAddExisting() {
    errorMessage = '';
    addExistingCanForce = false;
    addExistingForceMessage = '';
    sendAccountAddPlayer(1, addExistingName.trim(), addExistingPassword);
    statusMessage = 'Adding character…';
  }



  /** Show the create-character form, requesting server data if needed. */
  function handleShowCreateChar() {
    createCharVisible = true;
    characterSelectVisible = false;
    errorMessage = '';
    statusMessage = '';
    if (!charInfoRequested) {
      charInfoRequested = true;
      sendRequestInfo('race_list');
      sendRequestInfo('class_list');
      sendRequestInfo('newcharinfo');
    }
  }

  /** Return from character-creation form to the character-select panel. */
  function handleCancelCreateChar() {
    createCharVisible = false;
    startingMapVisible = false;
    characterSelectVisible = true;
    errorMessage = '';
    statusMessage = '';
  }

  /**
   * Called when the user clicks "Next" on the character-creation form.
   * If starting maps are available, show the starting-map page; otherwise
   * submit the character immediately.
   */
  function handleProceedToStartingMap() {
    errorMessage = '';
    const name = newCharName.trim();
    if (!name) {
      errorMessage = 'Please enter a character name.';
      return;
    }
    if (availableStartingMaps.length > 0) {
      createCharVisible = false;
      startingMapVisible = true;
    } else {
      handleCreateCharacter();
    }
  }

  /** Return from the starting-map page to the character-creation form. */
  function handleBackToCreateChar() {
    startingMapVisible = false;
    createCharVisible = true;
    errorMessage = '';
    statusMessage = '';
  }

  /**
   * Increment or decrement a stat allocation by `delta`.
   * - Allocation can never go below 0.
   * - Decreasing: the stat total (race + class bonus + alloc) must not drop
   *   below statMin (from newcharinfo).
   * - Increasing: the stat total must not exceed statMax, and the player must
   *   have remaining points. Increasing is always allowed even when the current
   *   total is still below statMin (e.g. a large race penalty), so the user
   *   can work their way back to a valid value.
   */
  function adjustStat(statName: string, delta: number) {
    const current = statAlloc[statName] ?? 0;
    const newVal = current + delta;
    if (newVal < 0) return;
    const raceBonus = availableRaces[selectedRaceIdx]?.statAdj[statName] ?? 0;
    const classBonus = availableClasses[selectedClassIdx]?.statAdj[statName] ?? 0;
    const newTotal = raceBonus + classBonus + newVal;
    if (delta < 0 && newTotal < newCharStatMin) return;
    if (delta > 0 && newTotal > newCharStatMax) return;
    statAlloc = { ...statAlloc, [statName]: newVal };
  }

  /** Send the createplayer command to the server. */
  function handleCreateCharacter() {
    errorMessage = '';
    const name = newCharName.trim();
    if (!name) {
      errorMessage = 'Please enter a character name.';
      return;
    }
    const password = getAccountPassword();
    if (confirmedLoginMethod >= 2 && availableRaces.length > 0 && availableClasses.length > 0) {
      const race  = availableRaces[selectedRaceIdx]!;
      const cls   = availableClasses[selectedClassIdx]!;
      const rChoices = race.choices.map((ch, i) => {
        const selIdx = raceChoiceSel[i] ?? 0;
        return { choiceName: ch.name, valueArch: ch.values[selIdx]?.arch ?? '' };
      }).filter(c => c.valueArch !== '');
      const cChoices = cls.choices.map((ch, i) => {
        const selIdx = classChoiceSel[i] ?? 0;
        return { choiceName: ch.name, valueArch: ch.values[selIdx]?.arch ?? '' };
      }).filter(c => c.valueArch !== '');
      const sAlloc = newCharStatNames.map(sn => ({
        statName: sn,
        value: statAlloc[sn] ?? 0,
      }));
      const startingMapArch = availableStartingMaps[selectedStartingMapIdx]?.archName;
      sendCreatePlayer(name, password, race.archName, cls.archName, rChoices, cChoices, sAlloc, startingMapArch);
    } else {
      // loginmethod 1 — just name + password.
      sendCreatePlayer(name, password);
    }
    statusMessage = 'Creating character…';
  }

  $effect(() => {
    const cleanups = [
      gameEvents.on('query', (flags: number, prompt: string) => {
        // If the server sends an empty prompt, reuse the previous prompt text.
        if (prompt) {
          lastQueryPrompt = prompt;
        }
        queryPrompt = lastQueryPrompt;
        queryHidden = (flags & CS_QUERY_HIDEINPUT) !== 0;
        querySingleChar = (flags & CS_QUERY_SINGLECHAR) !== 0;
        queryYesNo = (flags & CS_QUERY_YESNO) !== 0;
        queryInput = '';
      }),

      gameEvents.on('replyInfo', (infoType: string, text: string) => {
        const lines = parseMarkupLines(text, '#cccccc');
        const idx = serverInfoSections.findIndex(s => s.type === infoType);
        if (idx >= 0) {
          serverInfoSections = serverInfoSections.map((s, i) => i === idx ? { type: infoType, lines } : s);
        } else {
          serverInfoSections = [...serverInfoSections, { type: infoType, lines }];
        }
        // Keep the module-level cache in sync so sections survive unmount/remount.
        _cachedInfoSections = serverInfoSections;
      }),

      gameEvents.on('version', (_cs: number, _sc: number, verStr: string) => {
        statusMessage = `Server: ${verStr}. Handshaking…`;
        // Do NOT call sendAddMe() here when using account-based login.
        // The loginMethodConfirmed handler decides whether to send addme or
        // show the account login form, based on what the server supports.
        // When loginmethod 0 is requested the server will reply with loginmethod 0
        // in the setup response and loginMethodConfirmed(0) will call sendAddMe().
      }),

      gameEvents.on('loginMethodConfirmed', (method: number) => {
        confirmedLoginMethod = method;
        _cachedLoginMethod = method;
        if (method === 0) {
          // Server only supports legacy login: fall back to addme + query flow.
          sendAddMe();
        } else {
          // Server supports account-based login (method >= 1).
          accountLoginVisible = true;
          statusMessage = '';
        }
      }),

      gameEvents.on('accountPlayers', (players: AccountPlayer[]) => {
        characterList = players;
        characterSelectVisible = true;
        createCharVisible = false;
        addExistingVisible = false;
        accountLoginVisible = false;
        statusMessage = '';
        errorMessage = '';
        if (players.length === 0) {
          statusMessage = 'No characters yet. Create one to start playing!';
        }
      }),

      // ── Character-creation events ──────────────────────────────────────────

      gameEvents.on('raceListReceived', (archNames: string[]) => {
        // Initialise with arch-name-only placeholders; detail arrives via race_info.
        availableRaces = archNames.map(archName => ({
          archName, publicName: archName, description: '', statAdj: {}, choices: [],
        }));
        if (selectedRaceIdx >= availableRaces.length) selectedRaceIdx = 0;
        // Request detailed info for every race so the UI can show public names,
        // descriptions, stat bonuses, and optional choices.
        for (const archName of archNames) {
          sendRequestInfo(`race_info ${archName}`);
        }
      }),

      gameEvents.on('classListReceived', (archNames: string[]) => {
        availableClasses = archNames.map(archName => ({
          archName, publicName: archName, description: '', statAdj: {}, choices: [],
        }));
        if (selectedClassIdx >= availableClasses.length) selectedClassIdx = 0;
        // Request detailed info for every class so the UI can show public names,
        // descriptions, stat bonuses, and optional choices.
        for (const archName of archNames) {
          sendRequestInfo(`class_info ${archName}`);
        }
      }),

      gameEvents.on('raceInfoReceived', (info: RaceClassEntry) => {
        const idx = availableRaces.findIndex(r => r.archName === info.archName);
        const updated = idx >= 0 ? availableRaces.with(idx, info) : [...availableRaces, info];
        availableRaces = updated.toSorted((a, b) => a.publicName.localeCompare(b.publicName));
      }),

      gameEvents.on('classInfoReceived', (info: RaceClassEntry) => {
        const idx = availableClasses.findIndex(c => c.archName === info.archName);
        const updated = idx >= 0 ? availableClasses.with(idx, info) : [...availableClasses, info];
        availableClasses = updated.toSorted((a, b) => a.publicName.localeCompare(b.publicName));
      }),

      gameEvents.on('newCharInfoReceived', (info: NewCharInfo) => {
        newCharStatPoints = info.statPoints;
        newCharStatMin = info.statMin;
        newCharStatMax = info.statMax;
        newCharStatNames = info.statNames;
        // Reset per-stat allocations to zero whenever the stat list changes.
        statAlloc = Object.fromEntries(info.statNames.map(n => [n, 0]));
        // Server indicated starting-map selection is required.
        if (info.wantsStartingMap) {
          availableStartingMaps = [];
          selectedStartingMapIdx = 0;
          sendRequestInfo('startingmap');
        }
      }),

      gameEvents.on('startingMapReceived', (maps: StartingMapEntry[]) => {
        availableStartingMaps = maps;
        selectedStartingMapIdx = 0;
      }),

      gameEvents.on('addMeSuccess', () => {
        addMeSuccessReceived = true;
        if (queryPrompt) {
          // A query is still on screen – wait until the user answers it.
          statusMessage = '';
        } else {
          checkLoginComplete();
        }
      }),

      gameEvents.on('addMeFail', () => {
        errorMessage = 'Server rejected login.';
        statusMessage = '';
      }),

      gameEvents.on('failure', (command: string, message: string) => {
        if (command === 'accountaddplayer') {
          // The message starts with a force flag: "0 <text>" (not retriable)
          // or "1 <text>" (can retry with force=1).
          const spaceIdx = message.indexOf(' ');
          const forceFlag = spaceIdx > 0 ? parseInt(message.substring(0, spaceIdx), 10) : 0;
          const text = spaceIdx > 0 ? message.substring(spaceIdx + 1) : message;
          errorMessage = text;
          if (forceFlag === 1) {
            addExistingCanForce = true;
            addExistingForceMessage = text;
          }
        } else {
          errorMessage = `${command}: ${message}`;
        }
        statusMessage = '';
      }),

      gameEvents.on('drawInfo', (_color: number, message: string) => {
        if (!connected) return;
        statusMessage = message;
      }),
    ];

    return () => {
      // Unsubscribe all Login-screen handlers.  The event bus supports
      // multiple listeners, so App.svelte's own subscriptions (registered
      // in wireCallbacks()) coexist without conflict.
      for (const unsub of cleanups) unsub();
    };
  });
</script>

<svelte:window onkeydown={handleYesNoKeydown} />

{#if !connected}
  <a
    class="github-ribbon"
    href="https://github.com/bofh69/crossfire-web-client"
    target="_blank"
    rel="noopener noreferrer"
  >Fork me</a>
{/if}

<div class="login-container">
  <h1>⚔ Welcome to Crossfire ⚔</h1>
  <h2>The Multiplayer Adventure Game</h2>

  {#if !connected}
    <div class="login-form">
      {#if !hideAddressInput}
        <label>
          Server Address
          <input
            type="text"
            bind:value={serverAddress}
            placeholder="ws://hostname:port"
            disabled={connecting}
          />
        </label>
      {/if}
      <button onclick={handleConnect} disabled={connecting}>
        {connecting ? 'Connecting...' : 'Enter'}
      </button>
    </div>
    {#if statusMessage}
      <p class="status">{statusMessage}</p>
    {/if}
    {#if errorMessage}
      <p class="error">{errorMessage}</p>
    {/if}
  {:else}
    <div class="connected-layout">
      {#if serverInfoSections.length > 0}
        <div class="server-info">
          {#each serverInfoSections as section}
            <div class="info-section">
              <h3>{infoTypeLabel(section.type)}</h3>
              <div class="info-text">
                {#snippet spanList(spans: import('../lib/markup').MessageSpan[])}
                  {#each spans as span}<span
                    style:color={span.color}
                    style:font-weight={span.bold ? 'bold' : 'normal'}
                    style:font-style={span.italic ? 'italic' : 'normal'}
                    style:text-decoration={span.underline ? 'underline' : 'none'}
                  >{span.text}</span>{/each}
                {/snippet}
                {#each section.lines as line}{#if line.isTitle}<p class="info-title">{@render spanList(line.spans)}</p>{:else}<div class="info-line">{@render spanList(line.spans)}</div>{/if}{/each}
              </div>
            </div>
          {/each}
        </div>
      {/if}
      <div class="query-panel">
        {#if createCharVisible}
          <!-- Character creation form (loginmethod >= 1) -->
          <div class="login-form create-char-form">
            <h3 class="panel-title">Create New Character</h3>

            {#if confirmedLoginMethod >= 2 && newCharStatPoints === 0}
              <p class="status">Loading character options…</p>
            {/if}

            <label>
              Character Name
              <input type="text" bind:value={newCharName} autocomplete="off" />
            </label>

            {#if confirmedLoginMethod >= 2 && availableRaces.length > 0}
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
                <p class="rc-desc">{availableRaces[selectedRaceIdx]?.description}</p>
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
                  <p class="rc-desc">{availableClasses[selectedClassIdx]?.description}</p>
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
                      <span class:bonus-pos={rb > 0} class:bonus-neg={rb < 0}>{rb > 0 ? '+' : ''}{rb}</span>
                      <span class:bonus-pos={cb > 0} class:bonus-neg={cb < 0}>{cb > 0 ? '+' : ''}{cb}</span>
                      <span class="stat-spin">
                        <button class="spin-btn" onclick={() => adjustStat(sn, -1)} disabled={alloc <= 0 || rb + cb + alloc - 1 < newCharStatMin}>−</button>
                        <span class="spin-val">{alloc}</span>
                        <button class="spin-btn" onclick={() => adjustStat(sn, +1)} disabled={ccRemaining <= 0 || rb + cb + alloc + 1 > newCharStatMax}>+</button>
                      </span>
                      <span class:stat-bad={total < 1}>{total}</span>
                    </div>
                  {/each}
                  <div class="stat-remaining" class:points-over={ccRemaining < 0}>
                    Points remaining: {ccRemaining}
                  </div>
                </div>
              {/if}
            {/if}

            <button onclick={handleProceedToStartingMap} disabled={!ccCanCreate}>
              {availableStartingMaps.length > 0 ? 'Next →' : 'Create Character'}
            </button>
            <button class="back-btn" onclick={handleCancelCreateChar}>← Back</button>
          </div>

        {:else if startingMapVisible}
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
              <p class="entry-desc">{availableStartingMaps[selectedStartingMapIdx]?.description}</p>
            {/if}

            <button onclick={handleCreateCharacter}>Create Character</button>
            <button class="back-btn" onclick={handleBackToCreateChar}>← Back</button>
          </div>

        {:else if characterSelectVisible}
          <!-- Character selection (loginmethod >= 1) -->
          <div class="login-form">
            <h3 class="panel-title">Choose Character</h3>
            {#if characterList.length > 0}
              <div class="character-list">
                {#each characterList as char}
                  <button class="character-entry" onclick={() => handlePlayCharacter(char.name)}>
                    <span class="char-name">{char.name}</span>
                    <span class="char-details">{char.charClass} {char.race} — Level {char.level}</span>
                  </button>
                {/each}
              </div>
            {:else}
              <p class="status">No characters on this account yet.</p>
            {/if}
            <button onclick={handleShowCreateChar}>+ Create New Character</button>
            <button onclick={handleShowAddExisting}>+ Add Existing Character</button>
            <button
              class="back-btn"
              onclick={() => { characterSelectVisible = false; accountLoginVisible = true; errorMessage = ''; statusMessage = ''; }}
            >← Back</button>
          </div>
        {:else if addExistingVisible}
          <!-- Add existing character form (loginmethod >= 1) -->
          <div class="login-form">
            <h3 class="panel-title">Add Existing Character</h3>
            <label>
              Character Name
              <input
                type="text"
                bind:value={addExistingName}
                autocomplete="username"
              />
            </label>
            <label>
              Character Password
              <input
                type="password"
                bind:value={addExistingPassword}
                autocomplete="current-password"
              />
            </label>
            <button onclick={handleAddExisting}>Add Character</button>
            {#if addExistingCanForce}
              <p class="error">{addExistingForceMessage}</p>
              <p class="query-text">This character is already linked to another account. Override the link?</p>
              <button onclick={handleForceAddExisting}>Yes, override</button>
            {/if}
            <button class="back-btn" onclick={handleCancelAddExisting}>← Back</button>
          </div>
        {:else if accountLoginVisible}
          <!-- Account-based login / create form (loginmethod >= 1) -->
          <div class="login-form">
            <div class="tab-row">
              <button
                class:tab-active={!showNewAccount}
                onclick={() => { showNewAccount = false; errorMessage = ''; }}
              >Log In</button>
              <button
                class:tab-active={showNewAccount}
                onclick={() => { showNewAccount = true; errorMessage = ''; }}
              >New Account</button>
            </div>
            <label>
              Account Name
              <input
                type="text"
                bind:value={accountName}
                autocomplete="username"
              />
            </label>
            <label>
              Password
              <input
                type="password"
                bind:value={accountPassword}
                autocomplete={showNewAccount ? 'new-password' : 'current-password'}
              />
            </label>
            {#if showNewAccount}
              <label>
                Confirm Password
                <input
                  type="password"
                  bind:value={accountPasswordConfirm}
                  autocomplete="new-password"
                />
              </label>
              <button onclick={handleAccountNew}>Create Account</button>
            {:else}
              <button onclick={handleAccountLogin}>Log In</button>
            {/if}
          </div>
        {:else if queryPrompt}
          <!-- Legacy query-based login (loginmethod 0) -->
          <div class="login-form">
            <p class="query-text" aria-live="polite">{queryPrompt}</p>
            {#if queryYesNo}
              <div class="yesno-buttons">
                <button aria-keyshortcuts="y" onclick={() => sendQueryReply('y')}>Yes</button>
                <button aria-keyshortcuts="n" onclick={() => sendQueryReply('n')}>No</button>
              </div>
            {:else}
              <label>
                <input
                  type={queryHidden ? 'password' : 'text'}
                  bind:value={queryInput}
                  bind:this={queryInputEl}
                  onkeydown={handleQueryKeydown}
                />
              </label>
              {#if !querySingleChar}
                <button onclick={handleQuerySubmit}>Submit</button>
              {/if}
            {/if}
          </div>
        {/if}
        {#if statusMessage}
          <p class="status">{statusMessage}</p>
        {/if}
        {#if errorMessage}
          <p class="error">{errorMessage}</p>
        {/if}
      </div>
    </div>
  {/if}
</div>

<style>
  .login-container {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    padding: 2rem 1rem;
    gap: 1rem;
  }

  h1 {
    color: var(--text-warm);
    font-size: 2rem;
    margin-bottom: -1rem;
  }

  h2 {
    color: var(--text-warm);
    font-size: 1rem;
    margin-top: 0rem;
    margin-bottom: 1rem;
  }

  .connected-layout {
    display: flex;
    gap: 2rem;
    align-items: flex-start;
    width: 100%;
    max-width: 70%;
  }

  .server-info {
    flex: 1;
    max-height: 70vh;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 1rem;
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 1rem;
    background: var(--bg-darker);
  }

  .info-section h3 {
    color: var(--text-warm);
    font-size: 0.9rem;
    margin: 0 0 0.5rem 0;
    border-bottom: 1px solid var(--border);
    padding-bottom: 0.25rem;
  }

  .info-text {
    font-family: 'Courier New', monospace;
    font-size: 0.85rem;
    line-height: 1.3;
  }

  .info-line {
    padding: 1px 0;
    word-wrap: break-word;
  }

  .info-title {
    color: var(--text-warm);
    font-weight: bold;
    font-size: 0.9rem;
    margin: 0.5rem 0 0.25rem 0;
    padding: 0;
    word-wrap: break-word;
  }

  .query-panel {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
    min-width: 280px;
  }

  .login-form {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
    width: 320px;
  }

  label {
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
    color: var(--text-warm-dim);
    font-size: 0.9rem;
  }

  input {
    padding: 0.5rem;
    border: 1px solid var(--border-light);
    border-radius: 4px;
    background: var(--bg-lighter);
    color: var(--text-bright);
    font-size: 1rem;
  }

  input:focus {
    outline: none;
    border-color: var(--accent);
  }

  button {
    padding: 0.6rem 1rem;
    border: 1px solid var(--accent);
    border-radius: 4px;
    background: var(--bg-warm);
    color: var(--text-warm);
    font-size: 1rem;
    cursor: pointer;
  }

  button:hover:not(:disabled) {
    background: var(--bg-warm-hover);
  }

  button:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .status {
    color: #a0c0a0;
    font-size: 0.85rem;
  }

  .error {
    color: #e06060;
    font-size: 0.85rem;
  }

  .query-text {
    color: var(--text-warm-dim);
    font-size: 0.9rem;
    margin: 0;
  }

  .yesno-buttons {
    display: flex;
    gap: 0.5rem;
  }

  .yesno-buttons button {
    flex: 1;
  }

  .github-ribbon {
    position: fixed;
    top: 0.5rem;
    right: -4.5rem;
    z-index: 100;
    display: block;
    width: 12rem;
    padding: 0.35rem 0;
    background: var(--accent);
    color: var(--text-warm);
    font-size: 0.8rem;
    font-weight: bold;
    text-align: center;
    text-decoration: none;
    transform: rotate(45deg);
    box-shadow: 0 2px 6px rgba(0, 0, 0, 0.5);
    transition: background 0.2s;
  }

  .github-ribbon:hover {
    background: #9a8a6a;
  }

  .panel-title {
    color: var(--text-warm);
    font-size: 1rem;
    margin: 0 0 0.5rem 0;
    border-bottom: 1px solid var(--border);
    padding-bottom: 0.25rem;
  }

  .tab-row {
    display: flex;
    gap: 0;
    border-bottom: 1px solid var(--border);
    margin-bottom: 0.25rem;
  }

  .tab-row button {
    flex: 1;
    border: 1px solid var(--border);
    border-bottom: none;
    border-radius: 4px 4px 0 0;
    background: var(--bg-darker);
    color: var(--text-warm-dim);
    font-size: 0.9rem;
    padding: 0.4rem 0.75rem;
  }

  .tab-row button.tab-active {
    background: var(--bg-warm);
    color: var(--text-warm);
  }

  .character-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    max-height: 40vh;
    overflow-y: auto;
  }

  .character-entry {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 0.1rem;
    padding: 0.5rem 0.75rem;
    border: 1px solid var(--border);
    border-radius: 4px;
    background: var(--bg-darker);
    cursor: pointer;
    text-align: left;
  }

  .character-entry:hover {
    background: var(--bg-warm);
    border-color: var(--accent);
  }

  .char-name {
    color: var(--text-warm);
    font-weight: bold;
    font-size: 1rem;
  }

  .char-details {
    color: var(--text-warm-dim);
    font-size: 0.8rem;
  }

  .back-btn {
    align-self: flex-start;
    background: transparent;
    border: 1px solid var(--border);
    color: var(--text-warm-dim);
    font-size: 0.85rem;
    padding: 0.3rem 0.6rem;
  }

  .back-btn:hover {
    background: var(--bg-darker);
    color: var(--text-warm);
    border-color: var(--accent);
  }

  /* ── Character-creation styles ─────────────────────────────────────────── */

  .create-char-form {
    width: 360px;
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
    max-height: 5em;
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

  .bonus-pos { color: #80d080; }
  .bonus-neg { color: #e06060; }

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
</style>
