/**
 * Default keybindings converted from old/common/def-keys.
 *
 * Original format: <keysym> <keynro> <flags> <string>
 * Keysyms are mapped to web KeyboardEvent.key values.
 */

export enum KeyBindingFlags {
  NORMAL = 'N',
  FIRE = 'F',
  RUN = 'R',
  ALL = 'A',
  EDIT = 'E',
}

export interface KeyBinding {
  keysym: string;
  keyCode: number;
  flags: string;
  command: string;
}

export function getDefaultKeyBindings(): KeyBinding[] {
  return [
    // Basic Keys
    // The trailing space on "say " and "chat " is intentional.
    { keysym: '"', keyCode: 1, flags: 'AE', command: 'say ' },
    { keysym: 'Enter', keyCode: 1, flags: 'AE', command: 'chat ' },
    { keysym: ';', keyCode: 0, flags: 'NE', command: 'reply' },

    { keysym: ',', keyCode: 1, flags: 'A', command: 'take' },
    { keysym: '<', keyCode: 0, flags: 'F', command: 'get all' },
    { keysym: '.', keyCode: 1, flags: 'N', command: 'stay fire' },
    { keysym: '?', keyCode: 1, flags: 'A', command: 'help' },

    { keysym: 'a', keyCode: 1, flags: 'N', command: 'apply' },
    { keysym: 'd', keyCode: 1, flags: 'N', command: 'disarm' },
    { keysym: 'e', keyCode: 1, flags: 'NR', command: 'examine' },
    { keysym: 's', keyCode: 1, flags: 'F', command: 'brace' },
    { keysym: 's', keyCode: 1, flags: 'N', command: 'search' },
    { keysym: 't', keyCode: 1, flags: 'N', command: 'ready_skill throwing' },

    // Nethack-Style (Normal)
    { keysym: 'b', keyCode: 1, flags: 'N', command: 'southwest' },
    { keysym: 'h', keyCode: 1, flags: 'N', command: 'west' },
    { keysym: 'j', keyCode: 1, flags: 'N', command: 'south' },
    { keysym: 'k', keyCode: 1, flags: 'N', command: 'north' },
    { keysym: 'l', keyCode: 1, flags: 'N', command: 'east' },
    { keysym: 'n', keyCode: 1, flags: 'N', command: 'southeast' },
    { keysym: 'u', keyCode: 1, flags: 'N', command: 'northeast' },
    { keysym: 'y', keyCode: 1, flags: 'N', command: 'northwest' },

    // Nethack-Style (Run)
    { keysym: 'b', keyCode: 1, flags: 'R', command: 'southwest' },
    { keysym: 'h', keyCode: 1, flags: 'R', command: 'west' },
    { keysym: 'j', keyCode: 1, flags: 'R', command: 'south' },
    { keysym: 'k', keyCode: 1, flags: 'R', command: 'north' },
    { keysym: 'l', keyCode: 1, flags: 'R', command: 'east' },
    { keysym: 'n', keyCode: 1, flags: 'R', command: 'southeast' },
    { keysym: 'u', keyCode: 1, flags: 'R', command: 'northeast' },
    { keysym: 'y', keyCode: 1, flags: 'R', command: 'northwest' },

    // Nethack-Style (Fire)
    { keysym: 'b', keyCode: 1, flags: 'F', command: 'southwest' },
    { keysym: 'h', keyCode: 1, flags: 'F', command: 'west' },
    { keysym: 'j', keyCode: 1, flags: 'F', command: 'south' },
    { keysym: 'k', keyCode: 1, flags: 'F', command: 'north' },
    { keysym: 'l', keyCode: 1, flags: 'F', command: 'east' },
    { keysym: 'n', keyCode: 1, flags: 'F', command: 'southeast' },
    { keysym: 'u', keyCode: 1, flags: 'F', command: 'northeast' },
    { keysym: 'y', keyCode: 1, flags: 'F', command: 'northwest' },

    // Arrow Keys
    { keysym: 'ArrowUp', keyCode: 1, flags: 'A', command: 'north' },
    { keysym: 'ArrowDown', keyCode: 1, flags: 'A', command: 'south' },
    { keysym: 'ArrowLeft', keyCode: 1, flags: 'A', command: 'west' },
    { keysym: 'ArrowRight', keyCode: 1, flags: 'A', command: 'east' },

    // Number Pad Arrow Keys (NumLock on)
    { keysym: '8', keyCode: 1, flags: 'A', command: 'north' },
    { keysym: '2', keyCode: 1, flags: 'A', command: 'south' },
    { keysym: '4', keyCode: 1, flags: 'A', command: 'west' },
    { keysym: '6', keyCode: 1, flags: 'A', command: 'east' },
    { keysym: '7', keyCode: 1, flags: 'A', command: 'northwest' },
    { keysym: '9', keyCode: 1, flags: 'A', command: 'northeast' },
    { keysym: '5', keyCode: 1, flags: 'A', command: 'stay' },
    { keysym: '1', keyCode: 1, flags: 'A', command: 'southwest' },
    { keysym: '3', keyCode: 1, flags: 'A', command: 'southeast' },

    // Windows NumLock-off equivalents (Fire)
    { keysym: 'End', keyCode: 1, flags: 'F', command: 'southwest f' },
    { keysym: 'Home', keyCode: 1, flags: 'F', command: 'northwest f' },
    { keysym: 'PageUp', keyCode: 1, flags: 'F', command: 'northeast f' },
    { keysym: 'PageDown', keyCode: 1, flags: 'F', command: 'southeast f' },

    // Windows NumLock-off equivalents (Normal)
    { keysym: 'End', keyCode: 1, flags: 'N', command: 'southwest' },
    { keysym: 'Home', keyCode: 1, flags: 'N', command: 'northwest' },
    { keysym: 'PageUp', keyCode: 1, flags: 'N', command: 'northeast' },
    { keysym: 'PageDown', keyCode: 1, flags: 'N', command: 'southeast' },

    // Windows NumLock-off equivalents (Run)
    { keysym: 'End', keyCode: 1, flags: 'R', command: 'southwest' },
    { keysym: 'Home', keyCode: 1, flags: 'R', command: 'northwest' },
    { keysym: 'PageUp', keyCode: 1, flags: 'R', command: 'northeast' },
    { keysym: 'PageDown', keyCode: 1, flags: 'R', command: 'southeast' },

    // Sun Type 4 Keyboard / Numpad navigation keys (NumLock off)
    { keysym: 'ArrowUp', keyCode: 1, flags: 'A', command: 'north' },
    { keysym: 'ArrowDown', keyCode: 1, flags: 'A', command: 'south' },
    { keysym: 'ArrowRight', keyCode: 1, flags: 'A', command: 'east' },
    { keysym: 'ArrowLeft', keyCode: 1, flags: 'A', command: 'west' },
    { keysym: 'Home', keyCode: 1, flags: 'A', command: 'northwest' },
    { keysym: 'PageUp', keyCode: 1, flags: 'A', command: 'northeast' },
    { keysym: 'End', keyCode: 1, flags: 'A', command: 'southwest' },
    { keysym: 'PageDown', keyCode: 1, flags: 'A', command: 'southeast' },

    // Action Rotation
    { keysym: '+', keyCode: 1, flags: 'A', command: 'rotateshoottype' },
    { keysym: '-', keyCode: 1, flags: 'A', command: 'rotateshoottype -' },
    { keysym: '-', keyCode: 1, flags: 'N', command: 'rotateshoottype -1' },
    { keysym: '+', keyCode: 1, flags: 'NF', command: 'rotateshoottype' },
  ];
}
