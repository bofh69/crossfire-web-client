<script lang="ts">
  import {
    getBindings,
    flagsToDisplayString,
    saveBindingScopes,
    type KeyBind,
  } from '../lib/keys';
  import type { HotbarSlot } from '../lib/hotbar';
  import {
    getActiveProfileName,
    getButtonMappings,
    getStickConfig,
    directionName,
    saveGamepadButtonScopes,
    type AxisConfigTarget,
    type AxisConfigStep,
    type ButtonMapping,
  } from '../lib/gamepad';

  type DialogMode =
    | 'idle'
    | 'bind-capture'
    | 'bind-confirm'
    | 'unbind-capture'
    | 'unbind-confirm'
    | 'show-bindings'
    | 'about'
    | 'gp-show-bindings'
    | 'gp-axis-config'
    | 'gp-button-capture'
    | 'gp-button-confirm'
    | 'bind-cmd-key-input'
    | 'bind-cmd-key-capture'
    | 'bind-cmd-key-confirm'
    | 'bind-cmd-gp-input'
    | 'bind-cmd-gp-capture'
    | 'bind-cmd-gp-confirm'
    | 'bind-cmd-hotbar-input'
    | 'bind-cmd-hotbar-confirm'
    ;

  interface Props {
    dialogMode: DialogMode;
    dialogCommand: string;
    dialogKeyStr: string;
    dialogExisting: KeyBind | null;
    dialogBindCmdEdit: boolean;
    dialogBindCmdAny: boolean;
    gpAxisTarget: AxisConfigTarget;
    gpAxisStep: AxisConfigStep;
    gpAxisTestDir: number;
    gpCapturedButton: number;
    gpButtonExistingCmd: string | null;

    // Callbacks
    onCancelBind: () => void;
    onConfirmBind: () => void;
    onCancelUnbind: () => void;
    onConfirmUnbind: () => void;
    onCloseBindings: () => void;
    onCloseAbout: () => void;
    onCloseGamepadBindings: () => void;
    onAcceptAxisConfig: () => void;
    onCancelGamepadAxisConfig: () => void;
    onCancelGamepadButtonCapture: () => void;
    onConfirmGamepadButtonBind: () => void;
    onCancelGamepadButtonBind: () => void;
    onRemoveGamepadButton: (button: number) => void;
    onBeginBindCmdKeyCapture: () => void;
    onCancelBindCmdKey: () => void;
    onCancelBindCmdKeyCapture: () => void;
    onConfirmBindCmdKey: () => void;
    onCancelBindCmdKeyConfirm: () => void;
    onBeginBindCmdGpCapture: () => void;
    onCancelBindCmdGp: () => void;
    onCancelBindCmdGpCapture: () => void;
    onConfirmBindCmdGp: () => void;
    onCancelBindCmdGpConfirm: () => void;
    dialogHotbarSlot: number;
    dialogHotbarLabel: string;
    hotbarExistingSlot: HotbarSlot | null;
    onBindCmdHotbar: () => void;
    onCancelBindCmdHotbar: () => void;
    onConfirmBindCmdHotbar: () => void;
    onCancelBindCmdHotbarConfirm: () => void;
  }

  let {
    dialogMode,
    dialogCommand = $bindable(),
    dialogKeyStr,
    dialogExisting,
    dialogBindCmdEdit = $bindable(),
    dialogBindCmdAny = $bindable(),
    gpAxisTarget,
    gpAxisStep,
    gpAxisTestDir,
    gpCapturedButton,
    gpButtonExistingCmd,
    onCancelBind,
    onConfirmBind,
    onCancelUnbind,
    onConfirmUnbind,
    onCloseBindings,
    onCloseAbout,
    onCloseGamepadBindings,
    onAcceptAxisConfig,
    onCancelGamepadAxisConfig,
    onCancelGamepadButtonCapture,
    onConfirmGamepadButtonBind,
    onCancelGamepadButtonBind,
    onRemoveGamepadButton,
    onBeginBindCmdKeyCapture,
    onCancelBindCmdKey,
    onCancelBindCmdKeyCapture,
    onConfirmBindCmdKey,
    onCancelBindCmdKeyConfirm,
    onBeginBindCmdGpCapture,
    onCancelBindCmdGp,
    onCancelBindCmdGpCapture,
    onConfirmBindCmdGp,
    onCancelBindCmdGpConfirm,
    dialogHotbarSlot = $bindable(),
    dialogHotbarLabel = $bindable(),
    hotbarExistingSlot,
    onBindCmdHotbar,
    onCancelBindCmdHotbar,
    onConfirmBindCmdHotbar,
    onCancelBindCmdHotbarConfirm,
  }: Props = $props();

  // ── Show-bindings local state ──────────────────────────────────────────────
  // Track pending scope changes (KeyBind reference → desired global value) for
  // the key bindings table.  The player edits checkboxes, then clicks "Save".

  /**
   * Snapshot of the binding list, kept as $state so Svelte re-renders the
   * #each block when it changes.  Refreshed every time the dialog opens and
   * after every save.  (Plain getBindings() returns a non-reactive array from
   * module state; the #each block would hold stale object references after a
   * save if we called getBindings() directly.)
   */
  let bindingsSnapshot = $state<readonly KeyBind[]>([]);
  /** Same idea for gamepad button mappings. */
  let gpMappingsSnapshot = $state<readonly ButtonMapping[]>([]);

  /** Pending scope changes for key bindings: maps KeyBind reference → newGlobal */
  let pendingKeyScopes = $state(new Map<KeyBind, boolean>());

  function isKeyGlobalPending(kb: KeyBind): boolean {
    return pendingKeyScopes.has(kb) ? pendingKeyScopes.get(kb)! : kb.global;
  }

  function toggleKeyScope(kb: KeyBind) {
    const current = isKeyGlobalPending(kb);
    const newMap = new Map(pendingKeyScopes);
    if (current === kb.global) {
      // Flip from the original
      newMap.set(kb, !current);
    } else {
      // Was pending; revert back to original
      newMap.delete(kb);
    }
    pendingKeyScopes = newMap;
  }

  function saveKeyBindingScopes() {
    const changes: Array<{ keysym: string; flags: number; newGlobal: boolean }> = [];
    for (const [kb, newGlobal] of pendingKeyScopes) {
      changes.push({ keysym: kb.keysym, flags: kb.flags, newGlobal });
    }
    saveBindingScopes(changes);
    // Update global values in the snapshot in-place (preserving list order) so
    // the checkboxes immediately reflect the saved state without resorting.
    const saved = new Map(pendingKeyScopes);
    pendingKeyScopes = new Map();
    bindingsSnapshot = bindingsSnapshot.map((kb) =>
      saved.has(kb) ? { ...kb, global: saved.get(kb)! } : kb
    );
  }

  function resetPendingKeyScopes() {
    pendingKeyScopes = new Map();
  }

  // ── Show-gamepad-bindings local state ─────────────────────────────────────
  /** Pending scope changes for gamepad buttons: maps button number → newGlobal */
  let pendingGpScopes = $state(new Map<number, boolean>());

  function isGpGlobalPending(mapping: ButtonMapping): boolean {
    return pendingGpScopes.has(mapping.button)
      ? pendingGpScopes.get(mapping.button)!
      : (mapping.global ?? false);
  }

  function toggleGpScope(mapping: ButtonMapping) {
    const current = isGpGlobalPending(mapping);
    const newMap = new Map(pendingGpScopes);
    const originalGlobal = mapping.global ?? false;
    if (current === originalGlobal) {
      newMap.set(mapping.button, !current);
    } else {
      newMap.delete(mapping.button);
    }
    pendingGpScopes = newMap;
  }

  function saveGpButtonScopes() {
    const changes: Array<{ button: number; newGlobal: boolean }> = [];
    for (const [button, newGlobal] of pendingGpScopes) {
      changes.push({ button, newGlobal });
    }
    saveGamepadButtonScopes(changes);
    // Update global values in the snapshot in-place (preserving list order).
    const saved = new Map(pendingGpScopes);
    pendingGpScopes = new Map();
    gpMappingsSnapshot = gpMappingsSnapshot.map((m) =>
      saved.has(m.button) ? { ...m, global: saved.get(m.button)! } : m
    );
  }

  function resetPendingGpScopes() {
    pendingGpScopes = new Map();
  }

  // Refresh snapshots (and clear any pending edits) whenever a bindings dialog
  // opens.  This ensures stale references from a previous open are discarded.
  $effect(() => {
    if (dialogMode === 'show-bindings') {
      bindingsSnapshot = getBindings();
      pendingKeyScopes = new Map();
    } else if (dialogMode === 'gp-show-bindings') {
      gpMappingsSnapshot = getButtonMappings();
      pendingGpScopes = new Map();
    }
  });
