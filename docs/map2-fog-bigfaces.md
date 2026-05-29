# Map2 fog-of-war and big-face handling in mapdata

This document describes how the web client processes `map2` updates for fog of
war and multi-tile ("big-face") objects.

## Map2 update flow

`Map2Cmd` in `/tmp/workspace/bofh69/crossfire-web-client/src/lib/cmd_map.ts`
parses each coordinate block and stages updates with
`mapdata_begin_cell_update(x, y)`.

Within a coordinate block:

- `type 0x0` (`MAP2_TYPE_CLEAR`) calls `clear_space()`
- `type 0x1` (`MAP2_TYPE_DARKNESS`) calls `set_darkness(value)`
- `type 0x2` (`MAP2_TYPE_LABEL`) updates labels
- `type 0x10..` updates layer faces/animations (`set_face_layer` /
  `set_anim_layer`)
- `255` ends the coordinate block and triggers `commit()`

Before each in-view commit, `mapdata_clear_old()` prepares the previous cell
state for a fresh server update.

## Fog-of-war behavior

Fog state is represented by `MapCellState.Fog` in
`/tmp/workspace/bofh69/crossfire-web-client/src/lib/mapdata.ts`.

### Entering fog

When `clear_space()` is received:

- the cell transitions to `Fog`
- the client preserves remembered data needed for fog rendering, especially
  big-face heads from the previous visible state
- per-layer `layerUpdatedAfterClear` flags are reset for the new fog cycle

`set_check_space()` may keep the cell as `Fog`/`Empty` if nothing remains after
processing that coordinate block.

### Returning from fog to visible

At `begin()` time for in-view fog cells, mapdata prepares a visible snapshot:

- non-preserved big-face heads are cleared from the working copy
- darkness is reset unless preserved post-clear big-face layers exist
- state is set to `Visible` for normal in-view updates

At `commit()` time, if the update includes regular visible content (not only
big-face bookkeeping), untouched stale fog layers are dropped so the cell
reflects the new visible snapshot.

## Big-face (multi-tile) behavior

A big face is any head face with size `>1x1`.

### Head/tail model

Mapdata stores:

- a head face at the bottom-right anchor tile
- tail coverage in neighboring tiles (`tailFace`, `tailSizeX`, `tailSizeY`)

During commit:

- previous tail coverage is cleared with `expandClearFaceFromLayer(...)`
- new tail coverage is spread to covered tiles for each big-face head

### Big-face-only update guards

`map2` can send updates that are only big-face bookkeeping (for example,
`clear_space` + big-face head hints) without normal visible content.

To avoid losing fog memory or falsely marking a cell visible, `commit()` has
big-face guards:

1. If an update is big-face-head-only and would otherwise clear meaningful
   non-big-face data, mapdata restores non-big-face layers/darkness/labels from
   the original cell and keeps the cell in `Fog`.
2. If a non-visible cell receives only a big-face head update, mapdata restores
   other layers and keeps the original non-visible state.

Updated big-face layers are tracked in `layerUpdatedAfterClear` so later
Fog→Visible transitions can preserve valid surviving big-face heads.

## Out-of-view big faces and scroll interaction

`map2` may include coordinates outside the visible viewport for big images whose
coverage extends into view.

For out-of-view updates, commit does not write normal in-view face arrays; it
updates big-face tracking structures (`bigfaces` / `activeBigfaces`) so map
scroll and redraw logic can still invalidate affected in-view tiles.

On `mapdata_scroll(...)`, mapdata marks tiles overlapped by tracked big faces
for redraw and removes big faces that moved out of range.
