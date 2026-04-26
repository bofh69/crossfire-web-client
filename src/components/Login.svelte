<script lang="ts">
  import { onMount } from 'svelte';
  import { clientConnect, clientNegotiate, sendAddMe, sendAccountLogin, sendAccountNew, sendAccountPlay } from '../lib/client';
  import { gameEvents } from '../lib/events';
  import { sendReply } from '../lib/player';
  import { CS_QUERY_HIDEINPUT, CS_QUERY_SINGLECHAR, CS_QUERY_YESNO, EPORT } from '../lib/protocol';
  import { type InfoLine, parseMarkupLines } from '../lib/markup';
  import { wantConfig } from '../lib/init';
  import type { AccountPlayer } from '../lib/events';

  interface Props {
    onLoggedIn: () => void;
  }

  let { onLoggedIn }: Props = $props();

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
  let connected = $state(false);
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

  /** Sections received from replyinfo (motd, news, rules). */
  interface InfoSection { type: string; lines: InfoLine[]; }
  let serverInfoSections = $state<InfoSection[]>([]);

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
  let characterList = $state<AccountPlayer[]>([]);
  /** True when the character selection panel is visible. */
  let characterSelectVisible = $state(false);
  /** Account name input. */
  let accountName = $state('');
  /** Account password input (login form). */
  let accountPassword = $state('');
  /** Confirm-password field for new-account creation. */
  let accountPasswordConfirm = $state('');

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
    sendAccountNew(accountName.trim(), accountPassword);
    statusMessage = 'Creating account…';
  }

  function handlePlayCharacter(name: string) {
    sendAccountPlay(name);
    statusMessage = 'Starting game…';
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
        accountLoginVisible = false;
        statusMessage = '';
        errorMessage = '';
        if (players.length === 0) {
          statusMessage = 'No characters yet. Create one to start playing!';
        }
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
        errorMessage = `${command}: ${message}`;
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
        {#if characterSelectVisible}
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
</style>
