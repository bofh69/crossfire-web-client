/**
 * Protocol constants and types for the Crossfire web client.
 * Converted from the C codebase (newclient.h, client.h, item.h, mapdata.h, p_cmd.h).
 */

// ──────────────────────────────────────────────────────────────────────────────
// Character stat constants (CS_STAT_*)
// ──────────────────────────────────────────────────────────────────────────────
export const CS_STAT_HP = 1;
export const CS_STAT_MAXHP = 2;
export const CS_STAT_SP = 3;
export const CS_STAT_MAXSP = 4;
export const CS_STAT_STR = 5;
export const CS_STAT_INT = 6;
export const CS_STAT_WIS = 7;
export const CS_STAT_DEX = 8;
export const CS_STAT_CON = 9;
export const CS_STAT_CHA = 10;
export const CS_STAT_LEVEL = 12;
export const CS_STAT_WC = 13;
export const CS_STAT_AC = 14;
export const CS_STAT_DAM = 15;
export const CS_STAT_ARMOUR = 16;
export const CS_STAT_SPEED = 17;
export const CS_STAT_FOOD = 18;
export const CS_STAT_WEAP_SP = 19;
export const CS_STAT_RANGE = 20;
export const CS_STAT_TITLE = 21;
export const CS_STAT_POW = 22;
export const CS_STAT_GRACE = 23;
export const CS_STAT_MAXGRACE = 24;
export const CS_STAT_FLAGS = 25;
export const CS_STAT_WEIGHT_LIM = 26;
export const CS_STAT_EXP64 = 28;
export const CS_STAT_SPELL_ATTUNE = 29;
export const CS_STAT_SPELL_REPEL = 30;
export const CS_STAT_SPELL_DENY = 31;

// Race stat modifiers
export const CS_STAT_RACE_STR = 32;
export const CS_STAT_RACE_INT = 33;
export const CS_STAT_RACE_WIS = 34;
export const CS_STAT_RACE_DEX = 35;
export const CS_STAT_RACE_CON = 36;
export const CS_STAT_RACE_CHA = 37;
export const CS_STAT_RACE_POW = 38;

// Base stat values
export const CS_STAT_BASE_STR = 39;
export const CS_STAT_BASE_INT = 40;
export const CS_STAT_BASE_WIS = 41;
export const CS_STAT_BASE_DEX = 42;
export const CS_STAT_BASE_CON = 43;
export const CS_STAT_BASE_CHA = 44;
export const CS_STAT_BASE_POW = 45;

// Applied stat modifiers (gear/skills)
export const CS_STAT_APPLIED_STR = 46;
export const CS_STAT_APPLIED_INT = 47;
export const CS_STAT_APPLIED_WIS = 48;
export const CS_STAT_APPLIED_DEX = 49;
export const CS_STAT_APPLIED_CON = 50;
export const CS_STAT_APPLIED_CHA = 51;
export const CS_STAT_APPLIED_POW = 52;

// Golem stats
export const CS_STAT_GOLEM_HP = 53;
export const CS_STAT_GOLEM_MAXHP = 54;

// Resistances
export const CS_STAT_RESIST_START = 100;
export const CS_STAT_RESIST_END = 117;
export const CS_STAT_RES_PHYS = 100;
export const CS_STAT_RES_MAG = 101;
export const CS_STAT_RES_FIRE = 102;
export const CS_STAT_RES_ELEC = 103;
export const CS_STAT_RES_COLD = 104;
export const CS_STAT_RES_CONF = 105;
export const CS_STAT_RES_ACID = 106;
export const CS_STAT_RES_DRAIN = 107;
export const CS_STAT_RES_GHOSTHIT = 108;
export const CS_STAT_RES_POISON = 109;
export const CS_STAT_RES_SLOW = 110;
export const CS_STAT_RES_PARA = 111;
export const CS_STAT_TURN_UNDEAD = 112;
export const CS_STAT_RES_FEAR = 113;
export const CS_STAT_RES_DEPLETE = 114;
export const CS_STAT_RES_DEATH = 115;
export const CS_STAT_RES_HOLYWORD = 116;
export const CS_STAT_RES_BLIND = 117;

