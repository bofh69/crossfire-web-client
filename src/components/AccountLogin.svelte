<script lang="ts">
  /**
   * AccountLogin — the account step of the login screen (loginmethod >= 1):
   * log in to an existing account or create a new one.
   */
  import {
    sendAccountLogin,
    sendAccountNew,
    setAccountPassword,
  } from "../lib/client";
  import { disconnectAndReload } from "../lib/disconnect";

  interface Props {
    /** Whether the form is shown.  The typed values are kept while hidden. */
    visible: boolean;
    /** Show an error message; "" clears it. */
    onError: (text: string) => void;
    /** Show a status message; "" clears it. */
    onStatus: (text: string) => void;
  }

  let { visible, onError, onStatus }: Props = $props();

  /** True when showing "create new account" form instead of login form. */
  let showNewAccount = $state(false);
  /** Account name input. */
  let accountName = $state("");
  /** Account password input (login form). */
  let accountPassword = $state("");
  /** Confirm-password field for new-account creation. */
  let accountPasswordConfirm = $state("");

  function handleAccountLogin() {
    onError("");
    if (!accountName.trim() || !accountPassword) {
      onError("Please enter both an account name and password.");
      return;
    }
    // Cache the password so it can be included in a future createplayer packet.
    setAccountPassword(accountPassword);
    sendAccountLogin(accountName.trim(), accountPassword);
    onStatus("Logging in…");
  }

  function handleAccountNew() {
    onError("");
    if (!accountName.trim() || !accountPassword) {
      onError("Please fill in all fields.");
      return;
    }
    if (accountPassword !== accountPasswordConfirm) {
      onError("Passwords do not match.");
      return;
    }
    setAccountPassword(accountPassword);
    sendAccountNew(accountName.trim(), accountPassword);
    onStatus("Creating account…");
  }
</script>

{#if visible}
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
          onError("");
        }}>Log In</button
      >
      <button
        type="button"
        class:tab-active={showNewAccount}
        onclick={() => {
          showNewAccount = true;
          onError("");
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
        autocomplete={showNewAccount ? "new-password" : "current-password"}
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
    <button onclick={disconnectAndReload}>Disconnect</button>
  </form>
{/if}

<style>
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
</style>
