/**
 * image.ts – Face/image data management for the Crossfire web client.
 * Ported from old/common/image.c. Handles face metadata, image caching,
 * smooth-face mappings, and PNG blob creation from server data.
 */

import {
    getShortFromData,
    getIntFromData,
    getStringFromData,
} from './newsocket.js';
import { BinaryReader } from './binary_reader.js';
import { LOG } from './misc.js';
import { LogLevel, MAXPIXMAPNUM, MAX_FACE_SETS } from './protocol.js';
import { TILE_SIZE } from './constants.js';

import type { FaceInformation } from './protocol.js';

// ---------------------------------------------------------------------------
// Magic-map synthetic face constants
// ---------------------------------------------------------------------------

/**
 * Base face ID for client-generated magic-map color tiles.
 * The face ID for color index `c` (0..12) is `MAGIC_MAP_FACE_BASE | c`.
 * These IDs are deliberately far above MAXPIXMAPNUM so they never
 * collide with server-assigned face numbers.
 */
export const MAGIC_MAP_FACE_BASE = 0xfffffe00;

/**
 * Base face ID for the 16 wall-shape faces.
 * The face ID for a wall with neighbor bitmask `m` (0..15) is
 * `MAGIC_MAP_WALL_FACE_BASE | m`.
 *
 * The four direction bits:
 *   MAGIC_MAP_WALL_ABOVE = 0x8 – wall neighbor above
 *   MAGIC_MAP_WALL_BELOW = 0x4 – wall neighbor below
 *   MAGIC_MAP_WALL_LEFT  = 0x2 – wall neighbor to the left
 *   MAGIC_MAP_WALL_RIGHT = 0x1 – wall neighbor to the right
 */
export const MAGIC_MAP_WALL_FACE_BASE = MAGIC_MAP_FACE_BASE | 0x10;

/** Bit set in the wall neighbor mask when there is a wall above. */
export const MAGIC_MAP_WALL_ABOVE = 0x8;
/** Bit set in the wall neighbor mask when there is a wall below. */
export const MAGIC_MAP_WALL_BELOW = 0x4;
/** Bit set in the wall neighbor mask when there is a wall to the left. */
export const MAGIC_MAP_WALL_LEFT  = 0x2;
/** Bit set in the wall neighbor mask when there is a wall to the right. */
export const MAGIC_MAP_WALL_RIGHT = 0x1;

/**
 * Colour palette for the 13 magic-map colour indices (0 = void/black).
 * Matches the palette used in MagicMap.svelte and the original GTK client.
 */
const MAGIC_MAP_COLORS: readonly string[] = [
    '#000000', // 0 – Black (empty/void)
    '#ffffff', // 1 – White
    '#000080', // 2 – Navy
    '#ff0000', // 3 – Red
    '#ffa500', // 4 – Orange
    '#1e90ff', // 5 – DodgerBlue
    '#ee9a00', // 6 – DarkOrange2
    '#2e8b57', // 7 – SeaGreen
    '#8fbc8f', // 8 – DarkSeaGreen
    '#808080', // 9 – Grey50
    '#a0522d', // 10 – Sienna
    '#ffd700', // 11 – Gold
    '#f0e68c', // 12 – Khaki
];

/**
 * Register synthetic TILE_SIZE×TILE_SIZE solid-colour face URLs for each
 * magic-map colour index (0..12), plus 16 wall-line faces for every
 * combination of the four cardinal neighbor directions.
 * Safe to call multiple times (idempotent).
 * No-op outside a browser context (guards against SSR / unit-test environments).
 */
