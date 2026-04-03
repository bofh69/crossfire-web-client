/**
 * Client initialization for the Crossfire web client.
 * Port of old/common/init.c configuration and startup logic.
 */

import type { Player } from "./protocol";
import {
    CS_NUM_SKILLS,
    InputState,
    CONFIG_APPLY_CONTAINER,
    CONFIG_AUTO_AFK,
    CONFIG_CACHE,
    CONFIG_CWINDOW,
    CONFIG_DARKNESS,
    CONFIG_DEBOUNCE,
    CONFIG_DISPLAYMODE,
    CONFIG_DOWNLOAD,
    CONFIG_ECHO,
    CONFIG_FASTTCP,
    CONFIG_FOGWAR,
    CONFIG_FOODBEEP,
    CONFIG_GRAD_COLOR,
    CONFIG_ICONSCALE,
    CONFIG_INV_MENU,
    CONFIG_LIGHTING,
    CONFIG_MAPHEIGHT,
    CONFIG_MAPSCALE,
    CONFIG_MAPSCROLL,
    CONFIG_MAPWIDTH,
    CONFIG_MUSIC_VOL,
    CONFIG_NUMS,
    CONFIG_POPUPS,
    CONFIG_PORT,
    CONFIG_RESISTS,
    CONFIG_SERVER_TICKS,
    CONFIG_SHOWGRID,
    CONFIG_SHOWICON,
    CONFIG_SIGNPOPUP,
    CONFIG_SMOOTH,
    CONFIG_SOUND,
    CONFIG_SPLASH,
    CONFIG_SPLITINFO,
    CONFIG_SPLITWIN,
    CONFIG_TIMESTAMP,
    CONFIG_TOOLTIPS,
    CONFIG_TRIMINFO,
    CFG_DM_PIXMAP,
    CFG_LT_PIXEL,
    COMMAND_WINDOW,
    EPORT,
    MAX_SKILL,
} from "./protocol";

import { loadConfig, saveConfig } from "./storage";
import { playerItem, mapItem, setCpl as setCplInItem } from "./item";
import { setCpl as setCplInPlayer } from "./player";

// ──────────────────────────────────────────────────────────────────────────────
// Configuration name/value arrays
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Human-readable names for each CONFIG index.
 * Indices match the CONFIG_* constants from protocol.ts.
 * Index 0 is unused (CONFIG constants start at 1).
 */
export const configNames: string[] = [
    "",                       // 0  (unused)
    "download_all_images",    // 1  CONFIG_DOWNLOAD
    "echo_bindings",          // 2  CONFIG_ECHO
    "fasttcpsend",            // 3  CONFIG_FASTTCP
    "command_window",         // 4  CONFIG_CWINDOW
    "cacheimages",            // 5  CONFIG_CACHE
    "fog_of_war",             // 6  CONFIG_FOGWAR
    "iconscale",              // 7  CONFIG_ICONSCALE
    "mapscale",               // 8  CONFIG_MAPSCALE
    "popups",                 // 9  CONFIG_POPUPS
    "displaymode",            // 10 CONFIG_DISPLAYMODE
    "showicon",               // 11 CONFIG_SHOWICON
    "tooltips",               // 12 CONFIG_TOOLTIPS
    "sound",                  // 13 CONFIG_SOUND
    "splitinfo",              // 14 CONFIG_SPLITINFO
    "split",                  // 15 CONFIG_SPLITWIN
    "show_grid",              // 16 CONFIG_SHOWGRID
    "lighting",               // 17 CONFIG_LIGHTING
    "trim_info_window",       // 18 CONFIG_TRIMINFO
    "map_width",              // 19 CONFIG_MAPWIDTH
    "map_height",             // 20 CONFIG_MAPHEIGHT
    "foodbeep",               // 21 CONFIG_FOODBEEP
    "darkness",               // 22 CONFIG_DARKNESS
    "port",                   // 23 CONFIG_PORT
    "grad_color_bars",        // 24 CONFIG_GRAD_COLOR
    "resistances",            // 25 CONFIG_RESISTS
    "smoothing",              // 26 CONFIG_SMOOTH
    "nosplash",               // 27 CONFIG_SPLASH
    "auto_apply_container",   // 28 CONFIG_APPLY_CONTAINER
    "mapscroll",              // 29 CONFIG_MAPSCROLL
    "sign_popups",            // 30 CONFIG_SIGNPOPUP
    "message_timestamping",   // 31 CONFIG_TIMESTAMP
    "auto_afk",               // 32 CONFIG_AUTO_AFK
    "inv_menu",               // 33 CONFIG_INV_MENU
    "music_vol",              // 34 CONFIG_MUSIC_VOL
    "server_ticks",           // 35 CONFIG_SERVER_TICKS
    "debounce",               // 36 CONFIG_DEBOUNCE
];

