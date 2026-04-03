<script lang="ts">
  import { mapdata_cell, getViewSize, getPlayerPosition } from '../lib/mapdata';
  import { getFaceUrl } from '../lib/image';
  import { lookAt } from '../lib/player';
  import { MapCellState, MAXLAYERS } from '../lib/protocol';

  const TILE_SIZE = 32;

  let canvas: HTMLCanvasElement | undefined = $state();
  let mapVersion = $state(0);

  /** Whether a requestAnimationFrame callback is already pending. */
  let rafPending = false;
  /** Number of redraw requests coalesced into the current rAF frame. */
  let pendingRedrawCount = 0;
  /** Running count of draw calls for stats logging. */
  let drawCount = 0;
  /** Timestamp of the last stats log. */
  let lastStatsTime = performance.now();

  function scheduleRedraw() {
    pendingRedrawCount++;
    if (rafPending) return;
    rafPending = true;
    requestAnimationFrame(() => {
      rafPending = false;
      if (pendingRedrawCount > 1) {
        console.debug(`[perf:map] coalesced ${pendingRedrawCount} redraw requests into 1 frame`);
      }
      pendingRedrawCount = 0;
      mapVersion++;
    });
  }

  export function redrawMap() {
    scheduleRedraw();
  }

  $effect(() => {
    // Subscribe to mapVersion to trigger redraws
    void mapVersion;
    if (!canvas) return;
    drawMap(canvas);
  });

  function drawMap(c: HTMLCanvasElement) {
    const t0 = performance.now();
    performance.mark('drawMap-start');
    const ctx = c.getContext('2d');
    if (!ctx) return;

    // Use the view dimensions (e.g. 20×20) instead of the full fog map.
    const view = getViewSize();
    const vw = view.width || 1;
    const vh = view.height || 1;

    // The player position gives the top-left of the view on the fog map.
    const plPos = getPlayerPosition();

    const canvasW = vw * TILE_SIZE;
    const canvasH = vh * TILE_SIZE;
    if (c.width !== canvasW) c.width = canvasW;
    if (c.height !== canvasH) c.height = canvasH;

    ctx.fillStyle = '#111';
    ctx.fillRect(0, 0, canvasW, canvasH);

    let tilesDrawn = 0;
    let imagesDrawn = 0;
    let placeholders = 0;
    let loadsStarted = 0;

    // Iterate over the view-relative coordinates.
    // The player is at the centre: (vw/2, vh/2).
    for (let vy = 0; vy < vh; vy++) {
      for (let vx = 0; vx < vw; vx++) {
        // Absolute coordinates on the fog map.
        const ax = plPos.x + vx;
        const ay = plPos.y + vy;

        const cell = mapdata_cell(ax, ay);
        if (cell.state === MapCellState.Empty) continue;

        tilesDrawn++;
        const px = vx * TILE_SIZE;
        const py = vy * TILE_SIZE;

        // Draw fog as slightly lighter
        if (cell.state === MapCellState.Fog) {
          ctx.fillStyle = '#1a1a1a';
          ctx.fillRect(px, py, TILE_SIZE, TILE_SIZE);
        }

        // Draw layers bottom to top
        for (let layer = 0; layer < MAXLAYERS; layer++) {
          const head = cell.heads[layer];
          const tail = cell.tails[layer];

          // Tail cell: skip — the head cell draws the full multi-tile image.
          if (head.face === 0 && tail.face !== 0) continue;
          if (head.face === 0) continue;

          const url = getFaceUrl(head.face);
          if (url) {
            const img = imageCache.get(url);
            if (img) {
              // Use the image's natural pixel dimensions so multi-tile images
              // are drawn at full size.  The formula aligns the image's
              // bottom-right corner with the head tile's bottom-right corner,
              // matching the C client's convention.
              const drawW = img.naturalWidth;
              const drawH = img.naturalHeight;
              const drawX = px + TILE_SIZE - drawW;
              const drawY = py + TILE_SIZE - drawH;
              ctx.drawImage(img, drawX, drawY, drawW, drawH);
              imagesDrawn++;
            } else {
              if (!loadingUrls.has(url) && !failedUrls.has(url)) loadsStarted++;
              loadImage(url);
              ctx.fillStyle = layer === 0 ? '#222' : '#333';
              ctx.fillRect(px + 1, py + 1, TILE_SIZE - 2, TILE_SIZE - 2);
              placeholders++;
            }
          } else {
            ctx.fillStyle = layer === 0 ? '#222' : '#333';
            ctx.fillRect(px + 1, py + 1, TILE_SIZE - 2, TILE_SIZE - 2);
            placeholders++;
          }
        }

        // Apply darkness overlay
        if (cell.darkness > 0 && cell.state === MapCellState.Visible) {
          const alpha = Math.min(cell.darkness / 255, 0.8);
          ctx.fillStyle = `rgba(0, 0, 0, ${alpha})`;
          ctx.fillRect(px, py, TILE_SIZE, TILE_SIZE);
        }
      }
    }

    performance.mark('drawMap-end');
    performance.measure('drawMap', 'drawMap-start', 'drawMap-end');
    const elapsed = performance.now() - t0;
    drawCount++;

    if (elapsed > 5) {
      console.warn(`[perf:map] drawMap took ${elapsed.toFixed(1)}ms — tiles:${tilesDrawn} imgs:${imagesDrawn} placeholders:${placeholders} loadsStarted:${loadsStarted}`);
    }

    const now = performance.now();
    if (now - lastStatsTime > 5000) {
      const dt = (now - lastStatsTime) / 1000;
      console.info(`[perf:map] ${drawCount} draws in ${dt.toFixed(1)}s (${(drawCount / dt).toFixed(1)} draws/s), imageCache:${imageCache.size} loading:${loadingUrls.size} failed:${failedUrls.size}`);
      drawCount = 0;
      lastStatsTime = now;
    }
  }

  const imageCache = new Map<string, HTMLImageElement>();
  /** URLs for which a load is already in flight. */
  const loadingUrls = new Set<string>();
  /** URLs that permanently failed to load (don't retry). */
  const failedUrls = new Set<string>();

  function loadImage(url: string) {
    if (imageCache.has(url) || loadingUrls.has(url) || failedUrls.has(url)) return;
    loadingUrls.add(url);
    const img = new Image();
    const loadStart = performance.now();
    img.onload = () => {
      const elapsed = performance.now() - loadStart;
      loadingUrls.delete(url);
      imageCache.set(url, img);
      if (elapsed > 100) {
        console.debug(`[perf:map] image load took ${elapsed.toFixed(0)}ms: ${url}`);
      }
      scheduleRedraw();
    };
    img.onerror = () => {
      loadingUrls.delete(url);
      failedUrls.add(url);
      console.warn(`[perf:map] image load failed: ${url}`);
    };
    img.src = url;
  }

  function handleClick(e: MouseEvent) {
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const tileX = Math.floor((e.clientX - rect.left) / TILE_SIZE);
    const tileY = Math.floor((e.clientY - rect.top) / TILE_SIZE);
    const view = getViewSize();
    // Player is always at the centre of the view.
    const centerX = Math.floor(view.width / 2);
    const centerY = Math.floor(view.height / 2);
    const dx = tileX - centerX;
    const dy = tileY - centerY;
    lookAt(dx, dy);
  }
</script>

<div class="game-map">
  <canvas
    bind:this={canvas}
    onclick={handleClick}
    width={640}
    height={640}
  ></canvas>
</div>

<style>
  .game-map {
    display: flex;
    align-items: center;
    justify-content: center;
    background: #111;
    overflow: hidden;
    width: 100%;
    height: 100%;
  }

  canvas {
    image-rendering: pixelated;
    cursor: crosshair;
  }
</style>
