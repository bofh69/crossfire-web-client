<script lang="ts">
  import { clientConnect, clientNegotiate, sendAddMe } from '../lib/client';
  import { callbacks } from '../lib/commands';
  import { sendReply } from '../lib/player';
  import { CS_QUERY_HIDEINPUT, CS_QUERY_SINGLECHAR, CS_QUERY_YESNO } from '../lib/protocol';

  interface Props {
    onLoggedIn: () => void;
  }

  let { onLoggedIn }: Props = $props();

  let serverAddress = $state('ws://localhost:13327');
  let connected = $state(false);
  let connecting = $state(false);
  let errorMessage = $state('');
  let queryPrompt = $state('');
  let queryHidden = $state(false);
  let querySingleChar = $state(false);
  let queryYesNo = $state(false);
  let queryInput = $state('');
  let statusMessage = $state('');

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

  function checkLoginComplete() {
    if (addMeSuccessReceived && !queryPrompt) {
      statusMessage = 'Login successful!';
      onLoggedIn();
    }
  }

  async function handleConnect() {
    errorMessage = '';
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
    if (queryInput.length > 0 || queryPrompt.length > 0) {
      sendQueryReply(queryInput);
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
    callbacks.onQuery = (flags: number, prompt: string) => {
      // If the server sends an empty prompt, reuse the previous prompt text.
      if (prompt) {
        queryPrompt = prompt;
      }
      queryHidden = (flags & CS_QUERY_HIDEINPUT) !== 0;
      querySingleChar = (flags & CS_QUERY_SINGLECHAR) !== 0;
      queryYesNo = (flags & CS_QUERY_YESNO) !== 0;
      queryInput = '';
    };

    callbacks.onVersion = (_cs: number, _sc: number, verStr: string) => {
      statusMessage = `Server: ${verStr}. Sending addme...`;
      sendAddMe();
    };

    callbacks.onAddMeSuccess = () => {
      addMeSuccessReceived = true;
      if (queryPrompt) {
        // A query is still on screen – wait until the user answers it.
        statusMessage = 'Authenticated. Answer the prompt to enter the game.';
      } else {
        checkLoginComplete();
      }
    };

    callbacks.onAddMeFail = () => {
      errorMessage = 'Server rejected login.';
      statusMessage = '';
    };

    callbacks.onFailure = (command: string, message: string) => {
      errorMessage = `${command}: ${message}`;
    };

    callbacks.onDrawInfo = (_color: number, message: string) => {
      if (!connected) return;
      statusMessage = message;
    };

    return () => {
      callbacks.onVersion = undefined;
      callbacks.onAddMeSuccess = undefined;
      callbacks.onAddMeFail = undefined;
      callbacks.onFailure = undefined;
      // onQuery and onDrawInfo are NOT cleared here: wireCallbacks() in App.svelte
      // sets them synchronously before this cleanup runs (Svelte defers $effect
      // cleanup to the next microtask), so clearing them here would silently
      // destroy the handlers that the game screen just installed.  App.svelte's
      // handleDisconnect() is responsible for clearing these when needed.
    };
  });
</script>

<svelte:window onkeydown={handleYesNoKeydown} />

<div class="login-container">
  <h1>⚔ Crossfire Web Client</h1>

  {#if !connected}
    <div class="login-form">
      <label>
        Server Address
        <input
          type="text"
          bind:value={serverAddress}
          placeholder="ws://hostname:port"
          disabled={connecting}
        />
      </label>
      <button onclick={handleConnect} disabled={connecting}>
        {connecting ? 'Connecting...' : 'Connect'}
      </button>
    </div>
  {:else if queryPrompt}
    <div class="login-form">
      <p class="query-text">{queryPrompt}</p>
      {#if queryYesNo}
        <div class="yesno-buttons">
          <button onclick={() => sendQueryReply('y')}>Yes</button>
          <button onclick={() => sendQueryReply('n')}>No</button>
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

<style>
  .login-container {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    gap: 1rem;
  }

  h1 {
    color: #e0d0b0;
    font-size: 2rem;
    margin-bottom: 1rem;
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
</style>
