<script lang="ts">
  import { onMount } from 'svelte';
  import {
    keyEventToString,
    findBindingForEvent,
    findBindingForEventWithFlags,
    bindCommandToEvent,
    bindCommandWithFlags,
    unbindEvent,
    getBindings,
    flagsToDisplayString,
    saveBindingScopes,
    KEYF_EDIT,
    KEYF_ANY,
    type KeyBind,
  } from '../../lib/keys';
  import { getLastCommand } from '../../lib/player';
  import {
    setHotbarSlot,
    getHotbarSlots,
    type HotbarSlot,
  } from '../../lib/hotbar';
  import { gameEvents } from '../../lib/events';

  type DialogMode =
    | 'idle'
    | 'bind-capture'
    | 'bind-confirm'
    | 'unbind-capture'
    | 'unbind-confirm'
    | 'show-bindings'
    | 'bind-cmd-key-input'
    | 'bind-cmd-key-capture'
    | 'bind-cmd-key-confirm'
    | 'bind-cmd-hotbar-input'
    | 'bind-cmd-hotbar-confirm'
    ;

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

  let dialogMode = $state<DialogMode>('idle');
  let dialogKeyStr = $state('');
  let dialogCommand = $state('');
  let dialogExisting = $state<KeyBind | null>(null);
  let capturedEvent = $state<KeyboardEvent | null>(null);
  let dialogBindCmdEdit = $state(false);
  let dialogBindCmdAny = $state(false);
  let dialogHotbarSlot = $state(0);
  let dialogHotbarLabel = $state('');
  let hotbarExistingSlot = $state<HotbarSlot | null>(null);

  let bindingsSnapshot = $state<readonly KeyBind[]>([]);
  let pendingKeyScopes = $state(new Map<KeyBind, boolean>());

  onMount(() => {
    return gameEvents.on('openKeyBind', startBind);
  });

  export function isDialogActive(): boolean {
    return dialogMode !== 'idle';
  }

  // ── Bind last command to key ──────────────────────────────────────

  export function startBind() {
    const cmd = getLastCommand();
    if (!cmd) { onClose(); return; }
    dialogCommand = cmd;
    dialogMode = 'bind-capture';
    onClose();
  }

  function handleBindCapture(e: KeyboardEvent) {
    if (dialogMode !== 'bind-capture') return;
    if (['Shift', 'Control', 'Alt', 'Meta'].includes(e.key)) return;
    e.preventDefault();
    capturedEvent = e;
    dialogKeyStr = keyEventToString(e);
    dialogExisting = findBindingForEvent(e);
    dialogMode = 'bind-confirm';
  }

  function confirmBind() {
    if (capturedEvent) bindCommandToEvent(capturedEvent, dialogCommand);
    dialogMode = 'idle';
    capturedEvent = null;
  }

  function cancelBind() {
    dialogMode = 'idle';
    capturedEvent = null;
  }

  // ── Unbind a key ──────────────────────────────────────────────────

  function startUnbind() {
    dialogMode = 'unbind-capture';
    onClose();
  }

  function handleUnbindCapture(e: KeyboardEvent) {
    if (dialogMode !== 'unbind-capture') return;
    if (['Shift', 'Control', 'Alt', 'Meta'].includes(e.key)) return;
    e.preventDefault();
    capturedEvent = e;
    dialogKeyStr = keyEventToString(e);
    dialogExisting = findBindingForEvent(e);
    dialogMode = 'unbind-confirm';
  }

  function confirmUnbind() {
    if (capturedEvent) unbindEvent(capturedEvent);
    dialogMode = 'idle';
    capturedEvent = null;
  }

  function cancelUnbind() {
    dialogMode = 'idle';
    capturedEvent = null;
  }

  // ── Show key bindings ─────────────────────────────────────────────

  function showBindings() {
    dialogMode = 'show-bindings';
    onClose();
  }

  function isKeyGlobalPending(kb: KeyBind): boolean {
    return pendingKeyScopes.has(kb) ? pendingKeyScopes.get(kb)! : kb.global;
  }

  function toggleKeyScope(kb: KeyBind) {
    const current = isKeyGlobalPending(kb);
    const newMap = new Map(pendingKeyScopes);
    if (current === kb.global) {
      newMap.set(kb, !current);
    } else {
      newMap.delete(kb);
    }
    pendingKeyScopes = newMap;
  }

  function closeBindings() {
    pendingKeyScopes = new Map();
    dialogMode = 'idle';
  }

  function saveKeyBindingScopes() {
    const changes: Array<{ keysym: string; flags: number; newGlobal: boolean }> = [];
    for (const [kb, newGlobal] of pendingKeyScopes) {
      changes.push({ keysym: kb.keysym, flags: kb.flags, newGlobal });
    }
    saveBindingScopes(changes);
    const saved = new Map(pendingKeyScopes);
    pendingKeyScopes = new Map();
    bindingsSnapshot = bindingsSnapshot.map((kb) =>
      saved.has(kb) ? { ...kb, global: saved.get(kb)! } : kb
    );
  }

  // ── Bind entered command to key ───────────────────────────────────

  function startBindCmdKey() {
    dialogCommand = '';
    dialogBindCmdEdit = false;
    dialogBindCmdAny = false;
    dialogMode = 'bind-cmd-key-input';
    onClose();
  }

  function beginBindCmdKeyCapture() {
    dialogMode = 'bind-cmd-key-capture';
  }

  function handleBindCmdKeyCapture(e: KeyboardEvent) {
    if (dialogMode !== 'bind-cmd-key-capture') return;
    if (['Shift', 'Control', 'Alt', 'Meta'].includes(e.key)) return;
    e.preventDefault();
    capturedEvent = e;
    dialogKeyStr = keyEventToString(e);
    const extraFlags = (dialogBindCmdAny ? KEYF_ANY : 0) | (dialogBindCmdEdit ? KEYF_EDIT : 0);
    dialogExisting = findBindingForEventWithFlags(e, extraFlags);
    dialogMode = 'bind-cmd-key-confirm';
  }

  function confirmBindCmdKey() {
    if (capturedEvent) {
      const extraFlags = (dialogBindCmdAny ? KEYF_ANY : 0) | (dialogBindCmdEdit ? KEYF_EDIT : 0);
      bindCommandWithFlags(capturedEvent, dialogCommand, extraFlags);
    }
    dialogMode = 'idle';
    capturedEvent = null;
  }

  function cancelBindCmdKeyConfirm() {
    capturedEvent = null;
    dialogMode = 'bind-cmd-key-input';
  }

  function cancelBindCmdKeyCapture() {
    capturedEvent = null;
    dialogMode = 'bind-cmd-key-input';
  }

  function cancelBindCmdKey() {
    capturedEvent = null;
    dialogMode = 'idle';
  }

  // ── Bind entered command to hotbar slot ───────────────────────────

  function startBindCmdHotbar() {
    dialogCommand = '';
    dialogHotbarLabel = '';
    dialogHotbarSlot = 0;
    hotbarExistingSlot = null;
    dialogMode = 'bind-cmd-hotbar-input';
    onClose();
  }

  function handleBindCmdHotbar() {
    const existing = getHotbarSlots()[dialogHotbarSlot] ?? null;
    if (existing) {
      hotbarExistingSlot = existing;
      dialogMode = 'bind-cmd-hotbar-confirm';
    } else {
      setHotbarSlot(dialogHotbarSlot, {
        label: dialogHotbarLabel || dialogCommand,
        command: dialogCommand,
      });
      dialogMode = 'idle';
    }
  }

  function confirmBindCmdHotbar() {
    setHotbarSlot(dialogHotbarSlot, {
      label: dialogHotbarLabel || dialogCommand,
      command: dialogCommand,
    });
    dialogMode = 'idle';
  }

  function cancelBindCmdHotbarConfirm() {
    hotbarExistingSlot = null;
    dialogMode = 'bind-cmd-hotbar-input';
  }

  function cancelBindCmdHotbar() {
    dialogMode = 'idle';
  }

  // ── Window key handler ────────────────────────────────────────────

  function handleWindowKeydown(e: KeyboardEvent) {
    if (dialogMode === 'bind-capture') {
      handleBindCapture(e);
    } else if (dialogMode === 'unbind-capture') {
      handleUnbindCapture(e);
    } else if (dialogMode === 'bind-cmd-key-capture') {
      handleBindCmdKeyCapture(e);
    }
  }

  // Refresh snapshot when the bindings dialog opens
  $effect(() => {
    if (dialogMode === 'show-bindings') {
      bindingsSnapshot = getBindings();
      pendingKeyScopes = new Map();
    }
  });