// Skill info
export const CS_STAT_SKILLINFO = 140;
export const CS_NUM_SKILLS = 50;

// ──────────────────────────────────────────────────────────────────────────────
// Fire/run state flags (SF_*)
// ──────────────────────────────────────────────────────────────────────────────
export const SF_FIREON = 0x01;
export const SF_RUNON = 0x02;

// ──────────────────────────────────────────────────────────────────────────────
// Item flags (F_*)
// ──────────────────────────────────────────────────────────────────────────────
export const F_APPLIED = 0x000f;
export const F_UNIDENTIFIED = 0x0010;
export const F_READ = 0x0020;
export const F_BLESSED = 0x0100;
export const F_UNPAID = 0x0200;
export const F_MAGIC = 0x0400;
export const F_CURSED = 0x0800;
export const F_DAMNED = 0x1000;
export const F_OPEN = 0x2000;
export const F_NOPICK = 0x4000;
export const F_LOCKED = 0x8000;

// ──────────────────────────────────────────────────────────────────────────────
// Apply types
// ──────────────────────────────────────────────────────────────────────────────
export enum ApplyType {
    None = 0,
    Readied = 1,
    Wielded = 2,
    Worn = 3,
    Active = 4,
    Applied = 5,
}

// ──────────────────────────────────────────────────────────────────────────────
// Item update flags (UPD_*)
// ──────────────────────────────────────────────────────────────────────────────
export const UPD_LOCATION = 0x01;
export const UPD_FLAGS = 0x02;
export const UPD_WEIGHT = 0x04;
export const UPD_FACE = 0x08;
export const UPD_NAME = 0x10;
export const UPD_ANIM = 0x20;
export const UPD_ANIMSPEED = 0x40;
export const UPD_NROF = 0x80;
export const UPD_ALL = 0xff;

// Spell update flags
export const UPD_SP_MANA = 0x01;
export const UPD_SP_GRACE = 0x02;
export const UPD_SP_DAMAGE = 0x04;

// ──────────────────────────────────────────────────────────────────────────────
// Message colors and flags (NDI_*)
// ──────────────────────────────────────────────────────────────────────────────
export const NDI_BLACK = 0;
export const NDI_WHITE = 1;
export const NDI_NAVY = 2;
export const NDI_RED = 3;
export const NDI_ORANGE = 4;
export const NDI_BLUE = 5;
export const NDI_DK_ORANGE = 6;
export const NDI_GREEN = 7;
export const NDI_LT_GREEN = 8;
export const NDI_GREY = 9;
export const NDI_BROWN = 10;
export const NDI_GOLD = 11;
export const NDI_TAN = 12;
export const NDI_MAX_COLOR = 12;

export const NDI_COLOR_MASK = 0xff;
export const NDI_UNIQUE = 0x100;
export const NDI_ALL = 0x200;
export const NDI_ALL_DMS = 0x400;

// ──────────────────────────────────────────────────────────────────────────────
// Message types (MSG_TYPE_*)
// ──────────────────────────────────────────────────────────────────────────────
export const MSG_TYPE_BOOK = 1;
export const MSG_TYPE_CARD = 2;
export const MSG_TYPE_PAPER = 3;
export const MSG_TYPE_SIGN = 4;
export const MSG_TYPE_MONUMENT = 5;
export const MSG_TYPE_DIALOG = 6;
export const MSG_TYPE_MOTD = 7;
export const MSG_TYPE_ADMIN = 8;
export const MSG_TYPE_SHOP = 9;
export const MSG_TYPE_COMMAND = 10;
export const MSG_TYPE_ATTRIBUTE = 11;
export const MSG_TYPE_SKILL = 12;
export const MSG_TYPE_APPLY = 13;
export const MSG_TYPE_ATTACK = 14;
export const MSG_TYPE_COMMUNICATION = 15;
export const MSG_TYPE_SPELL = 16;
export const MSG_TYPE_ITEM = 17;
export const MSG_TYPE_MISC = 18;
export const MSG_TYPE_VICTIM = 19;
export const MSG_TYPE_CLIENT = 20;
export const MSG_TYPE_LAST = 21;

