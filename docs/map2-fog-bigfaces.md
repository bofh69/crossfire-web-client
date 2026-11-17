# Map2 fog-of-war and big-face handling in mapdata

This document describes how the web client processes `map2` updates for fog of
war and multi-tile ("big-face") objects.

## Map2 update flow

`Map2Cmd` in `src/lib/cmd_map.ts`
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

After the whole packet has been applied, `Map2Cmd` calls
`mapdata_drop_unrefreshed_big_faces()` (see
[Big faces the server no longer shows](#big-faces-the-server-no-longer-shows)).

## Fog-of-war behavior

Fog state is represented by `MapCellState.Fog` in `src/lib/mapdata.ts`.

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
- tail coverage in neighboring tiles (`tailFace`, `tailSizeX`, `tailSizeY`),
  where `(tailSizeX, tailSizeY)` is the offset from the tail to its head
- `headCurrent`, a per-layer flag on the head, see
  [Which big faces are current](#which-big-faces-are-current)

The renderer draws each tail from that tail's own data, not by looking up the
head. A tail left behind after its head is gone is therefore still drawn, so
all code that removes a big face must also remove every tail pointing to it.

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

### Which big faces are current

The server sends a big face whenever any tile it covers is visible to the
player, even when its head is in a cell the player cannot see. A big face is
therefore either:

- **current**: the server has sent it and has not cleared it since, so it is
  still there, or
- **remembered**: it is only kept for fog rendering, and the object may have
  moved or disappeared since it was last seen.

`headCurrent[idx * L + layer]` tracks this for heads inside the view:

- It is set to 1 when the server sends a face for that layer (the layer is
  dirty in the update), and to 0 if that face is 0.
- It is reset to 0 when the cell gets a `clear_space`, or when the head face
  changes without being sent (for example cleared by another update).
- Otherwise the value from before the update is kept. The flags are
  snapshotted in `begin()`, because clearing the old faces during `commit()`
  resets them before the head is written back.
- It is reset when a head is cleared (`expandClearFace`) or the cell is
  cleared (`mapdataClear`), and is moved along with the other per-layer arrays
  when the virtual map is recentered.

Heads just outside the view are kept in `bigfaces[]`. The server sends those
only while they are current, so a head found there counts as current.

### Big faces the server no longer shows

`mapdata_drop_unrefreshed_big_faces()` runs after every `map2` packet. For each
visible in-view cell, it checks every layer's tail: if the tail's head is not
current (`isCurrentBigFaceHead()`), the server would have sent that face had it
still been there. The object is gone, so `removeBigFace()` removes the whole
big face: every tail that points to the head, including tails in fog cells,
and the head itself.

This removes, for example, the old image of a monster that moved while the
player could not see its head: as soon as any of its old tiles becomes
visible, the image is dropped instead of lingering in fog next to the new one.

### Moved creatures seen again

A creature can also move while every tile of its old image is outside the view
or in fog. No visible cell then covers the old image, so the rule above never
removes it, and when the creature is seen again its old and new images would
both be drawn.

Creatures cannot overlap each other, so when a big face is written on a
creature layer (`FIRST_CREATURE_LAYER`, layer 6, and up),
`clearOverlappedRememberedBigFaces()` removes any big face on the same layer
whose footprint overlaps the new one and whose head is only remembered
(`isRememberedHead()`: outside the view or in fog). It is called both from
`commit()`, for visible in-view heads, and from `expandSetBigface()`, for heads
just outside the view. It must run before the new tails are written, since it
finds the old faces through the tails they left in the new footprint.

A creature that moved further than its own size since it was last seen does not
overlap its old image, and is not handled.

### Dangling tails

When a fog cell becomes visible again, `commit()` drops the remembered layers
the server did not resend. A remembered tail on such a layer is cleared only if
it is dangling (`isDanglingTail()`): no big face with the same face is found at
its head position, neither in the virtual map nor in `bigfaces[]`. Only that
cell's tail is cleared; whatever head is now at the target position is left
alone.

Similarly, `expandClearBigface()` only clears the in-view tail mirrored from a
`bigfaces[]` head if the tail still belongs to that head, since an in-view face
may have taken the cell over since.

## Out-of-view big faces and scroll interaction

`map2` may include coordinates outside the visible viewport for big images whose
coverage extends into view.

For out-of-view updates, commit does not write normal in-view face arrays; it
updates big-face tracking structures (`bigfaces` / `activeBigfaces`) so map
scroll and redraw logic can still invalidate affected in-view tiles.

On `mapdata_scroll(...)`, mapdata marks tiles overlapped by tracked big faces
for redraw and removes big faces that moved out of range.
