<script lang="ts">
  import { clientDisconnect } from '../lib/client';
  import PickupMenu from './PickupMenu.svelte';
  import {
    keyEventToString,
    findBindingForEvent,
    bindCommandToEvent,
    unbindEvent,
    getBindings,
    flagsToDisplayString,
    type KeyBind,
  } from '../lib/keys';
  import { getLastCommand } from '../lib/player';

  interface Props {
    onDisconnect: () => void;
  }

  let { onDisconnect }: Props = $props();

  let activeMenu = $state<string | null>(null);
  let pickupMenu: PickupMenu | undefined = $state();

  /** Persist the pickup mode so it survives menu close/reopen. */
  let currentPickupMode = $state(0x80000000 >>> 0); // PU_NEWMODE

  // ── Key-bind dialog state ─────────────────────────────────────────────────
  type DialogMode =
    | 'idle'
    | 'bind-capture'        // waiting for the key to bind
    | 'bind-confirm'        // key captured; asking to overwrite an existing binding
    | 'unbind-capture'      // waiting for the key to unbind
    | 'unbind-confirm'      // key captured; asking to confirm removal
    | 'show-bindings';      // showing all key bindings

  let dialogMode = $state<DialogMode>('idle');
  let dialogKeyStr = $state('');             // human-readable key (e.g. "Ctrl+f")
  let dialogCommand = $state('');           // command being bound
  let dialogExisting = $state<KeyBind | null>(null); // existing binding for the key
  let capturedEvent = $state<KeyboardEvent | null>(null);

  function toggleMenu(menu: string) {
    activeMenu = activeMenu === menu ? null : menu;
  }

  function closeMenu() {
    activeMenu = null;
  }

  function handleDisconnect() {
    clientDisconnect();
    onDisconnect();
    closeMenu();
  }

  /** Called by the parent when the server sends a pickup update. */
  export function setPickupMode(mode: number) {
    currentPickupMode = mode >>> 0;
    pickupMenu?.setPickupMode(currentPickupMode);
  }

  /** Returns true while a key-capture or confirm dialog is showing. */
  export function isDialogActive(): boolean {
    return dialogMode !== 'idle';
  }

  // ── Bind last command ──────────────────────────────────────────────────────

  function startBind() {
    const cmd = getLastCommand();
    if (!cmd) {
      // No command has been sent yet; nothing to bind.
      closeMenu();
      return;
    }
    dialogCommand = cmd;
    dialogMode = 'bind-capture';
    closeMenu();
  }

  function handleBindCapture(e: KeyboardEvent) {
    if (dialogMode !== 'bind-capture') return;
    // Ignore bare modifier keys
    if (['Shift','Control','Alt','Meta'].includes(e.key)) return;
    e.preventDefault();
    capturedEvent = e;
    dialogKeyStr = keyEventToString(e);
    dialogExisting = findBindingForEvent(e);
    dialogMode = 'bind-confirm';
  }

  function confirmBind() {
    if (capturedEvent) {
      bindCommandToEvent(capturedEvent, dialogCommand);
    }
    dialogMode = 'idle';
    capturedEvent = null;
  }

  function cancelBind() {
    dialogMode = 'idle';
    capturedEvent = null;
  }

  // ── Unbind a key ──────────────────────────────────────────────────────────

  function startUnbind() {
    dialogMode = 'unbind-capture';
    closeMenu();
  }

  function handleUnbindCapture(e: KeyboardEvent) {
    if (dialogMode !== 'unbind-capture') return;
    if (['Shift','Control','Alt','Meta'].includes(e.key)) return;
    e.preventDefault();
    capturedEvent = e;
    dialogKeyStr = keyEventToString(e);
    dialogExisting = findBindingForEvent(e);
    dialogMode = 'unbind-confirm';
  }

  function confirmUnbind() {
    if (capturedEvent) {
      unbindEvent(capturedEvent);
    }
    dialogMode = 'idle';
    capturedEvent = null;
  }

  function cancelUnbind() {
    dialogMode = 'idle';
    capturedEvent = null;
  }

  // ── Show key bindings ──────────────────────────────────────────────────────

  function showBindings() {
    dialogMode = 'show-bindings';
    closeMenu();
  }

  function closeBindings() {
    dialogMode = 'idle';
  }

  function handleWindowKeydown(e: KeyboardEvent) {
    if (dialogMode === 'bind-capture') {
      handleBindCapture(e);
    } else if (dialogMode === 'unbind-capture') {
      handleUnbindCapture(e);
    }
  }
