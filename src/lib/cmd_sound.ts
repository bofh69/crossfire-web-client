/**
 * cmd_sound.ts — Sound/music command handlers.
 * Extracted from commands.ts.
 */

import { BinaryReader } from './binary_reader.js';
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
  const reader = new BinaryReader(data, len);
  const x = reader.readInt8();
  const y = reader.readInt8();
  const dir = reader.readUint8();
  const vol = reader.readUint8();
  const type = reader.readUint8();
  const lenSound = reader.readUint8();
  const sound = reader.readString(lenSound);
  const lenSource = reader.readUint8();
  const source = reader.readString(lenSource);
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
