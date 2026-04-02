/**
 * Miscellaneous utility functions for the Crossfire web client.
 * Port of old/common/misc.c logging functionality.
 */

import { LogLevel } from "./protocol";

/** Minimum log level; messages below this threshold are suppressed. */
export let minLog: LogLevel = LogLevel.Info;

/**
 * Set the minimum log level threshold.
 */
export function setMinLog(level: LogLevel): void {
    minLog = level;
}

/**
 * Log a message to the browser console at the appropriate severity.
 * Messages below the current {@link minLog} threshold are suppressed.
 */
export function LOG(level: LogLevel, origin: string, message: string): void {
    if (level < minLog) {
        return;
    }

    const formatted = `(${origin}) ${message}`;

    switch (level) {
        case LogLevel.Debug:
            console.debug(formatted);
            break;
        case LogLevel.Info:
            console.info(formatted);
            break;
        case LogLevel.Warning:
            console.warn(formatted);
            break;
        case LogLevel.Error:
            console.error(formatted);
            break;
        case LogLevel.Critical:
            console.error(formatted);
            break;
    }
}
