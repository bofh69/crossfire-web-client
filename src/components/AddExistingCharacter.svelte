<script lang="ts">
  /**
   * AddExistingCharacter — the login-screen step that links an existing
   * character to the account (loginmethod >= 1), with an override when the
   * server says the character is linked to another account.
   */
  import { onMount } from "svelte";
  import { sendAccountAddPlayer } from "../lib/client";
  import { gameEvents } from "../lib/events";

  interface Props {
    /** Called when the user goes back to the character list. */
    onCancel: () => void;
    /** Show an error message; "" clears it. */
    onError: (text: string) => void;
    /** Show a status message; "" clears it. */
    onStatus: (text: string) => void;
  }

  let { onCancel, onError, onStatus }: Props = $props();

  /** Character name input. */
  let name = $state("");
  /** Character password input. */
  let password = $state("");
  /** True when the server failure response said force=1 is allowed. */
  let canForce = $state(false);
  /** The error text from the server when force is possible. */
  let forceMessage = $state("");

  /** Submit the add-existing request (normal, force=0). */
  function handleAddExisting() {
    onError("");
    const trimmed = name.trim();
    if (!trimmed || !password) {
      onError("Please enter both a character name and password.");
      return;
    }
    canForce = false;
    forceMessage = "";
    sendAccountAddPlayer(0, trimmed, password);
    onStatus("Adding character…");
  }

  /** Re-submit with force=1 when the server said it is allowed. */
  function handleForceAddExisting() {
    onError("");
    canForce = false;
    forceMessage = "";
    sendAccountAddPlayer(1, name.trim(), password);
    onStatus("Adding character…");
  }

  onMount(() =>
    gameEvents.on("failure", (command: string, message: string) => {
      if (command !== "accountaddplayer") return;
      // The message starts with a force flag: "0 <text>" (not retriable)
      // or "1 <text>" (can retry with force=1).
      const spaceIdx = message.indexOf(" ");
      const forceFlag =
        spaceIdx > 0 ? parseInt(message.substring(0, spaceIdx), 10) : 0;
      const text = spaceIdx > 0 ? message.substring(spaceIdx + 1) : message;
      onError(text);
      onStatus("");
      if (forceFlag === 1) {
        canForce = true;
        forceMessage = text;
      }
    }),
  );
</script>

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
    <input type="text" bind:value={name} autocomplete="username" autofocus />
  </label>
  <label>
    Character Password
    <input
      type="password"
      bind:value={password}
      autocomplete="current-password"
    />
  </label>
  <button type="submit">Add Character</button>
  {#if canForce}
    <p class="error">{forceMessage}</p>
    <p class="query-text">
      This character is already linked to another account. Override the link?
    </p>
    <button type="button" onclick={handleForceAddExisting}>Yes, override</button
    >
  {/if}
  <button type="button" class="back-btn" onclick={onCancel}>← Back</button>
</form>

<style>
  .query-text {
    color: var(--text-warm-dim);
    font-size: 0.9rem;
    margin: 0;
  }
</style>