// Book subtypes
export const MSG_TYPE_BOOK_CLASP_1 = 1;
export const MSG_TYPE_BOOK_CLASP_2 = 2;
export const MSG_TYPE_BOOK_ELEGANT_1 = 3;
export const MSG_TYPE_BOOK_ELEGANT_2 = 4;
export const MSG_TYPE_BOOK_QUARTO_1 = 5;
export const MSG_TYPE_BOOK_QUARTO_2 = 6;
export const MSG_TYPE_BOOK_SPELL_EVOKER = 7;
export const MSG_TYPE_BOOK_SPELL_PRAYER = 8;
export const MSG_TYPE_BOOK_SPELL_PYRO = 9;
export const MSG_TYPE_BOOK_SPELL_SORCERER = 10;
export const MSG_TYPE_BOOK_SPELL_SUMMONER = 11;

// Card subtypes
export const MSG_TYPE_CARD_SIMPLE_1 = 1;
export const MSG_TYPE_CARD_SIMPLE_2 = 2;
export const MSG_TYPE_CARD_SIMPLE_3 = 3;
export const MSG_TYPE_CARD_ELEGANT_1 = 4;
export const MSG_TYPE_CARD_ELEGANT_2 = 5;
export const MSG_TYPE_CARD_ELEGANT_3 = 6;
export const MSG_TYPE_CARD_STRANGE_1 = 7;
export const MSG_TYPE_CARD_STRANGE_2 = 8;
export const MSG_TYPE_CARD_STRANGE_3 = 9;
export const MSG_TYPE_CARD_MONEY_1 = 10;
export const MSG_TYPE_CARD_MONEY_2 = 11;
export const MSG_TYPE_CARD_MONEY_3 = 12;

// Paper subtypes
export const MSG_TYPE_PAPER_NOTE_1 = 1;
export const MSG_TYPE_PAPER_NOTE_2 = 2;
export const MSG_TYPE_PAPER_NOTE_3 = 3;
export const MSG_TYPE_PAPER_LETTER_OLD_1 = 4;
export const MSG_TYPE_PAPER_LETTER_OLD_2 = 5;
export const MSG_TYPE_PAPER_LETTER_NEW_1 = 6;
export const MSG_TYPE_PAPER_LETTER_NEW_2 = 7;
export const MSG_TYPE_PAPER_ENVELOPE_1 = 8;
export const MSG_TYPE_PAPER_ENVELOPE_2 = 9;
export const MSG_TYPE_PAPER_SCROLL_OLD_1 = 10;
export const MSG_TYPE_PAPER_SCROLL_OLD_2 = 11;
export const MSG_TYPE_PAPER_SCROLL_NEW_1 = 12;
export const MSG_TYPE_PAPER_SCROLL_NEW_2 = 13;
export const MSG_TYPE_PAPER_SCROLL_MAGIC = 14;

// Sign subtypes
export const MSG_TYPE_SIGN_BASIC = 1;
export const MSG_TYPE_SIGN_DIR_LEFT = 2;
export const MSG_TYPE_SIGN_DIR_RIGHT = 3;
export const MSG_TYPE_SIGN_DIR_BOTH = 4;
export const MSG_TYPE_SIGN_MAGIC_MOUTH = 5;

// Monument subtypes
export const MSG_TYPE_MONUMENT_STONE_1 = 1;
export const MSG_TYPE_MONUMENT_STONE_2 = 2;
export const MSG_TYPE_MONUMENT_STONE_3 = 3;
export const MSG_TYPE_MONUMENT_STATUE_1 = 4;
export const MSG_TYPE_MONUMENT_STATUE_2 = 5;
export const MSG_TYPE_MONUMENT_STATUE_3 = 6;
export const MSG_TYPE_MONUMENT_GRAVESTONE_1 = 7;
export const MSG_TYPE_MONUMENT_GRAVESTONE_2 = 8;
export const MSG_TYPE_MONUMENT_GRAVESTONE_3 = 9;
export const MSG_TYPE_MONUMENT_WALL_1 = 10;
export const MSG_TYPE_MONUMENT_WALL_2 = 11;
export const MSG_TYPE_MONUMENT_WALL_3 = 12;

