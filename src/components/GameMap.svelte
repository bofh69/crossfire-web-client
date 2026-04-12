<script lang="ts">
  import { onMount } from 'svelte';
  import { mapdata_cell, mapdata_can_smooth, mapdata_contains, getViewSize, getPlayerPosition } from '../lib/mapdata';
  import { getFaceUrl, getSmoothFace } from '../lib/image';
  import { lookAt } from '../lib/player';
  import { set_move_to, moveToX, moveToY } from '../lib/mapdata';
  import { MapCellState, MAXLAYERS, Map2Label, LogLevel } from '../lib/protocol';
  import { clientMapsize } from '../lib/client';
  import { wantConfig, useConfig } from '../lib/init';
  import { gameEvents } from '../lib/events';
  import { LOG } from '../lib/misc';
  import { SLOW_DRAW_THRESHOLD_MS, DRAW_STATS_INTERVAL_MS } from '../lib/constants';
  import { loadConfig, saveConfig } from '../lib/storage';

  const TILE_SIZE = 32;
  const BASE_FONT_SIZE = 10;
  const LABEL_PAD = 3;
  const MIN_SCALE = 1;
  const MAX_SCALE = 8;
  const ZOOM_STORAGE_KEY = 'tileScale';
  /**
   * Minimum number of tiles that must be visible in each dimension.
   * computeScale() only increases the scale when at least this many tiles
   * would still fit at the next step, so that scale-up only happens on large
   * displays where the extra pixels are genuinely useful.
   * Used as a fallback when the user has not chosen an explicit zoom level.
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

  /**
   * User-chosen tile scale (1 = 32 px, 2 = 64 px, …).
   * null means "auto" — computeScale() is used instead.
   * Persisted in localStorage so it survives page reloads.
   */
  let storedScale = $state<number | null>(loadConfig<number | null>(ZOOM_STORAGE_KEY, null));

  /** Effective tile size used in the last draw — kept for click-to-tile mapping. */
  let currentTileSize = TILE_SIZE;
  /** Draw offsets used in the last draw — kept for click-to-tile mapping. */
  let currentDrawOffsetX = 0;
  let currentDrawOffsetY = 0;
  /**
   * Display tile count in the last draw.  The player is always at display tile
   * ⌊currentDisplayW/2⌋, so click handlers use this to compute the dx/dy
   * offset from the player.
   */
  let currentDisplayW = 1;
  let currentDisplayH = 1;

  /**
   * Track the last tile-count dimensions sent to the server so we only send a
   * new `setup mapsize` command when the desired dimensions actually change.
   * Using -1 as sentinel ensures the first valid dimensions are always sent.
   */
  let lastRequestedW = -1;
  let lastRequestedH = -1;

  /**
   * Whenever the container is resized or the zoom level changes, recompute the
   * ideal tile count and notify the server.  We request exactly as many tiles as
   * fit in the display.  If the server caps the confirmed view at a smaller size,
   * the renderer still shows the full displayW × displayH grid — tiles outside
   * the server-confirmed area are drawn as fog of war from the local fog map.
   *
   * We also keep wantConfig up to date so that reconnects negotiate the same
   * dimensions without needing to wait for another resize event.
   */
  $effect(() => {
    if (containerW <= 0 || containerH <= 0) return;
    const scale = storedScale ?? computeScale(containerW, containerH);
    const tileSize = TILE_SIZE * scale;
    const desiredW = Math.ceil(containerW / tileSize);
    const desiredH = Math.ceil(containerH / tileSize);
    if (desiredW === lastRequestedW && desiredH === lastRequestedH) return;
    lastRequestedW = desiredW;
    lastRequestedH = desiredH;
    wantConfig.mapWidth = desiredW;
    wantConfig.mapHeight = desiredH;
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
        LOG(LogLevel.Debug, 'GameMap', `coalesced ${pendingRedrawCount} redraw requests into 1 frame`);
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
      gameEvents.on('zoomIn', () => {
        const current = storedScale ?? computeScale(containerW, containerH);
        storedScale = Math.min(current + 1, MAX_SCALE);
        saveConfig(ZOOM_STORAGE_KEY, storedScale);
        scheduleRedraw();
      }),
      gameEvents.on('zoomOut', () => {
        const current = storedScale ?? computeScale(containerW, containerH);
        storedScale = Math.max(current - 1, MIN_SCALE);
        saveConfig(ZOOM_STORAGE_KEY, storedScale);
        scheduleRedraw();
      }),
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

  // ── Smooth tile rendering ──────────────────────────────────────────────────
  // Neighbour offsets (N, NE, E, SE, S, SW, W, NW)
  const SMOOTH_DX = [0, 1, 1, 1, 0, -1, -1, -1] as const;
  const SMOOTH_DY = [-1, -1, 0, 1, 1, 1, 0, -1] as const;
  // Border bitmask contribution per direction (N=2, E=4, S=8, W=1; diagonals=0)
  const SMOOTH_BWEIGHTS = [2, 0, 4, 0, 8, 0, 1, 0] as const;
  // Corner bitmask contribution per direction (NE=2, SE=4, SW=8, NW=1; cardinals=0)
  const SMOOTH_CWEIGHTS = [0, 2, 0, 4, 0, 8, 0, 1] as const;
  // Which corner bits a cardinal neighbour excludes (e.g. N excludes NW and NE)
  const SMOOTH_BC_EXCLUDE = [1 + 2, 0, 2 + 4, 0, 4 + 8, 0, 8 + 1, 0] as const;

  // Pre-allocated scratch arrays for drawsmooth to avoid per-tile GC pressure.
  const _slevels = new Int32Array(8);
  const _sfaces = new Int32Array(8);
  const _partdone = new Uint8Array(8);

  /**
   * Draw smooth-transition overlays for one tile/layer.
   *
   * Mirrors `drawsmooth()` in old/gtk-v2/src/map.c.  For each of the eight
   * neighbours that has a *higher* smooth level at this layer we collect its
   * smooth-face and level, then draw the appropriate sub-tile from that
   * smooth-face image onto the current tile to blend the edge.
   *
   * The smooth-face image is a 16-tile-wide, 2-tile-tall sprite sheet:
   *   row 0 – border blends (column index = 4-bit bitmask: N=2 E=4 S=8 W=1)
   *   row 1 – corner blends (column index = 4-bit bitmask: NE=2 SE=4 SW=8 NW=1)
   *
   * @param ctx      Canvas 2D context.
   * @param ax, ay   Absolute (virtual-map) coordinates of the tile.
   * @param layer    Layer being drawn.
   * @param px, py   Pixel position of the tile's top-left corner on the canvas.
   * @param tileSize Effective tile size in pixels (TILE_SIZE × integer scale).
   */
  function drawsmooth(
    ctx: CanvasRenderingContext2D,
    ax: number, ay: number,
    layer: number,
    px: number, py: number,
    tileSize: number,
  ): void {
    const cell = mapdata_cell(ax, ay);

    // Smooth only makes sense when there is visible content on some layer ≤ current.
    let hasFace = false;
    for (let i = 0; i <= layer; i++) {
      if (cell.heads[i]!.face !== 0) { hasFace = true; break; }
    }
    if (!hasFace || !mapdata_can_smooth(ax, ay, layer)) return;

    const mySmooth = cell.smooth[layer]!;

    // Collect neighbour smooth data.
    for (let i = 0; i < 8; i++) {
      const emx = ax + SMOOTH_DX[i]!;
      const emy = ay + SMOOTH_DY[i]!;
      if (!mapdata_contains(emx, emy)) {
        _slevels[i] = 0;
        _sfaces[i] = 0;
      } else {
        const ncell = mapdata_cell(emx, emy);
        if (ncell.smooth[layer]! > mySmooth) {
          _slevels[i] = ncell.smooth[layer]!;
          _sfaces[i] = getSmoothFace(ncell.heads[layer]!.face);
        } else {
          _slevels[i] = 0;
          _sfaces[i] = 0;
        }
      }
      _partdone[i] = 0;
    }

    // Draw overlays from the lowest smooth level upward.
    while (true) {
      let lowestIdx = -1;
      for (let i = 0; i < 8; i++) {
        if (_slevels[i]! > 0 && _partdone[i] === 0 &&
            (lowestIdx < 0 || _slevels[i]! < _slevels[lowestIdx]!)) {
          lowestIdx = i;
        }
      }
      if (lowestIdx < 0) break;

      let weight = 0;
      let weightC = 15;
      for (let i = 0; i < 8; i++) {
        if (_slevels[i]! === _slevels[lowestIdx]! && _sfaces[i]! === _sfaces[lowestIdx]!) {
          _partdone[i] = 1;
          weight += SMOOTH_BWEIGHTS[i]!;
          weightC &= ~SMOOTH_BC_EXCLUDE[i]!;
        } else {
          weightC &= ~SMOOTH_CWEIGHTS[i]!;
        }
      }

      const smoothFaceNum = _sfaces[lowestIdx]!;
      if (smoothFaceNum <= 0) continue;

      const smoothUrl = getFaceUrl(smoothFaceNum);
      if (!smoothUrl) continue;

      const smoothImg = imageCache.get(smoothUrl);
      if (!smoothImg) {
        loadImage(smoothUrl);
        continue;
      }

      // Draw border sub-tile (row 0, column = weight bitmask).
      if (weight > 0) {
        ctx.drawImage(smoothImg,
          weight * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE,
          px, py, tileSize, tileSize);
      }
      // Draw corner sub-tile (row 1, column = weightC bitmask).
      if (weightC > 0) {
        ctx.drawImage(smoothImg,
          weightC * TILE_SIZE, TILE_SIZE, TILE_SIZE, TILE_SIZE,
          px, py, tileSize, tileSize);
      }
    }
  }

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
    // each dimension, unless the user has chosen an explicit zoom level.
    // Integer multiples keep pixel art crisp (32 → 64 → 96 → …).
    // The server fills the view with more or fewer tiles (via clientMapsize);
    // we request enough tiles to cover the container so no black border appears.
    const scale = storedScale ?? computeScale(containerW, containerH);
    const tileSize = TILE_SIZE * scale;
    currentTileSize = tileSize;

    // How many tiles fit in the display.  Using ceil ensures we always cover
    // the full container with no black gaps.  The server may have confirmed a
    // smaller view (vw × vh); in that case tiles beyond the server view are
    // drawn from the local fog map and rendered as fog of war.
    const displayW = Math.max(1, Math.ceil(containerW / tileSize));
    const displayH = Math.max(1, Math.ceil(containerH / tileSize));
    currentDisplayW = displayW;
    currentDisplayH = displayH;

    // The player is always at the centre of the server-confirmed view.  To
    // centre the player on the display we start rendering at:
    //   mx_start = plPos.x + ⌊vw/2⌋ − ⌊displayW/2⌋
    // When displayW > vw this is to the left of the server view, so some
    // display tiles fall outside the server view and show as fog.
    // When displayW < vw we crop inward, showing only the central portion.
    const mx_start = plPos.x + Math.floor(vw / 2) - Math.floor(displayW / 2);
    const my_start = plPos.y + Math.floor(vh / 2) - Math.floor(displayH / 2);

    // Canvas always fills the full container.
    const canvasW = Math.max(containerW, 1);
    const canvasH = Math.max(containerH, 1);
    if (c.width !== canvasW) c.width = canvasW;
    if (c.height !== canvasH) c.height = canvasH;

    // displayW * tileSize >= containerW (by the ceil above), so tiles always
    // cover the canvas; no centering offset is needed.
    currentDrawOffsetX = 0;
    currentDrawOffsetY = 0;

    ctx.fillStyle = '#111';
    ctx.fillRect(0, 0, canvasW, canvasH);

    let tilesDrawn = 0;
    let imagesDrawn = 0;
    let placeholders = 0;
    let loadsStarted = 0;

    /** Returns true when (ax, ay) is within the server-confirmed view. */
    const inServerView = (ax: number, ay: number): boolean =>
      ax >= plPos.x && ax < plPos.x + vw &&
      ay >= plPos.y && ay < plPos.y + vh;

    // Iterate layer by layer (matching the old C client's map_draw_layer approach).
    // This ensures correct z-ordering: all tiles for layer N are fully drawn before
    // any tile at layer N+1, so large multi-tile images never overdraw objects on a
    // higher layer that happen to sit on an earlier tile in scan order.

    // Pass 1: fog-of-war background.
    // Tiles outside the server-confirmed view are always shown as fog, even if the
    // local fog map has them in the Empty state (unvisited territory still reads as
    // unknown/fog from the player's perspective).
    for (let vy = 0; vy < displayH; vy++) {
      for (let vx = 0; vx < displayW; vx++) {
        const ax = mx_start + vx;
        const ay = my_start + vy;
        const px = vx * tileSize;
        const py = vy * tileSize;

        // Tile falls outside the local fog map — leave as black background.
        if (!mapdata_contains(ax, ay)) continue;

        if (!inServerView(ax, ay)) {
          // Outside server view: always draw fog background.
          tilesDrawn++;
          ctx.fillStyle = '#1a1a1a';
          ctx.fillRect(px, py, tileSize, tileSize);
          continue;
        }

        const cell = mapdata_cell(ax, ay);
        if (cell.state === MapCellState.Empty) continue;
        tilesDrawn++;
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
    //
    // Tiles outside the server view but inside the fog map may have Fog state
    // with previously received face data; these ARE drawn (with fog overlay in
    // Pass 3) so previously-seen areas remain visible when zoomed out.
    //
    // Bigface (multi-tile) heads outside the server view are mirrored into
    // cells[] by expandSetBigface, so the normal cell loop already finds them.
    // For heads that are outside the *display* canvas (but within cells[]), the
    // tail→head detection below draws them from their off-screen position and
    // lets the canvas clip the invisible portion.
    const imgScale = scale;
    // Reuse a single Set across all layers (cleared per layer) to track
    // off-screen head positions already drawn by the tail-detection path,
    // preventing duplicate rendering when multiple visible tails point to the
    // same head.  String keys avoid integer-arithmetic collision edge-cases.
    const offscreenHeadsDrawn = new Set<string>();
    for (let layer = 0; layer < MAXLAYERS; layer++) {
      offscreenHeadsDrawn.clear();

      for (let vy = 0; vy < displayH; vy++) {
        for (let vx = 0; vx < displayW; vx++) {
          const ax = mx_start + vx;
          const ay = my_start + vy;
          if (!mapdata_contains(ax, ay)) continue;
          const cell = mapdata_cell(ax, ay);
          const head = cell.heads[layer]!;
          const tail = cell.tails[layer]!;

          // Skip cells with no face data at all: Empty cells that have neither
          // a head face nor a tail pointing to an off-screen head.
          if (cell.state === MapCellState.Empty && head.face === 0 && tail.face === 0) continue;

          const px = vx * tileSize;
          const py = vy * tileSize;

          if (head.face !== 0) {
            // This cell is a head.  Draw the face image bottom-right-aligned.
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
          } else if (tail.face !== 0) {
            // This cell is a tail of a multi-tile object whose head is
            // elsewhere.  If the head is within the display the cells loop
            // will draw it when it reaches that cell; skip here to avoid
            // duplicate rendering.  If the head is outside the display
            // (either because displayW < vw or because the object is a
            // bigface) draw the image now at the head's off-screen position
            // and let the canvas clip the invisible portion.
            const headAx = ax + tail.sizeX;
            const headAy = ay + tail.sizeY;
            const headVx = headAx - mx_start;
            const headVy = headAy - my_start;
            if (headVx >= 0 && headVx < displayW && headVy >= 0 && headVy < displayH) {
              // Head is in display range; the cells loop handles it.
            } else {
              const key = `${headAx},${headAy}`;
              if (!offscreenHeadsDrawn.has(key)) {
                offscreenHeadsDrawn.add(key);
                const url = getFaceUrl(tail.face);
                if (url) {
                  const img = imageCache.get(url);
                  if (img) {
                    const drawW = img.naturalWidth * imgScale;
                    const drawH = img.naturalHeight * imgScale;
                    ctx.drawImage(img,
                      headVx * tileSize + tileSize - drawW,
                      headVy * tileSize + tileSize - drawH,
                      drawW, drawH);
                    imagesDrawn++;
                  } else {
                    if (!loadingUrls.has(url) && !failedUrls.has(url)) loadsStarted++;
                    loadImage(url);
                  }
                }
              }
            }
          }

          if (useConfig.smooth) {
            drawsmooth(ctx, ax, ay, layer, px, py, tileSize);
          }
        }
      }
    }

    // Pass 2 cleanup: re-cover Empty tiles so that multi-tile images drawn from
    // an off-screen or Empty-state head position cannot bleed into unexplored
    // territory.  The canvas may have received image pixels in the region of
    // those tiles during Pass 2; painting the background colour on top erases
    // them while leaving properly-visible (non-Empty) tiles intact.
    for (let vy = 0; vy < displayH; vy++) {
      for (let vx = 0; vx < displayW; vx++) {
        const ax = mx_start + vx;
        const ay = my_start + vy;
        if (!mapdata_contains(ax, ay)) continue;
        const cell = mapdata_cell(ax, ay);
        if (cell.state !== MapCellState.Empty) continue;
        // Inside the server view, empty tiles are pure black.
        // Outside the server view, empty tiles use the fog background.
        ctx.fillStyle = inServerView(ax, ay) ? '#111' : '#1a1a1a';
        ctx.fillRect(vx * tileSize, vy * tileSize, tileSize, tileSize);
      }
    }

    // Pass 3: darkness overlay (applied on top of all layers).
    // Tiles outside the server-confirmed view always receive the fog-of-war
    // opacity (+0.2) regardless of their local fog-map state.
    for (let vy = 0; vy < displayH; vy++) {
      for (let vx = 0; vx < displayW; vx++) {
        const ax = mx_start + vx;
        const ay = my_start + vy;
        if (!mapdata_contains(ax, ay)) continue;

        let alpha = 0;
        if (!inServerView(ax, ay)) {
          // Outside server view: apply the fog overlay.  If the tile was
          // previously seen (Fog state), preserve its stored darkness and add
          // the +0.2 fog penalty (same formula as in-view fog tiles).
          // Unvisited tiles (Empty state) get a flat 0.2.
          const cell = mapdata_cell(ax, ay);
          if (cell.state === MapCellState.Fog) {
            alpha = Math.min(cell.darkness / 255 + 0.2, 0.8);
          } else {
            alpha = 0.2;
          }
        } else {
          const cell = mapdata_cell(ax, ay);
          if (cell.state === MapCellState.Empty) continue;
          if (cell.state === MapCellState.Visible) {
            if (cell.darkness > 0) {
              alpha = Math.min(cell.darkness / 255, 0.8);
            }
          } else if (cell.state === MapCellState.Fog) {
            // Dim out-of-sight tiles to indicate they're no longer visible,
            // matching the old client which added +0.2 opacity for fog-of-war cells.
            alpha = Math.min(cell.darkness / 255 + 0.2, 0.8);
          }
        }

        if (alpha > 0) {
          ctx.fillStyle = `rgba(0, 0, 0, ${alpha})`;
          ctx.fillRect(vx * tileSize, vy * tileSize, tileSize, tileSize);
        }
      }
    }

    // Pass 4: draw labels on top of everything (matching old C client's map_draw_labels).
    // Labels are only shown for tiles inside the server-confirmed view.
    const fontSize = Math.round(BASE_FONT_SIZE * scale);
    ctx.font = `${fontSize}px sans-serif`;
    // The player is always centred in the server view; skip their own name label there.
    // In display coords, the player tile is at ⌊displayW/2⌋.
    const playerDisplayX = Math.floor(displayW / 2);
    const playerDisplayY = Math.floor(displayH / 2);
    for (let vy = 0; vy < displayH; vy++) {
      for (let vx = 0; vx < displayW; vx++) {
        const ax = mx_start + vx;
        const ay = my_start + vy;
        if (!mapdata_contains(ax, ay)) continue;

        // Labels are only visible inside the confirmed server view.
        if (!inServerView(ax, ay)) continue;

        const cell = mapdata_cell(ax, ay);
        if (cell.state !== MapCellState.Visible || cell.labels.length === 0) continue;

        const isPlayerTile = vx === playerDisplayX && vy === playerDisplayY;
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

    // Pass 5: draw a thin yellow border on the move-to target tile so the
    // player can see where they right-clicked.  Cleared automatically when
    // the player arrives or when move-to is cancelled by manual input.
    if (moveToX !== 0 || moveToY !== 0) {
      const tmx = moveToX - mx_start;
      const tmy = moveToY - my_start;
      if (tmx >= 0 && tmx < displayW && tmy >= 0 && tmy < displayH) {
        ctx.save();
        ctx.strokeStyle = 'rgba(255, 255, 0, 0.85)';
        ctx.lineWidth = Math.max(1, Math.round(scale));
        ctx.strokeRect(
          tmx * tileSize + ctx.lineWidth / 2,
          tmy * tileSize + ctx.lineWidth / 2,
          tileSize - ctx.lineWidth,
          tileSize - ctx.lineWidth,
        );
        ctx.restore();
      }
    }

    performance.mark('drawMap-end');
    performance.measure('drawMap', 'drawMap-start', 'drawMap-end');
    const elapsed = performance.now() - t0;
    drawCount++;

    if (elapsed > SLOW_DRAW_THRESHOLD_MS) {
      LOG(LogLevel.Warning, 'perf:map', `drawMap took ${elapsed.toFixed(1)}ms — tiles:${tilesDrawn} imgs:${imagesDrawn} placeholders:${placeholders} loadsStarted:${loadsStarted}`);
    }

    const now = performance.now();
    if (now - lastStatsTime > DRAW_STATS_INTERVAL_MS) {
      const dt = (now - lastStatsTime) / 1000;
      LOG(LogLevel.Info, 'perf:map', `${drawCount} draws in ${dt.toFixed(1)}s (${(drawCount / dt).toFixed(1)} draws/s), imageCache:${imageCache.size} loading:${loadingUrls.size} failed:${failedUrls.size}`);
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
        LOG(LogLevel.Debug, 'perf:map', `image load took ${elapsed.toFixed(0)}ms: ${url}`);
      }
      scheduleRedraw();
    };
    img.onerror = () => {
      loadingUrls.delete(url);
      failedUrls.add(url);
      LOG(LogLevel.Warning, 'perf:map', `image load failed: ${url}`);
    };
    img.src = url;
  }

  function handleClick(e: MouseEvent) {
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    // Convert canvas pixel position to a display-tile index, then compute the
    // offset from the player who is always at ⌊currentDisplayW/2⌋.
    const tileX = Math.floor((e.clientX - rect.left - currentDrawOffsetX) / currentTileSize);
    const tileY = Math.floor((e.clientY - rect.top - currentDrawOffsetY) / currentTileSize);
    const dx = tileX - Math.floor(currentDisplayW / 2);
    const dy = tileY - Math.floor(currentDisplayH / 2);
    lookAt(dx, dy);
  }

  function handleRightClick(e: MouseEvent) {
    e.preventDefault();
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const tileX = Math.floor((e.clientX - rect.left - currentDrawOffsetX) / currentTileSize);
    const tileY = Math.floor((e.clientY - rect.top - currentDrawOffsetY) / currentTileSize);
    const dx = tileX - Math.floor(currentDisplayW / 2);
    const dy = tileY - Math.floor(currentDisplayH / 2);
    set_move_to(dx, dy);
  }
</script>

<div class="game-map" bind:clientWidth={containerW} bind:clientHeight={containerH}>
  <canvas
    bind:this={canvas}
    onclick={handleClick}
    oncontextmenu={handleRightClick}
    width={640}
    height={640}
  ></canvas>
</div>

<style>
  .game-map {
    position: relative;
    background: #111;
    overflow: hidden;
    width: 100%;
    height: 100%;
  }

  canvas {
    display: block;
    image-rendering: pixelated;
    cursor: crosshair;
  }
</style>