export function registerMagicMapFaces(): void {
    if (typeof document === 'undefined') return;
    const canvas = document.createElement('canvas');
    canvas.width = TILE_SIZE;
    canvas.height = TILE_SIZE;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // --- Solid-colour tiles for each colour index ---
    for (let i = 0; i < MAGIC_MAP_COLORS.length; i++) {
        const faceId = MAGIC_MAP_FACE_BASE | i;
        if (faceUrls.has(faceId)) continue; // already registered
        ctx.clearRect(0, 0, TILE_SIZE, TILE_SIZE);
        ctx.fillStyle = MAGIC_MAP_COLORS[i]!;
        ctx.fillRect(0, 0, TILE_SIZE, TILE_SIZE);
        faceUrls.set(faceId, canvas.toDataURL('image/png'));
        faceSizes.set(faceId, { w: TILE_SIZE, h: TILE_SIZE });
    }

    const cx = TILE_SIZE / 2;
    const cy = TILE_SIZE / 2;

    // Helper: draw a line segment with a gray outline and a black inner stroke.
    function drawSegment(
        x1: number, y1: number,
        x2: number, y2: number,
    ): void {
        ctx!.strokeStyle = '#808080';
        ctx!.lineWidth = 5;
        ctx!.beginPath();
        ctx!.moveTo(x1, y1);
        ctx!.lineTo(x2, y2);
        ctx!.stroke();

        ctx!.strokeStyle = '#000000';
        ctx!.lineWidth = 3;
        ctx!.beginPath();
        ctx!.moveTo(x1, y1);
        ctx!.lineTo(x2, y2);
        ctx!.stroke();
    }

    // --- 16 wall faces, one per neighbor-bitmask (0..15) ---
    for (let mask = 0; mask <= 0x0f; mask++) {
        const faceId = MAGIC_MAP_WALL_FACE_BASE | mask;
        if (faceUrls.has(faceId)) continue;

        ctx.clearRect(0, 0, TILE_SIZE, TILE_SIZE);

        if (mask === 0) {
            // Isolated wall: draw a small dot at the center.
            ctx.fillStyle = '#808080';
            ctx.beginPath();
            ctx.arc(cx, cy, 3, 0, Math.PI * 2);
            ctx.fill();
            ctx.fillStyle = '#000000';
            ctx.beginPath();
            ctx.arc(cx, cy, 2, 0, Math.PI * 2);
            ctx.fill();
        } else {
            // Draw a half-segment from the center toward each set neighbor.
            if (mask & MAGIC_MAP_WALL_ABOVE) drawSegment(cx, cy, cx, 0);
            if (mask & MAGIC_MAP_WALL_BELOW) drawSegment(cx, cy, cx, TILE_SIZE);
            if (mask & MAGIC_MAP_WALL_LEFT)  drawSegment(cx, cy, 0,  cy);
            if (mask & MAGIC_MAP_WALL_RIGHT) drawSegment(cx, cy, TILE_SIZE, cy);
        }

        faceUrls.set(faceId, canvas.toDataURL('image/png'));
        faceSizes.set(faceId, { w: TILE_SIZE, h: TILE_SIZE });
    }
}

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

/** Blob URLs keyed by face number. */
const faceUrls = new Map<number, string>();

/** Image dimensions keyed by face number. */
const faceSizes = new Map<number, { w: number; h: number }>();

/** Smooth-face mapping: source face → smoothing face. */
const smoothFaces = new Map<number, number>();

/**
 * Pending face names received via Face2Cmd, indexed by face number.
 * Used to associate a name with image data that arrives later via Image2Cmd.
 */
const faceToName = new Map<number, string>();

/** Checksums received via Face2Cmd, indexed by face number. */
const faceChecksums = new Map<number, number>();

/** Global face/image-set metadata populated by getImageInfo. */
const faceInfo: FaceInformation = {
    faceset: 0,
    wantFaceset: '',
    numImages: 0,
    bmapsChecksum: 0,
    oldBmapsChecksum: 0,
    cacheHits: 0,
    cacheMisses: 0,
    haveFacesetInfo: false,
    facesets: [],
};

// ---------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------

/** Return the Blob URL for a face, or `null` if the face has not been loaded. */
export function getFaceUrl(face: number): string | null {
    return faceUrls.get(face) ?? null;
}

/** Return the pixel dimensions of a face image ({w, h}). Defaults to 32×32. */
export function getMapImageSize(face: number): { w: number; h: number } {
    return faceSizes.get(face) ?? { w: 32, h: 32 };
}