// Dialog subtypes
export const MSG_TYPE_DIALOG_NPC = 1;
export const MSG_TYPE_DIALOG_ALTAR = 2;
export const MSG_TYPE_DIALOG_MAGIC_EAR = 3;

// Admin subtypes
export const MSG_TYPE_ADMIN_RULES = 1;
export const MSG_TYPE_ADMIN_NEWS = 2;
export const MSG_TYPE_ADMIN_PLAYER = 3;
export const MSG_TYPE_ADMIN_DM = 4;
export const MSG_TYPE_ADMIN_HISCORE = 5;
export const MSG_TYPE_ADMIN_LOADSAVE = 6;
export const MSG_TYPE_ADMIN_LOGIN = 7;
export const MSG_TYPE_ADMIN_VERSION = 8;
export const MSG_TYPE_ADMIN_ERROR = 9;

// Shop subtypes
export const MSG_TYPE_SHOP_LISTING = 1;
export const MSG_TYPE_SHOP_PAYMENT = 2;
export const MSG_TYPE_SHOP_SELL = 3;
export const MSG_TYPE_SHOP_MISC = 4;

// Command subtypes
export const MSG_TYPE_COMMAND_WHO = 1;
export const MSG_TYPE_COMMAND_MAPS = 2;
export const MSG_TYPE_COMMAND_BODY = 3;
export const MSG_TYPE_COMMAND_MALLOC = 4;
export const MSG_TYPE_COMMAND_WEATHER = 5;
export const MSG_TYPE_COMMAND_STATISTICS = 6;
export const MSG_TYPE_COMMAND_CONFIG = 7;
export const MSG_TYPE_COMMAND_INFO = 8;
export const MSG_TYPE_COMMAND_QUESTS = 9;
export const MSG_TYPE_COMMAND_DEBUG = 10;
export const MSG_TYPE_COMMAND_ERROR = 11;
export const MSG_TYPE_COMMAND_SUCCESS = 12;
export const MSG_TYPE_COMMAND_FAILURE = 13;
export const MSG_TYPE_COMMAND_EXAMINE = 14;
export const MSG_TYPE_COMMAND_INVENTORY = 15;
export const MSG_TYPE_COMMAND_HELP = 16;
export const MSG_TYPE_COMMAND_DM = 17;
export const MSG_TYPE_COMMAND_NEWPLAYER = 18;

// Attribute subtypes
export const MSG_TYPE_ATTRIBUTE_ATTACKTYPE_GAIN = 1;
export const MSG_TYPE_ATTRIBUTE_ATTACKTYPE_LOSS = 2;
export const MSG_TYPE_ATTRIBUTE_PROTECTION_GAIN = 3;
export const MSG_TYPE_ATTRIBUTE_PROTECTION_LOSS = 4;
export const MSG_TYPE_ATTRIBUTE_MOVE = 5;
export const MSG_TYPE_ATTRIBUTE_RACE = 6;
export const MSG_TYPE_ATTRIBUTE_BAD_EFFECT_START = 7;
export const MSG_TYPE_ATTRIBUTE_BAD_EFFECT_END = 8;
export const MSG_TYPE_ATTRIBUTE_STAT_GAIN = 9;
export const MSG_TYPE_ATTRIBUTE_STAT_LOSS = 10;
export const MSG_TYPE_ATTRIBUTE_LEVEL_GAIN = 11;
export const MSG_TYPE_ATTRIBUTE_LEVEL_LOSS = 12;
export const MSG_TYPE_ATTRIBUTE_GOOD_EFFECT_START = 13;
export const MSG_TYPE_ATTRIBUTE_GOOD_EFFECT_END = 14;
export const MSG_TYPE_ATTRIBUTE_GOD = 15;

