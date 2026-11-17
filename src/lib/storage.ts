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
  /** Snapshot of the keys for key(); rebuilt after the keys change. */
  private keyList: string[] | null = null;

  get length(): number {
    return this.items.size;
  }

  getItem(key: string): string | null {
    return this.items.get(key) ?? null;
  }

  key(index: number): string | null {
    this.keyList ??= [...this.items.keys()];
    return this.keyList[index] ?? null;
  }

  removeItem(key: string): void {
    if (this.items.delete(key)) this.keyList = null;
  }

  setItem(key: string, value: string): void {
    if (!this.items.has(key)) this.keyList = null;
    this.items.set(key, value);
  }
}

/** Replaces `localStorage` when set by {@link useInMemoryConfigStore}. */
let memoryStore: MemoryConfigStore | null = null;

/** The store that {@link migrateConfigKeys} last ran on. */
let migratedStore: ConfigStore | null = null;

function configStore(): ConfigStore {
  const store = memoryStore ?? localStorage;
  if (store !== migratedStore) {
    migratedStore = store;
    migrateConfigKeys(store);
  }
  return store;
}

/** Return every prefixed key in `store`. */
function prefixedStorageKeys(store: ConfigStore = configStore()): string[] {
  const keys: string[] = [];
  for (let i = 0; i < store.length; i++) {
    const key = store.key(i);
    if (key !== null && key.startsWith(STORAGE_PREFIX)) {
      keys.push(key);
    }
  }
  return keys;
}

// ──────────────────────────────────────────────────────────────────────────────
// Key migrations
// ──────────────────────────────────────────────────────────────────────────────

/** Config key holding the number of {@link KEY_MIGRATIONS} applied. */
const STORAGE_VERSION_KEY = "storage_version";

/**
 * Renames of old config keys, one function per storage version, applied in
 * order.  Each returns the new key (without the prefix) for an old key, or
 * null to keep the key.
 */
const KEY_MIGRATIONS: ReadonlyArray<(key: string) => string | null> = [
  // Version 1: per-character keys are "<feature>_char_<sanitised name>".
  (key) => {
    if (key === "cf_keybindings") return "keybindings";
    const renames: ReadonlyArray<[string, string, boolean]> = [
      // [old prefix, new prefix, whether the rest is an unsanitised name]
      ["cf_gamepad_bindings_", "gamepad_", false],
      ["cf_gamepad_char_buttons_", "gamepad_char_", false],
      ["cf_keybindings_char_", "keybindings_char_", true],
      ["hotbar_slots_char_", "hotbar_char_", false],
      ["inv_filters_", "inv_filters_char_", true],
    ];
    for (const [oldPrefix, newPrefix, sanitise] of renames) {
      if (key.startsWith(oldPrefix)) {
        const rest = key.substring(oldPrefix.length);
        return newPrefix + (sanitise ? sanitiseKeySegment(rest) : rest);
      }
    }
    return null;
  },
];

/**
 * Rename the old keys in `store` to the current names, unless that has
 * already been done.  A renamed value never replaces an existing one.
 */
function migrateConfigKeys(store: ConfigStore): void {
  const versionKey = STORAGE_PREFIX + STORAGE_VERSION_KEY;
  const version = Number(store.getItem(versionKey)) || 0;
  if (version >= KEY_MIGRATIONS.length) return;
  for (const migrate of KEY_MIGRATIONS.slice(version)) {
    for (const fullKey of prefixedStorageKeys(store)) {
      const newKey = migrate(fullKey.substring(STORAGE_PREFIX.length));
      if (newKey === null) continue;
      const value = store.getItem(fullKey);
      store.removeItem(fullKey);
      const newFullKey = STORAGE_PREFIX + newKey;
      if (value !== null && store.getItem(newFullKey) === null) {
        store.setItem(newFullKey, value);
      }
    }
  }
  store.setItem(versionKey, JSON.stringify(KEY_MIGRATIONS.length));
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
 * Make `name` safe to use as part of a config key, by replacing unusual
 * characters with "_" and limiting the length.
 */
export function sanitiseKeySegment(name: string): string {
  return name.replace(/[^a-zA-Z0-9_-]/g, "_").substring(0, 80);
}

/**
 * The config key for a per-character setting: `prefix`, "_" and the
 * sanitised `charName`.
 */
export function charConfigKey(prefix: string, charName: string): string {
  return `${prefix}_${sanitiseKeySegment(charName)}`;
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
  // Backups from older versions use the old key names.
  migrateConfigKeys(store);
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
