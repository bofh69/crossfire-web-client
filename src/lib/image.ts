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
import { saveCacheData, loadCacheData } from './storage.js';
import { LOG } from './misc.js';
import { LogLevel, MAXPIXMAPNUM, MAX_FACE_SETS } from './protocol.js';

import type { FaceInformation } from './protocol.js';

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

    // Try to load from the browser cache first
    loadCacheData(`face_${pnum}_${checksum}`).then((cached) => {
        if (cached instanceof Blob) {
            faceInfo.cacheHits++;
            applyFaceBlob(pnum, cached);
        } else {
            // Image not cached – the server will send image data via Image2Cmd
            faceInfo.cacheMisses++;
        }
    }).catch(() => {
        faceInfo.cacheMisses++;
    });
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

    // Persist to browser cache (using a Blob for storage compatibility)
    const checksum = faceChecksums.get(pnum) ?? 0;
    const blob = new Blob([pngBytes], { type: 'image/png' });
    saveCacheData(`face_${pnum}_${checksum}`, blob).catch(() => {
        LOG(LogLevel.Warning, 'Image2Cmd', `Failed to cache face ${pnum}`);
    });

    // Clean up temporary name mapping
    faceToName.delete(pnum);
    faceChecksums.delete(pnum);
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
 * Create a Blob URL from a PNG Blob, extract its dimensions via an
 * off-screen image element, and store the results.
 * For the real-time image path (Image2Cmd), prefer applyFacePngBytes so
 * the dimensions are set synchronously before map data references the face.
 */
function applyFaceBlob(pnum: number, blob: Blob): void {
    // Revoke any previous URL for this face to avoid memory leaks
    const oldUrl = faceUrls.get(pnum);
    if (oldUrl) {
        URL.revokeObjectURL(oldUrl);
    }

    const url = URL.createObjectURL(blob);
    faceUrls.set(pnum, url);

    // Decode image dimensions asynchronously (used for cached blobs)
    if (typeof createImageBitmap === 'function') {
        createImageBitmap(blob).then((bmp) => {
            faceSizes.set(pnum, { w: bmp.width, h: bmp.height });
            bmp.close();
        }).catch(() => {
            faceSizes.set(pnum, { w: 32, h: 32 });
        });
    } else {
        // Fallback: assume default tile size
        faceSizes.set(pnum, { w: 32, h: 32 });
    }
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
