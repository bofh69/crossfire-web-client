import { beforeEach, describe, expect, test, vi } from "vitest";

type Storage = typeof import("../src/lib/storage");

/** Make a minimal `localStorage` holding `items`. */
function fakeLocalStorage(items: Record<string, string>) {
  const store = new Map(Object.entries(items));
  return {
    get length() {
      return store.size;
    },
    getItem: (key: string) => store.get(key) ?? null,
    key: (index: number) => [...store.keys()][index] ?? null,
    removeItem: (key: string) => void store.delete(key),
    setItem: (key: string, value: string) => void store.set(key, value),
    items: store,
  };
}

/** A config backup with the given values. */
function backup(values: Record<string, unknown>) {
  return {
    magic: "crossfire-web-client-config-backup",
    version: 1,
    values,
  };
}

/** Old key names and the values stored under them. */
const OLD_VALUES = {
  cf_keybindings: [{ keysym: "a", flags: "N", command: "apply" }],
  "cf_keybindings_char_Mr Foo": [{ keysym: "b", flags: "N", command: "b" }],
  cf_gamepad_bindings_Pad_1: { buttons: [] },
  cf_gamepad_char_buttons_Pad_1_Mr_Foo: [{ button: 1, command: "x" }],
  hotbar_slots_char_Mr_Foo: [{ label: "L", command: "cast" }],
  "inv_filters_Mr Foo": ["weapons"],
  tileScale: 2,
};

/** The new key names for OLD_VALUES. */
const NEW_VALUES = {
  keybindings: OLD_VALUES.cf_keybindings,
  keybindings_char_Mr_Foo: OLD_VALUES["cf_keybindings_char_Mr Foo"],
  gamepad_Pad_1: OLD_VALUES.cf_gamepad_bindings_Pad_1,
  gamepad_char_Pad_1_Mr_Foo: OLD_VALUES.cf_gamepad_char_buttons_Pad_1_Mr_Foo,
  hotbar_char_Mr_Foo: OLD_VALUES.hotbar_slots_char_Mr_Foo,
  inv_filters_char_Mr_Foo: OLD_VALUES["inv_filters_Mr Foo"],
  tileScale: 2,
  storage_version: 1,
};

describe("config key migration", () => {
  let storage: Storage;

  beforeEach(async () => {
    vi.resetModules();
    vi.unstubAllGlobals();
    storage = await import("../src/lib/storage");
  });

  test("renames old keys in localStorage on first use", () => {
    const local = fakeLocalStorage(
      Object.fromEntries(
        Object.entries(OLD_VALUES).map(([k, v]) => [
          "crossfire_" + k,
          JSON.stringify(v),
        ]),
      ),
    );
    vi.stubGlobal("localStorage", local);
    expect(storage.loadConfig("keybindings", null)).toEqual(
      OLD_VALUES.cf_keybindings,
    );
    expect(storage.exportConfigBackup().values).toEqual(NEW_VALUES);
  });

  test("renames old keys in an imported backup", () => {
    storage.useInMemoryConfigStore();
    storage.importConfigBackup(backup(OLD_VALUES));
    expect(storage.exportConfigBackup().values).toEqual(NEW_VALUES);
  });

  test("keeps a value already stored under the new key", () => {
    storage.useInMemoryConfigStore();
    storage.importConfigBackup(
      backup({ cf_keybindings: ["old"], keybindings: ["new"] }),
    );
    expect(storage.loadConfig("keybindings", null)).toEqual(["new"]);
    expect(storage.loadConfig("cf_keybindings", null)).toBeNull();
  });

  test("leaves a migrated backup unchanged", () => {
    storage.useInMemoryConfigStore();
    storage.importConfigBackup(backup(NEW_VALUES));
    expect(storage.exportConfigBackup().values).toEqual(NEW_VALUES);
  });

  test("charConfigKey sanitises the character name", () => {
    expect(storage.charConfigKey("hotbar_char", "Mr Foo!")).toBe(
      "hotbar_char_Mr_Foo_",
    );
  });
});