/**
 * Return the size of a face in tiles ({w, h}).  Divides the stored pixel
 * dimensions by {@link TILE_SIZE} and rounds, so a 160×96 px face becomes
 * 5×3 tiles.  Always returns at least 1×1.
 *
 * Pass this to {@link setGetMapImageSize} so that mapdata correctly tracks
 * multi-tile (bigface) objects.
 */
export function getFaceTileSize(face: number): { w: number; h: number } {
    const { w, h } = getMapImageSize(face);
    return {
        w: Math.max(1, Math.round(w / TILE_SIZE)),
        h: Math.max(1, Math.round(h / TILE_SIZE)),
    };
}

/** Return the smoothing face number for `face`, or 0 if none is set. */
export function getSmoothFace(face: number): number {
    return smoothFaces.get(face) ?? 0;
}

// ---------------------------------------------------------------------------
// Smooth face mapping
// ---------------------------------------------------------------------------

/** Register a smooth-face association (server "smooth" command). */
export function addSmooth(face: number, smoothFace: number): void {
    smoothFaces.set(face, smoothFace);
}

// ---------------------------------------------------------------------------
// Cache initialisation / reset
// ---------------------------------------------------------------------------

/** Initialise the image cache data structures. */
export function initCommonCacheData(): void {
    faceUrls.clear();
    faceSizes.clear();
    smoothFaces.clear();
    faceToName.clear();
    faceChecksums.clear();
    faceInfo.facesets = [];
    for (let i = 0; i < MAX_FACE_SETS; i++) {
        faceInfo.facesets.push({
            setnum: i,
            fallback: 0,
            prefix: '',
            fullname: '',
            size: '',
            extension: '',
            comment: '',
        });
    }
    faceInfo.haveFacesetInfo = false;
    faceInfo.cacheHits = 0;
    faceInfo.cacheMisses = 0;
}

/**
 * Reset per-connection image cache state.
 * Revokes outstanding Blob URLs to free memory.
 */
export function resetImageCacheData(): void {
    for (const url of faceUrls.values()) {
        URL.revokeObjectURL(url);
    }
    faceUrls.clear();
    faceSizes.clear();
    faceToName.clear();
    faceChecksums.clear();
}

// ---------------------------------------------------------------------------
// Face2Cmd – server sends face number + checksum + name
// ---------------------------------------------------------------------------

/**
 * Handle the "face2" server command.
 *
 * Wire format (binary, big-endian):
 *   uint16  pnum      – face / pixmap number
 *   uint8   setnum    – face-set index
 *   int32   checksum  – image checksum
 *   string  name      – face name (remaining bytes)
 */
export function Face2Cmd(data: DataView, len: number): void {
    if (len < 7) {
        LOG(LogLevel.Warning, 'Face2Cmd', 'Packet too short');
        return;
    }

    const pnum = getShortFromData(data, 0);
    // setnum at offset 2 (1 byte) – not used beyond logging
    const checksum = getIntFromData(data, 3);

    const bytes = new Uint8Array(data.buffer, data.byteOffset, len);
    const faceName = getStringFromData(bytes, 7, len - 7);

    if (pnum < 0 || pnum >= MAXPIXMAPNUM) {
        LOG(LogLevel.Warning, 'Face2Cmd', `Face number ${pnum} out of range`);
        return;
    }

    faceToName.set(pnum, faceName);
    faceChecksums.set(pnum, checksum);
}

// ---------------------------------------------------------------------------
// Image2Cmd – server sends actual PNG data
// ---------------------------------------------------------------------------

/**
 * Handle the "image2" server command.
 *
 * Wire format (binary, big-endian):
 *   int32   pnum   – face / pixmap number
 *   uint8   setnum – face-set index
 *   int32   plen   – payload length (PNG bytes)
 *   bytes   data   – raw PNG image data
 */
