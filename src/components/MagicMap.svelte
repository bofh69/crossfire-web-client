<script lang="ts">
  import { onMount } from "svelte";
  import { getCpl } from "../lib/init";
  import { FACE_COLOR_MASK, SHOWMAGIC_FLASH_BIT } from "../lib/protocol";
  import { gameEvents } from "../lib/events";

  /**
   * Magic map color palette matching the old GTK client (old/gtk-v2/src/main.c).
   * Index 0-12 correspond to the root_color[] array used in the C client.
   */
  const MAGIC_MAP_COLORS: string[] = [
    "#000000", // 0 - Black (empty/void)
    "#ffffff", // 1 - White (player flash)
    "#000080", // 2 - Navy
    "#ff0000", // 3 - Red
    "#ffa500", // 4 - Orange
    "#1e90ff", // 5 - DodgerBlue
    "#ee9a00", // 6 - DarkOrange2
    "#2e8b57", // 7 - SeaGreen
    "#8fbc8f", // 8 - DarkSeaGreen
    "#808080", // 9 - Grey50
    "#a0522d", // 10 - Sienna
    "#ffd700", // 11 - Gold
    "#f0e68c", // 12 - Khaki
  ];

  /**
   * What each color mostly shows, from the magicmap colors of the archetypes
   * (the server's color names in brackets).
   */
  const MAGIC_MAP_LEGEND: string[] = [
    "Unknown, dark walls", // 0 black
    "Snow, metal", // 1 white
    "Rivers, deep water", // 2 blue
    "Lava, fire", // 3 red
    "Doors, items", // 4 orange
    "Lakes, shallow water", // 5 light_blue
    "Castle walls, moats", // 6 dark_orange
    "Grass, woods", // 7 green
    "Fields", // 8 light_green
    "Stone walls, floors, roads", // 9 grey
    "Wooden walls, buildings", // 10 brown
    "Lights, runes", // 11 yellow
    "People, containers", // 12 khaki
  ];

  let canvas: HTMLCanvasElement | undefined = $state();
  let drawVersion = $state(0);
  /** Color indexes used in the shown map, for the legend. */
  let usedColors = $state<number[]>([]);

  /** Whether a requestAnimationFrame callback is already pending. */
  let rafPending = false;

  function scheduleRedraw() {
    if (rafPending) return;
    rafPending = true;
    requestAnimationFrame(() => {
      rafPending = false;
      drawVersion++;
    });
  }

  function show() {
    scheduleRedraw();
  }

  function flashPlayerPos() {
    const cpl = getCpl();
    if (!cpl || !cpl.showmagic) return;
    scheduleRedraw();
  }

  function hide() {
    const cpl = getCpl();
    if (cpl) cpl.showmagic = 0;
  }

  onMount(() => {
    const cleanups = [
      gameEvents.on("magicMap", show),
      gameEvents.on("tick", flashPlayerPos),
    ];
    return () => {
      for (const unsub of cleanups) unsub();
    };
  });

  $effect(() => {
    void drawVersion;
    if (!canvas) return;
    drawMagicMap(canvas);
  });

  function drawMagicMap(c: HTMLCanvasElement) {
    const cpl = getCpl();
    if (!cpl || !cpl.magicmap || cpl.mmapx === 0 || cpl.mmapy === 0) return;

    const ctx = c.getContext("2d");
    if (!ctx) return;

    const parentW = c.parentElement?.clientWidth ?? 640;
    const parentH = c.parentElement?.clientHeight ?? 640;

    // Calculate tile size to fit the magic map within the canvas.
    // Keep tiles square by using the smaller dimension.
    let tileSize = Math.min(
      Math.floor(parentW / cpl.mmapx),
      Math.floor(parentH / cpl.mmapy),
    );
    if (tileSize < 1) tileSize = 1;

    const canvasW = tileSize * cpl.mmapx;
    const canvasH = tileSize * cpl.mmapy;
    if (c.width !== canvasW) c.width = canvasW;
    if (c.height !== canvasH) c.height = canvasH;

    // Clear the canvas with black.
    ctx.fillStyle = "#000000";
    ctx.fillRect(0, 0, canvasW, canvasH);

    // Draw each tile as a colored square.
    const used = new Set<number>();
    for (let y = 0; y < cpl.mmapy; y++) {
      for (let x = 0; x < cpl.mmapx; x++) {
        const val = cpl.magicmap[y * cpl.mmapx + x]!;
        const colorIdx = val & FACE_COLOR_MASK;
        used.add(colorIdx);
        const color = MAGIC_MAP_COLORS[colorIdx] ?? MAGIC_MAP_COLORS[0]!;
        ctx.fillStyle = color;
        ctx.fillRect(tileSize * x, tileSize * y, tileSize, tileSize);
      }
    }

    // The legend lists the colors in use; black (unknown) last.
    const nextUsed = [...used]
      .filter((i) => i > 0 && i < MAGIC_MAP_LEGEND.length)
      .sort((a, b) => a - b);
    if (used.has(0)) nextUsed.push(0);
    if (nextUsed.join() !== usedColors.join()) usedColors = nextUsed;

    // Flash the player position: alternate between Black and White each tick.
    if (cpl.showmagic) {
      const flashColor =
        cpl.showmagic & SHOWMAGIC_FLASH_BIT
          ? MAGIC_MAP_COLORS[0]! // Black
          : MAGIC_MAP_COLORS[1]!; // White
      ctx.fillStyle = flashColor;
      ctx.fillRect(
        tileSize * cpl.pmapx,
        tileSize * cpl.pmapy,
        tileSize,
        tileSize,
      );
    }
  }
