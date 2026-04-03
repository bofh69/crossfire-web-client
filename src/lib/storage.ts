/**
 * Client-side storage utilities for the Crossfire web client.
 * Replaces C file I/O with localStorage (config/keybindings) and
 * IndexedDB (image cache).
 */

const STORAGE_PREFIX = "crossfire_";
const DB_NAME = "crossfire_cache";
const DB_VERSION = 1;
const IMAGE_STORE = "images";

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
// IndexedDB helpers (image / data cache)
// ──────────────────────────────────────────────────────────────────────────────

function openDB(): Promise<IDBDatabase> {
    return new Promise<IDBDatabase>((resolve, reject) => {
        const request = indexedDB.open(DB_NAME, DB_VERSION);

        request.onupgradeneeded = () => {
            const db = request.result;
            if (!db.objectStoreNames.contains(IMAGE_STORE)) {
                db.createObjectStore(IMAGE_STORE);
            }
        };

        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
    });
}

/**
 * Save binary or structured data into the IndexedDB cache store.
 */
export async function saveCacheData(
    key: string,
    data: ArrayBuffer | Blob | string,
): Promise<void> {
    const db = await openDB();
    return new Promise<void>((resolve, reject) => {
        const tx = db.transaction(IMAGE_STORE, "readwrite");
        const store = tx.objectStore(IMAGE_STORE);
        const request = store.put(data, key);

        request.onsuccess = () => resolve();
        request.onerror = () => reject(request.error);
        tx.oncomplete = () => db.close();
    });
}

/**
 * Load data from the IndexedDB cache store.
 * Returns `null` when the key does not exist.
 */
export async function loadCacheData(
    key: string,
): Promise<ArrayBuffer | Blob | string | null> {
    const db = await openDB();
    return new Promise<ArrayBuffer | Blob | string | null>(
        (resolve, reject) => {
            const tx = db.transaction(IMAGE_STORE, "readonly");
            const store = tx.objectStore(IMAGE_STORE);
            const request = store.get(key);

            request.onsuccess = () =>
                resolve(
                    (request.result as ArrayBuffer | Blob | string) ?? null,
                );
            request.onerror = () => reject(request.error);
            tx.oncomplete = () => db.close();
        },
    );
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
