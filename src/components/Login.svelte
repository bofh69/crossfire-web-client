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
    clientDisconnect,
    clientNegotiate,
    sendAccountLogin,
    sendAccountPlay,
    sendAddMe,
    setAccountPassword,
  } from "../lib/client";
  import type { ReconnectLogin } from "../lib/client";
  import { clientTour } from "../lib/client_tour.svelte";
  import { onMount } from "svelte";
  import { gameEvents } from "../lib/events";
  import { EPORT } from "../lib/protocol";
  import AccountLogin from "./AccountLogin.svelte";
  import AddExistingCharacter from "./AddExistingCharacter.svelte";
  import CharacterList from "./CharacterList.svelte";
  import CreateCharacter from "./CreateCharacter.svelte";
  import QueryPrompt, { ServerQuery } from "./QueryPrompt.svelte";
  import { parseMarkupLines } from "../lib/markup";
  import { wantConfig } from "../lib/init";
  import { loadConfig, saveConfig } from "../lib/storage";
  import type { AccountPlayer } from "../lib/events";

  interface Props {
    onLoggedIn: () => void;
    /** When returning from gameplay (e.g. bed-to-reality), pre-populate the
     *  character list and show the character-select screen immediately. */
    initialCharacters?: AccountPlayer[] | null;
    /** When reconnecting after a lost connection, connect at once and log
     *  back in to this account and character. */
    reconnectLogin?: ReconnectLogin | null;
  }

  let {
    onLoggedIn,
    initialCharacters = null,
    reconnectLogin = null,
  }: Props = $props();

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

  /** Config key of the last server address typed in the address field. */
  const SERVER_ADDRESS_KEY = "serverAddress";

  function defaultServerAddress(): string {
    if (urlParamServer) {
      return urlParamServer;
    }
    if (standardPort) {
      const scheme = window.location.protocol === "https:" ? "wss" : "ws";
      return `${scheme}://${window.location.host}/ws`;
    }
    return loadConfig(
      SERVER_ADDRESS_KEY,
      "ws://" + window.location.hostname + ":" + EPORT,
    );
  }

  let serverAddress = $state(defaultServerAddress());
  // Capture the initial prop value once — we only need it for initial state.
  // Using a local const avoids reactive-capture warnings from Svelte since we
  // deliberately want the snapshot at mount time, not a live derived value.
  // svelte-ignore state_referenced_locally
  const initChars = initialCharacters;
  // svelte-ignore state_referenced_locally
  /** Login details still to be replayed; kept if connecting fails, cleared once used or rejected. */
  let autoLogin = $state<ReconnectLogin | null>(reconnectLogin);
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

  /** The account-login step shown, or null for none (connecting, or the
   *  legacy query-based login of loginmethod 0). */
  let step = $state<
    "account" | "characters" | "addExisting" | "createChar" | null
  >(initChars !== null ? "characters" : null);
  /** Characters returned by the server's accountplayers command. */
  let characterList = $state<AccountPlayer[]>(initChars ?? []);

  /** Login method confirmed by the server (0 = legacy, 1 or 2 = account).
   *  Restored from the module-level cache when returning from gameplay so that
   *  the character-creation form renders correctly without a new handshake. */
  let confirmedLoginMethod = $state(
    initChars !== null ? _cachedLoginMethod : 0,
  );

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
      // Remember a typed address for the next visit.
      if (!hideAddressInput) saveConfig(SERVER_ADDRESS_KEY, serverAddress);
      statusMessage = "Connected. Negotiating...";
      // Reflect the connected server in the URL so the page can be bookmarked
      // or shared and will use the same server on reload.
      const params = new URLSearchParams(window.location.search);
      params.set("server", serverAddress);
      history.replaceState(null, "", "?" + params.toString());
      clientNegotiate();
    } catch (e) {
      // When reconnecting, keep the login details so "Reconnect" can retry.
      errorMessage = autoLogin
        ? "Couldn't reach the server. Press Reconnect to try again."
        : `Connection failed: ${e instanceof Error ? e.message : String(e)}`;
      connecting = false;
      statusMessage = "";
    }
  }

  /** Return to the "Enter" screen after the server closes the connection. */
  function handleConnectionLost() {
    if (!connected) return;
    connected = false;
    connecting = false;
    step = null;
    addMeSuccessReceived = false;
    statusMessage = "";
    errorMessage = autoLogin
      ? "Lost the connection to the server. Press Reconnect to try again."
      : "Lost the connection to the server. Press Enter to connect again.";
  }

  /**
   * Stop logging in automatically, falling back to the normal login screens.
   * The account form is shown if the connection is still up.
   */
  function cancelAutoLogin() {
    if (!autoLogin) return;
    autoLogin = null;
    if (connected && step === null && confirmedLoginMethod >= 1) {
      step = "account";
    }
  }

  // "Reconnect" on the in-game disconnect overlay: connect straight away and
  // log back in (see the loginMethodConfirmed and accountPlayers handlers).
  onMount(() => {
    if (autoLogin) void handleConnect();
  });

  function handleYesNoKeydown(e: KeyboardEvent) {
    if (query.handleYesNoKey(e)) {
      e.preventDefault();
      checkLoginComplete();
    }
  }

  /** Close the connection to the server and forget the account password. */
  function closeConnection() {
    setAccountPassword("");
    clientDisconnect();
    connected = false;
    connecting = false;
    step = null;
    addMeSuccessReceived = false;
  }

  /**
   * Log out of the account.  The server has no logout command, so close the
   * connection and connect again, which shows a fresh account form.
   */
  function handleLogOut() {
    closeConnection();
    void handleConnect();
  }

  /** "Disconnect" on the account form: go back to the "Enter" screen. */
  function handleDisconnect() {
    closeConnection();
    autoLogin = null;
    errorMessage = "";
    statusMessage = "";
  }

  function handlePlayCharacter(name: string) {
    sendAccountPlay(name);
    statusMessage = "Starting game…";
  }

  /** Show the login step `next`, clearing any messages. */
  function showStep(next: typeof step) {
    step = next;
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
          autoLogin = null;
          sendAddMe();
        } else if (autoLogin) {
          // Reconnecting: log back in to the same account.
          sendAccountLogin(autoLogin.accountName, autoLogin.password);
          statusMessage = "Logging in…";
        } else {
          // Server supports account-based login (method >= 1).
          step = "account";
          statusMessage = "";
        }
      }),

      gameEvents.on("accountPlayers", (players: AccountPlayer[]) => {
        characterList = players;
        step = "characters";
        statusMessage = "";
        errorMessage = "";
        if (players.length === 0) {
          statusMessage = "No characters yet. Create one to start playing!";
        }
        // Reconnecting: pick the same character again.
        if (autoLogin) {
          const name = autoLogin.characterName;
          autoLogin = null;
          if (players.some((p) => p.name === name)) handlePlayCharacter(name);
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
        cancelAutoLogin();
        errorMessage = "Server rejected login.";
        statusMessage = "";
      }),

      gameEvents.on("failure", (command: string, message: string) => {
        // AddExistingCharacter shows its own failures.
        if (command === "accountaddplayer") return;
        // The account wasn't made after all, so don't show its tour.
        if (command === "accountnew") clientTour.cancelRequest();
        cancelAutoLogin();
        errorMessage = `${command}: ${message}`;
        statusMessage = "";
      }),

      gameEvents.on("disconnect", handleConnectionLost),

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
    rel="noopener noreferrer"
  >
    Fork me
  </a>
{/if}

<div class="login-container">
  <h1>⚔ Welcome to Crossfire ⚔</h1>
  <h2>The Multiplayer Adventure Game</h2>
  {#if !connected}
    <div class="login-form">
      {#if !hideAddressInput}
        <label>
          Server Address
          <!-- svelte-ignore a11y_autofocus -->
          <input
            type="text"
            bind:value={serverAddress}
            autofocus
            onkeydown={(e) => {
              if (e.key === "Enter" && !connecting) handleConnect();
            }}
            placeholder="ws://hostname:port"
            disabled={connecting}
          />
        </label>
      {/if}
      <!-- Focus the button when there is no address to type. -->
      <!-- svelte-ignore a11y_autofocus -->
      <button
        onclick={handleConnect}
        disabled={connecting}
        autofocus={hideAddressInput}
      >
        {connecting ? "Connecting..." : autoLogin ? "Reconnect" : "Enter"}
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
      <!-- Character creation needs the whole width. -->
      {#if serverInfoSections.length > 0 && step !== "createChar"}
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
                        : "none"}
                    >
                      {span.text}
                    </span>{/each}
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
      <div class="query-panel" class:wide={step === "createChar"}>
        <CreateCharacter
          visible={step === "createChar"}
          loginMethod={confirmedLoginMethod}
          onCancel={() => showStep("characters")}
          onError={(text) => (errorMessage = text)}
          onStatus={(text) => (statusMessage = text)}
        />
        <AccountLogin
          visible={step === "account"}
          onDisconnect={handleDisconnect}
          onError={(text) => (errorMessage = text)}
          onStatus={(text) => (statusMessage = text)}
        />
        {#if step === "characters"}
          <CharacterList
            characters={characterList}
            onPlay={handlePlayCharacter}
            onCreate={() => showStep("createChar")}
            onAddExisting={() => showStep("addExisting")}
            onLogOut={handleLogOut}
          />
        {:else if step === "addExisting"}
          <AddExistingCharacter
            onCancel={() => showStep("characters")}
            onError={(text) => (errorMessage = text)}
            onStatus={(text) => (statusMessage = text)}
          />
        {:else if step === null && query.active}
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
    /* The rest of the UI turns text selection off (see app.css). */
    user-select: text;
    -webkit-user-select: text;
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

  .query-panel.wide {
    flex: 0 1 60rem;
    align-items: center;
    margin: 0 auto;
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