// Skill subtypes
export const MSG_TYPE_SKILL_MISSING = 1;
export const MSG_TYPE_SKILL_ERROR = 2;
export const MSG_TYPE_SKILL_SUCCESS = 3;
export const MSG_TYPE_SKILL_FAILURE = 4;
export const MSG_TYPE_SKILL_PRAY = 5;
export const MSG_TYPE_SKILL_LIST = 6;

// Apply subtypes
export const MSG_TYPE_APPLY_ERROR = 1;
export const MSG_TYPE_APPLY_UNAPPLY = 2;
export const MSG_TYPE_APPLY_SUCCESS = 3;
export const MSG_TYPE_APPLY_FAILURE = 4;
export const MSG_TYPE_APPLY_CURSED = 5;
export const MSG_TYPE_APPLY_TRAP = 6;
export const MSG_TYPE_APPLY_BADBODY = 7;
export const MSG_TYPE_APPLY_PROHIBITION = 8;
export const MSG_TYPE_APPLY_BUILD = 9;

// Attack subtypes
export const MSG_TYPE_ATTACK_DID_HIT = 1;
export const MSG_TYPE_ATTACK_PET_HIT = 2;
export const MSG_TYPE_ATTACK_FUMBLE = 3;
export const MSG_TYPE_ATTACK_DID_KILL = 4;
export const MSG_TYPE_ATTACK_PET_DIED = 5;
export const MSG_TYPE_ATTACK_NOKEY = 6;
export const MSG_TYPE_ATTACK_NOATTACK = 7;
export const MSG_TYPE_ATTACK_PUSHED = 8;
export const MSG_TYPE_ATTACK_MISS = 9;

// Communication subtypes
export const MSG_TYPE_COMMUNICATION_RANDOM = 1;
export const MSG_TYPE_COMMUNICATION_SAY = 2;
export const MSG_TYPE_COMMUNICATION_ME = 3;
export const MSG_TYPE_COMMUNICATION_TELL = 4;
export const MSG_TYPE_COMMUNICATION_EMOTE = 5;
export const MSG_TYPE_COMMUNICATION_PARTY = 6;
export const MSG_TYPE_COMMUNICATION_SHOUT = 7;
export const MSG_TYPE_COMMUNICATION_CHAT = 8;

// Spell subtypes
export const MSG_TYPE_SPELL_HEAL = 1;
export const MSG_TYPE_SPELL_PET = 2;
export const MSG_TYPE_SPELL_FAILURE = 3;
export const MSG_TYPE_SPELL_END = 4;
export const MSG_TYPE_SPELL_SUCCESS = 5;
export const MSG_TYPE_SPELL_ERROR = 6;
export const MSG_TYPE_SPELL_PERCEIVE_SELF = 7;
export const MSG_TYPE_SPELL_TARGET = 8;
export const MSG_TYPE_SPELL_INFO = 9;

// Item subtypes
export const MSG_TYPE_ITEM_REMOVE = 1;
export const MSG_TYPE_ITEM_ADD = 2;
export const MSG_TYPE_ITEM_CHANGE = 3;
export const MSG_TYPE_ITEM_INFO = 4;

// Victim subtypes
export const MSG_TYPE_VICTIM_SWAMP = 1;
export const MSG_TYPE_VICTIM_WAS_HIT = 2;
export const MSG_TYPE_VICTIM_STEAL = 3;
export const MSG_TYPE_VICTIM_SPELL = 4;
export const MSG_TYPE_VICTIM_DIED = 5;
export const MSG_TYPE_VICTIM_WAS_PUSHED = 6;

// Client subtypes
export const MSG_TYPE_CLIENT_CONFIG = 1;
export const MSG_TYPE_CLIENT_SERVER = 2;
export const MSG_TYPE_CLIENT_COMMAND = 3;
export const MSG_TYPE_CLIENT_QUERY = 4;
export const MSG_TYPE_CLIENT_DEBUG = 5;
export const MSG_TYPE_CLIENT_NOTICE = 6;
export const MSG_TYPE_CLIENT_METASERVER = 7;
export const MSG_TYPE_CLIENT_SCRIPT = 8;
export const MSG_TYPE_CLIENT_ERROR = 9;

