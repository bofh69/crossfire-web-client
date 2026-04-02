<script lang="ts">
  import { clientConnect, clientNegotiate, sendAddMe } from '../lib/client';
  import { callbacks } from '../lib/commands';
  import { sendReply } from '../lib/player';
  import { CS_QUERY_HIDEINPUT } from '../lib/protocol';

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
  let queryInput = $state('');
  let statusMessage = $state('');

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

  function handleQuerySubmit() {
    if (queryInput.length > 0 || queryPrompt.length > 0) {
      sendReply(queryInput);
      queryInput = '';
      queryPrompt = '';
    }
  }

  function handleQueryKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      handleQuerySubmit();
    }
  }

  $effect(() => {
    callbacks.onQuery = (flags: number, prompt: string) => {
      queryPrompt = prompt;
      queryHidden = (flags & CS_QUERY_HIDEINPUT) !== 0;
      queryInput = '';
    };

    callbacks.onVersion = (_cs: number, _sc: number, verStr: string) => {
      statusMessage = `Server: ${verStr}. Sending addme...`;
      sendAddMe();
    };

    callbacks.onAddMeSuccess = () => {
      statusMessage = 'Login successful!';
      onLoggedIn();
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
      callbacks.onQuery = undefined;
      callbacks.onVersion = undefined;
      callbacks.onAddMeSuccess = undefined;
      callbacks.onAddMeFail = undefined;
      callbacks.onFailure = undefined;
      callbacks.onDrawInfo = undefined;
    };
  });
</script>

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
      <label>
        {queryPrompt}
        <input
          type={queryHidden ? 'password' : 'text'}
          bind:value={queryInput}
          onkeydown={handleQueryKeydown}
          autofocus
        />
      </label>
      <button onclick={handleQuerySubmit}>Submit</button>
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
</style>
