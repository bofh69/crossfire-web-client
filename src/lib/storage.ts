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

/** The parts of the Web Storage API used for the configuration. */
type ConfigStore = Pick<
  Storage,
  "getItem" | "key" | "length" | "removeItem" | "setItem"
>;

/** A {@link ConfigStore} kept in memory only; nothing is persisted. */
class MemoryConfigStore implements ConfigStore {
  private readonly items = new Map<string, string>();

  get length(): number {
    return this.items.size;
  }

  getItem(key: string): string | null {
    return this.items.get(key) ?? null;
  }

  key(index: number): string | null {
    return [...this.items.keys()][index] ?? null;
  }

  removeItem(key: string): void {
    this.items.delete(key);
  }

  setItem(key: string, value: string): void {
    this.items.set(key, value);
  }
}

/** Replaces `localStorage` when set by {@link useInMemoryConfigStore}. */
let memoryStore: MemoryConfigStore | null = null;

function configStore(): ConfigStore {
  return memoryStore ?? localStorage;
}

/** Return every prefixed key in the active config store. */
function prefixedStorageKeys(): string[] {
  const store = configStore();
  const keys: string[] = [];
  for (let i = 0; i < store.length; i++) {
    const key = store.key(i);
    if (key !== null && key.startsWith(STORAGE_PREFIX)) {
      keys.push(key);
    }
  }
  return keys;
}

/**
 * Keep the configuration in a new, empty in-memory store instead of
 * `localStorage`. Used by the replay page so that it never changes the normal
 * client's saved configuration. Call it again to start over with defaults.
 */
export function useInMemoryConfigStore(): void {
  memoryStore = new MemoryConfigStore();
}

/**
 * Save a configuration value to localStorage.
 */
export function saveConfig<T>(key: string, value: T): void {
  const fullKey = STORAGE_PREFIX + key;
  configStore().setItem(fullKey, JSON.stringify(value));
}

/**
 * Load a configuration value from localStorage, returning `defaultValue` if
 * the key does not exist or the stored JSON is malformed.
 */
export function loadConfig<T>(key: string, defaultValue: T): T {
  const fullKey = STORAGE_PREFIX + key;
  const raw = configStore().getItem(fullKey);
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
  for (const fullKey of prefixedStorageKeys()) {
    const key = fullKey.substring(STORAGE_PREFIX.length);
    const raw = configStore().getItem(fullKey);
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

  const store = configStore();
  for (const key of prefixedStorageKeys()) {
    store.removeItem(key);
  }

  for (const [key, value] of Object.entries(rawData.values)) {
    store.setItem(STORAGE_PREFIX + key, JSON.stringify(value));
  }
}

export function isConfigBackupV1(data: unknown): data is ConfigBackupV1 {
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
  for (const key of prefixedStorageKeys()) {
    configStore().removeItem(key);
  }

  // Delete the entire IndexedDB database
  await new Promise<void>((resolve, reject) => {
    const request = indexedDB.deleteDatabase(DB_NAME);
    request.onsuccess = () => resolve();
    request.onerror = () => reject(request.error);
  });
}
