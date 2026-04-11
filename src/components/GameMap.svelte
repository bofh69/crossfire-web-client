<script lang="ts">
  import { onMount } from 'svelte';
  import { mapdata_cell, getViewSize, getPlayerPosition } from '../lib/mapdata';
  import { getFaceUrl } from '../lib/image';
  import { lookAt } from '../lib/player';
  import { MapCellState, MAXLAYERS, Map2Label, CONFIG_MAPWIDTH, CONFIG_MAPHEIGHT } from '../lib/protocol';
  import { clientMapsize } from '../lib/client';
  import { wantConfig } from '../lib/init';
  import { gameEvents } from '../lib/events';

  const TILE_SIZE = 32;
  const BASE_FONT_SIZE = 10;
  const LABEL_PAD = 3;
  /**
   * Minimum number of tiles that must be visible in each dimension.
   * computeScale() only increases the scale when at least this many tiles
   * would still fit at the next step, so that scale-up only happens on large
   * displays where the extra pixels are genuinely useful.
   */
  const MIN_TILES = 15;

  /**
   * Return the largest integer scale factor such that at least MIN_TILES tiles
   * still fit in both container dimensions.  Scale is always ≥ 1, so tiles are
   * always drawn at 32×32, 64×64, 96×96, … (exact multiples of TILE_SIZE).
   * This avoids the sub-pixel interpolation that makes pixel art look blurry.
   */
  function computeScale(cw: number, ch: number): number {
    if (cw <= 0 || ch <= 0) return 1;
    let scale = 1;
    while (
      Math.floor(cw / (TILE_SIZE * (scale + 1))) >= MIN_TILES &&
      Math.floor(ch / (TILE_SIZE * (scale + 1))) >= MIN_TILES
    ) {
      scale++;
    }
    return scale;
  }

  let canvas: HTMLCanvasElement | undefined = $state();
  let mapVersion = $state(0);
  let containerW = $state(0);
  let containerH = $state(0);

  /** Effective tile size used in the last draw — kept for click-to-tile mapping. */
  let currentTileSize = TILE_SIZE;

  /**
   * Track the last tile-count dimensions sent to the server so we only send a
   * new `setup mapsize` command when the desired dimensions actually change.
   * Using -1 as sentinel ensures the first valid dimensions are always sent.
   */
  let lastRequestedW = -1;
  let lastRequestedH = -1;

  /**
   * Whenever the container is resized, recompute the ideal tile count and
   * notify the server.  Using integer scale multiples keeps tiles at clean
   * pixel boundaries (32, 64, 96, …) and the server fills the view with more
   * or fewer tiles instead.
   *
   * We also keep wantConfig up to date so that reconnects negotiate the same
   * dimensions without needing to wait for another resize event.
   */
  $effect(() => {
    if (containerW <= 0 || containerH <= 0) return;
    const scale = computeScale(containerW, containerH);
    const tileSize = TILE_SIZE * scale;
    const desiredW = Math.floor(containerW / tileSize);
    const desiredH = Math.floor(containerH / tileSize);
    if (desiredW === lastRequestedW && desiredH === lastRequestedH) return;
    lastRequestedW = desiredW;
    lastRequestedH = desiredH;
    wantConfig[CONFIG_MAPWIDTH] = desiredW;
    wantConfig[CONFIG_MAPHEIGHT] = desiredH;
    clientMapsize(desiredW, desiredH);
  });

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

  onMount(() => {
    const cleanups = [
      gameEvents.on('mapUpdate', () => scheduleRedraw()),
      gameEvents.on('newMap', () => scheduleRedraw()),
      gameEvents.on('tick', () => scheduleRedraw()),
    ];
    return () => { for (const unsub of cleanups) unsub(); };
  });

  $effect(() => {
    // Subscribe to mapVersion and container dimensions to trigger redraws.
    void mapVersion;
    void containerW;
    void containerH;
    if (!canvas) return;
    drawMap(canvas);
  });

  function drawMap(c: HTMLCanvasElement) {
    const t0 = performance.now();
    performance.mark('drawMap-start');
    const ctx = c.getContext('2d');
    if (!ctx) return;

    // Disable image smoothing so pixel art tiles remain crisp at any integer
    // scale (2×, 3×, …).  Sub-pixel interpolation is what made the previous
    // fractional-scaling approach look blurry.
    ctx.imageSmoothingEnabled = false;

    // Use the view dimensions (e.g. 20×20) instead of the full fog map.
    const view = getViewSize();
    const vw = view.width || 1;
    const vh = view.height || 1;

    // The player position gives the top-left of the view on the fog map.
    const plPos = getPlayerPosition();

    // Use the largest integer scale factor that still fits MIN_TILES tiles in
    // each dimension.  This keeps tiles at clean multiples of TILE_SIZE
    // (32 → 64 → 96 → …) so pixel art never suffers sub-pixel blurring.
    // The server adjusts how many tiles it sends (via clientMapsize) to fill
    // the container; any leftover space shows as a black border.
    const scale = computeScale(containerW, containerH);
    const tileSize = TILE_SIZE * scale;
    currentTileSize = tileSize;

    const canvasW = vw * tileSize;
    const canvasH = vh * tileSize;
    if (c.width !== canvasW) c.width = canvasW;
    if (c.height !== canvasH) c.height = canvasH;

    ctx.fillStyle = '#111';
    ctx.fillRect(0, 0, canvasW, canvasH);

    let tilesDrawn = 0;
    let imagesDrawn = 0;
    let placeholders = 0;
    let loadsStarted = 0;

    // Iterate layer by layer (matching the old C client's map_draw_layer approach).
    // This ensures correct z-ordering: all tiles for layer N are fully drawn before
    // any tile at layer N+1, so large multi-tile images never overdraw objects on a
    // higher layer that happen to sit on an earlier tile in scan order.

    // Pass 1: fog-of-war background and per-tile setup (collect cell info).
    for (let vy = 0; vy < vh; vy++) {
      for (let vx = 0; vx < vw; vx++) {
        const ax = plPos.x + vx;
        const ay = plPos.y + vy;
        const cell = mapdata_cell(ax, ay);
        if (cell.state === MapCellState.Empty) continue;
        tilesDrawn++;
        const px = vx * tileSize;
        const py = vy * tileSize;
        if (cell.state === MapCellState.Fog) {
          ctx.fillStyle = '#1a1a1a';
          ctx.fillRect(px, py, tileSize, tileSize);
        }
      }
    }

    // Pass 2: draw all tiles for each layer in order (layer 0 first, then 1, …).
    // Images are scaled by the integer scale factor (naturalSize × scale), keeping
    // pixel art crisp.  imageSmoothingEnabled=false (set above) ensures the
    // canvas does not interpolate when drawing at non-1× sizes.
    const imgScale = scale;
    for (let layer = 0; layer < MAXLAYERS; layer++) {
      for (let vy = 0; vy < vh; vy++) {
        for (let vx = 0; vx < vw; vx++) {
          const ax = plPos.x + vx;
          const ay = plPos.y + vy;
          const cell = mapdata_cell(ax, ay);
          if (cell.state === MapCellState.Empty) continue;

          const head = cell.heads[layer]!;
          const tail = cell.tails[layer]!;

          // Tail cell: skip — the head cell draws the full multi-tile image.
          if (head.face === 0 && tail.face !== 0) continue;
          if (head.face === 0) continue;

          const px = vx * tileSize;
          const py = vy * tileSize;

          const url = getFaceUrl(head.face);
          if (url) {
            const img = imageCache.get(url);
            if (img) {
              // Scale the image by the integer scale factor so it fills the
              // larger tile exactly.  The formula aligns the image's
              // bottom-right corner with the head tile's bottom-right corner,
              // matching the C client's convention.
              const drawW = img.naturalWidth * imgScale;
              const drawH = img.naturalHeight * imgScale;
              const drawX = px + tileSize - drawW;
              const drawY = py + tileSize - drawH;
              ctx.drawImage(img, drawX, drawY, drawW, drawH);
              imagesDrawn++;
            } else {
              if (!loadingUrls.has(url) && !failedUrls.has(url)) loadsStarted++;
              loadImage(url);
              ctx.fillStyle = layer === 0 ? '#222' : '#333';
              ctx.fillRect(px + 1, py + 1, tileSize - 2, tileSize - 2);
              placeholders++;
            }
          } else {
            ctx.fillStyle = layer === 0 ? '#222' : '#333';
            ctx.fillRect(px + 1, py + 1, tileSize - 2, tileSize - 2);
            placeholders++;
          }
        }
      }
    }

    // Pass 3: darkness overlay (applied on top of all layers).
    for (let vy = 0; vy < vh; vy++) {
      for (let vx = 0; vx < vw; vx++) {
        const ax = plPos.x + vx;
        const ay = plPos.y + vy;
        const cell = mapdata_cell(ax, ay);
        if (cell.state === MapCellState.Empty) continue;

        let alpha = 0;
        if (cell.state === MapCellState.Visible) {
          if (cell.darkness > 0) {
            alpha = Math.min(cell.darkness / 255, 0.8);
          }
        } else if (cell.state === MapCellState.Fog) {
          // Dim out-of-sight tiles to indicate they're no longer visible,
          // matching the old client which added +0.2 opacity for fog-of-war cells.
          alpha = Math.min(cell.darkness / 255 + 0.2, 0.8);
        }

        if (alpha > 0) {
          ctx.fillStyle = `rgba(0, 0, 0, ${alpha})`;
          ctx.fillRect(vx * tileSize, vy * tileSize, tileSize, tileSize);
        }
      }
    }

    // Pass 4: draw labels on top of everything (matching old C client's map_draw_labels).
    const fontSize = Math.round(BASE_FONT_SIZE * scale);
    ctx.font = `${fontSize}px sans-serif`;
    // The player is always centred in the view; skip their own name label there.
    const playerVX = Math.floor(vw / 2);
    const playerVY = Math.floor(vh / 2);
    for (let vy = 0; vy < vh; vy++) {
      for (let vx = 0; vx < vw; vx++) {
        const ax = plPos.x + vx;
        const ay = plPos.y + vy;
        const cell = mapdata_cell(ax, ay);
        if (cell.state !== MapCellState.Visible || cell.labels.length === 0) continue;

        const isPlayerTile = vx === playerVX && vy === playerVY;
        const px = vx * tileSize;
        const py = vy * tileSize;
        // The first label is drawn above the tile (its bottom edge at py).
        // Additional labels stack downward from the tile top (py), over the tile.
        let aboveLabelBottomY = py;
        let belowLabelTopY = py;
        let labelIndex = 0;

        for (const lbl of cell.labels) {
          // Don't show the player's own name above their character.
          if (isPlayerTile && lbl.subtype === Map2Label.Player) continue;
          const metrics = ctx.measureText(lbl.label);
          const textW = metrics.width;
          const textH = (metrics.actualBoundingBoxAscent ?? 8) + (metrics.actualBoundingBoxDescent ?? 2);
          const lineH = textH + 2 * LABEL_PAD;

          let labelTopY: number;
          if (labelIndex === 0) {
            // First label: place above the tile, bottom edge at tile top.
            labelTopY = aboveLabelBottomY - lineH;
            aboveLabelBottomY = labelTopY;
          } else {
            // Subsequent labels: stack downward from the tile top, over the tile.
            labelTopY = belowLabelTopY;
            belowLabelTopY += lineH;
          }
          labelIndex++;

          // Center horizontally within the tile.
          const offX = tileSize / 2 - textW / 2;
          const bx = px + offX - LABEL_PAD;

          // Semi-transparent grey background.
          ctx.fillStyle = 'rgba(77, 77, 77, 0.5)';
          ctx.fillRect(bx, labelTopY, textW + 2 * LABEL_PAD, lineH);

          // Text color based on label subtype.
          ctx.strokeStyle = 'black';
          if (lbl.subtype === Map2Label.DM) {
            ctx.fillStyle = '#ff0000';
          } else if (lbl.subtype === Map2Label.PlayerParty) {
            ctx.fillStyle = 'rgb(177, 225, 255)';
          } else {
            ctx.fillStyle = '#fff'; // Bingo on color types!
          }
          const textBaselineY = labelTopY + LABEL_PAD + textH;
          ctx.strokeText(lbl.label, px + offX, textBaselineY);
          ctx.fillText(lbl.label, px + offX, textBaselineY);
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
    const tileX = Math.floor((e.clientX - rect.left) / currentTileSize);
    const tileY = Math.floor((e.clientY - rect.top) / currentTileSize);
    const view = getViewSize();
    // Player is always at the centre of the view.
    const centerX = Math.floor(view.width / 2);
    const centerY = Math.floor(view.height / 2);
    const dx = tileX - centerX;
    const dy = tileY - centerY;
    lookAt(dx, dy);
  }
</script>

<div class="game-map" bind:clientWidth={containerW} bind:clientHeight={containerH}>
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
