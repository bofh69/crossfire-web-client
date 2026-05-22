# UI navigation behavior and layout tuning

This document explains how UI navigation works in the web client, and what to
change when a layout update causes different navigation results.

## What UI navigation mode is

UI navigation mode lets players move focus through visible UI controls without
using the mouse.

- Default key binding to enter: `Tab` (`ui_nav` command)
- Exit commands: `ui_nav_exit` or `Escape`
- Open context menu for focused item: `ui_nav_menu` or keyboard menu key
- While in UI-nav mode: `Tab`/`Shift+Tab` cycle between major components
  (menubar, hotbar/tabs, active tab content, info filters), not per-row entries

## How focus targets are discovered

Target collection is implemented in `src/lib/ui_nav.ts`.

The navigation system scans for:

1. Any element with `data-ui-nav-id`
2. Any `<button>` that does not have `data-ui-nav-skip="true"`

Only visible, enabled, connected elements are kept. If a button has no
`data-ui-nav-id`, one is auto-generated (`ui-auto-target-*`).

## How initial focus is chosen

When `ui_nav` starts, focus is restored in this order:

1. Explicit target passed to `ui_nav [target]`
2. Current target (if still visible)
3. Last target
4. Last panel fallback
5. Default target (`menubar`)
6. First visible target in DOM order

Target aliases are resolved by app callbacks in `src/App.svelte` (for example
`inventory`, `spells`, `menubar`).

## Cross-component navigation rules

Each UI region navigates within itself along its primary axis. Directional input
on the other axis, or at the edge of the region, moves focus to an adjacent
region. The routing is implemented in the `resolveNavBoundary` callback in
`src/App.svelte`.

### Horizontal regions (left/right navigate within; up/down cross to other regions)

| Region          | Up                            | Down                                                                   | Left boundary | Right boundary |
| --------------- | ----------------------------- | ---------------------------------------------------------------------- | ------------- | -------------- |
| **Menubar**     | —                             | First open dropdown item (if any) → Hotbar (if visible) → Info filters | Info filters  | Tab headers    |
| **Hotbar**      | Menubar                       | Info filters                                                           | Info filters  | Tab headers    |
| **Tab headers** | Hotbar (if visible) → Menubar | See below                                                              | wraps         | wraps          |

**Tab headers → Down** depends on the active tab:

- Inventory tab → first Inventory filter icon
- Any other tab → first entry in that tab's list

When tab overflow is open (Protect/Quests/Know menu), pressing Enter on the
overflow tab keeps up/down within the overflow options first; moving down from
the bottom overflow option enters the selected tab's first list entry. Pressing
Down on the overflow tab header also opens the overflow menu.

### Vertical regions (up/down navigate within; left/right cross to other regions)

| Region                                          | Up boundary                   | Down boundary                                                     | Left         | Right                  |
| ----------------------------------------------- | ----------------------------- | ----------------------------------------------------------------- | ------------ | ---------------------- |
| **Info filters**                                | Hotbar (if visible) → Menubar | —                                                                 | —            | See below              |
| **Inventory filters**                           | Tab headers                   | First inventory item                                              | —            | —                      |
| **Inventory items**                             | Inventory filters             | First ground item (static `data-ui-nav-down-target` on last item) | Info filters | First ground item      |
| **Ground items**                                | Last inventory item           | —                                                                 | Info filters | —                      |
| **Spell/Skill/Protection/Quest/Knowledge list** | Tab headers                   | —                                                                 | Info filters | Last item of same list |

**Info filters → Right** depends on the active tab:

- Inventory tab → first Ground item (or first Inventory item if ground is empty)
- Any other tab → first entry in that tab's list

## Why layout changes can change navigation results

### Within-group movement

When a target has `data-ui-nav-group` and `data-ui-nav-group-policy`:

- Movement in the constrained axis stays within the group
- At the group boundary the `resolveNavBoundary` callback is consulted first,
  then the static `data-ui-nav-*-target` attribute, then sequential wrap

### Cross-axis movement

