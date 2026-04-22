/**
 * cmd_map.ts — Map-related command handlers.
 * Extracted from commands.ts.
 */

import {
  MAP2_COORD_OFFSET, MAP2_TYPE_CLEAR, MAP2_TYPE_DARKNESS, MAP2_TYPE_LABEL,
  MAP2_LAYER_START, MAX_VIEW,
  FACE_IS_ANIM,
  MAXLAYERS,
  type Animation,
} from './protocol.js';
import { BinaryReader } from './binary_reader.js';
import {
  mapdata_newmap, mapdata_scroll, mapdata_set_face_layer, mapdata_set_anim_layer,
  mapdata_set_darkness, mapdata_set_smooth, mapdata_clear_space,
  mapdata_set_check_space, mapdata_clear_old, mapdata_set_size,
  mapdata_clear_label_view, mapdata_add_label,
} from './mapdata.js';
import { animations } from './item.js';
import { addSmooth } from './image.js';
import { useConfig, getCpl } from './init.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { gameEvents } from './events.js';
import { perfLogging } from '../lib/debug';

export function SetupCmd(data: string): void {
  const parts = data.split(' ');
  for (let i = 0; i < parts.length - 1; i += 2) {
    const key = parts[i]!;
    const value = parts[i + 1]!;
    if (key === 'mapsize' && value !== 'FALSE') {
      const [w = NaN, h = NaN] = value.split('x').map(Number);
      if (!isNaN(w) && !isNaN(h) && w > 0 && h > 0) {
        useConfig.mapWidth = w;
        useConfig.mapHeight = h;
        mapdata_set_size(w, h);
      }
    }
    LOG(LogLevel.Debug, 'SetupCmd', `${key} = ${value}`);
  }
}

export function NewmapCmd(): void {
  mapdata_newmap();
  gameEvents.emit('newMap');
}

export function Map2Cmd(data: DataView, len: number): void {
  const t0 = performance.now();
  let tileCount = 0;
  const reader = new BinaryReader(data, len);
  while (reader.remaining > 0) {
    const mask = reader.readInt16();
    const x = ((mask >> 10) & 0x3F) - MAP2_COORD_OFFSET;
    const y = ((mask >> 4) & 0x3F) - MAP2_COORD_OFFSET;

    // A coord word with the scroll flag set encodes a map scroll, not a tile.
    if (mask & 0x1) {
      mapdata_scroll(x, y);
      continue;
    }

    // Clamp to the valid view coordinate range.
    const cx = Math.max(0, Math.min(x, MAX_VIEW - 1));
    const cy = Math.max(0, Math.min(y, MAX_VIEW - 1));

    mapdata_clear_old(cx, cy);
    tileCount++;

    // Inner loop: read per-tile type bytes until the 255 end-of-space marker.
    let labelsCleared = false;
    while (reader.remaining > 0) {
      const typeByte = reader.readUint8();

      if (typeByte === 255) {
        mapdata_set_check_space(cx, cy);
        break;
      }

      // Upper 3 bits encode the number of additional data bytes for this entry;
      // lower 5 bits are the entry type.
      const spaceLen = typeByte >> 5;
      const type = typeByte & 0x1F;

      if (type === MAP2_TYPE_CLEAR) {
        mapdata_clear_space(cx, cy);
      } else if (type === MAP2_TYPE_DARKNESS) {
        const value = reader.readUint8();
        mapdata_set_darkness(cx, cy, value);
      } else if (type === MAP2_TYPE_LABEL) {
        // spaceLen === 7 signals variable-length data: next byte is total length.
        reader.skip(1); // labelTotalLen (unused)
        const subtype = reader.readUint8();
        const strLen = reader.readUint8();
        const label = reader.readString(strLen);
        if (!labelsCleared) {
          mapdata_clear_label_view(cx, cy);
          labelsCleared = true;
        }
        mapdata_add_label(cx, cy, subtype, label);
      } else if (type >= MAP2_LAYER_START && type < MAP2_LAYER_START + MAXLAYERS) {
        const layer = type & 0xF;
        const faceOrAnim = reader.readInt16();
        if (!(faceOrAnim & FACE_IS_ANIM)) {
          mapdata_set_face_layer(cx, cy, faceOrAnim, layer);
        }
        if (spaceLen > 2) {
          const opt = reader.readUint8();
          if (faceOrAnim & FACE_IS_ANIM) {
            // opt is the animation speed.
            mapdata_set_anim_layer(cx, cy, faceOrAnim, opt, layer);
          } else {
            // opt is a smooth value.
            mapdata_set_smooth(cx, cy, opt, layer);
          }
        }
        // A fourth byte (when present) is always a smooth value.
        if (spaceLen > 3) {
          const opt = reader.readUint8();
          mapdata_set_smooth(cx, cy, opt, layer);
        }
      } else {
        // Unknown type: skip the declared number of data bytes.
        if (spaceLen !== 7) {
          reader.skip(spaceLen);
        } else {
          const extraLen = reader.readUint8();
          reader.skip(extraLen);
        }
      }
    }
  }
  const elapsed = performance.now() - t0;
  if (perfLogging && (elapsed > 1 || tileCount > 10)) {
    LOG(LogLevel.Debug, 'perf:map2', `parsed ${tileCount} tiles from ${len}B in ${elapsed.toFixed(1)}ms`);
  }
  gameEvents.emit('mapUpdate');
}