export function Image2Cmd(data: DataView, len: number): void {
    if (len < 9) {
        LOG(LogLevel.Warning, 'Image2Cmd', 'Packet too short');
        return;
    }

    const pnum = getIntFromData(data, 0);
    // setnum at offset 4 (1 byte) – informational
    const plen = getIntFromData(data, 5);

    if ((len - 9) !== plen) {
        LOG(LogLevel.Warning, 'Image2Cmd',
            `Length mismatch: payload=${len - 9}, declared=${plen}`);
        return;
    }

    const pngBytes = new Uint8Array(data.buffer, data.byteOffset + 9, plen).slice();

    // Register the face synchronously so sizeX/sizeY in mapdata are correct
    // when a subsequent map2 packet in the same receive loop references this face.
    applyFacePngBytes(pnum, pngBytes);
}

// ---------------------------------------------------------------------------
// getImageInfo – parse "replyinfo image_info" payload
// ---------------------------------------------------------------------------

/**
 * Parse the server's `replyinfo image_info` response.
 *
 * The payload is a newline-delimited text block:
 *   line 1: number of images
 *   line 2: bmaps checksum
 *   remaining lines: faceset descriptors (colon-separated, 7 fields each)
 */
export function getImageInfo(data: Uint8Array, len: number): void {
    const text = getStringFromData(data, 0, len);
    const lines = text.split('\n');

    if (lines.length < 2) {
        return;
    }

    faceInfo.numImages = parseInt(lines[0]!, 10) || 0;
    faceInfo.bmapsChecksum = parseInt(lines[1]!, 10) || 0;

    for (let i = 2; i < lines.length; i++) {
        const line = lines[i];
        if (!line) {
            continue;
        }
        const parts = line.split(':');
        if (parts.length < 7) {
            LOG(LogLevel.Warning, 'getImageInfo',
                `Bad faceset line, ignoring: ${line}`);
            continue;
        }
        const setnum = parseInt(parts[0]!, 10);
        if (setnum < 0 || setnum >= MAX_FACE_SETS) {
            LOG(LogLevel.Warning, 'getImageInfo',
                `setnum out of range: ${setnum}`);
            continue;
        }
        // Ensure facesets array is large enough
        while (faceInfo.facesets.length <= setnum) {
            faceInfo.facesets.push({
                setnum: faceInfo.facesets.length,
                fallback: 0,
                prefix: '',
                fullname: '',
                size: '',
                extension: '',
                comment: '',
            });
        }
        faceInfo.facesets[setnum] = {
            setnum,
            prefix: parts[1]!,
            fullname: parts[2]!,
            fallback: parseInt(parts[3]!, 10) || 0,
            size: parts[4]!,
            extension: parts[5]!,
            comment: parts[6]!,
        };
    }
    faceInfo.haveFacesetInfo = true;
}

// ---------------------------------------------------------------------------
// getImageSums – parse "replyinfo image_sums" payload
// ---------------------------------------------------------------------------

/**
 * Parse the server's `replyinfo image_sums` response.
 *
 * The payload is a string whose first two space-delimited tokens are the
 * start and stop image numbers. The remainder is packed binary data:
 *   uint16  imagenum
 *   int32   checksum
 *   uint8   faceset
 *   uint8   namelen
 *   bytes   name (namelen bytes)
 *
 * Because the caller already decoded the whole buffer as a string, we
 * re-encode to get at the binary portion.
 */
