<script lang="ts">
  import { tick, onMount } from "svelte";
  import {
    clientSendApply,
    clientSendExamine,
    clientSendMove,
  } from "../lib/player";
  import { toggleLocked, locateItem, sendMarkObj } from "../lib/item";
  import { getFaceUrl } from "../lib/image";
  import { getCpl, useConfig } from "../lib/init";
  import type { Item } from "../lib/protocol";
  import { F_UNIDENTIFIED } from "../lib/protocol";
  import { gameEvents } from "../lib/events";
  import { setHotbarSlot } from "../lib/hotbar";
  import ContextMenu from "./ContextMenu.svelte";
  import HotbarSlotPicker from "./HotbarSlotPicker.svelte";
  import MenuPrompt from "./MenuPrompt.svelte";
  import { playerStats } from "../lib/commands";
  import { charConfigKey, loadConfig, saveConfig } from "../lib/storage";
  import { capitalizeFirstLetter } from "../lib/misc";
  import { focusUiNavTarget, isUiNavEnabled } from "../lib/ui_nav";
  import { recordUiInteraction } from "../lib/websocket-recording";

  interface FlatItem {
    tag: number;
    fullName: string;
    name: string;
    dName: string;
    sName: string;
    /** Weight in kg of the whole stack (or container with contents); < 0 if not pickable. */
    weight: number;
    nrof: number;
    face: number;
    locked: boolean;
    applied: boolean;
    magical: boolean;
    cursed: boolean;
    damned: boolean;
    unpaid: boolean;
    open: boolean;
    depth: number;
    unidentified: boolean;
  }

  type InvFilter =
    | "applied"
    | "unapplied"
    | "unpaid"
    | "cursed"
    | "magical"
    | "nonmagical"
    | "locked"
    | "unlocked"
    | "unidentified";

  interface FilterDef {
    id: InvFilter;
    icon: string;
    tooltip: string;
    test: (i: FlatItem) => boolean;
  }

  const FILTERS: FilterDef[] = [
    {
      id: "applied",
      icon: "✋",
      tooltip: "Applied items",
      test: (i) => i.applied,
    },
    {
      id: "unapplied",
      icon: "🤚",
      tooltip: "Unapplied items",
      test: (i) => !i.applied,
    },
    {
      id: "unpaid",
      icon: "💰",
      tooltip: "Unpaid items",
      test: (i) => i.unpaid,
    },
    {
      id: "cursed",
      icon: "💀",
      tooltip: "Cursed / damned items",
      test: (i) => i.cursed || i.damned,
    },
    {
      id: "magical",
      icon: "✨",
      tooltip: "Magical items",
      test: (i) => i.magical,
    },
    {
      id: "nonmagical",
      icon: "🔰",
      tooltip: "Non-magical items",
      test: (i) => !i.magical,
    },
    {
      id: "locked",
      icon: "🔒",
      tooltip: "Locked items",
      test: (i) => i.locked,
    },
    {
      id: "unlocked",
      icon: "🔓",
      tooltip: "Unlocked items (including open containers)",
      test: (i) => !i.locked || i.open,
    },
    {
      id: "unidentified",
      icon: "❓",
      tooltip: "Unidentified items",
      test: (i) => i.unidentified,
    },
  ];

  // ── Inventory/ground split resize ────────────────────────────────
  const MIN_INV_FRAC = 0.1;
  const MAX_INV_FRAC = 0.9;

  let invContainerEl = $state<HTMLDivElement | undefined>();
  let invSplitFrac = $state(loadConfig<number>("layout_invSplitFrac", 0.5));
  let isDraggingInv = $state(false);

  function handleInvSplitStart(e: MouseEvent) {
    e.preventDefault();
    const container = invContainerEl!;
    const startFrac = invSplitFrac;
    const startY = e.clientY;
    const containerH = container.getBoundingClientRect().height;

    isDraggingInv = true;
    document.body.style.cursor = "row-resize";
    document.body.style.userSelect = "none";

    function onMove(me: MouseEvent) {
      const delta = me.clientY - startY;
      const newFrac = startFrac + delta / containerH;
      invSplitFrac = Math.max(MIN_INV_FRAC, Math.min(MAX_INV_FRAC, newFrac));
    }

    function onUp() {
      isDraggingInv = false;
      document.body.style.cursor = "";
      document.body.style.userSelect = "";
      saveConfig("layout_invSplitFrac", invSplitFrac);
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    }

    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup", onUp);
  }

  let playerItems: FlatItem[] = $state([]);
  let groundItems: FlatItem[] = $state([]);
  let invFilters = $state<Set<InvFilter>>(new Set());
  let filteredPlayerItems = $derived(
    invFilters.size === 0
      ? playerItems
      : playerItems.filter((item) =>
          FILTERS.some((f) => invFilters.has(f.id) && f.test(item)),
        ),
  );
  // Only top-level rows: a container's weight already includes its contents.
  let totalInventoryWeight = $derived(
    playerItems.reduce(
      (sum, item) => (item.depth === 0 ? sum + Math.max(0, item.weight) : sum),
      0,
    ),
  );
  let weightLimit = $state(playerStats.weightLimit / 1000);

  /** Key used to persist filters for a given character name. */
  function filtersKey(charName: string): string {
    return charConfigKey("inv_filters_char", charName);
  }

  /** Load the saved filter set for `charName` from localStorage. */
  function loadFiltersForChar(charName: string): void {
    const saved = loadConfig<InvFilter[]>(filtersKey(charName), []);
    const validIds = new Set(FILTERS.map((f) => f.id));
    invFilters = new Set(
      saved.filter((id): id is InvFilter => validIds.has(id)),
    );
  }

  /** Persist the current filter set for the current character. */
  function saveFilters(charName: string): void {
    saveConfig(filtersKey(charName), [...invFilters]);
  }

  /** Toggle a single filter on/off and save. */
  function toggleFilter(id: InvFilter): void {
    const next = new Set(invFilters);
    if (next.has(id)) {
      next.delete(id);
    } else {
      next.add(id);
    }
    recordUiInteraction("filterClick", {
      selector: `[data-ui-nav-id="ui-inventory-filter-${id}"]`,
      group: "inventory",
      id,
    });
    invFilters = next;
    const charName = getCpl()?.ob?.dName ?? "";
    if (charName) saveFilters(charName);
  }

  /** Track the last-seen character name so we load filters exactly once per character. */
  let currentCharName = "";
  let contextMenu = $state<{
    isGround: boolean;
    item: FlatItem;
    targetId: string;
    x: number;
    y: number;
  } | null>(null);
  let itemCount = $state(0);
  let showSlotPicker = $state(false);
  /** The "how many?" prompt shown in the context menu, if any. */
  let countPrompt = $state<{
    label: string;
    onConfirm: (count: number) => void;
  } | null>(null);

  /** How long a first click on an item waits for the second click that applies it. */
  const CLICK_APPLY_CONFIRM_MS = 3000;
  /** Tag of the item clicked once and waiting for a second click to apply. */
  let armedApplyTag = $state<number | null>(null);
  let armedApplyTimer: ReturnType<typeof setTimeout> | null = null;

  /** Element refs for preserving scroll positions across inventory updates. */
  let playerListEl: HTMLElement | null = null;
  let groundListEl: HTMLElement | null = null;

  /**
   * Return the visible item label, including the pluralized count when needed.
   */
  function getItemDisplayName(item: Item): string {
    return item.nrof > 1
      ? `${item.nrof} ${item.pName || item.dName}`
      : item.dName;
  }

  /**
   * Join attribute names into one parenthesized list with `&` before the final
   * value.
   */
  function formatAttributeList(attributes: string[]): string {
    if (attributes.length === 0) return "";
    if (attributes.length === 1) return ` (${attributes[0]})`;
    if (attributes.length === 2)
      return ` (${attributes[0]} & ${attributes[1]})`;
    return ` (${attributes.slice(0, -1).join(", ")} & ${attributes.at(-1)})`;
  }

  /**
   * Collapse repeated trailing ` (text)` segments into one grouped list.
   *
   * Example: `name (a) (b) (c)` => `name (a, b & c)`.
   */
  function normalizeTrailingAttributes(name: string): string {
    const tailMatch = /((?: \([^()]+\))+)$/.exec(name);
    if (!tailMatch) return name;

    const tail = tailMatch[1]!;
    const attributes = [...tail.matchAll(/ \(([^()]+)\)/g)].map(
      (match) => match[1]!,
    );
    if (attributes.length <= 1) return name;

    const prefix = name.slice(0, -tail.length);
    return `${prefix}${formatAttributeList(attributes)}`;
  }

  /** Return the full item label, including server-provided status suffixes. */
  function getItemFullName(item: Item): string {
    return normalizeTrailingAttributes(
      `${getItemDisplayName(item)}${item.flags}`,
    );
  }

  /**
   * Total weight of an item stack in kg. The server sends the weight of a
   * single item (containers include their contents), so multiply by `nrof`.
   */
  function stackWeight(item: Item): number {
    if (item.weight < 0) return item.weight;
    return item.weight * Math.max(1, item.nrof);
  }

  /** Flatten inventory tree nodes into a linear list for rendering. */
  function flattenItems(root: Item | null, depth = 0): FlatItem[] {
    const result: FlatItem[] = [];
    let item = root?.inv ?? null;
    while (item) {
      result.push({
        tag: item.tag,
        fullName: getItemFullName(item),
        name: getItemDisplayName(item),
        dName: item.dName,
        sName: item.sName,
        weight: stackWeight(item),
        nrof: item.nrof,
        face: item.face,
        locked: item.locked,
        applied: item.applied,
        magical: item.magical,
        cursed: item.cursed,
        damned: item.damned,
        unpaid: item.unpaid,
        open: item.open,
        depth,
        unidentified: !!(item.flagsval & F_UNIDENTIFIED),
      });
      // If the container is open, show its contents indented below it.
      if (item.open && item.inv) {
        result.push(...flattenItems(item, depth + 1));
      }
      item = item.next;
    }
    return result;
  }

  async function refreshInventory() {
    const playerRoot = getCpl()?.ob ?? null;
    const groundRoot = locateItem(0);

    // When the character name becomes available (or changes), load their saved filters.
    const charName = getCpl()?.ob?.dName ?? "";
    if (charName && charName !== currentCharName) {
      currentCharName = charName;
      loadFiltersForChar(charName);
    }

    const playerScrollTop = playerListEl?.scrollTop ?? 0;
    const groundScrollTop = groundListEl?.scrollTop ?? 0;

    playerItems = flattenItems(playerRoot);
    groundItems = flattenItems(groundRoot);
    itemCount = getCpl()?.count ?? 0;

    // Restore scroll positions after Svelte flushes DOM changes, so that
    // inserting/removing items (e.g. moving out of a container) doesn't jump
    // the list to an unexpected position.
    await tick();
    if (playerListEl) playerListEl.scrollTop = playerScrollTop;
    if (groundListEl) groundListEl.scrollTop = groundScrollTop;
  }

  onMount(() => {
    const cleanups = [
      gameEvents.on("playerUpdate", refreshInventory),
      // The typed item count changes without an event; polling it is cheap.
      gameEvents.on("tick", () => {
        itemCount = getCpl()?.count ?? 0;
      }),
      gameEvents.on("statsUpdate", () => {
        weightLimit = playerStats.weightLimit / 1000;
      }),
      gameEvents.on("uiNavTargetChanged", (targetId) => {
        if (
          contextMenu &&
          targetId !== contextMenu.targetId &&
          !targetId?.startsWith("ui-context-menu-")
        ) {
          closeContextMenu();
        }
      }),
    ];
    return () => {
      for (const unsub of cleanups) unsub();
      disarmApply();
    };
  });

  function handleApply(tag: number) {
    clientSendApply(tag);
  }

  function handleExamine(item: FlatItem) {
    clientSendExamine(item.tag);
    contextMenu = null;
  }

  /**
   * Apply an item clicked in a list.  When click confirmation is on, the
   * first click only arms the item and a second click applies it.
   */
  function handleRowClick(item: FlatItem) {
    if (!useConfig.confirmClickApply || armedApplyTag === item.tag) {
      disarmApply();
      handleApply(item.tag);
      return;
    }
    disarmApply();
    armedApplyTag = item.tag;
    armedApplyTimer = setTimeout(disarmApply, CLICK_APPLY_CONFIRM_MS);
  }

  /** Forget an item waiting for a second click. */
  function disarmApply() {
    if (armedApplyTimer !== null) clearTimeout(armedApplyTimer);
    armedApplyTimer = null;
    armedApplyTag = null;
  }

  /** The UI-nav target ID of an item row. */
  function rowNavId(item: FlatItem, isGround: boolean): string {
    return isGround
      ? `ui-ground-item-${item.tag}`
      : `ui-inventory-item-${item.tag}`;
  }

  /**
   * The row to focus after moving `count` of `item` (0 = all): the item
   * itself when part of the stack stays, otherwise the next item at the same
   * level, the previous one, or the enclosing container.
   */
  function rowNavIdAfterMove(
    item: FlatItem,
    count: number,
    isGround: boolean,
  ): string | null {
    if (count > 0 && count < item.nrof) return rowNavId(item, isGround);
    const rows = isGround ? groundItems : filteredPlayerItems;
    const index = rows.findIndex((row) => row.tag === item.tag);
    if (index < 0) return null;
    // Deeper rows right below the item are its own contents and move with it.
    let after = index + 1;
    while (after < rows.length && rows[after]!.depth > item.depth) after++;
    let target = rows[after]?.depth === item.depth ? rows[after] : undefined;
    for (let i = index - 1; !target && i >= 0; i--) {
      if (rows[i]!.depth <= item.depth) target = rows[i];
    }
    target ??= rows[after];
    return target ? rowNavId(target, isGround) : null;
  }

  /**
   * Move `count` of `item` (0 = all) to `loc`.  In UI-nav mode, stay in it
   * and focus the next item, so several items can be moved in a row.
   */
  function moveItem(
    item: FlatItem,
    loc: number,
    count: number,
    isGround: boolean,
  ) {
    const stayInUiNav = isUiNavEnabled();
    const focusId = stayInUiNav
      ? rowNavIdAfterMove(item, count, isGround)
      : null;
    clientSendMove(loc, item.tag, count, stayInUiNav);
    clearCount();
    closeContextMenu();
    if (focusId) void tick().then(() => focusUiNavTarget(focusId));
  }

  function handleDrop(item: FlatItem, count = getMoveCount()) {
    // Move item to ground (loc = 0)
    moveItem(item, 0, count, false);
  }

  function handlePickup(item: FlatItem, count = getMoveCount()) {
    // Move item from ground to player (loc = player tag)
    const playerTag = getCpl()?.ob?.tag ?? 0;
    moveItem(item, playerTag, count, true);
  }

  /** Ask how many of `item` to act on, then call `onConfirm` with the count. */
  function promptCount(label: string, onConfirm: (count: number) => void) {
    showSlotPicker = false;
    countPrompt = { label, onConfirm };
  }

  function handleLock(item: FlatItem) {
    const realItem = locateItem(item.tag);
    if (realItem) {
      toggleLocked(realItem);
    }
    contextMenu = null;
  }

  function handleMark(item: FlatItem) {
    const realItem = locateItem(item.tag);
    if (realItem) {
      sendMarkObj(realItem);
    }
    contextMenu = null;
  }

  /** Move an item between the player inventory and the open container. */
  function handleMoveToContainer(item: FlatItem, count = getMoveCount()) {
    const cpl = getCpl();
    const container = cpl?.container;
    if (!container) {
      contextMenu = null;
      return;
    }
    // If the item is currently in the container, move it to the player inventory.
    // Otherwise move it into the container.
    const realItem = locateItem(item.tag);
    const loc =
      realItem?.env?.tag === container.tag
        ? (cpl?.ob?.tag ?? 0)
        : container.tag;
    moveItem(item, loc, count, false);
  }

  function handleContextMenu(e: MouseEvent, item: FlatItem, isGround: boolean) {
    e.preventDefault();
    showSlotPicker = false;
    countPrompt = null;
    disarmApply();
    // Place the menu so the cursor sits slightly inside the top-left corner.
    contextMenu = {
      isGround,
      item,
      targetId: rowNavId(item, isGround),
      x: e.clientX - 8,
      y: e.clientY - 8,
    };
  }

  function closeContextMenu() {
    contextMenu = null;
    showSlotPicker = false;
    countPrompt = null;
  }

  function handleAddToHotbar(_item: FlatItem) {
    countPrompt = null;
    showSlotPicker = true;
  }

  function handleContextMenuActivate() {
    if (!contextMenu) {
      return;
    }
    handleApply(contextMenu.item.tag);
    closeContextMenu();
  }

  function handleSlotSelected(index: number) {
    if (contextMenu) {
      const item = contextMenu.item;
      setHotbarSlot(index, {
        label: item.sName,
        command: `apply ${item.sName}`,
        face: item.face,
        tag: item.tag,
        itemName: item.sName,
      });
    }
    closeContextMenu();
  }

  function formatWeight(w: number): string {
    if (w < 0) return ``;
    if (w < 1) return `${(w * 1000).toFixed(0)}g`;
    return `${w.toFixed(1)}kg`;
  }

  /** Return the number of items to move: use cpl.count if set, otherwise 0 (all). */
  function getMoveCount(): number {
    const cpl = getCpl();
    return cpl && cpl.count > 0 ? cpl.count : 0;
  }

  /** Reset the typed item count to zero. */
  function clearCount() {
    const cpl = getCpl();
    if (cpl) cpl.count = 0;
    itemCount = 0;
  }
