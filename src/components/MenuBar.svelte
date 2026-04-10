<script lang="ts">
  import { onMount } from 'svelte';
  import { clientDisconnect } from '../lib/client';
  import PickupMenu from './PickupMenu.svelte';
  import MenuBarDialogs from './MenuBarDialogs.svelte';
  import {
    keyEventToString,
    findBindingForEvent,
    findBindingForEventWithFlags,
    bindCommandToEvent,
    bindCommandWithFlags,
    unbindEvent,
    getBindings,
    flagsToDisplayString,
    KEYF_EDIT,
    KEYF_ANY,
    type KeyBind,
  } from '../lib/keys';
  import { getLastCommand } from '../lib/player';
  import {
    getMusicMuted, getSfxMuted,
    setMusicMuted, setSfxMuted,
  } from '../lib/sound';
  import {
    isGamepadConnected,
    getActiveProfileName,
    getButtonMappings,
    getStickConfig,
    setButtonCommand,
    removeButtonCommand,
    resetGamepadBindings,
    startAxisConfig,
    cancelAxisConfig,
    acceptAxisConfig,
    getAxisTestDirection,
    directionName,
    startButtonConfig,
    cancelButtonConfig,
    getButtonCommand,
    type AxisConfigTarget,
    type AxisConfigStep,
  } from '../lib/gamepad';
  import { gameEvents } from '../lib/events';

  interface Props {
    onDisconnect: () => void;
  }

  let { onDisconnect }: Props = $props();

  let activeMenu = $state<string | null>(null);
  let pickupMenu: PickupMenu | undefined = $state();

  /** Milliseconds after the cursor leaves the menu-bar before the open dropdown closes. */
  const MENU_FADE_MS = 2000;

  let menuFading = $state(false);
  let menuFadeTimer: ReturnType<typeof setTimeout> | null = null;

  /** Persist the pickup mode so it survives menu close/reopen. */
  let currentPickupMode = $state(0x80000000 >>> 0); // PU_NEWMODE

  // ── Key-bind dialog state ─────────────────────────────────────────────────
  type DialogMode =
    | 'idle'
    | 'bind-capture'        // waiting for the key to bind
    | 'bind-confirm'        // key captured; asking to overwrite an existing binding
    | 'unbind-capture'      // waiting for the key to unbind
    | 'unbind-confirm'      // key captured; asking to confirm removal
    | 'show-bindings'       // showing all key bindings
    | 'about'               // about dialog
    | 'gp-show-bindings'    // showing gamepad bindings
    | 'gp-axis-config'      // configuring a gamepad axis (multi-step)
    | 'gp-button-capture'   // waiting for a gamepad button press (binds last command)
    | 'gp-button-confirm'   // confirm binding button to last command
    | 'bind-cmd-key-input'   // entering command + flags before key capture
    | 'bind-cmd-key-capture' // waiting for the key to bind (entered command)
    | 'bind-cmd-key-confirm' // key captured; asking to confirm
    | 'bind-cmd-gp-input'    // entering command before button capture
    | 'bind-cmd-gp-capture'  // waiting for the gamepad button to bind (entered command)
    | 'bind-cmd-gp-confirm'  // button captured; asking to confirm
    ;

  let dialogMode = $state<DialogMode>('idle');
  let dialogKeyStr = $state('');             // human-readable key (e.g. "Ctrl+f")
  let dialogCommand = $state('');           // command being bound
  let dialogExisting = $state<KeyBind | null>(null); // existing binding for the key
  let capturedEvent = $state<KeyboardEvent | null>(null);

  // ── Gamepad dialog state ────────────────────────────────────────────────
  let gpAxisTarget = $state<AxisConfigTarget>('walk');
  let gpAxisStep = $state<AxisConfigStep>('move-north');
  let gpAxisTestDir = $state(0);
  let gpAxisTestTimer: ReturnType<typeof setInterval> | null = null;
  let gpCapturedButton = $state(-1);
  let gpButtonExistingCmd = $state<string | null>(null);

  // ── Bind-command-to-key/button dialog state ────────────────────────────────
  let dialogBindCmdEdit = $state(false);   // KEYF_EDIT ("Further edit")
  let dialogBindCmdAny  = $state(false);   // KEYF_ANY  ("Any modifier")

  onMount(() => {
    const cleanups = [
      gameEvents.on('pickupUpdate', setPickupMode),
      gameEvents.on('statsUpdate', (stats) => {
        if (stats.range !== undefined) setRange(stats.range);
      }),
      gameEvents.on('openKeyBind', startBind),
      gameEvents.on('openGamepadBind', startGamepadButtonBind),
    ];
    return () => { for (const unsub of cleanups) unsub(); };
  });

  function toggleMenu(menu: string) {
    clearMenuFadeTimer();
    menuFading = false;
    activeMenu = activeMenu === menu ? null : menu;
  }

  function closeMenu() {
    clearMenuFadeTimer();
    menuFading = false;
    activeMenu = null;
  }

  function clearMenuFadeTimer() {
    if (menuFadeTimer !== null) {
      clearTimeout(menuFadeTimer);
      menuFadeTimer = null;
    }
  }

  function handleMenuBarMouseLeave() {
    if (activeMenu === null) return;
    menuFading = true;
    menuFadeTimer = setTimeout(() => {
      activeMenu = null;
      menuFading = false;
      menuFadeTimer = null;
    }, MENU_FADE_MS);
  }

  function handleMenuBarMouseEnter() {
    clearMenuFadeTimer();
    menuFading = false;
  }

  function handleDisconnect() {
    clientDisconnect();
    onDisconnect();
    closeMenu();
  }

  function setPickupMode(mode: number) {
    currentPickupMode = mode >>> 0;
    pickupMenu?.setPickupMode(currentPickupMode);
  }

  /** The currently readied range item (spell, skill, bow, etc.). */
  let currentRange = $state('');

  function setRange(range: string) {
    currentRange = range;
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

  function showAbout() {
    dialogMode = 'about';
    closeMenu();
  }

  function closeAbout() {
    dialogMode = 'idle';
  }

  // ── Gamepad dialogs ───────────────────────────────────────────────────────

  function showGamepadBindings() {
    dialogMode = 'gp-show-bindings';
    closeMenu();
  }

  function closeGamepadBindings() {
    dialogMode = 'idle';
  }

  function startGamepadAxisConfig(target: AxisConfigTarget) {
    gpAxisTarget = target;
    gpAxisStep = 'move-north';
    dialogMode = 'gp-axis-config';
    closeMenu();
    startAxisConfig(
      target,
      (step) => {
        gpAxisStep = step;
        if (step === 'testing') {
          // Start polling live direction for the test display.
          gpAxisTestDir = 0;
          gpAxisTestTimer = setInterval(() => {
            gpAxisTestDir = getAxisTestDirection();
          }, 100);
        }
      },
      (_axes) => {
        // Done (accepted or aborted).
        stopAxisTestTimer();
        dialogMode = 'idle';
      },
    );
  }

  function stopAxisTestTimer() {
    if (gpAxisTestTimer !== null) {
      clearInterval(gpAxisTestTimer);
      gpAxisTestTimer = null;
    }
  }

  function cancelGamepadAxisConfig() {
    stopAxisTestTimer();
    cancelAxisConfig();
    dialogMode = 'idle';
  }

  function handleAcceptAxisConfig() {
    stopAxisTestTimer();
    acceptAxisConfig();
    // The onDone callback will set dialogMode = 'idle'.
  }

  function startGamepadButtonBind() {
    const cmd = getLastCommand();
    if (!cmd) {
      closeMenu();
      return;
    }
    dialogCommand = cmd;
    dialogMode = 'gp-button-capture';
    closeMenu();
    startButtonConfig((button: number) => {
      gpCapturedButton = button;
      gpButtonExistingCmd = getButtonCommand(button);
      dialogMode = 'gp-button-confirm';
    });
  }

  function cancelGamepadButtonCapture() {
    cancelButtonConfig();
    dialogMode = 'idle';
  }

  function confirmGamepadButtonBind() {
    if (gpCapturedButton >= 0 && dialogCommand) {
      setButtonCommand(gpCapturedButton, dialogCommand);
    }
    dialogMode = 'idle';
  }

  function cancelGamepadButtonBind() {
    dialogMode = 'idle';
  }

  // ── Bind entered command to key ───────────────────────────────────────────

  function startBindCmdKey() {
    dialogCommand = '';
    dialogBindCmdEdit = false;
    dialogBindCmdAny = false;
    dialogMode = 'bind-cmd-key-input';
    closeMenu();
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

  // ── Bind entered command to gamepad button ────────────────────────────────

  function startBindCmdGp() {
    dialogCommand = '';
    dialogMode = 'bind-cmd-gp-input';
    closeMenu();
  }

  function beginBindCmdGpCapture() {
    dialogMode = 'bind-cmd-gp-capture';
    startButtonConfig((button: number) => {
      gpCapturedButton = button;
      gpButtonExistingCmd = getButtonCommand(button);
      dialogMode = 'bind-cmd-gp-confirm';
    });
  }

  function confirmBindCmdGp() {
    if (gpCapturedButton >= 0 && dialogCommand) {
      setButtonCommand(gpCapturedButton, dialogCommand);
    }
    dialogMode = 'idle';
  }

  function cancelBindCmdGpConfirm() {
    dialogMode = 'bind-cmd-gp-input';
  }

  function cancelBindCmdGpCapture() {
    cancelButtonConfig();
    dialogMode = 'bind-cmd-gp-input';
  }

  function cancelBindCmdGp() {
    dialogMode = 'idle';
  }

  function handleRemoveGamepadButton(button: number) {
    removeButtonCommand(button);
  }

  function handleResetGamepad() {
    resetGamepadBindings();
    closeMenu();
  }

  // ── Sound mute toggles ────────────────────────────────────────────────────

  let musicMuted = $state(getMusicMuted());
  let sfxMuted = $state(getSfxMuted());

  function toggleMusicMute() {
    musicMuted = !musicMuted;
    setMusicMuted(musicMuted);
    closeMenu();
  }

  function toggleSfxMute() {
    sfxMuted = !sfxMuted;
    setSfxMuted(sfxMuted);
    closeMenu();
  }

  function handleWindowKeydown(e: KeyboardEvent) {
    if (dialogMode === 'bind-capture') {
      handleBindCapture(e);
    } else if (dialogMode === 'unbind-capture') {
      handleUnbindCapture(e);
    } else if (dialogMode === 'bind-cmd-key-capture') {
      handleBindCmdKeyCapture(e);
    }
  }
</script>

<svelte:window
  onclick={closeMenu}
  onkeydown={handleWindowKeydown}
/>

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="menu-bar" onclick={(e: MouseEvent) => e.stopPropagation()} onmouseenter={handleMenuBarMouseEnter} onmouseleave={handleMenuBarMouseLeave}>
  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('file')} oncontextmenu={(e) => { e.preventDefault(); toggleMenu('file'); }}>File</button>
    {#if activeMenu === 'file'}
      <div class="dropdown" class:fading={menuFading}>
        <button
          onclick={handleDisconnect}
          oncontextmenu={(e) => { e.preventDefault(); handleDisconnect(); }}
        >Disconnect</button>
      </div>
    {/if}
  </div>

  <div class="menu-item">
    <!-- svelte-ignore a11y_click_events_have_key_events -->
    <!-- svelte-ignore a11y_no_static_element_interactions -->
    <button class="menu-button" onclick={() => toggleMenu('pickup')} oncontextmenu={(e) => { e.preventDefault(); toggleMenu('pickup'); }}>Pickup</button>
    {#if activeMenu === 'pickup'}
      <!-- svelte-ignore a11y_click_events_have_key_events -->
      <!-- svelte-ignore a11y_no_static_element_interactions -->
      <div class="dropdown" class:fading={menuFading} onclick={(e: MouseEvent) => e.stopPropagation()}>
        <PickupMenu bind:this={pickupMenu} initialMode={currentPickupMode} />
      </div>
    {/if}
  </div>

  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('keyboard')} oncontextmenu={(e) => { e.preventDefault(); toggleMenu('keyboard'); }}>Keyboard</button>
    {#if activeMenu === 'keyboard'}
      <div class="dropdown" class:fading={menuFading}>
        <button
          onclick={startBind}
          oncontextmenu={(e) => { e.preventDefault(); startBind(); }}
        >Bind last command to key…</button>
        <button
          onclick={startBindCmdKey}
          oncontextmenu={(e) => { e.preventDefault(); startBindCmdKey(); }}
        >Bind command to key…</button>
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

  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('gamepad')} oncontextmenu={(e) => { e.preventDefault(); toggleMenu('gamepad'); }}>Gamepad</button>
    {#if activeMenu === 'gamepad'}
      <div class="dropdown" class:fading={menuFading}>
        {#if isGamepadConnected()}
          <button
            onclick={startGamepadButtonBind}
            oncontextmenu={(e) => { e.preventDefault(); startGamepadButtonBind(); }}
          >Bind last command to button…</button>
          <button
            onclick={startBindCmdGp}
            oncontextmenu={(e) => { e.preventDefault(); startBindCmdGp(); }}
          >Bind command to button…</button>
          <button
            onclick={() => startGamepadAxisConfig('walk')}
            oncontextmenu={(e) => { e.preventDefault(); startGamepadAxisConfig('walk'); }}
          >Configure walk/run stick…</button>
          <button
            onclick={() => startGamepadAxisConfig('fire')}
            oncontextmenu={(e) => { e.preventDefault(); startGamepadAxisConfig('fire'); }}
          >Configure fire stick…</button>
          <button
            onclick={handleResetGamepad}
            oncontextmenu={(e) => { e.preventDefault(); handleResetGamepad(); }}
          >Reset to defaults</button>
          <button
            onclick={showGamepadBindings}
            oncontextmenu={(e) => { e.preventDefault(); showGamepadBindings(); }}
          >Show gamepad bindings</button>
        {:else}
          <button disabled>No gamepad connected</button>
        {/if}
      </div>
    {/if}
  </div>

  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('sound')} oncontextmenu={(e) => { e.preventDefault(); toggleMenu('sound'); }}>Sound</button>
    {#if activeMenu === 'sound'}
      <div class="dropdown" class:fading={menuFading}>
        <button
          onclick={toggleMusicMute}
          oncontextmenu={(e) => { e.preventDefault(); toggleMusicMute(); }}
        >{musicMuted ? 'Unmute Music' : 'Mute Music'}</button>
        <button
          onclick={toggleSfxMute}
          oncontextmenu={(e) => { e.preventDefault(); toggleSfxMute(); }}
        >{sfxMuted ? 'Unmute Sound Effects' : 'Mute Sound Effects'}</button>
      </div>
    {/if}
  </div>

  <div class="menu-item">
    <button class="menu-button" onclick={() => toggleMenu('help')} oncontextmenu={(e) => { e.preventDefault(); toggleMenu('help'); }}>Help</button>
    {#if activeMenu === 'help'}
      <div class="dropdown" class:fading={menuFading}>
        <button
          onclick={showAbout}
          oncontextmenu={(e) => { e.preventDefault(); showAbout(); }}
        >About Crossfire Web Client</button>
      </div>
    {/if}
  </div>

  <div class="spacer"></div>
  {#if currentRange}
    <span class="range-label">{currentRange}</span>
  {/if}
  <span class="title">Crossfire</span>
</div>

<!-- ── Key-capture / confirm dialogs ── -->
<MenuBarDialogs
  {dialogMode}
  bind:dialogCommand
  {dialogKeyStr}
  {dialogExisting}
  bind:dialogBindCmdEdit
  bind:dialogBindCmdAny
  {gpAxisTarget}
  {gpAxisStep}
  {gpAxisTestDir}
  {gpCapturedButton}
  {gpButtonExistingCmd}
  onCancelBind={cancelBind}
  onConfirmBind={confirmBind}
  onCancelUnbind={cancelUnbind}
  onConfirmUnbind={confirmUnbind}
  onCloseBindings={closeBindings}
  onCloseAbout={closeAbout}
  onCloseGamepadBindings={closeGamepadBindings}
  onAcceptAxisConfig={handleAcceptAxisConfig}
  onCancelGamepadAxisConfig={cancelGamepadAxisConfig}
  onCancelGamepadButtonCapture={cancelGamepadButtonCapture}
  onConfirmGamepadButtonBind={confirmGamepadButtonBind}
  onCancelGamepadButtonBind={cancelGamepadButtonBind}
  onRemoveGamepadButton={handleRemoveGamepadButton}
  onBeginBindCmdKeyCapture={beginBindCmdKeyCapture}
  onCancelBindCmdKey={cancelBindCmdKey}
  onCancelBindCmdKeyCapture={cancelBindCmdKeyCapture}
  onConfirmBindCmdKey={confirmBindCmdKey}
  onCancelBindCmdKeyConfirm={cancelBindCmdKeyConfirm}
  onBeginBindCmdGpCapture={beginBindCmdGpCapture}
  onCancelBindCmdGp={cancelBindCmdGp}
  onCancelBindCmdGpCapture={cancelBindCmdGpCapture}
  onConfirmBindCmdGp={confirmBindCmdGp}
  onCancelBindCmdGpConfirm={cancelBindCmdGpConfirm}
/>

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
    opacity: 1;
    transition: opacity 2s ease-out;
  }

  .dropdown.fading {
    opacity: 0;
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

  .range-label {
    color: #a0b0c0;
    font-size: 0.75rem;
    padding-right: 0.75rem;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    max-width: 200px;
  }

  .title {
    color: #7a6a4a;
    font-size: 0.75rem;
    padding-right: 0.5rem;
  }

  .dropdown button:disabled {
    color: #666;
    cursor: default;
  }

  .dropdown button:disabled:hover {
    background: none;
  }
</style>

