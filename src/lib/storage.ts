/**
 * Client-side storage utilities for the Crossfire web client.
 * Replaces C file I/O with localStorage (config/keybindings) and
 * IndexedDB (image cache).
 */

const STORAGE_PREFIX = "crossfire_";
const DB_NAME = "crossfire_cache";

// ──────────────────────────────────────────────────────────────────────────────
// localStorage helpers (configuration and keybindings)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Save a configuration value to localStorage.
 */
export function saveConfig<T>(key: string, value: T): void {
    const fullKey = STORAGE_PREFIX + key;
    localStorage.setItem(fullKey, JSON.stringify(value));
}

/**
 * Load a configuration value from localStorage, returning `defaultValue` if
 * the key does not exist or the stored JSON is malformed.
 */
export function loadConfig<T>(key: string, defaultValue: T): T {
    const fullKey = STORAGE_PREFIX + key;
    const raw = localStorage.getItem(fullKey);
    if (raw === null) {
        return defaultValue;
    }
    try {
        return JSON.parse(raw) as T;
    } catch {
        return defaultValue;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Bulk operations
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Remove all Crossfire data from both localStorage and IndexedDB.
 */
export async function clearAllData(): Promise<void> {
    // Clear prefixed localStorage keys
    const keysToRemove: string[] = [];
    for (let i = 0; i < localStorage.length; i++) {
        const key = localStorage.key(i);
        if (key !== null && key.startsWith(STORAGE_PREFIX)) {
            keysToRemove.push(key);
        }
    }
    for (const key of keysToRemove) {
        localStorage.removeItem(key);
    }

    // Delete the entire IndexedDB database
    await new Promise<void>((resolve, reject) => {
        const request = indexedDB.deleteDatabase(DB_NAME);
        request.onsuccess = () => resolve();
        request.onerror = () => reject(request.error);
    });
}
