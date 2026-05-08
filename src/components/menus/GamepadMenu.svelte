<script lang="ts">
  import { onMount } from "svelte";
  import {
    isGamepadConnected,
    setButtonCommand,
    removeButtonCommand,
    resetGamepadBindings,
    startAxisConfig,
    cancelAxisConfig,
    acceptAxisConfig,
    getAxisTestDirection,
    startButtonConfig,
    cancelButtonConfig,
    getButtonCommand,
    getButtonMappings,
    getStickConfig,
    directionName,
    saveGamepadButtonScopes,
    getActiveProfileName,
    type AxisConfigTarget,
    type AxisConfigStep,
    type ButtonMapping,
  } from "../../lib/gamepad";
  import { getLastCommand } from "../../lib/player";
  import { gameEvents } from "../../lib/events";

  type DialogMode =
    | "idle"
    | "gp-show-bindings"
    | "gp-axis-config"
    | "gp-button-capture"
    | "gp-button-confirm"
    | "bind-cmd-gp-input"
    | "bind-cmd-gp-capture"
    | "bind-cmd-gp-confirm";

  interface Props {
    fading: boolean;
    isOpen: boolean;
    onToggle: () => void;
    onClose: () => void;
  }
  let { fading, isOpen, onToggle, onClose }: Props = $props();

  let dialogMode = $state<DialogMode>("idle");
  let dialogCommand = $state("");
  let gpAxisTarget = $state<AxisConfigTarget>("walk");
  let gpAxisStep = $state<AxisConfigStep>("move-north");
  let gpAxisTestDir = $state(0);
  let gpAxisTestTimer: ReturnType<typeof setInterval> | null = null;
  let gpCapturedButton = $state(-1);
  let gpButtonExistingCmd = $state<string | null>(null);

  let gpMappingsSnapshot = $state<readonly ButtonMapping[]>([]);
  let pendingGpScopes = $state(new Map<number, boolean>());

  onMount(() => {
    return gameEvents.on("openGamepadBind", startGamepadButtonBind);
  });

  export function isDialogActive(): boolean {
    return dialogMode !== "idle";
  }

  // ── Show gamepad bindings ─────────────────────────────────────────

  function showGamepadBindings() {
    dialogMode = "gp-show-bindings";
    onClose();
  }

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

  function closeGamepadBindings() {
    pendingGpScopes = new Map();
    dialogMode = "idle";
  }

  function saveGpButtonScopes() {
    const changes: Array<{ button: number; newGlobal: boolean }> = [];
    for (const [button, newGlobal] of pendingGpScopes) {
      changes.push({ button, newGlobal });
    }
    saveGamepadButtonScopes(changes);
    const saved = new Map(pendingGpScopes);
    pendingGpScopes = new Map();
    gpMappingsSnapshot = gpMappingsSnapshot.map((m) =>
      saved.has(m.button) ? { ...m, global: saved.get(m.button)! } : m,
    );
  }

  function handleRemoveGamepadButton(button: number) {
    removeButtonCommand(button);
    gpMappingsSnapshot = getButtonMappings();
  }

  // ── Gamepad axis config ───────────────────────────────────────────

  function startGamepadAxisConfig(target: AxisConfigTarget) {
    gpAxisTarget = target;
    gpAxisStep = "move-north";
    dialogMode = "gp-axis-config";
    onClose();
    startAxisConfig(
      target,
      (step) => {
        gpAxisStep = step;
        if (step === "testing") {
          gpAxisTestDir = 0;
          gpAxisTestTimer = setInterval(() => {
            gpAxisTestDir = getAxisTestDirection();
          }, 100);
        }
      },
      (_axes) => {
        stopAxisTestTimer();
        dialogMode = "idle";
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
    dialogMode = "idle";
  }

  function handleAcceptAxisConfig() {
    stopAxisTestTimer();
    acceptAxisConfig();
    // The onDone callback will set dialogMode = 'idle'.
  }

  // ── Bind last command to gamepad button ───────────────────────────

  export function startGamepadButtonBind() {
    const cmd = getLastCommand();
    if (!cmd) {
      onClose();
      return;
    }
    dialogCommand = cmd;
    dialogMode = "gp-button-capture";
    onClose();
    startButtonConfig((button: number) => {
      gpCapturedButton = button;
      gpButtonExistingCmd = getButtonCommand(button);
      dialogMode = "gp-button-confirm";
    });
  }

  function cancelGamepadButtonCapture() {
    cancelButtonConfig();
    dialogMode = "idle";
  }

  function confirmGamepadButtonBind() {
    if (gpCapturedButton >= 0 && dialogCommand) {
      setButtonCommand(gpCapturedButton, dialogCommand);
    }
    dialogMode = "idle";
  }

  function cancelGamepadButtonBind() {
    dialogMode = "idle";
  }

  // ── Bind entered command to gamepad button ────────────────────────

  function startBindCmdGp() {
    dialogCommand = "";
    dialogMode = "bind-cmd-gp-input";
    onClose();
  }

  function beginBindCmdGpCapture() {
    dialogMode = "bind-cmd-gp-capture";
    startButtonConfig((button: number) => {
      gpCapturedButton = button;
      gpButtonExistingCmd = getButtonCommand(button);
      dialogMode = "bind-cmd-gp-confirm";
    });
  }

  function confirmBindCmdGp() {
    if (gpCapturedButton >= 0 && dialogCommand) {
      setButtonCommand(gpCapturedButton, dialogCommand);
    }
    dialogMode = "idle";
  }

  function cancelBindCmdGpConfirm() {
    dialogMode = "bind-cmd-gp-input";
  }

  function cancelBindCmdGpCapture() {
    cancelButtonConfig();
    dialogMode = "bind-cmd-gp-input";
  }

  function cancelBindCmdGp() {
    dialogMode = "idle";
  }

  function handleResetGamepad() {
    resetGamepadBindings();
    onClose();
  }

  // Refresh snapshot when the bindings dialog opens
  $effect(() => {
    if (dialogMode === "gp-show-bindings") {
      gpMappingsSnapshot = getButtonMappings();
      pendingGpScopes = new Map();
    }
  });
</script>

<div class="menu-item">
  <button
    class="menu-button"
    onclick={onToggle}
    oncontextmenu={(e) => {
      e.preventDefault();
      onToggle();
    }}>Gamepad</button
  >
  {#if isOpen}
    <div class="dropdown" class:fading>
      {#if isGamepadConnected()}
        <button
          onclick={() => startGamepadButtonBind()}
          oncontextmenu={(e) => {
            e.preventDefault();
            startGamepadButtonBind();
          }}>Bind last command to button…</button
        >
        <button
          onclick={startBindCmdGp}
          oncontextmenu={(e) => {
            e.preventDefault();
            startBindCmdGp();
          }}>Bind command to button…</button
        >
        <button
          onclick={() => startGamepadAxisConfig("walk")}
          oncontextmenu={(e) => {
            e.preventDefault();
            startGamepadAxisConfig("walk");
          }}>Configure walk/run stick…</button
        >
        <button
          onclick={() => startGamepadAxisConfig("fire")}
          oncontextmenu={(e) => {
            e.preventDefault();
            startGamepadAxisConfig("fire");
          }}>Configure fire stick…</button
        >
        <button
          onclick={handleResetGamepad}
          oncontextmenu={(e) => {
            e.preventDefault();
            handleResetGamepad();
          }}>Reset to defaults</button
        >
        <button
          onclick={showGamepadBindings}
          oncontextmenu={(e) => {
            e.preventDefault();
            showGamepadBindings();
          }}>Show gamepad bindings</button
        >
      {:else}
        <button disabled>No gamepad connected</button>
      {/if}
    </div>
  {/if}
</div>

{#if dialogMode === "gp-show-bindings"}
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
              <th
                title="When checked the binding applies to all characters; unchecked = this character only"
                >Global</th
              >
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
                    title={isGpGlobalPending(mapping)
                      ? "Global (all characters)"
                      : "This character only"}
                  />
                </td>
                <td>
                  <button
                    class="btn-small btn-danger"
                    onclick={() => {
                      if (
                        confirm(
                          `Remove binding for button B${mapping.button} (${mapping.command})?`,
                        )
                      ) {
                        handleRemoveGamepadButton(mapping.button);
                      }
                    }}>✕</button
                  >
                </td>
              </tr>
            {/each}
          </tbody>
        </table>
      </div>
      <div class="dialog-buttons">
        {#if pendingGpScopes.size > 0}
          <button class="btn-primary" onclick={saveGpButtonScopes}
            >Save scope changes</button
          >
        {/if}
        <button onclick={closeGamepadBindings}>Close</button>
      </div>
    </div>
  </div>
{:else if dialogMode === "gp-axis-config"}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">
        Configure {gpAxisTarget === "walk" ? "Walk/Run" : "Fire"} Stick
      </p>
      {#if gpAxisStep === "move-north"}
        <p class="dialog-prompt">Push the stick <strong>north</strong> (up)…</p>
      {:else if gpAxisStep === "move-east"}
        <p class="dialog-prompt">
          Push the stick <strong>east</strong> (right)…
        </p>
      {:else if gpAxisStep === "move-south"}
        <p class="dialog-prompt">
          Push the stick <strong>south</strong> (down)…
        </p>
      {:else if gpAxisStep === "move-west"}
        <p class="dialog-prompt">
          Push the stick <strong>west</strong> (left)…
        </p>
      {:else if gpAxisStep === "testing"}
        <p>Move the stick to test. Current direction:</p>
        <p class="axis-test-direction">
          {gpAxisTestDir > 0 ? directionName(gpAxisTestDir) : "(center)"}
        </p>
        <div class="dialog-buttons">
          <button class="btn-primary" onclick={handleAcceptAxisConfig}
            >Accept</button
          >
          <button onclick={cancelGamepadAxisConfig}>Abort</button>
        </div>
      {/if}
      {#if gpAxisStep !== "testing"}
        <div class="dialog-buttons">
          <button onclick={cancelGamepadAxisConfig}>Cancel</button>
        </div>
      {/if}
    </div>
  </div>
{:else if dialogMode === "gp-button-capture"}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind Gamepad Button</p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      <p class="dialog-prompt">Press the gamepad button you want to bind…</p>
      <div class="dialog-buttons">
        <button onclick={cancelGamepadButtonCapture}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === "gp-button-confirm"}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind Gamepad Button</p>
      <p>Button: <strong>B{gpCapturedButton}</strong></p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      {#if gpButtonExistingCmd}
        <p class="dialog-warn">
          Already bound to: <strong>{gpButtonExistingCmd}</strong>
        </p>
        <p>Overwrite?</p>
      {/if}
      <div class="dialog-buttons">
        <button class="btn-primary" onclick={confirmGamepadButtonBind}>
          {gpButtonExistingCmd ? "Overwrite" : "Bind"}
        </button>
        <button onclick={cancelGamepadButtonBind}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === "bind-cmd-gp-input"}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to gamepad button</p>
      <label class="dialog-field-label" for="bind-cmd-gp-input">Command:</label>
      <!-- svelte-ignore a11y_autofocus -->
      <input
        id="bind-cmd-gp-input"
        class="dialog-input"
        type="text"
        bind:value={dialogCommand}
        autofocus
      />
      <div class="dialog-buttons">
        <button
          class="btn-primary"
          disabled={!dialogCommand}
          onclick={beginBindCmdGpCapture}>Capture Button…</button
        >
        <button onclick={cancelBindCmdGp}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === "bind-cmd-gp-capture"}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to gamepad button</p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      <p class="dialog-prompt">Press the gamepad button you want to bind…</p>
      <div class="dialog-buttons">
        <button onclick={cancelBindCmdGpCapture}>Cancel</button>
      </div>
    </div>
  </div>
{:else if dialogMode === "bind-cmd-gp-confirm"}
  <div class="dialog-overlay">
    <div class="dialog">
      <p class="dialog-title">Bind command to gamepad button</p>
      <p>Button: <strong>B{gpCapturedButton}</strong></p>
      <p>Command: <strong>{dialogCommand}</strong></p>
      {#if gpButtonExistingCmd}
        <p class="dialog-warn">
          Already bound to: <strong>{gpButtonExistingCmd}</strong>
        </p>
        <p>Overwrite?</p>
      {/if}
      <div class="dialog-buttons">
        <button class="btn-primary" onclick={confirmBindCmdGp}>
          {gpButtonExistingCmd ? "Overwrite" : "Bind"}
        </button>
        <button onclick={cancelBindCmdGpConfirm}>Cancel</button>
      </div>
    </div>
  </div>
{/if}