// ──────────────────────────────────────────────────────────────────────────────
// Sound types (SOUND_TYPE_*)
// ──────────────────────────────────────────────────────────────────────────────
export const SOUND_TYPE_LIVING = 1;
export const SOUND_TYPE_SPELL = 2;
export const SOUND_TYPE_ITEM = 3;
export const SOUND_TYPE_GROUND = 4;
export const SOUND_TYPE_HIT = 5;
export const SOUND_TYPE_HIT_BY = 6;

// ──────────────────────────────────────────────────────────────────────────────
// Map protocol constants (MAP2_*)
// ──────────────────────────────────────────────────────────────────────────────
export const MAP2_COORD_OFFSET = 15;
export const MAP2_TYPE_CLEAR = 0x0;
export const MAP2_TYPE_DARKNESS = 0x1;
export const MAP2_TYPE_LABEL = 0x2;
export const MAP2_LAYER_START = 0x10;
export const MAP2_ADD_LENGTH = 0b11100000;

export enum Map2Label {
    Player = 1,
    PlayerParty = 2,
    DM = 3,
    NPC = 4,
    Sign = 5,
    Say = 6,
    Chat = 7,
}

// ──────────────────────────────────────────────────────────────────────────────
// Query types (CS_QUERY_*)
// ──────────────────────────────────────────────────────────────────────────────
export const CS_QUERY_YESNO = 0x1;
export const CS_QUERY_SINGLECHAR = 0x2;
export const CS_QUERY_HIDEINPUT = 0x4;

// ──────────────────────────────────────────────────────────────────────────────
// Animation and face flags
// ──────────────────────────────────────────────────────────────────────────────
export const FACE_IS_ANIM = 1 << 15;
export const ANIM_RANDOM = 1 << 13;
export const ANIM_SYNC = 2 << 13;
export const ANIM_FLAGS_MASK = 0x6000;
export const ANIM_MASK = 0x1fff;

// ──────────────────────────────────────────────────────────────────────────────
// Account character login fields (ACL_*)
// ──────────────────────────────────────────────────────────────────────────────
export const ACL_NAME = 1;
export const ACL_CLASS = 2;
export const ACL_RACE = 3;
export const ACL_LEVEL = 4;
export const ACL_FACE = 5;
export const ACL_PARTY = 6;
export const ACL_MAP = 7;
export const ACL_FACE_NUM = 8;

// ──────────────────────────────────────────────────────────────────────────────
// Network and buffer constants
// ──────────────────────────────────────────────────────────────────────────────
export const MAXSOCKBUF = 2 + 65535 + 1;
export const FLOAT_MULTI = 100000;

// ──────────────────────────────────────────────────────────────────────────────
// Map and layer constants
// ──────────────────────────────────────────────────────────────────────────────
export const MAXLAYERS = 10;
export const MAX_VIEW = 64;
export const MAP1_LAYERS = 3;

// ──────────────────────────────────────────────────────────────────────────────
// General size constants
// ──────────────────────────────────────────────────────────────────────────────
export const MAX_BUF = 256;
export const BIG_BUF = 1024;
export const EPORT = 13327;
export const VERSION_CS = 1023;
export const VERSION_SC = 1030;

export const COMMAND_WINDOW = 10;
export const COMMAND_MAX = 255;
export const MAXANIM = 2000;
export const MAX_SKILL = 50;

export const MAX_FACE_SETS = 20;
export const MAX_IMAGE_SIZE = 320;
export const MAP_MAX_SIZE = 25;
export const MAXPIXMAPNUM = 10000;

export const NUM_RESISTS = 18;
export const NUM_NEW_CHAR_STATS = 7;
export const NAME_LEN = 128;
export const NO_ITEM_TYPE = 30000;

