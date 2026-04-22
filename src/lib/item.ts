/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file
 * TypeScript conversion of item.c / item.h – provides functions that
 * create, update, remove, and sort items (inventory objects).
 */

import {
    type Item,
    type Animation,
    type Player,
    ApplyType,
    NO_ITEM_TYPE,
    F_APPLIED,
    F_OPEN,
    F_DAMNED,
    F_CURSED,
    F_BLESSED,
    F_MAGIC,
    F_UNPAID,
    F_LOCKED,
    F_READ,
    LogLevel,
} from "./protocol";
import { SockList, CrossfireSocket } from "./newsocket";
import { LOG } from "./misc";

// ──────────────────────────────────────────────────────────────────────────────
// Item‐type classification table (parsed from old/common/item-types).
//
// Each index 0‥255 holds an array of match patterns.  A leading '^'
// means "match at the start of the name only"; otherwise the pattern
// can appear anywhere in the name.  Index 255 is reserved for
// unmatched items.
// ──────────────────────────────────────────────────────────────────────────────

const NUM_ITEM_TYPES = 256;

const itemTypes: (string[] | null)[] = new Array<string[] | null>(NUM_ITEM_TYPES).fill(null);

// Containers
itemTypes[1] = ["sack", "Luggage", "pouch", "quiver", "bag", "chest", "key ring"];
// Melee weapons
itemTypes[2] = [
    "axe", "club", "dagger", "falchion", "hammer", "katana", "mace",
    "magnifying glass", "morningstar", "nunchacu", "quarterstaff", "sabre",
    "scimitar", "shovel", "^spear", "stake", "^sword", "Belzebub's sword",
    "Firebrand", "Harakiri sword", "broadsword", "light sword",
    "Serpentman sword", "shortsword", "long sword", "taifu", "trident",
    "BoneCrusher", "Darkblade", "Demonslayer", "Dragonslayer", "Excalibur",
    "firebrand", "Firestar", "Flame Tongue", "FlameTongue", "Frost Hammer",
    "Katana of Masamune", "Lightning sticks", "Mjoellnir", "Mournblade",
    "Sting", "Stormbringer", "Trident",
];
// Ranged weapons
itemTypes[3] = ["^bow", "elven bow", "long bow", "crossbow", "sling", "arrow", "^bolt", "boulder"];
// Body armor
itemTypes[10] = ["mail", "leather", "^robe", "shirt", "apron", "hauberk"];
// Head armor
itemTypes[11] = ["helmet", "Crown", "crown"];
// Shields
itemTypes[12] = ["shield", "Demonspawn Shield"];
// Feet / hands
itemTypes[13] = ["boot", "glove", "gauntlet", "shoe"];
// Waist
itemTypes[14] = ["girdle"];
// Cloaks
itemTypes[15] = ["cloak"];
// Arms
itemTypes[16] = ["bracer"];
// Food & drink
itemTypes[20] = [
    "apple", "booze", "bread", "cabbage", "cake", "carrot", "chocolate",
    "clover", "cup ", "egg", "fish", "food", "mint sprig", "mushroom",
    "onion", "orange", "potato", "roast bird", "steak", "waybread", "^water",
];
// Gems
itemTypes[30] = ["diamond", "emerald", "gold nugget", "pearl", "ruby", "sapphire"];
// Currency
itemTypes[31] = ["coin"];
// Rods
itemTypes[45] = ["rod", "Rod"];
// Wands
itemTypes[47] = ["wand"];
// Staffs
itemTypes[48] = ["staff"];
// Horns
itemTypes[52] = ["horn"];
// Amulets
itemTypes[53] = ["amulet"];
// Rings
itemTypes[54] = ["ring", "Ring "];
// Scrolls
itemTypes[55] = ["scroll"];
// Spellbooks
itemTypes[56] = [
    "grimore", "grimoire", "hymnal", "manual", "prayerbook",
    "sacred text", "spellbook", "testiment", "treatise", "tome",
];
// Readables / books
itemTypes[57] = [
    "book", "catalog", "codex", "collection", "compendium", "compilation",
    "divine text", "divine work", "encyclopedia", "exposition", "file ",
    "formulary", "guide ", "holy book ", "holy record ", "index",
    "moral text", "notes", "note", "pamphlet", "record ", "tables",
    "transcript", "volume",
];
// Potions
itemTypes[59] = ["potion", "bottle"];
// Balms / dust / figurines
itemTypes[61] = ["^balm", "^dust", "dust ", "figurine"];
// Item building scrolls
itemTypes[63] = ["Improve", "Lower Weapon", "Enchant Weapon", "Prepare Weapon", "Enchant Armour"];
// Skill objects
itemTypes[65] = ["holy symbol", "lockpick", "talisman", "writing pen"];
// Keys
itemTypes[67] = ["key", "Key"];
// Body parts
itemTypes[70] = [
    "arm", "claw", "corpse", "dragon scale", "ectoplasm", "eye", "finger",
    "foot", "hand", "head", "Head", "heart", "icor", "leg", "lich dust",
    "liver", "orc chop", "pixie dust", "residue", "skin", "stinger",
    "tongue", "tooth", "^wing",
];
// Minerals / alchemy
itemTypes[71] = ["dirt", "lead", "mandrake root", "pile", "rock", "stone"];
// Tools / light
itemTypes[80] = ["flint and steel", "torch"];
// Misc / quest
itemTypes[90] = [
    "clock", "flower", "Gate Pass", "Glowing Crystal", "gravestone",
    "icecube", "library card", "Passport", "Port Pass", "rose",
    "Apartment Extender",
];
// Furniture
itemTypes[100] = ["chair", "table"];