/** Desired configuration values. */
export const wantConfig: number[] = new Array<number>(CONFIG_NUMS).fill(0);

/** Active configuration values (may differ from wantConfig during negotiation). */
export const useConfig: number[] = new Array<number>(CONFIG_NUMS).fill(0);

// ──────────────────────────────────────────────────────────────────────────────
// Configuration defaults
// ──────────────────────────────────────────────────────────────────────────────

const TRUE = 1;
const FALSE = 0;

/**
 * Populate {@link wantConfig} with built-in defaults and copy them into
 * {@link useConfig}. Matches the C `init_config()` function.
 */
function initConfig(): void {
    wantConfig[CONFIG_APPLY_CONTAINER] = TRUE;
    wantConfig[CONFIG_CACHE] = FALSE;
    wantConfig[CONFIG_CWINDOW] = COMMAND_WINDOW;
    wantConfig[CONFIG_DARKNESS] = TRUE;
    wantConfig[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
    wantConfig[CONFIG_DOWNLOAD] = FALSE;
    wantConfig[CONFIG_ECHO] = FALSE;
    wantConfig[CONFIG_FASTTCP] = TRUE;
    wantConfig[CONFIG_FOGWAR] = TRUE;
    wantConfig[CONFIG_FOODBEEP] = FALSE;
    wantConfig[CONFIG_GRAD_COLOR] = FALSE;
    wantConfig[CONFIG_ICONSCALE] = 100;
    wantConfig[CONFIG_LIGHTING] = CFG_LT_PIXEL;
    wantConfig[CONFIG_MAPHEIGHT] = 20;
    wantConfig[CONFIG_MAPSCALE] = 100;
    wantConfig[CONFIG_MAPSCROLL] = TRUE;
    wantConfig[CONFIG_MAPWIDTH] = 20;
    wantConfig[CONFIG_POPUPS] = FALSE;
    wantConfig[CONFIG_PORT] = EPORT;
    wantConfig[CONFIG_RESISTS] = 0;
    wantConfig[CONFIG_SHOWGRID] = FALSE;
    wantConfig[CONFIG_SHOWICON] = FALSE;
    wantConfig[CONFIG_SIGNPOPUP] = TRUE;
    wantConfig[CONFIG_SMOOTH] = FALSE;
    wantConfig[CONFIG_SOUND] = TRUE;
    wantConfig[CONFIG_SPLASH] = TRUE;
    wantConfig[CONFIG_SPLITINFO] = FALSE;
    wantConfig[CONFIG_SPLITWIN] = FALSE;
    wantConfig[CONFIG_TIMESTAMP] = FALSE;
    wantConfig[CONFIG_TOOLTIPS] = TRUE;
    wantConfig[CONFIG_TRIMINFO] = FALSE;
    wantConfig[CONFIG_AUTO_AFK] = 300;
    wantConfig[CONFIG_INV_MENU] = TRUE;
    wantConfig[CONFIG_MUSIC_VOL] = 100;
    wantConfig[CONFIG_SERVER_TICKS] = FALSE;
    wantConfig[CONFIG_DEBOUNCE] = TRUE;

    for (let i = 0; i < CONFIG_NUMS; i++) {
        useConfig[i] = wantConfig[i];
    }
}

/**
 * Load any user-overridden config values from localStorage and merge them
 * into {@link wantConfig} / {@link useConfig}.
 */
function loadSavedConfig(): void {
    for (let i = 1; i < CONFIG_NUMS; i++) {
        const name = configNames[i];
        if (name) {
            const saved = loadConfig<number | null>(`config_${name}`, null);
            if (saved !== null) {
                wantConfig[i] = saved;
                useConfig[i] = saved;
            }
        }
    }
}

/**
 * Persist the current {@link wantConfig} values to localStorage so they
 * survive page reloads.
 */
export function saveCurrentConfig(): void {
    for (let i = 1; i < CONFIG_NUMS; i++) {
        const name = configNames[i];
        if (name) {
            saveConfig(`config_${name}`, wantConfig[i]);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Player data
// ──────────────────────────────────────────────────────────────────────────────

/** Skill experience values for the current player. */
export const skillExp: number[] = new Array<number>(MAX_SKILL).fill(0);

/** Skill levels for the current player. */
export const skillLevel: number[] = new Array<number>(MAX_SKILL).fill(0);

/**
 * Reset player experience and skill data.
 * Equivalent to the C `reset_player_data()` function.
 */
export function resetPlayerData(): void {
    skillExp.fill(0);
    skillLevel.fill(0);
}

// ──────────────────────────────────────────────────────────────────────────────
// Client lifecycle
// ──────────────────────────────────────────────────────────────────────────────

/** The global player data structure, equivalent to C `cpl`. */
let cplInstance: Player | null = null;

/** Return the global Player instance (creating it if needed). */
export function getCpl(): Player | null { return cplInstance; }

/**
 * One-time client startup initialization. Sets built-in defaults, then
 * overlays any previously-saved user configuration from localStorage.
 * Equivalent to the C `client_init()` function.
 */
export function clientInit(): void {
    initConfig();
    loadSavedConfig();
    resetPlayerData();
    initPlayerData();
}

/**
 * Allocate the player and map root items and wire up the global `cpl`
 * structure.  Equivalent to the C code in `client_init()`:
 *     cpl.ob = player_item();
 *     cpl.below = map_item();
 */
function initPlayerData(): void {
    const ob = playerItem();
    const below = mapItem();
    const p: Player = {
        ob,
        below,
        container: null,
        countLeft: 0,
        inputState: InputState.Playing,
        lastCommand: '',
        inputText: '',
        ranges: [],
        readySpell: 0,
        stats: {
            Str: 0, Dex: 0, Con: 0, Wis: 0, Cha: 0, Int: 0, Pow: 0,
            wc: 0, ac: 0, level: 0, hp: 0, maxhp: 0, sp: 0, maxsp: 0,
            grace: 0, maxgrace: 0, exp: BigInt(0), food: 0, dam: 0,
            speed: 0, weaponSp: 0, attuned: 0, repelled: 0, denied: 0,
            flags: 0,
            resists: new Array(30).fill(0),
            resistChange: false,
            skillLevel: new Array(CS_NUM_SKILLS).fill(0),
            skillExp: new Array(CS_NUM_SKILLS).fill(BigInt(0)),
            weightLimit: 0,
            golemHp: 0, golemMaxhp: 0,
        },
        spelldata: [],
        title: '',
        range: '',
        spellsUpdated: 0,
        fireOn: false,
        runOn: false,
        metaOn: false,
        altOn: false,
        noEcho: false,
        count: 0,
        mmapx: 0, mmapy: 0,
        pmapx: 0, pmapy: 0,
        magicmap: null,
        showmagic: 0,
        mapxres: 0, mapyres: 0,
        name: '',
    };
    cplInstance = p;
    setCplInItem(p);
    setCplInPlayer(p);
}

/**
 * Reset client state between connections to different servers.
 * Equivalent to the C `client_reset()` function.
 */
export function clientReset(): void {
    resetPlayerData();

    // Re-apply defaults, then reload user overrides.
    initConfig();
    loadSavedConfig();
}
