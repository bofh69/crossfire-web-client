<script lang="ts">
  import { onMount } from 'svelte';
  import { extendedCommand, completeCommand, getCompletionMatches } from '../lib/p_cmd';
  import { InputState } from '../lib/protocol';
  import { getCpl } from '../lib/init';
  import { type MessageSpan, colorForNdi, parseMarkup } from '../lib/markup';
  import { gameEvents } from '../lib/events';
  import { MSG_BUFFER_MAX, MSG_BUFFER_TRIM } from '../lib/constants';

  let { inputDisabled = false }: { inputDisabled?: boolean } = $props();

  interface Message {
    spans: MessageSpan[];
    rawText: string;
    rawColor: number;
    count: number;
  }

  let messages: Message[] = $state([]);
  let commandInput = $state('');
  let messagesDiv: HTMLDivElement | undefined = $state();
  let inputEl: HTMLInputElement | undefined = $state();

  // Command history: oldest entry at index 0, newest at the end.
  let commandHistory: string[] = $state([]);
  // -1 = not browsing history (live input); counts from newest (0) to oldest (length-1).
  let historyIndex = $state(-1);
  // Current input saved before browsing so we can restore it on ArrowDown.
  let savedInput = $state('');

  function addMessage(color: number, text: string) {
    const baseColor = colorForNdi(color);
    const lastMsg = messages[messages.length - 1];
    if (lastMsg && lastMsg.rawText === text && lastMsg.rawColor === color) {
      messages = [...messages.slice(0, -1), { ...lastMsg, count: lastMsg.count + 1 }];
    } else {
      const spans = parseMarkup(text, baseColor);
      messages = [...messages, { spans, rawText: text, rawColor: color, count: 1 }];
      // Keep a reasonable buffer
      if (messages.length > MSG_BUFFER_MAX) {
        messages = messages.slice(-MSG_BUFFER_TRIM);
      }
    }
    scrollToBottom();
  }

  /**
   * Focus the command input field, optionally pre-filling it with text.
   * Used by the keybinding system when entering command mode.
   */
  function focusInput(prefill?: string) {
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

  onMount(() => {
    const cleanups = [
      gameEvents.on('drawInfo', addMessage),
      gameEvents.on('drawExtInfo', (color, _type, _subtype, message) => addMessage(color, message)),
      gameEvents.on('focusCommandInput', (prefill) => focusInput(prefill)),
    ];
    return () => { for (const unsub of cleanups) unsub(); };
  });

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
      historyIndex = -1;
      blurInput();
      e.stopPropagation();
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (commandHistory.length === 0) return;
      if (historyIndex === -1) {
        // Start browsing: save live input and jump to the newest entry.
        savedInput = commandInput;
        historyIndex = 0;
      } else if (historyIndex < commandHistory.length - 1) {
        historyIndex++;
      }
      commandInput = commandHistory[commandHistory.length - 1 - historyIndex]!;
      // Move cursor to end after value update.
      requestAnimationFrame(() => {
        inputEl?.setSelectionRange(commandInput.length, commandInput.length);
      });
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (historyIndex === -1) return;
      if (historyIndex > 0) {
        historyIndex--;
        commandInput = commandHistory[commandHistory.length - 1 - historyIndex]!;
      } else {
        // Back to live input.
        historyIndex = -1;
        commandInput = savedInput;
      }
      requestAnimationFrame(() => {
        inputEl?.setSelectionRange(commandInput.length, commandInput.length);
      });
    } else if (e.key === 'Tab') {
      e.preventDefault();
      const completed = completeCommand(commandInput, commandHistory);
      if (completed !== commandInput) {
        commandInput = completed;
      } else {
        // Input already equals the longest common prefix — show all matches.
        const matches = getCompletionMatches(commandInput, commandHistory);
        if (matches.length > 1) {
          addMessage(0, matches.join('  '));
        }
      }
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
      historyIndex = -1;
      blurInput();
      return;
    }

    // Append to history, avoiding consecutive duplicates.
    if (commandHistory.length === 0 || commandHistory[commandHistory.length - 1] !== cmd) {
      commandHistory = [...commandHistory, cmd];
    }
    historyIndex = -1;
    savedInput = '';

    extendedCommand(cmd);
    commandInput = '';
    blurInput();
  }
</script>

<div class="info-panel">
  <div class="messages" bind:this={messagesDiv}>
    {#each messages as msg}
      <div class="message">{#if msg.count > 1}<span class="repeat-count">{msg.count}×</span>{/if}{#each msg.spans as span}<span
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
    background: var(--bg-panel);
    border: 1px solid var(--border);
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

  .repeat-count {
    display: inline-block;
    font-weight: bold;
    color: #999;
    background: #2e2e2e;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 0 3px;
    font-size: 0.75em;
    margin-right: 0.35em;
    vertical-align: middle;
  }

  .input-row {
    display: flex;
    border-top: 1px solid var(--border);
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
    color: var(--text-bright);
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
    border-left: 1px solid var(--border);
    background: var(--border);
    color: var(--text);
    cursor: pointer;
    font-size: 0.85rem;
  }

  button:hover {
    background: var(--border-mid);
  }
</style>