// ──────────────────────────────────────────────────────────────────────────────
// Apply‐type display strings (indexed by ApplyType enum value).
// ──────────────────────────────────────────────────────────────────────────────

const applyStrings: string[] = [
    "", " (readied)", " (wielded)", " (worn)", " (active)", " (applied)",
];

// ──────────────────────────────────────────────────────────────────────────────
// Number‐to‐word helper (matches the C get_number)
// ──────────────────────────────────────────────────────────────────────────────

const numberWords: string[] = [
    "no", "a", "two", "three", "four",
    "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen",
    "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
    "twenty",
];

export function getNumber(i: number): string {
    if (i <= 20) {
        return numberWords[i]!;
    }
    return String(i);
}

// ──────────────────────────────────────────────────────────────────────────────
// Module‐level state
// ──────────────────────────────────────────────────────────────────────────────

/** Fast lookup by tag. */
const itemsByTag: Map<number, Item> = new Map();

/** Player root item (corresponds to C `player`). */
let player: Item = newItem();

/** Map / ground root item (corresponds to C `map`). */
let map: Item = newItem();
map.weight = -1;

/** External references that must be wired up by the application. */
export let csocket: CrossfireSocket | null = null;
export let cpl: Player | null = null;
export let animations: Animation[] = [];

export function setCSocket(s: CrossfireSocket): void { csocket = s; }
export function setCpl(p: Player): void { cpl = p; }
export function setAnimations(a: Animation[]): void { animations = a; }

// ──────────────────────────────────────────────────────────────────────────────
// Item event callbacks – toolkit implements these via the setters below.
// ──────────────────────────────────────────────────────────────────────────────

let itemEventItemDeleting: (it: Item) => void = () => {};
let itemEventContainerClearing: (container: Item) => void = () => {};
let itemEventItemChanged: (it: Item) => void = () => {};

export function onItemDeleting(cb: (it: Item) => void): void { itemEventItemDeleting = cb; }
export function onContainerClearing(cb: (container: Item) => void): void { itemEventContainerClearing = cb; }
export function onItemChanged(cb: (it: Item) => void): void { itemEventItemChanged = cb; }