</script>

<svelte:window onkeydown={handleWindowKeydown} />

<div class="menu-item">
  <button class="menu-button" onclick={onToggle} oncontextmenu={(e) => { e.preventDefault(); onToggle(); }}>Keyboard</button>
  {#if isOpen}
    <div class="dropdown" class:fading>
      <button
        onclick={startBind}
        oncontextmenu={(e) => { e.preventDefault(); startBind(); }}
      >Bind last command to key…</button>
      <button
        onclick={startBindCmdKey}
        oncontextmenu={(e) => { e.preventDefault(); startBindCmdKey(); }}
      >Bind command to key…</button>
      <button
        onclick={startBindCmdHotbar}
        oncontextmenu={(e) => { e.preventDefault(); startBindCmdHotbar(); }}
      >Bind command to hotbar slot…</button>
      <button
        onclick={startUnbind}
        oncontextmenu={(e) => { e.preventDefault(); startUnbind(); }}
      >Unbind a key…</button>
      <button
        onclick={showBindings}
        oncontextmenu={(e) => { e.preventDefault(); showBindings(); }}
      >Show key bindings</button>
    </div>
  {/if}
</div>

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
    <div class="dialog dialog-widest">
      <p class="dialog-title">Key Bindings</p>
      <div class="bindings-table-wrapper">
        <table class="bindings-table">
          <thead>
            <tr>
              <th>Key</th>
              <th>Modifiers</th>
              <th>Command</th>
              <th title="When checked the binding applies to all characters; unchecked = this character only">Global</th>
            </tr>
          </thead>
          <tbody>
            {#each bindingsSnapshot as kb}
              <tr>
                <td>{kb.keysym}</td>
                <td>{flagsToDisplayString(kb.flags)}</td>
                <td class="command-cell">{kb.command}</td>
                <td>
                  <input
                    type="checkbox"
                    checked={isKeyGlobalPending(kb)}
                    onchange={() => toggleKeyScope(kb)}
                    title={isKeyGlobalPending(kb) ? 'Global (all characters)' : 'This character only'}
                  />
                </td>
              </tr>
            {/each}
          </tbody>
        </table>
      </div>
      <div class="dialog-buttons">
        {#if pendingKeyScopes.size > 0}
          <button class="btn-primary" onclick={saveKeyBindingScopes}>Save scope changes</button>
        {/if}
        <button onclick={closeBindings}>Close</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-cmd-key-input'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to key</p>
      <label class="dialog-field-label" for="bind-cmd-key-input">Command:</label>
      <!-- svelte-ignore a11y_autofocus -->
      <input id="bind-cmd-key-input" class="dialog-input" type="text" bind:value={dialogCommand} autofocus />
      <label class="dialog-checkbox-label">
        <input type="checkbox" bind:checked={dialogBindCmdEdit} />
        Further edit
      </label>
      <label class="dialog-checkbox-label">
        <input type="checkbox" bind:checked={dialogBindCmdAny} />
        Any modifier
      </label>
      <div class="dialog-buttons">
        <button class="btn-primary" disabled={!dialogCommand} onclick={beginBindCmdKeyCapture}>Capture Key…</button>
        <button onclick={cancelBindCmdKey}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-cmd-key-capture'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to key</p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      <p class="dialog-prompt">Press the key combination you want to bind…</p>
      <div class="dialog-buttons">
        <button onclick={cancelBindCmdKeyCapture}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-cmd-key-confirm'}
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
        <button class="btn-primary" onclick={confirmBindCmdKey}>
          {dialogExisting ? 'Overwrite' : 'Bind'}
        </button>
        <button onclick={cancelBindCmdKeyConfirm}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-cmd-hotbar-input'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to hotbar slot</p>
      <label class="dialog-field-label" for="bind-cmd-hotbar-cmd">Command:</label>
      <!-- svelte-ignore a11y_autofocus -->
      <input id="bind-cmd-hotbar-cmd" class="dialog-input" type="text" bind:value={dialogCommand} autofocus />
      <label class="dialog-field-label" for="bind-cmd-hotbar-label">Label (leave blank to use command):</label>
      <input id="bind-cmd-hotbar-label" class="dialog-input" type="text" bind:value={dialogHotbarLabel} placeholder={dialogCommand} />
      <label class="dialog-field-label" for="bind-cmd-hotbar-slot">Slot:</label>
      <select id="bind-cmd-hotbar-slot" class="dialog-input dialog-select" bind:value={dialogHotbarSlot}>
        {#each {length: 12} as _, i}
          <option value={i}>F{i + 1}</option>
        {/each}
      </select>
      <div class="dialog-buttons">
        <button class="btn-primary" disabled={!dialogCommand} onclick={handleBindCmdHotbar}>Bind</button>
        <button onclick={cancelBindCmdHotbar}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-cmd-hotbar-confirm'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to hotbar slot</p>
      <p>Slot: <strong>F{dialogHotbarSlot + 1}</strong></p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      {#if hotbarExistingSlot}
        <p class="dialog-warn">Slot already assigned to: <strong>{hotbarExistingSlot.label}</strong></p>
        <p>Overwrite?</p>
      {/if}
      <div class="dialog-buttons">
        <button class="btn-primary" onclick={confirmBindCmdHotbar}>Overwrite</button>
        <button onclick={cancelBindCmdHotbarConfirm}>Back</button>
      </div>
    </div>
  </div>
{/if}
