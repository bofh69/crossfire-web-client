<script module lang="ts">
  import type { InfoLine } from "../lib/markup";

  /** Cached server info sections (motd, news, rules) that survive component
   *  unmount/remount.  When the player returns to the login screen after a
   *  bed-to-reality logout the server won't resend these, so we keep the last
   *  known values here and restore them on the next mount. */
  interface InfoSection {
    type: string;
    lines: InfoLine[];
  }
  let _cachedInfoSections: InfoSection[] = [];

  /** Cached login method confirmed by the server.  Like _cachedInfoSections,
   *  this survives component unmount/remount so that the character-creation
   *  form shows race/class/stat widgets correctly when the player returns to
   *  the login screen after bed-to-reality (the loginMethodConfirmed event is
   *  only emitted once, during the initial connection handshake). */
  let _cachedLoginMethod = 0;
</script>

<script lang="ts">
  import {
    clientConnect,
    clientNegotiate,
    sendAccountAddPlayer,
    sendAccountLogin,
    sendAccountNew,
    sendAccountPlay,
    sendAddMe,
    setAccountPassword,
  } from "../lib/client";
  import { disconnectAndReload } from "../lib/disconnect";
  import { gameEvents } from "../lib/events";
  import { EPORT } from "../lib/protocol";
  import CreateCharacter from "./CreateCharacter.svelte";
  import QueryPrompt, { ServerQuery } from "./QueryPrompt.svelte";
  import { parseMarkupLines } from "../lib/markup";
  import { wantConfig } from "../lib/init";
  import type { AccountPlayer } from "../lib/events";

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
    const raw = new URLSearchParams(window.location.search).get("server") ?? "";
    const HANDLER_PREFIX = "web+crossfire:";
    const addr = raw.startsWith(HANDLER_PREFIX)
      ? raw.slice(HANDLER_PREFIX.length)
      : raw;
    return addr.startsWith("ws://") || addr.startsWith("wss://") ? addr : "";
  })();

  /** Login method override from the `?loginmethod=` URL parameter (0, 1, or 2). */
  const urlParamLoginMethod = (() => {
    const raw = new URLSearchParams(window.location.search).get("loginmethod");
    if (raw === null) return null;
    const v = parseInt(raw, 10);
    return !isNaN(v) && v >= 0 && v <= 2 ? v : null;
  })();

  /** True when the page was loaded on a standard HTTP/HTTPS port (80 or 443).
   *  In that case the server address is derived automatically and the input
   *  field is hidden.  The `file:` protocol is excluded so that opening the
   *  built bundle directly from disk still shows the server-address input. */
  const standardPort = (() => {
    if (window.location.protocol === "file:") return false;
    const port = window.location.port;
    return port === "" || port === "80" || port === "443";
  })();

  /** True when the address input should be hidden (standard port or URL param). */
  const hideAddressInput = standardPort || urlParamServer !== "";

  function defaultServerAddress(): string {
    if (urlParamServer) {
      return urlParamServer;
    }
    if (standardPort) {
      const scheme = window.location.protocol === "https:" ? "wss" : "ws";
      return `${scheme}://${window.location.host}/ws`;
    }
    return "ws://" + window.location.hostname + ":" + EPORT;
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
  let errorMessage = $state("");
  const query = new ServerQuery();
  let statusMessage = $state("");

  /** Sections received from replyinfo (motd, news, rules).
   *  Initialised from the module-level cache so they survive logout/remount. */
  let serverInfoSections = $state<InfoSection[]>(_cachedInfoSections);

  /** Set to true once the server sends addme_success. We only switch to the
   *  game screen when this is true AND no query prompt is pending, so that a
   *  server that sends addme_success quickly (before the user finishes
   *  answering login prompts) doesn't cause a premature state change. */
  let addMeSuccessReceived = $state(false);

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
  let accountName = $state("");
  /** Account password input (login form). */
  let accountPassword = $state("");
  /** Confirm-password field for new-account creation. */
  let accountPasswordConfirm = $state("");

  // ── Add-existing-character state (loginmethod >= 1) ────────────────────────

  /** True when the "add existing character" form is visible. */
  let addExistingVisible = $state(false);
  /** Character name input for add-existing flow. */
  let addExistingName = $state("");
  /** Character password input for add-existing flow. */
  let addExistingPassword = $state("");
  /** True when the server failure response said force=1 is allowed. */
  let addExistingCanForce = $state(false);
  /** The error text from the server when force is possible. */
  let addExistingForceMessage = $state("");

  // ── Character-creation state (loginmethod >= 2) ────────────────────────────

  /** Login method confirmed by the server (0 = legacy, 1 or 2 = account).
   *  Restored from the module-level cache when returning from gameplay so that
   *  the character-creation form renders correctly without a new handshake. */
  let confirmedLoginMethod = $state(
    initChars !== null ? _cachedLoginMethod : 0,
  );

  /** True when the character-creation form is visible. */
  let createCharVisible = $state(false);

  function infoTypeLabel(type: string): string {
    switch (type) {
      case "motd":
        return "Message of the Day";
      case "news":
        return "News";
      case "rules":
        return "Rules";
      default:
        return type;
    }
  }

  function checkLoginComplete() {
    if (addMeSuccessReceived && !query.active) {
      statusMessage = "Login successful!";
      onLoggedIn();
    }
  }

  async function handleConnect() {
    errorMessage = "";
    serverInfoSections = [];
    connecting = true;
    statusMessage = "Connecting...";

    // Apply URL-param login method override before negotiating.
    if (urlParamLoginMethod !== null) {
      wantConfig.loginMethod = urlParamLoginMethod;
    }

    try {
      await clientConnect(serverAddress);
      connected = true;
      statusMessage = "Connected. Negotiating...";
      // Reflect the connected server in the URL so the page can be bookmarked
      // or shared and will auto-connect on reload.
      const params = new URLSearchParams(window.location.search);
      params.set("server", serverAddress);
      history.replaceState(null, "", "?" + params.toString());
      clientNegotiate();
    } catch (e) {
      errorMessage = `Connection failed: ${e instanceof Error ? e.message : String(e)}`;
      connecting = false;
      statusMessage = "";
    }
  }

  function handleDisconnectBtn() {
    disconnectAndReload();
  }

  function handleYesNoKeydown(e: KeyboardEvent) {
    if (query.handleYesNoKey(e)) {
      e.preventDefault();
      checkLoginComplete();
    }
  }

  // ── Account login handlers ─────────────────────────────────────────────────

  function handleAccountLogin() {
    errorMessage = "";
    if (!accountName.trim() || !accountPassword) {
      errorMessage = "Please enter both an account name and password.";
      return;
    }
    // Cache the password so it can be included in a future createplayer packet.
    setAccountPassword(accountPassword);
    sendAccountLogin(accountName.trim(), accountPassword);
    statusMessage = "Logging in…";
  }

  function handleAccountNew() {
    errorMessage = "";
    if (!accountName.trim() || !accountPassword) {
      errorMessage = "Please fill in all fields.";
      return;
    }
    if (accountPassword !== accountPasswordConfirm) {
      errorMessage = "Passwords do not match.";
      return;
    }
    setAccountPassword(accountPassword);
    sendAccountNew(accountName.trim(), accountPassword);
    statusMessage = "Creating account…";
  }

  function handlePlayCharacter(name: string) {
    sendAccountPlay(name);
    statusMessage = "Starting game…";
  }

  // ── Add-existing-character helpers ─────────────────────────────────────────

  /** Show the add-existing-character form. */
  function handleShowAddExisting() {
    addExistingVisible = true;
    characterSelectVisible = false;
    addExistingName = "";
    addExistingPassword = "";
    addExistingCanForce = false;
    addExistingForceMessage = "";
    errorMessage = "";
    statusMessage = "";
  }

  /** Return from the add-existing form to the character-select panel. */
  function handleCancelAddExisting() {
    addExistingVisible = false;
    characterSelectVisible = true;
    addExistingName = "";
    addExistingPassword = "";
    addExistingCanForce = false;
    addExistingForceMessage = "";
    errorMessage = "";
    statusMessage = "";
  }

  /** Submit the add-existing request (normal, force=0). */
  function handleAddExisting() {
    errorMessage = "";
    const name = addExistingName.trim();
    if (!name || !addExistingPassword) {
      errorMessage = "Please enter both a character name and password.";
      return;
    }
    addExistingCanForce = false;
    addExistingForceMessage = "";
    sendAccountAddPlayer(0, name, addExistingPassword);
    statusMessage = "Adding character…";
  }

  /** Re-submit with force=1 when the server said it is allowed. */
  function handleForceAddExisting() {
    errorMessage = "";
    addExistingCanForce = false;
    addExistingForceMessage = "";
    sendAccountAddPlayer(1, addExistingName.trim(), addExistingPassword);
    statusMessage = "Adding character…";
  }

  /** Show the create-character form. */
  function handleShowCreateChar() {
    createCharVisible = true;
    characterSelectVisible = false;
    errorMessage = "";
    statusMessage = "";
  }

  /** Return from character creation to the character-select panel. */
  function handleCancelCreateChar() {
    createCharVisible = false;
    characterSelectVisible = true;
    errorMessage = "";
    statusMessage = "";
  }

  $effect(() => {
    const cleanups = [
      query.listen(),

      gameEvents.on("replyInfo", (infoType: string, text: string) => {
        const lines = parseMarkupLines(text, "#cccccc");
        const idx = serverInfoSections.findIndex((s) => s.type === infoType);
        if (idx >= 0) {
          serverInfoSections = serverInfoSections.map((s, i) =>
            i === idx ? { type: infoType, lines } : s,
          );
        } else {
          serverInfoSections = [
            ...serverInfoSections,
            { type: infoType, lines },
          ];
        }
        // Keep the module-level cache in sync so sections survive unmount/remount.
        _cachedInfoSections = serverInfoSections;
      }),

      gameEvents.on("version", (_cs: number, _sc: number, verStr: string) => {
        statusMessage = `Server: ${verStr}. Handshaking…`;
        // Do NOT call sendAddMe() here when using account-based login.
        // The loginMethodConfirmed handler decides whether to send addme or
        // show the account login form, based on what the server supports.
        // When loginmethod 0 is requested the server will reply with loginmethod 0
        // in the setup response and loginMethodConfirmed(0) will call sendAddMe().
      }),

      gameEvents.on("loginMethodConfirmed", (method: number) => {
        confirmedLoginMethod = method;
        _cachedLoginMethod = method;
        if (method === 0) {
          // Server only supports legacy login: fall back to addme + query flow.
          sendAddMe();
        } else {
          // Server supports account-based login (method >= 1).
          accountLoginVisible = true;
          statusMessage = "";
        }
      }),

      gameEvents.on("accountPlayers", (players: AccountPlayer[]) => {
        characterList = players;
        characterSelectVisible = true;
        createCharVisible = false;
        addExistingVisible = false;
        accountLoginVisible = false;
        statusMessage = "";
        errorMessage = "";
        if (players.length === 0) {
          statusMessage = "No characters yet. Create one to start playing!";
        }
      }),

      gameEvents.on("addMeSuccess", () => {
        addMeSuccessReceived = true;
        if (query.active) {
          // A query is still on screen – wait until the user answers it.
          statusMessage = "";
        } else {
          checkLoginComplete();
        }
      }),

      gameEvents.on("addMeFail", () => {
        errorMessage = "Server rejected login.";
        statusMessage = "";
      }),

      gameEvents.on("failure", (command: string, message: string) => {
        if (command === "accountaddplayer") {
          // The message starts with a force flag: "0 <text>" (not retriable)
          // or "1 <text>" (can retry with force=1).
          const spaceIdx = message.indexOf(" ");
          const forceFlag =
            spaceIdx > 0 ? parseInt(message.substring(0, spaceIdx), 10) : 0;
          const text = spaceIdx > 0 ? message.substring(spaceIdx + 1) : message;
          errorMessage = text;
          if (forceFlag === 1) {
            addExistingCanForce = true;
            addExistingForceMessage = text;
          }
        } else {
          errorMessage = `${command}: ${message}`;
        }
        statusMessage = "";
      }),

      gameEvents.on("drawInfo", (_color: number, message: string) => {
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
    rel="noopener noreferrer">Fork me</a
  >
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
        {connecting ? "Connecting..." : "Enter"}
      </button>
    </div>
    {#if statusMessage}
      <p class="status">{statusMessage}</p>
    {/if}
    {#if errorMessage}
      <p class="error">{errorMessage}</p>
    {/if}
    <p class="game-info">
      Learn more about the game
      <a href="https://crossfire.real-time.com/" target="_blank">here</a>.
    </p>
  {:else}
    <div class="connected-layout">
      {#if serverInfoSections.length > 0}
        <div class="server-info">
          {#each serverInfoSections as section}
            <div class="info-section">
              <h3>{infoTypeLabel(section.type)}</h3>
              <div class="info-text">
                {#snippet spanList(
                  spans: import("../lib/markup").MessageSpan[],
                )}
                  {#each spans as span}<span
                      style:color={span.color}
                      style:font-weight={span.bold ? "bold" : "normal"}
                      style:font-style={span.italic ? "italic" : "normal"}
                      style:font-family={span.fontFamily}
                      style:text-decoration={span.underline
                        ? "underline"
                        : "none"}>{span.text}</span
                    >{/each}
                {/snippet}
                {#each section.lines as line}{#if line.isTitle}<p
                      class="info-title"
                    >
                      {@render spanList(line.spans)}
                    </p>{:else}<div class="info-line">
                      {@render spanList(line.spans)}
                    </div>{/if}{/each}
              </div>
            </div>
          {/each}
        </div>
      {/if}
      <div class="query-panel">
        <CreateCharacter
          visible={createCharVisible}
          loginMethod={confirmedLoginMethod}
          onCancel={handleCancelCreateChar}
          onError={(text) => (errorMessage = text)}
          onStatus={(text) => (statusMessage = text)}
        />
        {#if createCharVisible}
          <!-- CreateCharacter above is shown instead of the other panels. -->
        {:else if characterSelectVisible}
          <!-- Character selection (loginmethod >= 1) -->
          <div class="login-form">
            <h3 class="panel-title">Choose Character</h3>
            {#if characterList.length > 0}
              <div class="character-list">
                {#each characterList as char}
                  <button
                    class="character-entry"
                    onclick={() => handlePlayCharacter(char.name)}
                  >
                    <span class="char-name">{char.name}</span>
                    <span class="char-details"
                      >{char.charClass} {char.race} — Level {char.level}</span
                    >
                  </button>
                {/each}
              </div>
            {:else}
              <p class="status">No characters on this account yet.</p>
            {/if}
            <button onclick={handleShowCreateChar}
              >+ Create New Character</button
            >
            <button onclick={handleShowAddExisting}
              >+ Add Existing Character</button
            >
            <button
              class="back-btn"
              onclick={() => {
                characterSelectVisible = false;
                accountLoginVisible = true;
                errorMessage = "";
                statusMessage = "";
              }}>← Back</button
            >
          </div>
        {:else if addExistingVisible}
          <!-- Add existing character form (loginmethod >= 1) -->
          <form
            class="login-form"
            onsubmit={(e) => {
              e.preventDefault();
              handleAddExisting();
            }}
          >
            <h3 class="panel-title">Add Existing Character</h3>
            <label>
              Character Name
              <!-- svelte-ignore a11y_autofocus -->
              <input
                type="text"
                bind:value={addExistingName}
                autocomplete="username"
                autofocus
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
            <button type="submit">Add Character</button>
            {#if addExistingCanForce}
              <p class="error">{addExistingForceMessage}</p>
              <p class="query-text">
                This character is already linked to another account. Override
                the link?
              </p>
              <button type="button" onclick={handleForceAddExisting}
                >Yes, override</button
              >
            {/if}
            <button
              type="button"
              class="back-btn"
              onclick={handleCancelAddExisting}>← Back</button
            >
          </form>
        {:else if accountLoginVisible}
          <!-- Account-based login / create form (loginmethod >= 1) -->
          <form
            class="login-form"
            onsubmit={(e) => {
              e.preventDefault();
              showNewAccount ? handleAccountNew() : handleAccountLogin();
            }}
          >
            <div class="tab-row">
              <button
                type="button"
                class:tab-active={!showNewAccount}
                onclick={() => {
                  showNewAccount = false;
                  errorMessage = "";
                }}>Log In</button
              >
              <button
                type="button"
                class:tab-active={showNewAccount}
                onclick={() => {
                  showNewAccount = true;
                  errorMessage = "";
                }}>New Account</button
              >
            </div>
            <label>
              Account Name
              <!-- svelte-ignore a11y_autofocus -->
              <input
                type="text"
                bind:value={accountName}
                autocomplete="username"
                autofocus
              />
            </label>
            <label>
              Password
              <input
                type="password"
                bind:value={accountPassword}
                autocomplete={showNewAccount
                  ? "new-password"
                  : "current-password"}
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
              <button type="submit">Create Account</button>
            {:else}
              <button type="submit">Log In</button>
            {/if}
            <button onclick={handleDisconnectBtn}>Disconnect</button>
          </form>
        {:else if query.active}
          <!-- Legacy query-based login (loginmethod 0) -->
          <div class="login-form">
            <QueryPrompt {query} onReplied={checkLoginComplete} />
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
    min-height: 100dvh;
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

  .game-info {
    font-size: 0.9rem;
    color: var(--text-warm-dim);
    margin: 0;
  }

  .connected-layout {
    display: flex;
    gap: 2rem;
    align-items: flex-start;
    width: 100%;
    max-width: 72rem;
  }

  .server-info {
    flex: 1 1 24rem;
    min-width: 18rem;
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
    font-family: var(--mono);
    font-size: 0.85rem;
    line-height: 1.3;
  }

  .info-line {
    padding: 1px 0;
    word-wrap: break-word;
    white-space: pre-wrap;
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
    flex: 0 1 24rem;
    min-width: 18rem;
  }

  .query-text {
    color: var(--text-warm-dim);
    font-size: 0.9rem;
    margin: 0;
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

  @media (max-width: 900px) {
    .connected-layout {
      gap: 1rem;
    }

    .server-info,
    .query-panel {
      min-width: 0;
    }
  }

  @media (max-width: 720px) {
    .connected-layout {
      flex-direction: column;
      align-items: stretch;
    }

    .query-panel {
      order: 1;
      width: 100%;
    }

    .server-info {
      order: 2;
      width: 100%;
      max-height: 40vh;
    }
  }
</style>