</script>

<div class="magic-map-overlay" role="img" aria-label="Magic map">
  <div class="magic-map-container">
    <div class="magic-map-header">
      <span>Magic Map</span>
      <button
        class="close-btn"
        onclick={() => hide()}
        aria-label="Close magic map"
      >
        &times;
      </button>
    </div>
    <div class="magic-map-canvas-wrap">
      <canvas bind:this={canvas} width={640} height={640}></canvas>
    </div>
    <ul class="magic-map-legend" aria-label="Map colors">
      <li>
        <span class="swatch you" aria-hidden="true"></span>You (flashing)
      </li>
      {#each usedColors as colorIdx (colorIdx)}
        <li>
          <span
            class="swatch"
            style:background={MAGIC_MAP_COLORS[colorIdx]}
            aria-hidden="true"
          ></span>{MAGIC_MAP_LEGEND[colorIdx]}
        </li>
      {/each}
    </ul>
  </div>
</div>

<style>
  .magic-map-overlay {
    position: absolute;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    background: rgba(0, 0, 0, 0.7);
    z-index: 50;
  }

  .magic-map-container {
    display: flex;
    flex-direction: column;
    max-width: 95%;
    max-height: 95%;
    background: #1e1e1e;
    border: 1px solid #7a6a4a;
    border-radius: 6px;
    overflow: hidden;
  }

  .magic-map-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0.3rem 0.6rem;
    background: #252525;
    border-bottom: 1px solid #333;
    color: #c0b090;
    font-size: 0.85rem;
  }

  .close-btn {
    background: none;
    border: none;
    color: #c0b090;
    font-size: 1.2rem;
    cursor: pointer;
    padding: 0 0.3rem;
    line-height: 1;
  }

  .close-btn:hover {
    color: #fff;
  }

  .magic-map-canvas-wrap {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 4px;
  }

  canvas {
    image-rendering: pixelated;
  }

  .magic-map-legend {
    display: flex;
    flex-wrap: wrap;
    gap: 0.2rem 0.8rem;
    margin: 0;
    padding: 0.3rem 0.6rem 0.4rem;
    list-style: none;
    border-top: 1px solid #333;
    color: #c0b090;
    font-size: 0.75rem;
  }

  .magic-map-legend li {
    display: flex;
    align-items: center;
    gap: 0.3rem;
  }

  .swatch {
    width: 0.8rem;
    height: 0.8rem;
    border: 1px solid #555;
    flex-shrink: 0;
  }

  .swatch.you {
    background: #ffffff;
    animation: you-flash 1s steps(2, jump-none) infinite;
  }

  @keyframes you-flash {
    from {
      background: #ffffff;
    }
    to {
      background: #000000;
    }
  }

  @media (prefers-reduced-motion: reduce) {
    .swatch.you {
      animation: none;
    }
  }
</style>
