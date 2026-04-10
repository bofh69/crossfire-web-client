<script lang="ts">
  import { clientConnect, clientNegotiate, sendAddMe } from '../lib/client';
  import { gameEvents } from '../lib/events';
  import { sendReply } from '../lib/player';
  import { CS_QUERY_HIDEINPUT, CS_QUERY_SINGLECHAR, CS_QUERY_YESNO, EPORT } from '../lib/protocol';
  import { type InfoLine, parseMarkupLines } from '../lib/markup';

  interface Props {
    onLoggedIn: () => void;
  }

  let { onLoggedIn }: Props = $props();

  /** True when the page was loaded on a standard HTTP/HTTPS port (80 or 443).
   *  In that case the server address is derived automatically and the input
   *  field is hidden. */
  const standardPort = (() => {
    const port = window.location.port;
    return port === '' || port === '80' || port === '443';
  })();

  function defaultServerAddress(): string {
    if (standardPort) {
      const scheme = window.location.protocol === 'https:' ? 'wss' : 'ws';
      return `${scheme}://${window.location.host}/ws`;
    }
    return 'ws://' + window.location.hostname + ':' + EPORT;
  }

  let serverAddress = $state(defaultServerAddress());
  let connected = $state(false);
  let connecting = $state(false);
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
        statusMessage = `Server: ${verStr}. Handshaking...`;
        sendAddMe();
      }),

      gameEvents.on('addMeSuccess', () => {
        addMeSuccessReceived = true;
        if (queryPrompt) {
          // A query is still on screen – wait until the user answers it.
          statusMessage = 'Connected.';
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
  >Fork me on GitHub</a>
{/if}

<div class="login-container">
  <h1>⚔ Crossfire Web Client</h1>

  {#if !connected}
    <div class="login-form">
      {#if !standardPort}
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
        {connecting ? 'Connecting...' : 'Connect'}
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
        {#if queryPrompt}
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
    color: #e0d0b0;
    font-size: 2rem;
    margin-bottom: 1rem;
  }

  .connected-layout {
    display: flex;
    gap: 2rem;
    align-items: flex-start;
    width: 100%;
    max-width: 960px;
  }

  .server-info {
    flex: 1;
    max-height: 70vh;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 1rem;
    border: 1px solid #333;
    border-radius: 6px;
    padding: 1rem;
    background: #151515;
  }

  .info-section h3 {
    color: #e0d0b0;
    font-size: 0.9rem;
    margin: 0 0 0.5rem 0;
    border-bottom: 1px solid #333;
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
    color: #e0d0b0;
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
    color: #c0b090;
    font-size: 0.9rem;
  }

  input {
    padding: 0.5rem;
    border: 1px solid #555;
    border-radius: 4px;
    background: #2a2a2a;
    color: #e0e0e0;
    font-size: 1rem;
  }

  input:focus {
    outline: none;
    border-color: #7a6a4a;
  }

  button {
    padding: 0.6rem 1rem;
    border: 1px solid #7a6a4a;
    border-radius: 4px;
    background: #4a3a2a;
    color: #e0d0b0;
    font-size: 1rem;
    cursor: pointer;
  }

  button:hover:not(:disabled) {
    background: #5a4a3a;
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
    color: #c0b090;
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
    top: 2.5rem;
    right: -3rem;
    z-index: 100;
    display: block;
    width: 12rem;
    padding: 0.35rem 0;
    background: #7a6a4a;
    color: #e0d0b0;
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
</style>
