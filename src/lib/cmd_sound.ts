/**
 * cmd_sound.ts — Sound/music command handlers.
 * Extracted from commands.ts.
 */

import {
  getStringFromData,
} from './newsocket.js';
import { LOG } from './misc.js';
import { LogLevel } from './protocol.js';
import { playSound, playMusic } from './sound.js';

/**
 * Handle the sound2 command from the server.
 *
 * Binary format:
 *   sound2 {x:int8}{y:int8}{dir:uint8}{vol:uint8}{type:uint8}
 *          {len_sound:uint8}{sound:str}{len_source:uint8}{source:str}
 */
export function Sound2Cmd(data: DataView, len: number): void {
  if (len < 8) {
    LOG(LogLevel.Warning, 'Sound2Cmd', `Command too short: ${len} bytes`);
    return;
  }
  const x = data.getInt8(0);
  const y = data.getInt8(1);
  const dir = data.getUint8(2);
  const vol = data.getUint8(3);
  const type = data.getUint8(4);
  const lenSound = data.getUint8(5);

  if (6 + lenSound + 1 > len) {
    LOG(LogLevel.Warning, 'Sound2Cmd', `Sound length overflows: ${lenSound}`);
    return;
  }

  const bytes = new Uint8Array(data.buffer, data.byteOffset);
  const sound = getStringFromData(bytes, 6, lenSound);
  const lenSource = data.getUint8(6 + lenSound);

  if (6 + lenSound + 1 + lenSource > len) {
    LOG(LogLevel.Warning, 'Sound2Cmd', `Source length overflows: ${lenSource}`);
    return;
  }

  const source = getStringFromData(bytes, 6 + lenSound + 1, lenSource);
  playSound(x, y, dir, vol, type, sound, source);
}

/**
 * Handle the music command from the server.
 *
 * Text format: music {name}
 * The name is the music track to play (without path/extension), or "NONE".
 */
export function MusicCmd(data: string): void {
  const name = data.trim();
  playMusic(name);
}