// ──────────────────────────────────────────────────────────────────────────────
// Item creation / destruction
// ──────────────────────────────────────────────────────────────────────────────

/** Allocate and initialise a fresh Item with safe defaults. */
function newItem(): Item {
    return {
        next: null,
        prev: null,
        env: null,
        inv: null,
        dName: "",
        sName: "",
        pName: "",
        flags: "",
        tag: 0,
        nrof: 0,
        weight: 0,
        face: 0,
        animationId: 0,
        animSpeed: 0,
        animState: 0,
        lastAnim: 0,
        magical: false,
        cursed: false,
        damned: false,
        blessed: false,
        unpaid: false,
        locked: false,
        applied: false,
        open: false,
        wasOpen: false,
        read: false,
        invUpdated: false,
        applyType: ApplyType.None,
        flagsval: 0,
        type: NO_ITEM_TYPE,
    };
}

/** Recursively free all items in a linked‐list and their inventories. */
export function freeAllItems(op: Item | null): void {
    while (op) {
        if (op.inv) {
            freeAllItems(op.inv);
        }
        const next = op.next;
        itemsByTag.delete(op.tag);
        op = next;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type‐from‐name classification
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Determine the item type for sorting by matching `name` against the
 * item‐types table.  Returns 255 if no match is found.
 */
export function getTypeFromName(name: string): number {
    for (let type = 0; type < NUM_ITEM_TYPES; type++) {
        const patterns = itemTypes[type];
        if (!patterns) continue;

        for (const pattern of patterns) {
            if (pattern.startsWith("^")) {
                // Match only at the start of the name (case‐insensitive).
                const suffix = pattern.slice(1);
                if (name.toLowerCase().startsWith(suffix.toLowerCase())) {
                    return type;
                }
            } else {
                // Match anywhere in the name (case‐sensitive, per C strstr).
                if (name.includes(pattern)) {
                    return type;
                }
            }
        }
    }
    LOG(LogLevel.Warning, 'item', `getTypeFromName: Could not find match for ${name}`);
    return 255;
}

// ──────────────────────────────────────────────────────────────────────────────
// Lookup
// ──────────────────────────────────────────────────────────────────────────────

/** Locate an item by its unique tag.  Tag 0 returns the map root. */
export function locateItem(tag: number): Item | null {
    if (tag === 0) {
        return map;
    }
    return itemsByTag.get(tag) ?? null;
}

// ──────────────────────────────────────────────────────────────────────────────
// Linked‐list helpers
// ──────────────────────────────────────────────────────────────────────────────

/** Insert `newItem` immediately before `before` in its parent's child list. */
function insertItemBeforeItem(ni: Item, before: Item): void {
    if (before.prev) {
        before.prev.next = ni;
    } else if (ni.env) {
        ni.env.inv = ni;
    }

    ni.prev = before.prev;
    before.prev = ni;
    ni.next = before;

    if (ni.env) {
        ni.env.invUpdated = true;
    }
}

/** Append `op` to the end of `env`'s inventory list. */
function addItem(env: Item, op: Item): void {
    let tmp: Item | null = env.inv;
    let last: Item | null = null;
    while (tmp) {
        last = tmp;
        tmp = tmp.next;
    }

    op.next = null;
    op.prev = last;
    op.env = env;
    if (!last) {
        env.inv = op;
    } else {
        last.next = op;
    }
}

/** Create a new item with the given `tag`, insert it into `env`. */
function createNewItem(env: Item | null, tag: number): Item {
    const op = newItem();
    op.tag = tag;
    op.locked = false;
    if (env) {
        addItem(env, op);
    }
    itemsByTag.set(tag, op);
    return op;
}

// ──────────────────────────────────────────────────────────────────────────────
// Sorting
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Re‐sort an item within its parent's inventory list after its type,
 * name, or lock status changes.  Map items are never re‐sorted.
 */
export function updateItemSort(it: Item): void {
    if (!it.env || it.env === it || it.env === map) {
        return;
    }

    // Already sorted properly relative to neighbours?
    if (it.prev && it.prev.type === it.type &&
        it.prev.locked === it.locked &&
        it.prev.sName.toLowerCase() === it.sName.toLowerCase()) {
        return;
    }
    if (it.next && it.next.type === it.type &&
        it.next.locked === it.locked &&
        it.next.sName.toLowerCase() === it.sName.toLowerCase()) {
        return;
    }

    // Remove from current position.
    if (it.prev) {
        it.prev.next = it.next;
    }
    if (it.next) {
        it.next.prev = it.prev;
    }
    if (it.env.inv === it) {
        it.env.inv = it.next;
    }

    let last: Item | null = null;
    for (let itmp: Item | null = it.env.inv; itmp; itmp = itmp.next) {
        last = itmp;

        if (itmp.type > it.type) {
            insertItemBeforeItem(it, itmp);
            return;
        } else if (itmp.type === it.type) {
            // Alphabetical within same type.
            if (itmp.sName.toLowerCase() < it.sName.toLowerCase()) {
                continue;
            }
            insertItemBeforeItem(it, itmp);
            return;
        }
    }

    // No match – append at end.
    if (last) {
        last.next = it;
    } else {
        it.env.inv = it;
    }
    it.prev = last;
    it.next = null;
}

// ──────────────────────────────────────────────────────────────────────────────
// Flag helpers
// ──────────────────────────────────────────────────────────────────────────────

function setFlagString(op: Item): void {
    let f = "";
    if (op.locked) f += " *";
    if (op.applyType) {
        f += (op.applyType < applyStrings.length)
            ? applyStrings[op.applyType]
            : " (undefined)";
    }
    if (op.open) f += " (open)";
    if (op.damned) f += " (damned)";
    if (op.cursed) f += " (cursed)";
    if (op.blessed) f += " (blessed)";
    if (op.magical) f += " (magic)";
    if (op.unpaid) f += " (unpaid)";
    if (op.read) f += " (read)";
    op.flags = f;
}

function getFlags(op: Item, flags: number): void {
    op.wasOpen  = op.open;
    op.open     = !!(flags & F_OPEN);
    op.damned   = !!(flags & F_DAMNED);
    op.cursed   = !!(flags & F_CURSED);
    op.blessed  = !!(flags & F_BLESSED);
    op.magical  = !!(flags & F_MAGIC);
    op.unpaid   = !!(flags & F_UNPAID);
    op.applied  = !!(flags & F_APPLIED);
    op.locked   = !!(flags & F_LOCKED);
    op.read     = !!(flags & F_READ);
    op.flagsval = flags;
    op.applyType = flags & F_APPLIED;
    setFlagString(op);
}

// ──────────────────────────────────────────────────────────────────────────────
// set / update / remove
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Populate an item with the given attributes.
 *
 * If `name` is non‐empty, the singular and plural names are extracted
 * (the server sends them as two NUL‐separated strings) and the display
 * name is rebuilt.
 */
export function setItemValues(
    op: Item,
    name: string,
    weight: number,
    face: number,
    flags: number,
    anim: number,
    animspeed: number,
    nrof: number,
    type: number,
): void {
    if (!op) {
        LOG(LogLevel.Error, 'item', 'setItemValues: item pointer is null');
        return;
    }

    // Internally we always have at least 1 item.
    if (nrof === 0) nrof = 1;

    let resort = true;
    if (name.length > 0) {
        // The server sends two NUL‐separated strings in the name buffer:
        //   singular_name \0 plural_name
        // In the C code these are split via strlen(name)+1.  In TypeScript
        // the TextDecoder preserves the NUL, so we split on '\0'.
        const nulIdx = name.indexOf('\0');
        if (nulIdx >= 0) {
            op.sName = name.substring(0, nulIdx);
            op.pName = name.substring(nulIdx + 1);
        } else {
            op.sName = name;
            op.pName = name;
        }
        // Force display‐name rebuild by making nrof differ.
        op.nrof = nrof + 1;
    } else {
        resort = false;
    }

    if (op.nrof !== nrof) {
        op.dName = (nrof !== 1) ? `${getNumber(nrof)} ${op.pName}` : op.sName;
        op.nrof = nrof;
    }

    if (op.env) {
        op.env.invUpdated = true;
    }
    op.weight = weight / 1000;
    op.face = face;
    op.animationId = anim;
    op.animSpeed = animspeed;
    op.type = type;
    getFlags(op, flags);

    if (op.env !== map && op.type === NO_ITEM_TYPE) {
        op.type = getTypeFromName(op.sName);
    }
    if (resort) {
        updateItemSort(op);
    }

    itemEventItemChanged(op);
}

/**
 * Remove an item from its parent's inventory and from the tag map.
 */
export function removeItem(op: Item | null): void {
    if (!op || op === player || op === map) {
        return;
    }

    itemEventItemDeleting(op);

    if (op.env) {
        op.env.invUpdated = true;
    }

    // Remove children unless this is the open container.
    if (op.inv && (!cpl || cpl.container !== op)) {
        removeItemInventory(op);
    }

    if (op.prev) {
        op.prev.next = op.next;
    } else if (op.env) {
        op.env.inv = op.next;
    }
    if (op.next) {
        op.next.prev = op.prev;
    }

    // Don't delete the open container from the map.
    if (cpl && cpl.container === op) {
        return;
    }

    itemsByTag.delete(op.tag);
}

/** Recursively remove all children of `op`. */
export function removeItemInventory(op: Item | null): void {
    if (!op) {
        return;
    }

    itemEventContainerClearing(op);

    op.invUpdated = true;
    while (op.inv) {
        removeItem(op.inv);
    }
}

/**
 * Toggle the locked flag on an item and send the command to the server.
 */
export function toggleLocked(op: Item): void {
    if (!op.env || op.env.tag === 0) {
        return; // item on the ground – don't lock
    }
    if (!csocket) return;

    const sl = new SockList();
    sl.addString("lock ");
    sl.addChar(op.locked ? 0 : 1);
    sl.addInt(op.tag);
    csocket.send(sl);
}

/** Send a "mark" command to the server for the given item. */
export function sendMarkObj(op: Item): void {
    if (!op.env || op.env.tag === 0) {
        return;
    }
    if (!csocket) return;

    const sl = new SockList();
    sl.addString("mark ");
    sl.addInt(op.tag);
    csocket.send(sl);
}

/**
 * Send an "inscribe" command to write a spell onto a scroll.
 *
 * @param spellTag  Tag of the spell object to inscribe.
 * @param scrollTag Tag of the scroll to write the spell onto.
 */
export function sendInscribe(spellTag: number, scrollTag: number): void {
    if (!csocket) return;

    const sl = new SockList();
    sl.addString("inscribe ");
    sl.addChar(0);       // version: only supported value is 0
    sl.addInt(spellTag);
    sl.addInt(scrollTag);
    csocket.send(sl);
}

// ──────────────────────────────────────────────────────────────────────────────
// Root item accessors
// ──────────────────────────────────────────────────────────────────────────────

/** (Re‑)initialise and return the player root item. */
export function playerItem(): Item {
    player = newItem();
    itemsByTag.delete(0); // clean stale tag‑0 if any
    return player;
}

/**
 * Register the player root item in the tag lookup map so that
 * `locateItem(playerTag)` returns it.  Called by PlayerCmd after the
 * server tells us the player's tag.
 */
export function registerPlayerTag(tag: number): void {
    // Remove any previous player tag registration.
    if (player.tag !== 0) {
        itemsByTag.delete(player.tag);
    }
    player.tag = tag;
    itemsByTag.set(tag, player);
}

/** (Re‑)initialise and return the map / ground root item. */
export function mapItem(): Item {
    map = newItem();
    map.weight = -1;
    return map;
}

// ──────────────────────────────────────────────────────────────────────────────
// High‐level update (called when the server sends an item2 / upditem)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Create or update an item.  If `tag` matches the player item the
 * player is updated directly; otherwise the item is looked up (or
 * created) in the normal inventory tree.
 */
export function updateItem(
    tag: number,
    loc: number,
    name: string,
    weight: number,
    face: number,
    flags: number,
    anim: number,
    animspeed: number,
    nrof: number,
    type: number,
): void {
    if (player.tag === tag) {
        player.dName = name;
        player.nrof = nrof;
        player.weight = weight / 1000;
        player.face = face;
        getFlags(player, flags);
        if (player.inv) {
            player.inv.invUpdated = true;
        }
        player.animationId = anim;
        player.animSpeed = animspeed;
        player.nrof = nrof;
        return;
    }

    let ip = locateItem(tag);
    const env = locateItem(loc);

    if (ip && ip.env !== env) {
        removeItem(ip);
        ip = null;
    }
    if (!ip) {
        ip = createNewItem(env, tag);
    }
    setItemValues(ip, name, weight, face, flags, anim, animspeed, nrof, type);

    // Track open container state: when an item transitions to open,
    // record it as the active container; when it closes, clear it.
    if (cpl) {
        if (ip.open && !ip.wasOpen) {
            cpl.container = ip;
        } else if (!ip.open && ip.wasOpen) {
            if (cpl.container === ip) {
                cpl.container = null;
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Animation tick
// ──────────────────────────────────────────────────────────────────────────────

/** Advance animated item faces by one tick. */
function animateList(head: Item | null): boolean {
    let gotOne = false;
    for (let ip: Item | null = head; ip; ip = ip.next) {
        if (ip.animationId > 0 && ip.animSpeed) {
            ip.lastAnim++;
            if (ip.lastAnim >= ip.animSpeed) {
                ip.animState++;
                const anim = animations[ip.animationId];
                if (anim && ip.animState >= anim.numAnimations) {
                    ip.animState = 0;
                }
                if (anim) {
                    ip.face = anim.faces[ip.animState]!;
                }
                ip.lastAnim = 0;
                gotOne = true;
            }
        }
    }
    return gotOne;
}

/**
 * Called once per tick to advance animation frames for items in the
 * player's inventory, an open container, or the map / look window.
 */
export function animateObjects(): void {
    // Player inventory
    if (animateList(player.inv)) {
        player.invUpdated = true;
    }

    if (cpl?.container) {
        // Open container
        if (animateList(cpl.container.inv)) {
            cpl.container.invUpdated = true;
        }
    } else if (cpl?.below) {
        // Ground / look window
        if (animateList(cpl.below.inv)) {
            cpl.below.invUpdated = true;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Utility: debug inventory dump
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Return the face number of the first item in the player's inventory whose
 * `dName` matches `name`, or `undefined` if no match is found.
 */
export function findPlayerItemFaceByName(name: string): number | undefined {
    for (let item: Item | null = player.inv; item; item = item.next) {
        if (item.dName === name) {
            return item.face;
        }
    }
    return undefined;
}

/** Print an item's inventory tree to the console (debug aid). */
export function printInventory(op: Item, indent: number = 0): void {
    const pad = " ".repeat(indent);
    LOG(LogLevel.Debug, 'item', `${pad}${op.dName} (tag=${op.tag}, weight=${op.weight.toFixed(1)} kg)`);
    for (let tmp: Item | null = op.inv; tmp; tmp = tmp.next) {
        const line = `${pad}  - ${tmp.nrof} ${tmp.dName}${tmp.flags} (${tmp.tag})`;
        LOG(LogLevel.Debug, 'item', `${line}  ${(tmp.nrof * tmp.weight).toFixed(1)} kg`);
        if (tmp.inv) {
            printInventory(tmp, indent + 2);
        }
    }
}
