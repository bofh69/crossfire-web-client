<script lang="ts">
  /**
   * GamepadButtonGlyph — a small picture of a gamepad button in the style of
   * the controller family (Xbox, PlayStation, Nintendo, Steam Deck or Steam
   * Controller), using Kenney's Input Prompts (CC0, www.kenney.nl).  Buttons
   * without a picture are shown as a labelled key cap.
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
    /** Also show the button's name next to a picture. */
    withLabel?: boolean;
  }
  let { button, family, withLabel = false }: Props = $props();

  /** URLs of the bundled prompt images, keyed by file name. */
  const PROMPT_URLS: Record<string, string> = Object.fromEntries(
    Object.entries(
      import.meta.glob<string>("../assets/input-prompts/*.svg", {
        eager: true,
        import: "default",
        query: "?url",
      }),
    ).map(([path, url]) => [path.replace(/^.*\/|\.svg$/g, ""), url]),
  );

  /**
   * Prompt image file names per family, indexed like
   * {@link STANDARD_BUTTON_NAMES}; "" means there is no picture.
   */
  const PROMPT_FILES: Record<ControllerFamily, readonly string[]> = {
    nintendo: [
      "switch_button_b",
      "switch_button_a",
      "switch_button_y",
      "switch_button_x",
      "switch_button_l",
      "switch_button_r",
      "switch_button_zl",
      "switch_button_zr",
      "switch_button_minus",
      "switch_button_plus",
      "switch_stick_l_press",
      "switch_stick_r_press",
      "switch_dpad_up",
      "switch_dpad_down",
      "switch_dpad_left",
      "switch_dpad_right",
      "switch_button_home",
    ],
    playstation3: [
      "playstation_button_color_cross",
      "playstation_button_color_circle",
      "playstation_button_color_square",
      "playstation_button_color_triangle",
      "playstation_trigger_l1",
      "playstation_trigger_r1",
      "playstation_trigger_l2",
      "playstation_trigger_r2",
      "playstation3_button_select",
      "playstation3_button_start",
      "playstation_button_l3",
      "playstation_button_r3",
      "playstation_dpad_up",
      "playstation_dpad_down",
      "playstation_dpad_left",
      "playstation_dpad_right",
      "",
    ],
    playstation: [
      "playstation_button_color_cross",
      "playstation_button_color_circle",
      "playstation_button_color_square",
      "playstation_button_color_triangle",
      "playstation_trigger_l1",
      "playstation_trigger_r1",
      "playstation_trigger_l2",
      "playstation_trigger_r2",
      "playstation5_button_create",
      "playstation5_button_options",
      "playstation_button_l3",
      "playstation_button_r3",
      "playstation_dpad_up",
      "playstation_dpad_down",
      "playstation_dpad_left",
      "playstation_dpad_right",
      "",
    ],
    steamcontroller: [
      "steam_button_color_a",
      "steam_button_color_b",
      "steam_button_color_x",
      "steam_button_color_y",
      "steam_lb",
      "steam_rb",
      "steam_lt",
      "steam_rt",
      "steam_button_back_icon",
      "steam_button_start_icon",
      "steam_stick_l_press",
      "steam_button_rp",
      "steam_dpad_up",
      "steam_dpad_down",
      "steam_dpad_left",
      "steam_dpad_right",
      "steam_button_steam",
    ],
    steamdeck: [
      "steamdeck_button_a",
      "steamdeck_button_b",
      "steamdeck_button_x",
      "steamdeck_button_y",
      "steamdeck_button_l1",
      "steamdeck_button_r1",
      "steamdeck_button_l2",
      "steamdeck_button_r2",
      "steamdeck_button_view",
      "steamdeck_button_options",
      "steamdeck_stick_l_press",
      "steamdeck_stick_r_press",
      "steamdeck_dpad_up",
      "steamdeck_dpad_down",
      "steamdeck_dpad_left",
      "steamdeck_dpad_right",
      "steamdeck_button_guide",
    ],
    xbox: [
      "xbox_button_color_a",
      "xbox_button_color_b",
      "xbox_button_color_x",
      "xbox_button_color_y",
      "xbox_lb",
      "xbox_rb",
      "xbox_lt",
      "xbox_rt",
      "xbox_button_view",
      "xbox_button_menu",
      "xbox_ls",
      "xbox_rs",
      "xbox_dpad_up",
      "xbox_dpad_down",
      "xbox_dpad_left",
      "xbox_dpad_right",
      "xbox_guide",
    ],
  };

  let label = $derived(buttonLabel(button, family));
  let url = $derived(
    STANDARD_BUTTON_NAMES.includes(button)
      ? PROMPT_URLS[PROMPT_FILES[family][buttonIndex(button)] ?? ""]
      : undefined,
  );
</script>

<span class="glyph" title={label}>
  {#if url}
    <img src={url} alt={label} />
    {#if withLabel}
      <span>{label}</span>
    {/if}
  {:else}
    <span class="cap">{label}</span>
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

  img {
    width: 2em;
    height: 2em;
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