export function mapScrollCmd(data: string): void {
  const parts = data.trim().split(' ');
  if (parts.length >= 2) {
    mapdata_scroll(parseInt(parts[0]!), parseInt(parts[1]!));
    gameEvents.emit('mapUpdate');
  }
}

export function MagicMapCmd(data: DataView, len: number): void {
  // The magicmap packet is MIXED format: ASCII header + binary data.
  // Header: "%d %d %d %d " (mmapx mmapy pmapx pmapy), then raw bytes.
  const bytes = new Uint8Array(data.buffer, data.byteOffset, len);

  // Find the 4 spaces that separate the ASCII header values from the binary data.
  let spaceCount = 0;
  let dataOffset = 0;
  for (let i = 0; i < len; i++) {
    if (bytes[i] === 0x20) { // ASCII space
      spaceCount++;
      if (spaceCount === 4) {
        dataOffset = i + 1;
        break;
      }
    }
  }
  if (spaceCount !== 4) {
    LOG(LogLevel.Warning, 'MagicMapCmd', 'Unable to find start of magic map data');
    return;
  }

  // Parse the ASCII header.
  const header = new TextDecoder().decode(bytes.subarray(0, dataOffset));
  const headerParts = header.trim().split(/\s+/);
  if (headerParts.length < 4) {
    LOG(LogLevel.Warning, 'MagicMapCmd', 'Could not parse magic map header');
    return;
  }

  const mmapx = parseInt(headerParts[0]!, 10);
  const mmapy = parseInt(headerParts[1]!, 10);
  const pmapx = parseInt(headerParts[2]!, 10);
  const pmapy = parseInt(headerParts[3]!, 10);

  if (mmapx === 0 || mmapy === 0) {
    LOG(LogLevel.Warning, 'MagicMapCmd', 'Empty magic map');
    return;
  }

  const dataLen = len - dataOffset;
  if (dataLen !== mmapx * mmapy) {
    LOG(LogLevel.Warning, 'MagicMapCmd',
      `Magic map size mismatch. Have ${dataLen} bytes, should have ${mmapx * mmapy}`);
    return;
  }

  const cpl = getCpl();
  if (!cpl) return;

  cpl.mmapx = mmapx;
  cpl.mmapy = mmapy;
  cpl.pmapx = pmapx;
  cpl.pmapy = pmapy;
  cpl.magicmap = new Uint8Array(bytes.subarray(dataOffset, dataOffset + dataLen));
  cpl.showmagic = 1;

  LOG(LogLevel.Info, 'MagicMapCmd',
    `Received magic map ${mmapx}x${mmapy}, player at (${pmapx},${pmapy})`);

  gameEvents.emit('magicMap');
}

export function AnimCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const animId = reader.readInt16();
  const animFlags = reader.readInt16();
  const faces: number[] = [];
  while (reader.remaining > 0) {
    faces.push(reader.readInt16());
  }
  const anim: Animation = {
    flags: animFlags,
    numAnimations: faces.length,
    speed: 0, speedLeft: 0, phase: 0,
    faces,
  };
  animations[animId] = anim;
}

export function SmoothCmd(data: DataView, len: number): void {
  const reader = new BinaryReader(data, len);
  const face = reader.readInt16();
  const smooth = reader.readInt16();
  addSmooth(face, smooth);
}