</script>

{#if dialogMode === 'bind-capture'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to key</p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      <p class="dialog-prompt">Press the key combination you want to bind…</p>
      <div class="dialog-buttons">
        <button onclick={onCancelBind}>Cancel</button>
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
        <button class="btn-primary" onclick={onConfirmBind}>
          {dialogExisting ? 'Overwrite' : 'Bind'}
        </button>
        <button onclick={onCancelBind}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'unbind-capture'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Unbind a key</p>
      <p class="dialog-prompt">Press the key combination you want to unbind…</p>
      <div class="dialog-buttons">
        <button onclick={onCancelUnbind}>Cancel</button>
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
          <button class="btn-danger" onclick={onConfirmUnbind}>Remove</button>
          <button onclick={onCancelUnbind}>Cancel</button>
        </div>
      {:else}
        <p class="dialog-warn">This key is not bound to any command.</p>
        <div class="dialog-buttons">
          <button onclick={onCancelUnbind}>Close</button>
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
        <button onclick={() => { resetPendingKeyScopes(); onCloseBindings(); }}>Close</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'about'}
  <div class="dialog-overlay">
    <div class="dialog dialog-wide">
      <p class="dialog-title">About Crossfire Web Client</p>
      <p>
        A web-based client for <a href="http://crossfire.real-time.com/" target="_blank" rel="noopener noreferrer">Crossfire</a>,
        the cooperative multi-player graphical RPG and adventure game.
      </p>
      <p>
        This client is based on the original
        <a href="http://crossfire.real-time.com/" target="_blank" rel="noopener noreferrer">Crossfire GTK client</a>
        and reimplemented for the browser.
      </p>
      <p>
        Source code is available on
        <a href="https://github.com/bofh69/crossfire-web-client" target="_blank" rel="noopener noreferrer">GitHub</a>.
      </p>
      <p class="dialog-credits">
        Built with
        <a href="https://svelte.dev/" target="_blank" rel="noopener noreferrer">Svelte</a>,
        <a href="https://vite.dev/" target="_blank" rel="noopener noreferrer">Vite</a>, and
        <a href="https://www.typescriptlang.org/" target="_blank" rel="noopener noreferrer">TypeScript</a>.
      </p>
      <div class="dialog-buttons">
        <button onclick={onCloseAbout}>Close</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'gp-show-bindings'}
  <div class="dialog-overlay">
    <div class="dialog dialog-widest">
      <p class="dialog-title">Gamepad Bindings — {getActiveProfileName()}</p>
      {#if getStickConfig()}
        {@const sticks = getStickConfig()}
        <p>Walk/Run stick: axes {sticks?.walk.axisX}, {sticks?.walk.axisY}</p>
        <p>Fire stick: axes {sticks?.fire.axisX}, {sticks?.fire.axisY}</p>
      {/if}
      <div class="bindings-table-wrapper">
        <table class="bindings-table">
          <thead>
            <tr>
              <th>Button</th>
              <th>Command</th>
              <th title="When checked the binding applies to all characters; unchecked = this character only">Global</th>
              <th>Delete</th>
            </tr>
          </thead>
          <tbody>
            {#each gpMappingsSnapshot as mapping}
              <tr>
                <td>B{mapping.button}</td>
                <td class="command-cell">{mapping.command}</td>
                <td>
                  <input
                    type="checkbox"
                    checked={isGpGlobalPending(mapping)}
                    onchange={() => toggleGpScope(mapping)}
                    title={isGpGlobalPending(mapping) ? 'Global (all characters)' : 'This character only'}
                  />
                </td>
                <td>
                  <button class="btn-small btn-danger" onclick={() => {
                    if (confirm(`Remove binding for button B${mapping.button} (${mapping.command})?`)) {
                      onRemoveGamepadButton(mapping.button);
                      gpMappingsSnapshot = getButtonMappings();
                    }
                  }}>✕</button>
                </td>
              </tr>
            {/each}
          </tbody>
        </table>
      </div>
      <div class="dialog-buttons">
        {#if pendingGpScopes.size > 0}
          <button class="btn-primary" onclick={saveGpButtonScopes}>Save scope changes</button>
        {/if}
        <button onclick={() => { resetPendingGpScopes(); onCloseGamepadBindings(); }}>Close</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'gp-axis-config'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Configure {gpAxisTarget === 'walk' ? 'Walk/Run' : 'Fire'} Stick</p>
      {#if gpAxisStep === 'move-north'}
        <p class="dialog-prompt">Push the stick <strong>north</strong> (up)…</p>
      {:else if gpAxisStep === 'move-east'}
        <p class="dialog-prompt">Push the stick <strong>east</strong> (right)…</p>
      {:else if gpAxisStep === 'move-south'}
        <p class="dialog-prompt">Push the stick <strong>south</strong> (down)…</p>
      {:else if gpAxisStep === 'move-west'}
        <p class="dialog-prompt">Push the stick <strong>west</strong> (left)…</p>
      {:else if gpAxisStep === 'testing'}
        <p>Move the stick to test. Current direction:</p>
        <p class="axis-test-direction">{gpAxisTestDir > 0 ? directionName(gpAxisTestDir) : '(center)'}</p>
        <div class="dialog-buttons">
          <button class="btn-primary" onclick={onAcceptAxisConfig}>Accept</button>
          <button onclick={onCancelGamepadAxisConfig}>Abort</button>
        </div>
      {/if}
      {#if gpAxisStep !== 'testing'}
        <div class="dialog-buttons">
          <button onclick={onCancelGamepadAxisConfig}>Cancel</button>
        </div>
      {/if}
    </div>
  </div>
{:else if dialogMode === 'gp-button-capture'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind Gamepad Button</p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      <p class="dialog-prompt">Press the gamepad button you want to bind…</p>
      <div class="dialog-buttons">
        <button onclick={onCancelGamepadButtonCapture}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'gp-button-confirm'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind Gamepad Button</p>
      <p>Button: <strong>B{gpCapturedButton}</strong></p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      {#if gpButtonExistingCmd}
        <p class="dialog-warn">Already bound to: <strong>{gpButtonExistingCmd}</strong></p>
        <p>Overwrite?</p>
      {/if}
      <div class="dialog-buttons">
        <button class="btn-primary" onclick={onConfirmGamepadButtonBind}>
          {gpButtonExistingCmd ? 'Overwrite' : 'Bind'}
        </button>
        <button onclick={onCancelGamepadButtonBind}>Cancel</button>
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
        <button class="btn-primary" disabled={!dialogCommand} onclick={onBeginBindCmdKeyCapture}>Capture Key…</button>
        <button onclick={onCancelBindCmdKey}>Cancel</button>
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
        <button onclick={onCancelBindCmdKeyCapture}>Cancel</button>
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
        <button class="btn-primary" onclick={onConfirmBindCmdKey}>
          {dialogExisting ? 'Overwrite' : 'Bind'}
        </button>
        <button onclick={onCancelBindCmdKeyConfirm}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-cmd-gp-input'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to gamepad button</p>
      <label class="dialog-field-label" for="bind-cmd-gp-input">Command:</label>
      <!-- svelte-ignore a11y_autofocus -->
      <input id="bind-cmd-gp-input" class="dialog-input" type="text" bind:value={dialogCommand} autofocus />
      <div class="dialog-buttons">
        <button class="btn-primary" disabled={!dialogCommand} onclick={onBeginBindCmdGpCapture}>Capture Button…</button>
        <button onclick={onCancelBindCmdGp}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-cmd-gp-capture'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to gamepad button</p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      <p class="dialog-prompt">Press the gamepad button you want to bind…</p>
      <div class="dialog-buttons">
        <button onclick={onCancelBindCmdGpCapture}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === 'bind-cmd-gp-confirm'}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to gamepad button</p>
      <p>Button: <strong>B{gpCapturedButton}</strong></p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      {#if gpButtonExistingCmd}
        <p class="dialog-warn">Already bound to: <strong>{gpButtonExistingCmd}</strong></p>
        <p>Overwrite?</p>
      {/if}
      <div class="dialog-buttons">
        <button class="btn-primary" onclick={onConfirmBindCmdGp}>
          {gpButtonExistingCmd ? 'Overwrite' : 'Bind'}
        </button>
        <button onclick={onCancelBindCmdGpConfirm}>Cancel</button>
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
        <button class="btn-primary" disabled={!dialogCommand} onclick={onBindCmdHotbar}>Bind</button>
        <button onclick={onCancelBindCmdHotbar}>Cancel</button>
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
        <button class="btn-primary" onclick={onConfirmBindCmdHotbar}>Overwrite</button>
        <button onclick={onCancelBindCmdHotbarConfirm}>Back</button>
      </div>
    </div>
  </div>
{/if}

<style>
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
    background: var(--bg-lighter);
    border: 1px solid var(--border-light);
    border-radius: 4px;
    padding: 1.5rem;
    min-width: 320px;
    max-width: 480px;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.7);
    color: var(--text);
    font-size: 0.85rem;
  }

  .dialog p {
    margin: 0.4rem 0;
  }

  .dialog-title {
    font-size: 1rem;
    font-weight: bold;
    color: var(--text-warm);
    margin-bottom: 0.8rem !important;
  }

  .dialog-prompt {
    color: #aaa;
    font-style: italic;
  }

  .dialog-warn {
    color: #ff8800;
  }

  .dialog-field-label {
    display: block;
    margin-bottom: 0.25rem;
    color: #aaa;
  }

  .dialog-input {
    width: 100%;
    padding: 0.4rem;
    border: 1px solid var(--border-light);
    border-radius: 3px;
    background: var(--bg-panel);
    color: var(--text-bright);
    font-family: 'Courier New', monospace;
    font-size: 0.85rem;
    margin-bottom: 0.6rem;
    box-sizing: border-box;
  }

  .dialog-select {
    font-family: inherit;
    cursor: pointer;
  }

  .dialog-checkbox-label {
    display: flex;
    align-items: center;
    gap: 0.3rem;
    margin-bottom: 0.3rem;
    color: #aaa;
    font-size: 0.8rem;
  }

  .dialog-wide {
    min-width: 420px;
    max-width: 600px;
  }

  .dialog-widest {
    min-width: 520px;
    max-width: min(950px, 95vw);
    box-sizing: border-box;
  }

  .dialog-buttons {
    display: flex;
    gap: 0.5rem;
    margin-top: 1rem;
    justify-content: flex-end;
  }

  .dialog-buttons button {
    padding: 0.35rem 0.75rem;
    border: 1px solid var(--border-light);
    border-radius: 3px;
    background: var(--border);
    color: var(--text);
    cursor: pointer;
    font-size: 0.8rem;
  }

  .dialog-buttons button:hover {
    background: var(--border-mid);
  }

  .btn-primary {
    background: #2a5a2a !important;
    border-color: #4a8a4a !important;
  }

  .btn-primary:hover {
    background: #3a7a3a !important;
  }

  .btn-danger {
    background: #5a2a2a !important;
    border-color: #8a4a4a !important;
  }

  .btn-danger:hover {
    background: #7a3a3a !important;
  }

  .btn-small {
    padding: 0.1rem 0.4rem;
    font-size: 0.7rem;
  }

  .dialog-credits {
    font-size: 0.8rem;
    color: var(--text-dim);
    margin-top: 1rem !important;
  }

  .bindings-table-wrapper {
    max-height: 300px;
    overflow-x: hidden;
    overflow-y: auto;
    margin: 0.5rem 0;
    border: 1px solid var(--border-mid);
    border-radius: 3px;
  }

  .bindings-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.8rem;
    table-layout: fixed;
  }

  .bindings-table th {
    background: var(--border);
    padding: 0.3rem 0.5rem;
    text-align: left;
    color: #999;
    position: sticky;
    top: 0;
    font-weight: normal;
  }

  .bindings-table td {
    padding: 0.25rem 0.5rem;
    border-top: 1px solid var(--border);
    color: var(--text);
  }

  .command-cell {
    word-break: break-word;
  }

  .axis-test-direction {
    font-size: 1.2rem;
    font-weight: bold;
    color: var(--text-warm);
    text-align: center;
    margin: 0.5rem 0 !important;
  }
</style>
