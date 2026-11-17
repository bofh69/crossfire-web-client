/**
 * panel_split.svelte.ts — Size of a game-layout panel that the user can
 * resize by dragging a handle, kept as a fraction of the layout in the config.
 */

import { loadConfig, saveConfig } from "./storage";

/** How a {@link PanelSplit} is measured, limited and stored. */
export interface PanelSplitOptions {
  /** "x" for a panel resized horizontally, "y" for one resized vertically. */
  axis: "x" | "y";
  /** Config key of the size, stored as a fraction of the layout size. */
  configKey: string;
  /** Smallest size, in pixels. */
  min: number;
  /** Largest size, as a fraction of the layout size. */
  maxFrac: number;
  /** CSS selector of the panel, inside the layout element. */
  panelSelector: string;
}

/**
 * The size of a panel at the right or bottom edge of the layout.  Dragging
 * the handle left or up makes the panel bigger.
 */
export class PanelSplit {
  /** The panel size in pixels, or null to use the default layout. */
  size = $state<number | null>(null);
  /** Whether the handle is being dragged. */
  dragging = $state(false);

  readonly #options: PanelSplitOptions;

  constructor(options: PanelSplitOptions) {
    this.#options = options;
    const { axis, configKey, min, maxFrac } = options;
    const frac = loadConfig<number | null>(configKey, null);
    if (frac !== null) {
      const viewportSize =
        axis === "x" ? window.innerWidth : window.innerHeight;
      this.size = Math.max(
        min,
        Math.min(
          Math.floor(maxFrac * viewportSize),
          Math.round(frac * viewportSize),
        ),
      );
    }
  }

  /**
   * Start dragging the handle, for the `mousedown` event `e`.  The size is
   * saved when the mouse button is released.
   */
  startDrag(e: MouseEvent, layout: HTMLElement): void {
    e.preventDefault();
    const { axis, configKey, min, maxFrac, panelSelector } = this.#options;
    const layoutSize = (): number => {
      const rect = layout.getBoundingClientRect();
      return axis === "x" ? rect.width : rect.height;
    };
    const mousePos = (me: MouseEvent): number =>
      axis === "x" ? me.clientX : me.clientY;

    const panelRect = layout
      .querySelector<HTMLElement>(panelSelector)!
      .getBoundingClientRect();
    const startSize = axis === "x" ? panelRect.width : panelRect.height;
    const startPos = mousePos(e);
    const max = Math.floor(layoutSize() * maxFrac);

    this.dragging = true;
    document.body.style.cursor = axis === "x" ? "col-resize" : "row-resize";
    document.body.style.userSelect = "none";

    const onMove = (me: MouseEvent): void => {
      const delta = startPos - mousePos(me);
      this.size = Math.max(min, Math.min(max, Math.round(startSize + delta)));
    };

    const onUp = (): void => {
      this.dragging = false;
      document.body.style.cursor = "";
      document.body.style.userSelect = "";
      if (this.size !== null) {
        saveConfig(configKey, this.size / layoutSize());
      }
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    };

    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup", onUp);
  }
}
