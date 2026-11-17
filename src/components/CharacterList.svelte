<script lang="ts">
  /**
   * CharacterList — the character-select step of the login screen
   * (loginmethod >= 1): the account's characters, and buttons to create or
   * add one.
   */
  import type { AccountPlayer } from "../lib/events";

  interface Props {
    /** The account's characters, from the server's accountplayers command. */
    characters: AccountPlayer[];
    /** Called when the user picks the character `name` to play. */
    onPlay: (name: string) => void;
    /** Called when the user wants to create a new character. */
    onCreate: () => void;
    /** Called when the user wants to add an existing character. */
    onAddExisting: () => void;
    /** Called when the user goes back to the account login. */
    onBack: () => void;
  }

  let { characters, onPlay, onCreate, onAddExisting, onBack }: Props = $props();
</script>

<div class="login-form">
  <h3 class="panel-title">Choose Character</h3>
  {#if characters.length > 0}
    <div class="character-list">
      {#each characters as char}
        <button class="character-entry" onclick={() => onPlay(char.name)}>
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
  <button onclick={onCreate}>+ Create New Character</button>
  <button onclick={onAddExisting}>+ Add Existing Character</button>
  <button class="back-btn" onclick={onBack}>← Back</button>
</div>

<style>
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