When a target is in a group and movement is on the unconstrained axis, the
`resolveNavBoundary` callback is consulted before falling back to geometric
picking and then sequential movement.

### Sequential (geometry-free) fallback

Sequential movement and geometry-based picking both use DOM order. Because of this:

- Reordering elements in the DOM changes fallback navigation order
- Hiding/showing sections can change which target becomes the next fallback
- Adding new buttons can unexpectedly join navigation unless skipped

## Focus visibility and menu close behavior

- When focus moves to a list entry inside a scrollable list, the navigation
  system scrolls enough to keep that entry fully visible (including first/last
  entries and sticky-header lists).
- If UI navigation focus moves to another target, open menus are closed
  automatically (menu bar dropdowns, and tab overflow unless focus is staying on
  the overflow tab/overflow options).
- Moving left/right between MenuBar buttons closes any open MenuBar dropdown.
- Moving away from a Hotbar slot closes that slot's context menu.
- If a list-entry context menu is open and focus moves to a different non-menu
  target, that list-entry context menu is closed.
- Pressing `Enter` on a context-menu-enabled list entry opens its context menu.
- If a list-entry context menu has no further up/down menu item to move to,
  up/down returns to the originating list and moves to the adjacent list entry
  instead of falling through to unrelated UI targets.
- If UI-nav exits while focus is inside a context menu, the remembered focus is
  restored to the originating non-menu list target (or another visible target in
  that same list group if the original entry was removed).

## Attributes that control navigation results

Use these attributes on focusable UI elements to stabilize behavior after layout
changes.

- `data-ui-nav-down-target`: explicit target ID when moving down at a boundary
- `data-ui-nav-entry`: marks panel entry points used by high-level target aliases
- `data-ui-nav-group`: groups related targets (filters, list rows, tabs)
- `data-ui-nav-group-policy`: `both`, `horizontal`, or `vertical`
- `data-ui-nav-id`: stable target identity used for focus restore and jumps
- `data-ui-nav-left-target`: explicit target ID when moving left at a boundary
- `data-ui-nav-menu="context"`: enables menu action for that target
- `data-ui-nav-panel`: panel membership for panel fallback behavior
- `data-ui-nav-right-target`: explicit target ID when moving right at a boundary
- `data-ui-nav-skip="true"`: exclude auto-collected buttons from UI navigation
- `data-ui-nav-up-target`: explicit target ID when moving up at a boundary

## Cross-component routing callback

The `UiNavCallbacks.resolveNavBoundary` callback in `src/App.svelte` receives:

- `panel` – the `data-ui-nav-panel` value of the focused element
- `group` – the `data-ui-nav-group` value (or null)
- `direction` – the requested move direction

It returns a target ID to jump to, or `null` to fall through to default behavior.

Two helper functions exported from `src/lib/ui_nav.ts` are used in the callback:

- `firstVisibleInGroup(group)` – first visible element in a group
- `lastVisibleInGroup(group)` – last visible element in a group

## Practical guidance when changing layout

If navigation feels wrong after a UI refactor:

1. Ensure every important target has a stable `data-ui-nav-id`
2. Set `data-ui-nav-panel` consistently for each panel
3. Add `data-ui-nav-group` + `data-ui-nav-group-policy` for lists and tab bars
4. Update `resolveNavBoundary` in `src/App.svelte` if cross-component routes change
5. Add directional boundary targets (`*-target`) for static per-element overrides
6. Add `data-ui-nav-skip="true"` to decorative or non-navigation buttons
7. Verify entry points in `src/App.svelte` still resolve to the intended IDs

## Related files

- `src/lib/ui_nav.ts` (navigation engine, `firstVisibleInGroup`, `lastVisibleInGroup`)
- `src/App.svelte` (`setUiNavCallbacks` — target aliases and `resolveNavBoundary`)
- `src/lib/p_cmd.ts` (`ui_nav`, `ui_nav_exit`, `ui_nav_menu` commands)
- `src/app.css` (`.ui-nav-active-target`, `body.ui-nav-mode` visuals)