export function getImageSums(data: string, _len: number): void {
    // Find the first two space-delimited tokens (start, stop).
    let idx = 0;

    // Skip first token (start)
    while (idx < data.length && data.charCodeAt(idx) !== 0x20) {
        idx++;
    }
    while (idx < data.length && data.charCodeAt(idx) === 0x20) {
        idx++;
    }

    // Skip second token (stop)
    while (idx < data.length && data.charCodeAt(idx) !== 0x20) {
        idx++;
    }
    while (idx < data.length && data.charCodeAt(idx) === 0x20) {
        idx++;
    }

    // Re-encode the remaining binary-ish portion to work with raw bytes.
    const encoder = new TextEncoder();
    const raw = encoder.encode(data);
    const view = new DataView(raw.buffer, raw.byteOffset + idx, raw.byteLength - idx);
    const reader = new BinaryReader(view);

    while (reader.remaining >= 8) {
        const imageNum = reader.readUint16();
        const checksum = reader.readUint32();
        reader.skip(1); // faceset (unused)
        const nameLen = reader.readUint8();

        if (nameLen > reader.remaining) {
            break;
        }

        const faceName = reader.readString(nameLen);

        if (imageNum >= 0 && imageNum < MAXPIXMAPNUM) {
            faceToName.set(imageNum, faceName);
            faceChecksums.set(imageNum, checksum);
        }
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * Read PNG image dimensions synchronously from raw PNG bytes.
 *
 * PNG structure:
 *   bytes  0-7  : PNG signature
 *   bytes  8-11 : IHDR chunk length (always 13)
 *   bytes 12-15 : "IHDR" marker
 *   bytes 16-19 : width  (uint32 big-endian)
 *   bytes 20-23 : height (uint32 big-endian)
 *
 * Returns {w:32, h:32} as a safe fallback for anything unexpected.
 */
function readPngDimensionsSync(pngBytes: Uint8Array): { w: number; h: number } {
    // Minimum header size: 8 (signature) + 4 (len) + 4 (type) + 4 (w) + 4 (h) = 24 bytes
    if (pngBytes.length < 24) return { w: 32, h: 32 };
    // Verify PNG signature bytes 1-3: 'P','N','G'
    if (pngBytes[1] !== 0x50 || pngBytes[2] !== 0x4E || pngBytes[3] !== 0x47) {
        return { w: 32, h: 32 };
    }
    const w = (pngBytes[16]! << 24 | pngBytes[17]! << 16 | pngBytes[18]! << 8 | pngBytes[19]!) >>> 0;
    const h = (pngBytes[20]! << 24 | pngBytes[21]! << 16 | pngBytes[22]! << 8 | pngBytes[23]!) >>> 0;
    return { w: w || 32, h: h || 32 };
}

/**
 * Register a face from raw PNG bytes, extracting dimensions synchronously
 * so that map data set up in the same receive loop sees the correct tile size.
 */
function applyFacePngBytes(pnum: number, pngBytes: Uint8Array): void {
    const oldUrl = faceUrls.get(pnum);
    if (oldUrl) {
        URL.revokeObjectURL(oldUrl);
    }

    // Copy into a plain ArrayBuffer to satisfy Blob's type constraints.
    const buffer = new ArrayBuffer(pngBytes.byteLength);
    new Uint8Array(buffer).set(pngBytes);
    const blob = new Blob([buffer], { type: 'image/png' });
    const url = URL.createObjectURL(blob);
    faceUrls.set(pnum, url);

    // Extract dimensions synchronously from PNG IHDR
    const dims = readPngDimensionsSync(pngBytes);
    faceSizes.set(pnum, dims);
}

// ---------------------------------------------------------------------------
// Debug helpers
// ---------------------------------------------------------------------------

/**
 * Return a human-readable dump of all known data for a single face number.
 * Returns an array of lines suitable for logging to the browser console.
 */
export function image_debug_face(face: number): string[] {
    const lines: string[] = [];
    lines.push(`Face ${face}:`);

    const name = faceToName.get(face);
    if (name !== undefined) {
        lines.push(`  name: ${name} (pending – image not yet loaded)`);
    }

    const checksum = faceChecksums.get(face);
    if (checksum !== undefined) {
        const hex = (checksum >>> 0).toString(16).padStart(8, '0');
        lines.push(`  checksum: 0x${hex} (pending – image not yet loaded)`);
    }

    const hasSizeEntry = faceSizes.has(face);
    const { w, h } = getMapImageSize(face);
    lines.push(`  pixel size: ${w}×${h}${hasSizeEntry ? '' : ' (default – image not loaded)'}`);

    const url = getFaceUrl(face);
    lines.push(`  image URL: ${url !== null ? url : '(none – image not loaded)'}`);

    const smooth = getSmoothFace(face);
    if (smooth !== 0) {
        lines.push(`  smooth face: ${smooth}`);
    }

    return lines;
}

// ---------------------------------------------------------------------------
// One-time initialisation of synthetic faces (runs at module load in browser)
// ---------------------------------------------------------------------------
registerMagicMapFaces();
