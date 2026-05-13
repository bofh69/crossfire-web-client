/**
 * Shared direction helpers used by input and movement code.
 */

/** Direction strings indexed by numeric direction (0–8). */
export const directions: readonly string[] = [
  "stay",
  "north",
  "northeast",
  "east",
  "southeast",
  "south",
  "southwest",
  "west",
  "northwest",
];

/**
 * Convert a direction number (0–8) to its command string.
 */
export function directionName(dir: number): string {
  if (dir >= 0 && dir < directions.length) {
    return directions[dir]!;
  }
  return "stay";
}

/**
 * If `command` is a direction word return its direction index (0–8),
 * otherwise return -1.
 */
export function directionFromCommand(command: string): number {
  for (let i = 0; i < directions.length; i++) {
    if (command === directions[i]) return i;
  }
  return -1;
}
