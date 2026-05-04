/**
 * Client initialization for the Crossfire web client.
 * Port of old/common/init.c configuration and startup logic.
 */

import type { Player } from "./protocol";
import {
    CS_NUM_SKILLS,
    InputState,
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
// Typed configuration object
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Strongly-typed client configuration.
 *
 * Boolean fields represent on/off toggles (previously stored as 1/0 in a plain
 * number[]).  Numeric fields carry true integer values (scales, port, timeouts,
 * enum-style selectors, etc.).
 *
 * The ordering mirrors the CONFIG_* constants in protocol.ts so that the
 * descriptors in CONFIG_DESCS stay easy to cross-reference.
 */
export interface ClientConfig {
    // --- boolean toggles (CONFIG_DOWNLOAD … CONFIG_DEBOUNCE) ---
    download: boolean;          // CONFIG_DOWNLOAD        = 1
    echo: boolean;              // CONFIG_ECHO            = 2
    fasttcp: boolean;           // CONFIG_FASTTCP         = 3
    cache: boolean;             // CONFIG_CACHE           = 5
    fogwar: boolean;            // CONFIG_FOGWAR          = 6
    popups: boolean;            // CONFIG_POPUPS          = 9
    showicon: boolean;          // CONFIG_SHOWICON        = 11
    tooltips: boolean;          // CONFIG_TOOLTIPS        = 12
    sound: boolean;             // CONFIG_SOUND           = 13
    splitinfo: boolean;         // CONFIG_SPLITINFO       = 14
    splitwin: boolean;          // CONFIG_SPLITWIN        = 15
    showgrid: boolean;          // CONFIG_SHOWGRID        = 16
    triminfo: boolean;          // CONFIG_TRIMINFO        = 18
    foodbeep: boolean;          // CONFIG_FOODBEEP        = 21
    darkness: boolean;          // CONFIG_DARKNESS        = 22
    gradColor: boolean;         // CONFIG_GRAD_COLOR      = 24
    smooth: boolean;            // CONFIG_SMOOTH          = 26
    splash: boolean;            // CONFIG_SPLASH          = 27
    applyContainer: boolean;    // CONFIG_APPLY_CONTAINER = 28
    mapscroll: boolean;         // CONFIG_MAPSCROLL       = 29
    signpopup: boolean;         // CONFIG_SIGNPOPUP       = 30
    timestamp: boolean;         // CONFIG_TIMESTAMP       = 31
    invMenu: boolean;           // CONFIG_INV_MENU        = 33
    serverTicks: boolean;       // CONFIG_SERVER_TICKS    = 35
    debounce: boolean;          // CONFIG_DEBOUNCE        = 36
    fogGrayscale: boolean;      // (web-client only) desaturate fog-of-war cells
    darknessInterpolation: boolean; // (web-client only) bilinear-interpolate darkness overlay

    // --- numeric values ---
    cWindow: number;            // CONFIG_CWINDOW         = 4  (command-window depth)
    iconscale: number;          // CONFIG_ICONSCALE       = 7  (%)
    mapscale: number;           // CONFIG_MAPSCALE        = 8  (%)
    displaymode: number;        // CONFIG_DISPLAYMODE     = 10 (CFG_DM_*)
    lighting: number;           // CONFIG_LIGHTING        = 17 (CFG_LT_*)
    mapWidth: number;           // CONFIG_MAPWIDTH        = 19 (tiles)
    mapHeight: number;          // CONFIG_MAPHEIGHT       = 20 (tiles)
    port: number;               // CONFIG_PORT            = 23
    resists: number;            // CONFIG_RESISTS         = 25 (0/1/2 display mode)
    autoAfk: number;            // CONFIG_AUTO_AFK        = 32 (seconds)
    musicVol: number;           // CONFIG_MUSIC_VOL       = 34 (0-100)

    // --- session-only settings (not persisted to localStorage) ---
    /** Desired login method to request from the server:
     *  0 = legacy addme/query flow,
     *  1 = account-based login (accountlogin/accountplayers/accountplay),
     *  2 = account-based login + enhanced character creation (createplayer with race/class).
     *  The server may respond with a lower value if it does not support the requested level.
     */
    loginMethod: number;
}

// ──────────────────────────────────────────────────────────────────────────────
// Configuration descriptors (replace the old configNames array)
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Metadata for each CONFIG_* slot.
 * `key`  – property name on {@link ClientConfig}.
 * `name` – localStorage key suffix (must match what old clients stored so that
 *          previously-saved settings are still loaded correctly).
 */
interface ConfigDesc {
    key: keyof ClientConfig;
    name: string;
}

/**
 * One entry per CONFIG_* index (1-based; index 0 is null / unused).
 * The index of each entry equals the matching CONFIG_* constant in protocol.ts.
 */
const CONFIG_DESCS: (ConfigDesc | null)[] = [
    null,                                                                     // 0  unused
    { key: 'download',       name: 'download_all_images'  },                 // 1  CONFIG_DOWNLOAD
    { key: 'echo',           name: 'echo_bindings'        },                 // 2  CONFIG_ECHO
    { key: 'fasttcp',        name: 'fasttcpsend'          },                 // 3  CONFIG_FASTTCP
    { key: 'cWindow',        name: 'command_window'       },                 // 4  CONFIG_CWINDOW
    { key: 'cache',          name: 'cacheimages'          },                 // 5  CONFIG_CACHE
    { key: 'fogwar',         name: 'fog_of_war'           },                 // 6  CONFIG_FOGWAR
    { key: 'iconscale',      name: 'iconscale'            },                 // 7  CONFIG_ICONSCALE
    { key: 'mapscale',       name: 'mapscale'             },                 // 8  CONFIG_MAPSCALE
    { key: 'popups',         name: 'popups'               },                 // 9  CONFIG_POPUPS
    { key: 'displaymode',    name: 'displaymode'          },                 // 10 CONFIG_DISPLAYMODE
    { key: 'showicon',       name: 'showicon'             },                 // 11 CONFIG_SHOWICON
    { key: 'tooltips',       name: 'tooltips'             },                 // 12 CONFIG_TOOLTIPS
    { key: 'sound',          name: 'sound'                },                 // 13 CONFIG_SOUND
    { key: 'splitinfo',      name: 'splitinfo'            },                 // 14 CONFIG_SPLITINFO
    { key: 'splitwin',       name: 'split'                },                 // 15 CONFIG_SPLITWIN
    { key: 'showgrid',       name: 'show_grid'            },                 // 16 CONFIG_SHOWGRID
    { key: 'lighting',       name: 'lighting'             },                 // 17 CONFIG_LIGHTING
    { key: 'triminfo',       name: 'trim_info_window'     },                 // 18 CONFIG_TRIMINFO
    { key: 'mapWidth',       name: 'map_width'            },                 // 19 CONFIG_MAPWIDTH
    { key: 'mapHeight',      name: 'map_height'           },                 // 20 CONFIG_MAPHEIGHT
    { key: 'foodbeep',       name: 'foodbeep'             },                 // 21 CONFIG_FOODBEEP
    { key: 'darkness',       name: 'darkness'             },                 // 22 CONFIG_DARKNESS
    { key: 'port',           name: 'port'                 },                 // 23 CONFIG_PORT
    { key: 'gradColor',      name: 'grad_color_bars'      },                 // 24 CONFIG_GRAD_COLOR
    { key: 'resists',        name: 'resistances'          },                 // 25 CONFIG_RESISTS
    { key: 'smooth',         name: 'smoothing'            },                 // 26 CONFIG_SMOOTH
    { key: 'splash',         name: 'nosplash'             },                 // 27 CONFIG_SPLASH
    { key: 'applyContainer', name: 'auto_apply_container' },                 // 28 CONFIG_APPLY_CONTAINER
    { key: 'mapscroll',      name: 'mapscroll'            },                 // 29 CONFIG_MAPSCROLL
    { key: 'signpopup',      name: 'sign_popups'          },                 // 30 CONFIG_SIGNPOPUP
    { key: 'timestamp',      name: 'message_timestamping' },                 // 31 CONFIG_TIMESTAMP
    { key: 'autoAfk',        name: 'auto_afk'             },                 // 32 CONFIG_AUTO_AFK
    { key: 'invMenu',        name: 'inv_menu'             },                 // 33 CONFIG_INV_MENU
    { key: 'musicVol',       name: 'music_vol'            },                 // 34 CONFIG_MUSIC_VOL
    { key: 'serverTicks',    name: 'server_ticks'         },                 // 35 CONFIG_SERVER_TICKS
    { key: 'debounce',       name: 'debounce'             },                 // 36 CONFIG_DEBOUNCE
    { key: 'fogGrayscale',           name: 'fog_grayscale'            },                 // (web-only)
    { key: 'darknessInterpolation',  name: 'darkness_interpolation'   },                 // (web-only)
];

/** Desired configuration values. */
export const wantConfig = {} as ClientConfig;

/** Active configuration values (may differ from wantConfig during negotiation). */
export const useConfig = {} as ClientConfig;

// ──────────────────────────────────────────────────────────────────────────────
// Browser capability detection
// ──────────────────────────────────────────────────────────────────────────────

/**
 * True when running in Firefox.  Firefox renders OffscreenCanvas CSS filters
 * entirely on the CPU, making the full-canvas grayscale pass used by the
 * fog-of-war desaturation effect 10–15× slower than in Chromium-based
 * browsers.  The feature is therefore disabled by default in Firefox; users
 * can still enable it via the Config menu.
 */
const isFirefox: boolean =
    typeof navigator !== 'undefined' && /Firefox\//.test(navigator.userAgent);

// ──────────────────────────────────────────────────────────────────────────────
// Configuration defaults
// ──────────────────────────────────────────────────────────────────────────────

/**
 * Populate {@link wantConfig} with built-in defaults and copy them into
 * {@link useConfig}. Matches the C `init_config()` function.
 */
function initConfig(): void {
    wantConfig.applyContainer = true;
    wantConfig.cache          = false;
    wantConfig.cWindow        = COMMAND_WINDOW;
    wantConfig.darkness       = true;
    wantConfig.debounce       = true;
    wantConfig.displaymode    = CFG_DM_PIXMAP;
    wantConfig.download       = false;
    wantConfig.echo           = false;
    wantConfig.fasttcp        = true;
    wantConfig.fogwar         = true;
    wantConfig.foodbeep       = false;
    wantConfig.gradColor      = false;
    wantConfig.iconscale      = 100;
    wantConfig.invMenu        = true;
    wantConfig.lighting       = CFG_LT_PIXEL;
    wantConfig.mapHeight      = 20;
    wantConfig.mapscale       = 100;
    wantConfig.mapscroll      = true;
    wantConfig.mapWidth       = 20;
    wantConfig.musicVol       = 100;
    wantConfig.popups         = false;
    wantConfig.port           = EPORT;
    wantConfig.resists        = 0;
    wantConfig.serverTicks    = false;
    wantConfig.showgrid       = false;
    wantConfig.showicon       = false;
    wantConfig.signpopup      = true;
    wantConfig.smooth         = true;
    wantConfig.sound          = true;
    wantConfig.splash         = true;
    wantConfig.splitinfo      = false;
    wantConfig.splitwin       = false;
    wantConfig.timestamp      = false;
    wantConfig.tooltips       = true;
    wantConfig.triminfo       = false;
    wantConfig.autoAfk        = 300;
    // Disabled by default on Firefox: the full-canvas CSS-filter grayscale pass
    // runs on the CPU in Firefox and is 10–15× slower than in Chrome.
    wantConfig.fogGrayscale         = !isFirefox;
    wantConfig.darknessInterpolation = true;
    wantConfig.loginMethod          = 2;

    Object.assign(useConfig, wantConfig);
}

/**
 * Load any user-overridden config values from localStorage and merge them
 * into {@link wantConfig} / {@link useConfig}.
 *
 * Boolean configs accept legacy stored numbers (0 → false, non-zero → true)
 * so that settings saved by earlier versions of the client still load correctly.
 */
function loadSavedConfig(): void {
    for (const desc of CONFIG_DESCS) {
        if (!desc) continue;
        const raw = loadConfig<boolean | number | null>(`config_${desc.name}`, null);
        if (raw === null) continue;
        const key = desc.key;
        const defaultVal = wantConfig[key];
        let coerced: boolean | number;
        if (typeof defaultVal === 'boolean') {
            // `raw` is guaranteed non-null here (guarded above).
            // Accept legacy stored numbers (0 → false, non-zero → true) so
            // that settings saved by older builds still load correctly.
            coerced = typeof raw === 'boolean' ? raw : (raw as number) !== 0;
        } else {
            coerced = typeof raw === 'number' ? raw : defaultVal;
        }
        (wantConfig as Record<keyof ClientConfig, boolean | number>)[key] = coerced;
        (useConfig  as Record<keyof ClientConfig, boolean | number>)[key] = coerced;
    }
}

/**
 * Persist the current {@link wantConfig} values to localStorage so they
 * survive page reloads.
 */
export function saveCurrentConfig(): void {
    for (const desc of CONFIG_DESCS) {
        if (!desc) continue;
        saveConfig(`config_${desc.name}`, wantConfig[desc.key]);
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
            title: '',
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
            range: '',
            raceStr: 0, raceInt: 0, raceWis: 0, raceDex: 0, raceCon: 0, raceCha: 0, racePow: 0,
            baseStr: 0, baseInt: 0, baseWis: 0, baseDex: 0, baseCon: 0, baseCha: 0, basePow: 0,
            appliedStr: 0, appliedInt: 0, appliedWis: 0, appliedDex: 0, appliedCon: 0, appliedCha: 0, appliedPow: 0,
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
