/**
 * Helpers for bindings that hold several commands.
 *
 * A key or gamepad button binding stores its commands in a single string,
 * one command per line.  Commands typed by the user can never contain a
 * newline, so the separator is unambiguous and older saved bindings (with a
 * single command) keep working unchanged.
 */

/** Separator between the commands of a multi-command binding. */
export const COMMAND_SEPARATOR = "\n";

/**
 * Append a command after the commands already in a binding.
 *
 * @param existing The binding's current command string (may be empty).
 * @param command The command to add at the end.
 * @returns The combined command string.
 */
export function appendCommand(existing: string, command: string): string {
  return existing ? existing + COMMAND_SEPARATOR + command : command;
}

/**
 * Format a binding's command string for display, separating multiple
 * commands with "; ".
 */
export function formatCommands(commands: string): string {
  return splitCommands(commands).join("; ");
}

/** Split a binding's command string into its individual commands. */
export function splitCommands(commands: string): string[] {
  return commands
    .split(COMMAND_SEPARATOR)
    .map((c) => c.trim())
    .filter((c) => c.length > 0);
}
