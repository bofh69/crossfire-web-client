<script lang="ts">
  import { extendedCommand } from '../lib/p_cmd';
  import { sendCommand } from '../lib/player';
  import { InputState } from '../lib/protocol';
  import { getCpl } from '../lib/init';
  import { type MessageSpan, colorForNdi, parseMarkup } from '../lib/markup';

  let { inputDisabled = false }: { inputDisabled?: boolean } = $props();

  interface Message {
    spans: MessageSpan[];
  }

  let messages: Message[] = $state([]);
  let commandInput = $state('');
  let messagesDiv: HTMLDivElement | undefined = $state();
  let inputEl: HTMLInputElement | undefined = $state();

  export function addMessage(color: number, text: string) {
    const baseColor = colorForNdi(color);
    const spans = parseMarkup(text, baseColor);
    messages = [...messages, { spans }];
    // Keep a reasonable buffer
    if (messages.length > 500) {
      messages = messages.slice(-400);
    }
    scrollToBottom();
  }

  /**
   * Focus the command input field, optionally pre-filling it with text.
   * Used by the keybinding system when entering command mode.
   */
  export function focusInput(prefill?: string) {
    if (prefill !== undefined) {
      commandInput = prefill;
    }
    requestAnimationFrame(() => {
      inputEl?.focus();
      // Place cursor at end of prefilled text.
      if (inputEl && prefill) {
        inputEl.setSelectionRange(prefill.length, prefill.length);
      }
    });
  }

  function scrollToBottom() {
    requestAnimationFrame(() => {
      if (messagesDiv) {
        messagesDiv.scrollTop = messagesDiv.scrollHeight;
      }
    });
  }

  /** The prefix that Enter/focusInput inserts by default. */
  const CHAT_PREFIX = 'chat ';

  function handleKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      // Stop propagation so the global window handler doesn't re-focus us
      // (the Enter binding would call focusCommandInput('chat ') again).
      e.stopPropagation();
      submitCommand();
    } else if (e.key === 'Escape') {
      // Leave command mode and return focus to the game.
      commandInput = '';
      blurInput();
    }
  }

  function handleBlur() {
    // When the input loses focus, return to Playing state.
    const cpl = getCpl();
    if (cpl && cpl.inputState === InputState.CommandMode) {
      cpl.inputState = InputState.Playing;
    }
  }

  function blurInput() {
    const cpl = getCpl();
    if (cpl) {
      cpl.inputState = InputState.Playing;
    }
    inputEl?.blur();
  }

  function submitCommand() {
    const cmd = commandInput.trim();

    // Empty input or bare chat prefix → just unfocus, don't send anything.
    if (cmd.length === 0 || cmd === CHAT_PREFIX.trim()) {
      commandInput = '';
      blurInput();
      return;
    }

    if (cmd.startsWith('/')) {
      extendedCommand(cmd);
    } else {
      sendCommand(cmd, 0, 1);
    }
    commandInput = '';
    blurInput();
  }
</script>

<div class="info-panel">
  <div class="messages" bind:this={messagesDiv}>
    {#each messages as msg}
      <div class="message">{#each msg.spans as span}<span
          style:color={span.color}
          style:font-weight={span.bold ? 'bold' : 'normal'}
          style:font-style={span.italic ? 'italic' : 'normal'}
          style:text-decoration={span.underline ? 'underline' : 'none'}
        >{span.text}</span>{/each}</div>
    {/each}
  </div>
  <div class="input-row" class:disabled={inputDisabled}>
    <input
      type="text"
      bind:value={commandInput}
      bind:this={inputEl}
      onkeydown={handleKeydown}
      onblur={handleBlur}
      placeholder="Type command..."
      disabled={inputDisabled}
    />
    <button onclick={submitCommand} disabled={inputDisabled}>Send</button>
  </div>
</div>

<style>
  .info-panel {
    display: flex;
    flex-direction: column;
    height: 100%;
    background: #1a1a1a;
    border: 1px solid #333;
  }

  .messages {
    flex: 1;
    overflow-y: auto;
    padding: 0.5rem;
    font-family: 'Courier New', monospace;
    font-size: 0.85rem;
    line-height: 1.3;
  }

  .message {
    padding: 1px 0;
    word-wrap: break-word;
  }

  .input-row {
    display: flex;
    border-top: 1px solid #333;
  }

  .input-row.disabled {
    opacity: 0.4;
    pointer-events: none;
  }

  input {
    flex: 1;
    padding: 0.4rem 0.5rem;
    border: none;
    background: #222;
    color: #e0e0e0;
    font-family: 'Courier New', monospace;
    font-size: 0.85rem;
  }

  input:focus {
    outline: none;
    background: #282828;
  }

  button {
    padding: 0.4rem 0.75rem;
    border: none;
    border-left: 1px solid #333;
    background: #333;
    color: #c0c0c0;
    cursor: pointer;
    font-size: 0.85rem;
  }

  button:hover {
    background: #444;
  }
</style>
