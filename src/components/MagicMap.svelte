<script lang="ts">
  import { getCpl } from '../lib/init';
  import { FACE_COLOR_MASK, SHOWMAGIC_FLASH_BIT } from '../lib/protocol';

  /**
   * Magic map color palette matching the old GTK client (old/gtk-v2/src/main.c).
   * Index 0-12 correspond to the root_color[] array used in the C client.
   */
  const MAGIC_MAP_COLORS: string[] = [
    '#000000', // 0 - Black (empty/void)
    '#ffffff', // 1 - White (player flash)
    '#000080', // 2 - Navy
    '#ff0000', // 3 - Red
    '#ffa500', // 4 - Orange
    '#1e90ff', // 5 - DodgerBlue
    '#ee9a00', // 6 - DarkOrange2
    '#2e8b57', // 7 - SeaGreen
    '#8fbc8f', // 8 - DarkSeaGreen
    '#808080', // 9 - Grey50
    '#a0522d', // 10 - Sienna
    '#ffd700', // 11 - Gold
    '#f0e68c', // 12 - Khaki
  ];

  let canvas: HTMLCanvasElement | undefined = $state();
  let drawVersion = $state(0);

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

  export function show() {
    scheduleRedraw();
  }

  export function flashPlayerPos() {
    const cpl = getCpl();
    if (!cpl || !cpl.showmagic) return;
    scheduleRedraw();
  }

  export function hide() {
    const cpl = getCpl();
    if (cpl) cpl.showmagic = 0;
  }

  $effect(() => {
    void drawVersion;
    if (!canvas) return;
    drawMagicMap(canvas);
  });

  function drawMagicMap(c: HTMLCanvasElement) {
    const cpl = getCpl();
    if (!cpl || !cpl.magicmap || cpl.mmapx === 0 || cpl.mmapy === 0) return;

    const ctx = c.getContext('2d');
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
    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, canvasW, canvasH);

    // Draw each tile as a colored square.
    for (let y = 0; y < cpl.mmapy; y++) {
      for (let x = 0; x < cpl.mmapx; x++) {
        const val = cpl.magicmap[y * cpl.mmapx + x];
        const colorIdx = val & FACE_COLOR_MASK;
        const color = MAGIC_MAP_COLORS[colorIdx] ?? MAGIC_MAP_COLORS[0];
        ctx.fillStyle = color;
        ctx.fillRect(tileSize * x, tileSize * y, tileSize, tileSize);
      }
    }

    // Flash the player position: alternate between Black and White each tick.
    if (cpl.showmagic) {
      const flashColor = (cpl.showmagic & SHOWMAGIC_FLASH_BIT)
        ? MAGIC_MAP_COLORS[0]  // Black
        : MAGIC_MAP_COLORS[1]; // White
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
      <button class="close-btn" onclick={() => hide()} aria-label="Close magic map">&times;</button>
    </div>
    <div class="magic-map-canvas-wrap">
      <canvas bind:this={canvas} width={640} height={640}></canvas>
    </div>
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
</style>
