/**
 * Client-side storage utilities for the Crossfire web client.
 * Replaces C file I/O with localStorage (config/keybindings) and
 * IndexedDB (image cache).
 */

const STORAGE_PREFIX = "crossfire_";
const DB_NAME = "crossfire_cache";
const CONFIG_BACKUP_MAGIC = "crossfire-web-client-config-backup";
const CONFIG_BACKUP_VERSION = 1;

export interface ConfigBackupV1 {
  magic: typeof CONFIG_BACKUP_MAGIC;
  version: typeof CONFIG_BACKUP_VERSION;
  values: Record<string, unknown>;
}

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

export function exportConfigBackup(): ConfigBackupV1 {
  const values: Record<string, unknown> = {};
  const localStorageKeys = Object.keys(localStorage);
  for (const fullKey of localStorageKeys) {
    if (!fullKey.startsWith(STORAGE_PREFIX)) continue;
    const key = fullKey.substring(STORAGE_PREFIX.length);
    const raw = localStorage.getItem(fullKey);
    if (raw === null) continue;
    try {
      values[key] = JSON.parse(raw) as unknown;
    } catch {
      values[key] = raw;
    }
  }
  return {
    magic: CONFIG_BACKUP_MAGIC,
    version: CONFIG_BACKUP_VERSION,
    values,
  };
}

export function importConfigBackup(rawData: unknown): void {
  if (!isConfigBackupV1(rawData)) {
    throw new Error("Invalid configuration backup file.");
  }

  const keysToRemove: string[] = [];
  const localStorageKeys = Object.keys(localStorage);
  for (const key of localStorageKeys) {
    if (key.startsWith(STORAGE_PREFIX)) {
      keysToRemove.push(key);
    }
  }
  for (const key of keysToRemove) {
    localStorage.removeItem(key);
  }

  for (const [key, value] of Object.entries(rawData.values)) {
    localStorage.setItem(STORAGE_PREFIX + key, JSON.stringify(value));
  }
}

function isConfigBackupV1(data: unknown): data is ConfigBackupV1 {
  if (!data || typeof data !== "object") return false;
  const obj = data as Record<string, unknown>;
  if (obj.magic !== CONFIG_BACKUP_MAGIC) return false;
  if (obj.version !== CONFIG_BACKUP_VERSION) return false;
  if (!obj.values || typeof obj.values !== "object") return false;
  if (Array.isArray(obj.values)) return false;
  return true;
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
