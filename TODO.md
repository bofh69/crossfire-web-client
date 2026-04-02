# Migration TODO

- [x] Move existing files (except AUTHORS) to `old/` directory
- [x] Setup new Vite + Svelte + TypeScript project in root
- [x] Copy `old/pixmaps/*.gif` and `old/pixmaps/*.png` to `src/assets/`
- [x] Create `INSTRUCTIONS.md` with development/test/deploy instructions
- [x] Convert `old/common/client.c` → `src/lib/client.ts`
- [x] Convert `old/common/commands.c` → `src/lib/commands.ts`
- [x] Convert `old/common/image.c` → `src/lib/image.ts`
- [x] Convert `old/common/init.c` → `src/lib/init.ts`
- [x] Convert `old/common/item.c` → `src/lib/item.ts`
- [x] Convert `old/common/mapdata.c` → `src/lib/mapdata.ts`
- [x] Convert `old/common/misc.c` → `src/lib/misc.ts`
- [x] Convert `old/common/newsocket.c` → `src/lib/newsocket.ts`
- [x] Convert `old/common/p_cmd.c` → `src/lib/p_cmd.ts`
- [x] Convert `old/common/player.c` → `src/lib/player.ts`
- [x] Convert `old/common/def-keys` → `src/lib/keys.ts` (as JS function)
- [x] Create `src/lib/protocol.ts` with shared protocol constants
- [x] Create `src/lib/storage.ts` for client-side storage
- [x] Convert GTK components to Svelte: App, Login, CreateChar
- [x] Convert GTK components to Svelte: Map, Info, Stats
- [x] Convert GTK components to Svelte: Inventory, Spells, Skills
- [x] Convert GTK components to Svelte: MenuBar, Pickup, Config, MagicMap
- [x] Create main entry point and integrate all components
- [x] Verify the project builds and runs