</script>

<svelte:window
  onclick={closeMenu}
  onkeydown={handleWindowKeydown}
/>

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="menu-bar" onclick={(e: MouseEvent) => e.stopPropagation()}>
  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('file')}>File</button>
    {#if activeMenu === 'file'}
      <div class="dropdown">
        <button onclick={handleDisconnect}>Disconnect</button>
      </div>
    {/if}
  </div>

  <div class="menu-item">
    <!-- svelte-ignore a11y_click_events_have_key_events -->
    <!-- svelte-ignore a11y_no_static_element_interactions -->
    <button class="menu-button" onclick={() => toggleMenu('pickup')}>Pickup</button>
    {#if activeMenu === 'pickup'}
      <!-- svelte-ignore a11y_click_events_have_key_events -->
      <!-- svelte-ignore a11y_no_static_element_interactions -->
      <div class="dropdown" onclick={(e: MouseEvent) => e.stopPropagation()}>
        <PickupMenu bind:this={pickupMenu} initialMode={currentPickupMode} />
      </div>
    {/if}
  </div>

  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('keyboard')}>Keyboard</button>
    {#if activeMenu === 'keyboard'}
      <div class="dropdown">
        <button onclick={startBind}>Bind last command to key…</button>
        <button onclick={startUnbind}>Unbind a key…</button>
        <button onclick={showBindings}>Show key bindings</button>
      </div>
    {/if}
  </div>

  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('help')}>Help</button>
    {#if activeMenu === 'help'}
      <div class="dropdown">
        <button onclick={closeMenu}>About Crossfire Web Client</button>
      </div>
    {/if}
  </div>

  <div class="spacer"></div>
  <span class="title">Crossfire</span>
</div>

<!-- ── Key-capture / confirm dialogs ── -->
{#if dialogMode === 'bind-capture'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to key</p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      <p class="dialog-prompt">Press the key combination you want to bind…</p>
      <div class="dialog-buttons">
        <button onclick={cancelBind}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-confirm'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to key</p>
      <p>Key: <strong>{dialogKeyStr}</strong></p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      {#if dialogExisting}
        <p class="dialog-warn">Already bound to: <strong>{dialogExisting.command}</strong></p>
        <p>Overwrite?</p>
      {/if}
      <div class="dialog-buttons">
        <button class="btn-primary" onclick={confirmBind}>
          {dialogExisting ? 'Overwrite' : 'Bind'}
        </button>
        <button onclick={cancelBind}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'unbind-capture'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Unbind a key</p>
      <p class="dialog-prompt">Press the key combination you want to unbind…</p>
      <div class="dialog-buttons">
        <button onclick={cancelUnbind}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'unbind-confirm'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Unbind a key</p>
      <p>Key: <strong>{dialogKeyStr}</strong></p>
      {#if dialogExisting}
        <p>Currently bound to: <strong>{dialogExisting.command}</strong></p>
        <p>Remove this binding?</p>
        <div class="dialog-buttons">
          <button class="btn-danger" onclick={confirmUnbind}>Remove</button>
          <button onclick={cancelUnbind}>Cancel</button>
        </div>
      {:else}
        <p class="dialog-warn">This key is not bound to any command.</p>
        <div class="dialog-buttons">
          <button onclick={cancelUnbind}>Close</button>
        </div>
      {/if}
    </div>
  </div>
{:else if dialogMode === 'show-bindings'}
  <div class="dialog-overlay">
    <div class="dialog dialog-wide">
      <p class="dialog-title">Key Bindings</p>
      <div class="bindings-table-wrapper">
        <table class="bindings-table">
          <thead>
            <tr><th>Key</th><th>Modifiers</th><th>Command</th></tr>
          </thead>
          <tbody>
            {#each getBindings() as kb, i}
              <tr>
                <td>{kb.keysym}</td>
                <td>{flagsToDisplayString(kb.flags)}</td>
                <td>{kb.command}</td>
              </tr>
            {/each}
          </tbody>
        </table>
      </div>
      <div class="dialog-buttons">
        <button onclick={closeBindings}>Close</button>
      </div>
    </div>
  </div>
{/if}

<style>
  .menu-bar {
    display: flex;
    align-items: center;
    background: #252525;
    border-bottom: 1px solid #333;
    height: 28px;
    padding: 0 0.25rem;
    font-size: 0.8rem;
    user-select: none;
  }

  .menu-item {
    position: relative;
  }

  .menu-button {
    padding: 0.2rem 0.6rem;
    border: none;
    background: none;
    color: #c0c0c0;
    cursor: pointer;
    font-size: 0.8rem;
  }

  .menu-button:hover {
    background: #333;
  }

  .dropdown {
    position: absolute;
    top: 100%;
    left: 0;
    background: #2a2a2a;
    border: 1px solid #444;
    min-width: 160px;
    z-index: 50;
    box-shadow: 0 2px 8px rgba(0,0,0,0.5);
  }

  .dropdown button {
    display: block;
    width: 100%;
    padding: 0.4rem 0.75rem;
    border: none;
    background: none;
    color: #c0c0c0;
    text-align: left;
    cursor: pointer;
    font-size: 0.8rem;
  }

  .dropdown button:hover {
    background: #3a3a3a;
  }

  .spacer {
    flex: 1;
  }

  .title {
    color: #7a6a4a;
    font-size: 0.75rem;
    padding-right: 0.5rem;
  }

  /* ── Dialog styles ── */
  .dialog-overlay {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.6);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 200;
  }

  .dialog {
    background: #2a2a2a;
    border: 1px solid #555;
    border-radius: 4px;
    padding: 1.5rem;
    min-width: 320px;
    max-width: 480px;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.7);
    color: #c0c0c0;
    font-size: 0.85rem;
  }

  .dialog p {
    margin: 0.4rem 0;
  }

  .dialog-title {
    font-size: 1rem;
    font-weight: bold;
    color: #e0d0b0;
    margin-bottom: 0.8rem !important;
  }

  .dialog-prompt {
    color: #aaa;
    font-style: italic;
  }

  .dialog-warn {
    color: #ff8800;
  }

  .dialog-buttons {
    display: flex;
    gap: 0.5rem;
    margin-top: 1rem;
    justify-content: flex-end;
  }

  .dialog-buttons button {
    padding: 0.35rem 0.9rem;
    border: 1px solid #555;
    border-radius: 3px;
    background: #333;
    color: #c0c0c0;
    cursor: pointer;
    font-size: 0.8rem;
  }

  .dialog-buttons button:hover {
    background: #444;
  }

  .btn-primary {
    border-color: #4488ff !important;
    color: #88bbff !important;
  }

  .btn-danger {
    border-color: #ff4444 !important;
    color: #ff8888 !important;
  }

  .dialog-wide {
    max-width: 600px;
    min-width: 400px;
  }

  .bindings-table-wrapper {
    max-height: 400px;
    overflow-y: auto;
    margin: 0.5rem 0;
  }

  .bindings-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.8rem;
  }

  .bindings-table th,
  .bindings-table td {
    padding: 0.25rem 0.5rem;
    text-align: left;
    border-bottom: 1px solid #444;
  }

  .bindings-table th {
    color: #e0d0b0;
    position: sticky;
    top: 0;
    background: #2a2a2a;
  }

  .bindings-table td {
    color: #c0c0c0;
  }
</style>