// ──────────────────────────────────────────────────────────────────────────────
// Send command options (SC_*)
// ──────────────────────────────────────────────────────────────────────────────
export const SC_NORMAL = 0;
export const SC_FIRERUN = 1;
export const SC_ALWAYS = 2;
export const SC_MOVETO = 3;

// ──────────────────────────────────────────────────────────────────────────────
// Client configuration constants (CONFIG_*)
// ──────────────────────────────────────────────────────────────────────────────
export const CONFIG_DOWNLOAD = 1;
export const CONFIG_ECHO = 2;
export const CONFIG_FASTTCP = 3;
export const CONFIG_CWINDOW = 4;
export const CONFIG_CACHE = 5;
export const CONFIG_FOGWAR = 6;
export const CONFIG_ICONSCALE = 7;
export const CONFIG_MAPSCALE = 8;
export const CONFIG_POPUPS = 9;
export const CONFIG_DISPLAYMODE = 10;
export const CONFIG_SHOWICON = 11;
export const CONFIG_TOOLTIPS = 12;
export const CONFIG_SOUND = 13;
export const CONFIG_SPLITINFO = 14;
export const CONFIG_SPLITWIN = 15;
export const CONFIG_SHOWGRID = 16;
export const CONFIG_LIGHTING = 17;
export const CONFIG_TRIMINFO = 18;
export const CONFIG_MAPWIDTH = 19;
export const CONFIG_MAPHEIGHT = 20;
export const CONFIG_FOODBEEP = 21;
export const CONFIG_DARKNESS = 22;
export const CONFIG_PORT = 23;
export const CONFIG_GRAD_COLOR = 24;
export const CONFIG_RESISTS = 25;
export const CONFIG_SMOOTH = 26;
export const CONFIG_SPLASH = 27;
export const CONFIG_APPLY_CONTAINER = 28;
export const CONFIG_MAPSCROLL = 29;
export const CONFIG_SIGNPOPUP = 30;
export const CONFIG_TIMESTAMP = 31;
export const CONFIG_AUTO_AFK = 32;
export const CONFIG_INV_MENU = 33;
export const CONFIG_MUSIC_VOL = 34;
export const CONFIG_SERVER_TICKS = 35;
export const CONFIG_DEBOUNCE = 36;
export const CONFIG_NUMS = 37;

// ──────────────────────────────────────────────────────────────────────────────
// Lighting types (CFG_LT_*)
// ──────────────────────────────────────────────────────────────────────────────
export const CFG_LT_NONE = 0;
export const CFG_LT_TILE = 1;
export const CFG_LT_PIXEL = 2;
export const CFG_LT_PIXEL_BEST = 3;

// ──────────────────────────────────────────────────────────────────────────────
// Display modes (CFG_DM_*)
// ──────────────────────────────────────────────────────────────────────────────
export const CFG_DM_PIXMAP = 0;
export const CFG_DM_SDL = 1;
export const CFG_DM_OPENGL = 2;

// ──────────────────────────────────────────────────────────────────────────────
// Enums
// ──────────────────────────────────────────────────────────────────────────────

export enum RangeType {
    None = 0,
    Bow = 1,
    Magic = 2,
    Wand = 3,
    Rod = 4,
    Scroll = 5,
    Horn = 6,
    Steal = 7,
    Size = 8,
}

export enum InputState {
    Playing = 0,
    ReplyOne = 1,
    ReplyMany = 2,
    ConfigureKeys = 3,
    CommandMode = 4,
    MetaserverSelect = 5,
}

export enum LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4,
}

export enum MapCellState {
    Empty = 0,
    Visible = 1,
    Fog = 2,
}

export enum CommCat {
    Misc = 0,
    Info = 2,
    Setup = 3,
    Script = 4,
    Debug = 5,
}

// ──────────────────────────────────────────────────────────────────────────────
// Interfaces and types
// ──────────────────────────────────────────────────────────────────────────────

