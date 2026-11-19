<script lang="ts">
  /**
   * GamepadButtonGlyph — a small picture of a gamepad button, drawn in the
   * style of the controller family (Xbox, PlayStation or Nintendo).
   *
   * Face buttons, sticks and the d-pad are drawn as SVG; other buttons are
   * shown as a labelled key cap.
   */
  import {
    buttonIndex,
    buttonLabel,
    STANDARD_BUTTON_NAMES,
    type ButtonId,
    type ControllerFamily,
  } from "../lib/gamepad_buttons";

  interface Props {
    /** The button to draw. */
    button: ButtonId;
    /** The controller family whose style to use. */
    family: ControllerFamily;
    /** Also show the button's name next to a drawn glyph. */
    withLabel?: boolean;
  }
  let { button, family, withLabel = false }: Props = $props();

  /** Symbol colours of the face buttons (bottom, right, left, top). */
  const FACE_COLORS: Record<ControllerFamily, readonly string[]> = {
    nintendo: ["#eeeeee", "#eeeeee", "#eeeeee", "#eeeeee"],
    playstation: ["#7ca7e8", "#ef6b6b", "#e58fc8", "#4fc9b0"],
    xbox: ["#6cc04a", "#e8483b", "#3b8de8", "#f2c230"],
  };

  /** Which arm of the d-pad each d-pad button is, as a rotation. */
  const DPAD_ROTATION: Record<string, number> = {
    dpup: 0,
    dpright: 90,
    dpdown: 180,
    dpleft: 270,
  };

  let label = $derived(buttonLabel(button, family));
  /** Standard-layout index, or -1 for a raw button. */
  let index = $derived(
    STANDARD_BUTTON_NAMES.includes(button) ? buttonIndex(button) : -1,
  );
  let kind = $derived(
    index >= 0 && index <= 3
      ? "face"
      : index === 10 || index === 11
        ? "stick"
        : index >= 12 && index <= 15
          ? "dpad"
          : "cap",
  );
  let color = $derived(FACE_COLORS[family][index] ?? "#eeeeee");
</script>

<span class="glyph" title={label} aria-label={label} role="img">
  {#if kind === "face"}
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <circle cx="12" cy="12" r="11" class="body" />
      {#if family === "playstation"}
        <g
          fill="none"
          stroke={color}
          stroke-width="2"
          stroke-linecap="round"
          stroke-linejoin="round"
        >
          {#if index === 0}
            <path d="M7.5 7.5 16.5 16.5 M16.5 7.5 7.5 16.5" />
          {:else if index === 1}
            <circle cx="12" cy="12" r="5" />
          {:else if index === 2}
            <rect x="7.5" y="7.5" width="9" height="9" />
          {:else}
            <path d="M12 6.5 17 15.5H7Z" />
          {/if}
        </g>
      {:else}
        <text x="12" y="16.5" fill={color}>{label}</text>
      {/if}
    </svg>
  {:else if kind === "stick"}
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <circle cx="12" cy="12" r="11" class="body" />
      <circle cx="12" cy="12" r="7.5" class="rim" />
      <text x="12" y="15.5" class="small">{index === 10 ? "L" : "R"}</text>
    </svg>
  {:else if kind === "dpad"}
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path class="body" d="M9 1.5h6v7.5h7.5v6H15v7.5H9V15H1.5V9H9Z" />
      <path
        class="active"
        d="M9.5 2h5v6.5L12 11 9.5 8.5Z"
        transform={`rotate(${DPAD_ROTATION[button] ?? 0} 12 12)`}
      />
    </svg>
  {:else}
    <span class="cap">{label}</span>
  {/if}
  {#if withLabel && kind !== "cap"}
    <span class="label">{label}</span>
  {/if}
</span>

<style>
  .glyph {
    display: inline-flex;
    align-items: center;
    gap: 0.35em;
    vertical-align: middle;
    line-height: 1;
  }

  svg {
    width: 1.6em;
    height: 1.6em;
  }

  .body {
    fill: #2a2a2a;
    stroke: #777;
    stroke-width: 1;
  }

  .rim {
    fill: none;
    stroke: #999;
    stroke-width: 1.2;
  }

  .active {
    fill: #7799ee;
  }

  text {
    font:
      bold 11px system-ui,
      sans-serif;
    text-anchor: middle;
  }

  text.small {
    fill: #ccc;
    font-size: 9px;
  }

  .cap {
    display: inline-block;
    min-width: 1.6em;
    padding: 0.15em 0.4em;
    border: 1px solid #777;
    border-radius: 0.5em;
    background: #2a2a2a;
    color: #ddd;
    font-size: 0.85em;
    font-weight: bold;
    text-align: center;
    white-space: nowrap;
  }
</style>
