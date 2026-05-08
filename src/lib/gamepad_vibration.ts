/**
 * gamepad_vibration.ts — HP-drop vibration feedback.
 * Extracted from gamepad.ts.
 */

import {
  VIBRATION_DURATION_MS,
  VIBRATION_HP_DROP_THRESHOLD,
  VIBRATION_WEAK_MAGNITUDE,
  VIBRATION_STRONG_MAGNITUDE,
} from "./constants";

/**
 * The player's HP at the time of the last stats update.
 * -1 means no previous value is known (first update after login/reconnect).
 */
let prevHp = -1;

/**
 * Notify the gamepad subsystem of the player's current HP and max HP.
 * If a gamepad is connected and HP has dropped by more than 10% of the
 * player's maximum HP since the last update, the controller is vibrated.
 */
export function notifyHpUpdate(
  hp: number,
  maxHp: number,
  activeGamepadIndex: number,
): void {
  if (activeGamepadIndex < 0) {
    prevHp = hp;
    return;
  }

  if (prevHp >= 0 && maxHp > 0 && hp < prevHp) {
    const drop = prevHp - hp;
    if (drop / maxHp >= VIBRATION_HP_DROP_THRESHOLD) {
      const gamepads = navigator.getGamepads();
      const gp = gamepads[activeGamepadIndex];
      if (gp?.vibrationActuator) {
        gp.vibrationActuator.playEffect("dual-rumble", {
          startDelay: 0,
          duration: VIBRATION_DURATION_MS,
          weakMagnitude: VIBRATION_WEAK_MAGNITUDE,
          strongMagnitude: VIBRATION_STRONG_MAGNITUDE,
        });
      }
    }
  }

  prevHp = hp;
}

/**
 * Reset the HP baseline used for vibration tracking.
 * Call this when a new player session starts so that the first stats update
 * does not mistakenly trigger a vibration.
 */
export function resetHpTracking(): void {
  prevHp = -1;
}