export interface Stats {
    Str: number;
    Dex: number;
    Con: number;
    Wis: number;
    Cha: number;
    Int: number;
    Pow: number;
    wc: number;
    ac: number;
    level: number;
    hp: number;
    maxhp: number;
    sp: number;
    maxsp: number;
    grace: number;
    maxgrace: number;
    exp: bigint;
    food: number;
    dam: number;
    speed: number;
    weaponSp: number;
    attuned: number;
    repelled: number;
    denied: number;
    flags: number;
    resists: number[];
    resistChange: boolean;
    skillLevel: number[];
    skillExp: bigint[];
    weightLimit: number;
    golemHp: number;
    golemMaxhp: number;
}

export interface Spell {
    name: string;
    message: string;
    tag: number;
    level: number;
    time: number;
    sp: number;
    grace: number;
    dam: number;
    skillNumber: number;
    skill: string;
    path: number;
    face: number;
    usage: number;
    requirements: string;
}

export interface Item {
    tag: number;
    nrof: number;
    weight: number;
    face: number;
    dName: string;
    sName: string;
    pName: string;
    flags: string;
    animationId: number;
    animSpeed: number;
    animState: number;
    lastAnim: number;
    magical: boolean;
    cursed: boolean;
    damned: boolean;
    blessed: boolean;
    unpaid: boolean;
    locked: boolean;
    applied: boolean;
    open: boolean;
    wasOpen: boolean;
    read: boolean;
    invUpdated: boolean;
    applyType: ApplyType;
    flagsval: number;
    type: number;
    env: Item | null;
    inv: Item | null;
    next: Item | null;
    prev: Item | null;
}

export interface MapCellLayer {
    face: number;
    sizeX: number;
    sizeY: number;
    animation: number;
    animationSpeed: number;
    animationLeft: number;
    animationPhase: number;
}

export interface MapCellTailLayer {
    face: number;
    sizeX: number;
    sizeY: number;
}

export interface MapLabel {
    subtype: number;
    label: string;
}

export interface MapCell {
    heads: MapCellLayer[];
    tails: MapCellTailLayer[];
    labels: MapLabel[];
    smooth: number[];
    darkness: number;
    needUpdate: boolean;
    needResmooth: boolean;
    state: MapCellState;
}

export interface Animation {
    flags: number;
    numAnimations: number;
    speed: number;
    speedLeft: number;
    phase: number;
    faces: number[];
}

export interface PlayerPosition {
    x: number;
    y: number;
}

export interface ClientSocket {
    ws: WebSocket | null;
    csVersion: number;
    scVersion: number;
    commandSent: number;
    commandReceived: number;
    commandTime: number;
    servername: string;
    dir: number[];
}

export interface Player {
    ob: Item | null;
    below: Item | null;
    container: Item | null;
    countLeft: number;
    inputState: InputState;
    lastCommand: string;
    inputText: string;
    ranges: (Item | null)[];
    readySpell: number;
    stats: Stats;
    spelldata: Spell[];
    title: string;
    range: string;
    spellsUpdated: number;
    fireOn: boolean;
    runOn: boolean;
    metaOn: boolean;
    altOn: boolean;
    noEcho: boolean;
    count: number;
    mmapx: number;
    mmapy: number;
    pmapx: number;
    pmapy: number;
    magicmap: Uint8Array | null;
    showmagic: number;
    mapxres: number;
    mapyres: number;
    name: string;
}

export interface FaceSet {
    setnum: number;
    fallback: number;
    prefix: string;
    fullname: string;
    size: string;
    extension: string;
    comment: string;
}

export interface FaceInformation {
    faceset: number;
    wantFaceset: string;
    numImages: number;
    bmapsChecksum: number;
    oldBmapsChecksum: number;
    cacheHits: number;
    cacheMisses: number;
    haveFacesetInfo: boolean;
    facesets: FaceSet[];
}

export interface CacheEntry {
    filename: string;
    checksum: number;
    imageData: Blob | null;
}

export interface ConsoleCommand {
    name: string;
    category: CommCat;
    description: string;
    handler: (args: string) => void;
}