</script>

{#snippet weightOrApplyHint(item: FlatItem)}
  {#if armedApplyTag === item.tag}
    <span class="apply-hint" role="status">Click again to apply</span>
  {:else}
    <span class="item-weight">{formatWeight(item.weight)}</span>
  {/if}
{/snippet}

<!-- svelte-ignore a11y_click_events_have_key_events -->
<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="inventory" bind:this={invContainerEl}>
  <div class="inv-section" style:flex="{invSplitFrac} 0 0">
    <h3
      data-ui-nav-entry="inventory"
      data-ui-nav-id="ui-panel-inventory"
      data-ui-nav-panel="inventory"
    >
      <span>
        Inventory ({filteredPlayerItems.length}{invFilters.size > 0
          ? `/${playerItems.length}`
          : ""})
      </span>
      <span class="inv-header-right">
        <span
          class="inv-total-weight"
          title={weightLimit > 0
            ? `Carrying ${formatWeight(totalInventoryWeight)}, weight limit ${formatWeight(weightLimit)}`
            : "Total carried weight"}
        >
          {formatWeight(totalInventoryWeight)}
        </span>
        {#if itemCount > 0}
          <span class="item-count">
            {itemCount}
            <!-- svelte-ignore a11y_click_events_have_key_events -->
            <!-- svelte-ignore a11y_no_static_element_interactions -->
            <span class="count-clear" onclick={clearCount}>✕</span>
          </span>
        {/if}
      </span>
    </h3>
    <div class="inv-filter-bar" role="toolbar" aria-label="Inventory filter">
      {#each FILTERS as f (f.id)}
        <button
          class="inv-filter-btn"
          class:active={invFilters.has(f.id)}
          data-ui-nav-id={`ui-inventory-filter-${f.id}`}
          data-ui-nav-group="inventory-filters"
          data-ui-nav-group-policy="horizontal"
          data-ui-nav-panel="inventory"
          title={f.tooltip}
          aria-label={f.tooltip}
          aria-pressed={invFilters.has(f.id)}
          onclick={() => toggleFilter(f.id)}
        >
          {f.icon}
        </button>
      {/each}
    </div>
    <div class="item-list" bind:this={playerListEl}>
      {#each filteredPlayerItems as item (item.tag)}
        <div
          class="item-row"
          class:cursed={item.cursed || item.damned}
          class:magical={item.magical}
          class:applied={item.applied}
          class:armed={armedApplyTag === item.tag}
          data-ui-nav-down-target={item === filteredPlayerItems.at(-1) &&
          groundItems.length > 0
            ? `ui-ground-item-${groundItems[0]!.tag}`
            : undefined}
          data-ui-nav-id={`ui-inventory-item-${item.tag}`}
          data-ui-nav-group="inventory-items"
          data-ui-nav-group-policy="vertical"
          data-ui-nav-menu="context"
          data-ui-nav-panel="inventory"
          style:padding-left="{0.4 + item.depth * 1}rem"
          title={capitalizeFirstLetter(item.fullName)}
          onclick={() => handleRowClick(item)}
          oncontextmenu={(e: MouseEvent) => handleContextMenu(e, item, false)}
        >
          {#if getFaceUrl(item.face)}
            <img src={getFaceUrl(item.face)} alt="" class="item-icon" />
          {:else}
            <span class="item-icon-placeholder">?</span>
          {/if}
          <span class="item-name">
            {capitalizeFirstLetter(item.name)}
            {#if item.open}📂{/if}
            {#if item.locked}🔒{/if}
            {#if item.applied}*{/if}
            {#if item.unpaid}(unpaid){/if}
          </span>
          {@render weightOrApplyHint(item)}
        </div>
      {/each}
    </div>
  </div>

  <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
  <div
    class="inv-resize-handle"
    class:dragging={isDraggingInv}
    role="separator"
    aria-label="Resize inventory split"
    aria-orientation="horizontal"
    onmousedown={handleInvSplitStart}
  ></div>

  <div class="inv-section" style:flex="{1 - invSplitFrac} 0 0">
    <h3 data-ui-nav-id="ui-panel-ground" data-ui-nav-panel="inventory">
      Ground ({groundItems.length})
    </h3>
    <div class="item-list" bind:this={groundListEl}>
      {#each groundItems as item (item.tag)}
        <div
          class="item-row"
          class:cursed={item.cursed || item.damned}
          class:magical={item.magical}
          class:armed={armedApplyTag === item.tag}
          data-ui-nav-id={`ui-ground-item-${item.tag}`}
          data-ui-nav-group="ground-items"
          data-ui-nav-group-policy="vertical"
          data-ui-nav-menu="context"
          data-ui-nav-panel="inventory"
          style:padding-left="{0.4 + item.depth * 1}rem"
          title={capitalizeFirstLetter(item.fullName)}
          onclick={() => handleRowClick(item)}
          oncontextmenu={(e: MouseEvent) => handleContextMenu(e, item, true)}
        >
          {#if getFaceUrl(item.face)}
            <img src={getFaceUrl(item.face)} alt="" class="item-icon" />
          {:else}
            <span class="item-icon-placeholder">?</span>
          {/if}
          <span class="item-name">
            {capitalizeFirstLetter(item.name)}
            {#if item.open}📂{/if}
          </span>
          {@render weightOrApplyHint(item)}
        </div>
      {/each}
    </div>
  </div>

  {#if contextMenu}
    {#snippet contextMenuContent()}
      {@const item = contextMenu!.item}
      {@const isGround = contextMenu!.isGround}
      {@const openContainer = getCpl()?.container ?? null}
      {@const realItem = locateItem(item.tag)}
      {@const inContainer =
        openContainer !== null && realItem?.env?.tag === openContainer.tag}
      {#snippet menuItem(label: string, action: () => void)}
        <button
          onclick={action}
          oncontextmenu={(e) => {
            e.preventDefault();
            action();
          }}
        >
          {label}
        </button>
      {/snippet}
      <button
        onclick={handleContextMenuActivate}
        oncontextmenu={(e) => {
          e.preventDefault();
          handleContextMenuActivate();
        }}
      >
        Apply
      </button>
      {#if isGround}
        <button
          onclick={() => contextMenu && handleExamine(contextMenu.item)}
          oncontextmenu={(e) => {
            e.preventDefault();
            contextMenu && handleExamine(contextMenu.item);
          }}
        >
          Examine
        </button>
        <button
          onclick={() => contextMenu && handlePickup(contextMenu.item)}
          oncontextmenu={(e) => {
            e.preventDefault();
            contextMenu && handlePickup(contextMenu.item);
          }}
        >
          Pickup
        </button>
        {#if item.nrof > 1}
          {@render menuItem("Pick up N…", () =>
            promptCount("Pick up how many?", (n) => handlePickup(item, n)),
          )}
        {/if}
      {:else}
        {#if openContainer !== null}
          <button
            onclick={() =>
              contextMenu && handleMoveToContainer(contextMenu.item)}
            oncontextmenu={(e) => {
              e.preventDefault();
              contextMenu && handleMoveToContainer(contextMenu.item);
            }}
          >
            {inContainer ? "Move to inventory" : "Move to container"}
          </button>
          {#if item.nrof > 1}
            {@render menuItem(
              inContainer ? "Move N to inventory…" : "Move N to container…",
              () =>
                promptCount("Move how many?", (n) =>
                  handleMoveToContainer(item, n),
                ),
            )}
          {/if}
        {/if}
        <button
          onclick={() => contextMenu && handleDrop(contextMenu.item)}
          oncontextmenu={(e) => {
            e.preventDefault();
            contextMenu && handleDrop(contextMenu.item);
          }}
        >
          Drop
        </button>
        {#if item.nrof > 1}
          {@render menuItem("Drop N…", () =>
            promptCount("Drop how many?", (n) => handleDrop(item, n)),
          )}
        {/if}
        <button
          onclick={() => contextMenu && handleExamine(contextMenu.item)}
          oncontextmenu={(e) => {
            e.preventDefault();
            contextMenu && handleExamine(contextMenu.item);
          }}
        >
          Examine
        </button>
        <button
          onclick={() => contextMenu && handleLock(contextMenu.item)}
          oncontextmenu={(e) => {
            e.preventDefault();
            contextMenu && handleLock(contextMenu.item);
          }}
        >
          {item.locked ? "Unlock" : "Lock"}
        </button>
        <button
          onclick={() => contextMenu && handleMark(contextMenu.item)}
          oncontextmenu={(e) => {
            e.preventDefault();
            contextMenu && handleMark(contextMenu.item);
          }}
        >
          Mark
        </button>
        <button
          onclick={() => contextMenu && handleAddToHotbar(contextMenu.item)}
          oncontextmenu={(e) => {
            e.preventDefault();
            contextMenu && handleAddToHotbar(contextMenu.item);
          }}
        >
          Add to hotbar…
        </button>
        {#if showSlotPicker}
          <HotbarSlotPicker
            onSelect={handleSlotSelected}
            onCancel={closeContextMenu}
          />
        {/if}
      {/if}
      {#if countPrompt}
        <MenuPrompt
          label={countPrompt.label}
          max={item.nrof}
          onConfirm={(count) => countPrompt?.onConfirm(Number(count))}
          onCancel={closeContextMenu}
        />
      {/if}
    {/snippet}
    <ContextMenu x={contextMenu.x} y={contextMenu.y} onClose={closeContextMenu}>
      {@render contextMenuContent()}
    </ContextMenu>
  {/if}
</div>

<style>
  .inventory {
    background: var(--bg-panel);
    border: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    height: 100%;
    overflow: hidden;
    position: relative;
  }

  .inv-section {
    display: flex;
    flex-direction: column;
    min-height: 0;
    overflow: hidden;
  }

  .inv-resize-handle {
    height: 6px;
    flex-shrink: 0;
    cursor: row-resize;
    background: transparent;
    transition: background 0.15s;
  }

  .inv-resize-handle:hover,
  .inv-resize-handle.dragging {
    background: rgba(122, 106, 74, 0.35);
  }
  h3 {
    margin: 0;
    padding: 0.3rem 0.4rem;
    color: var(--text-warm);
    font-size: 0.8rem;
    background: var(--bg-mid);
    display: flex;
    align-items: center;
  }

  .item-count {
    display: flex;
    align-items: center;
    gap: 0.2rem;
    color: #ffcc66;
    font-size: 0.75rem;
    font-weight: normal;
  }

  .count-clear {
    cursor: pointer;
    color: var(--text-dim);
    font-size: 0.7rem;
    padding: 0 0.15rem;
    border-radius: 2px;
  }

  .count-clear:hover {
    color: var(--danger-text);
    background: var(--bg-card);
  }

  .inv-header-right {
    margin-left: auto;
    display: flex;
    align-items: center;
    gap: 0.45rem;
  }

  .inv-total-weight {
    color: var(--text-dim);
    font-size: 0.72rem;
    font-weight: normal;
  }

  .item-list {
    flex: 1;
    overflow-y: auto;
    font-size: 0.78rem;
  }

  .item-row {
    display: flex;
    align-items: center;
    gap: 0.3rem;
    padding: 0.2rem 0.4rem;
    cursor: pointer;
    color: var(--text);
  }

  .item-row:hover {
    background: var(--bg-lighter);
  }

  .item-row.applied {
    color: #aaccff;
  }

  .item-row.magical {
    color: #88ccff;
    background: #1a2535;
  }

  .item-row.cursed {
    color: var(--danger-text);
    background: #351a1a;
  }

  .item-row.armed {
    outline: 1px solid var(--accent);
    outline-offset: -1px;
  }

  .item-icon {
    width: 24px;
    height: 24px;
    image-rendering: pixelated;
    flex-shrink: 0;
  }

  .item-icon-placeholder {
    width: 24px;
    height: 24px;
    display: flex;
    align-items: center;
    justify-content: center;
    background: var(--border);
    color: #666;
    font-size: 0.7rem;
    flex-shrink: 0;
  }

  .item-name {
    flex: 1;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .item-weight {
    color: var(--text-dim);
    font-size: 0.7rem;
    flex-shrink: 0;
  }

  .apply-hint {
    color: var(--text-warm);
    font-size: 0.7rem;
    flex-shrink: 0;
  }

  .inv-filter-bar {
    display: flex;
    flex-wrap: wrap;
    gap: 0;
    padding: 0.1rem 0.1rem;
    background: var(--bg-mid);
    border-bottom: 1px solid var(--border);
    flex-shrink: 0;
  }

  .inv-filter-btn {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 3px;
    color: var(--text-dim);
    cursor: pointer;
    font-size: 0.8rem;
    line-height: 1;
    padding: 0.15rem 0.16rem;
    transition:
      background 0.1s,
      color 0.1s,
      border-color 0.1s;
    filter: grayscale(1) opacity(0.55);
  }

  .inv-filter-btn:hover {
    border-color: var(--border-mid);
    color: var(--text-bright);
  }

  .inv-filter-btn.active {
    background: var(--bg-lighter);
    color: var(--text);
    border-color: transparent;
    filter: none;
  }

  .inv-filter-btn.active:hover {
    border-color: var(--border-mid);
  }
</style>
