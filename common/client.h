/**
 * @file client.h
 * Includes various dependencies header files needed by most everything.  It
 * also declares structures and other variables that the GUI portion needs.
 */

#ifndef _CLIENT_H
#define _CLIENT_H

#include "config.h"

// This is required for 'newclient.h' to expose client variables.
#define CLIENT_TYPES_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#ifdef HAVE_DMALLOC_H
#  include <dmalloc.h>
#endif

#ifdef WIN32
#  include <winsock2.h>
#endif

#include "item.h"
#include "shared/newclient.h"
#include "version.h"

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#define MAX_BUF 256
#define BIG_BUF 1024

/* used to register gui callbacks to extended texts
 * (which are supposed to be handled more friendly than raw text)*/
typedef void (*ExtTextManager)(int flag, int type, int subtype, char* message);

typedef struct TextManager{
    int type;
    ExtTextManager callback;
    struct TextManager* next;
} TextManager;

/* This is how often the client checks for X events, as well as how often
 * it performs animations (or will).  This value can be most anything.
 * IT is only configurable because the exact value it needs to be set to
 * has to be figured out.  This value is in microseconds (100,000 microseconds=
 * 0.1 seconds
 */
#define MAX_TIME 100000

/* This is the default port to connect to the server with. */
#define EPORT 13327

/* This is the default port to connect to the server with in string form. */
#define DEFPORT "13327"

#define VERSION_CS 1023
#define VERSION_SC 1029

extern char VERSION_INFO[256];

/**
 * Do not send more than this many outstanding commands to the server this is
 * only a default value.
 */
#define COMMAND_WINDOW 10

#define STRINGCOMMAND 0

/**
 * How many skill types server supports/client will get sent to it.  If more
 * skills are added to server, this needs to get increased.
 */
#define MAX_SKILL CS_NUM_SKILLS

#define MAXANIM 2000

/**
 * @defgroup SC_xxx SC_xxx send_command options.
 * Values assigned to send_command option.
 */
/*@{*/
#define SC_NORMAL  0
#define SC_FIRERUN 1
#define SC_ALWAYS  2
/*@}*/

typedef struct Animations {
    guint16  flags;
    guint8   num_animations;             /**< Number of animations.  Value of 2
                                         *   means only faces[0],[1] have
                                         *   meaningful values.
                                         */
    guint8   speed;
    guint8   speed_left;
    guint8   phase;
    guint16  *faces;

} Animations;

extern Animations animations[MAXANIM];

/**
 * Basic support for socket communications, including the file descriptor,
 * input buffer, server, server, version, etc. ClientSocket could probably
 * hold more of the global values - it could probably hold most all
 * socket/communication related values instead of globals.
 */
typedef struct {
    int fd;
    SockList    inbuf;
    int cs_version, sc_version;         /**< Server versions of these
                                         */
    int command_sent, command_received; /**< These are used for the newer
                                         *   'windowing' method of commands -
                                         *   number of last command sent,
                                         *   number of received confirmation
                                         */
    int command_time;                   /**< Time (in ms) players commands
                                         *   currently take to execute
                                         */
    char* servername;
} ClientSocket;

extern ClientSocket csocket;

extern char *sound_server;
extern const char *cache_dir, *config_dir;

typedef enum {
    Playing, Reply_One, Reply_Many, Configure_Keys, Command_Mode,
    Metaserver_Select
} Input_State;

typedef enum {
  range_bottom = -1, range_none = 0, range_bow = 1, range_magic = 2,
  range_wand = 3, range_rod = 4, range_scroll = 5, range_horn = 6,
  range_steal = 7,
  range_size = 8
} rangetype;

/**
 * @defgroup CONFIG_xxx CONFIG_xxx want_config array indices.
 * Definitions to index into an array of most of the configuration options.
 *
 * Instead of having a whole mess of variables of different names, instead use
 * a common 16 bit signed array, and index into these - this makes processing
 * in the GUI aspect of the GTK client much easier.
 *
 * There are also 2 elements - want_options, and use_options.  The former is
 * what the player wants to use, the latter is what is currently in use.
 * There are many options that can not be switched between during actual play,
 * but we want to record what the player has changed them to so that when we
 * save them out, we save what the player wants, and not what is currently
 * being used.
 *
 * Note that all the GUI interfaces may not use all these values, but making
 * them available here makes it easy for the GUI to present a nice interface.
 *
 * 0 is intentially skipped so the index into this doesn't get a default if a
 * table has a blank value
 *
 * CONFIG_NUMS is the number of configuration options; don't forget to add to
 * some of:
 *
 *   common/init.c config_names,
 *                 init_client_vars,
 *   x11/x11.c load_defaults
 *             save_defaults
 *   gtk/config.c load_defaults
 *                save_defaults
 *
 * and probably other places, if you add a new option.
 */
/*@{*/
#define CONFIG_DOWNLOAD         1
#define CONFIG_ECHO             2
#define CONFIG_FASTTCP          3
#define CONFIG_CWINDOW          4
#define CONFIG_CACHE            5
#define CONFIG_FOGWAR           6
#define CONFIG_ICONSCALE        7
#define CONFIG_MAPSCALE         8
#define CONFIG_POPUPS           9
#define CONFIG_DISPLAYMODE      10     /**< @sa CFG_DM_xxx */
#define CONFIG_SHOWICON         11
#define CONFIG_TOOLTIPS         12
#define CONFIG_SOUND            13
#define CONFIG_SPLITINFO        14
#define CONFIG_SPLITWIN         15
#define CONFIG_SHOWGRID         16
#define CONFIG_LIGHTING         17      /**< @sa CFG_LT_xxx */
#define CONFIG_TRIMINFO         18
#define CONFIG_MAPWIDTH         19
#define CONFIG_MAPHEIGHT        20
#define CONFIG_FOODBEEP         21
#define CONFIG_DARKNESS         22
#define CONFIG_PORT             23      /**< Is this useful any more? */
#define CONFIG_GRAD_COLOR       24
#define CONFIG_RESISTS          25
#define CONFIG_SMOOTH           26
#define CONFIG_SPLASH           27
#define CONFIG_APPLY_CONTAINER  28      /**< Reapply container */
#define CONFIG_MAPSCROLL        29      /**< Use bitmap operations for map
                                             scrolling */
#define CONFIG_SIGNPOPUP        30
#define CONFIG_TIMESTAMP        31
#define CONFIG_NUMS             32      /**< This should always be the last
                                             value in the CONFIG_xxx list. */
/*@}*/

/**
 * @defgroup CFG_LT_xxx CONFIG_LIGHTING values.
 * Values that may be assigned to want_config[CONFIG_LIGHTING].
 */
/*@{*/
#define CFG_LT_NONE         0
#define CFG_LT_TILE         1
#define CFG_LT_PIXEL        2
#define CFG_LT_PIXEL_BEST   3
/*@}*/

/**
 * @defgroup CFG_DM_xxx CONFIG_DISPLAYMODE values.
 * Values that may be assigned to want_config[CONFIG_DISPLAYMODE].
 */
/*@{*/
#define CFG_DM_PIXMAP       0
#define CFG_DM_SDL          1
#define CFG_DM_OPENGL       2
/*@}*/

extern gint16 want_config[CONFIG_NUMS], use_config[CONFIG_NUMS];

extern const char *const config_names[CONFIG_NUMS]; /**< See common/init.c -
                                                     *   number mapping used
                                                     *   when loading/saving
                                                     *   the values.
                                                     */
typedef struct Stat_struct {
    gint8 Str;                          /**< Strength */
    gint8 Dex;                          /**< Dexterity */
    gint8 Con;                          /**< Constitution */
    gint8 Wis;                          /**< Wisdom */
    gint8 Cha;                          /**< Charisma */
    gint8 Int;                          /**< Intelligence */
    gint8 Pow;                          /**< Power */
    gint8 wc;                           /**< Weapon Class */
    gint8 ac;                           /**< Armour Class */
    gint8 level;                        /**< Experience level */
    gint16 hp;                          /**< Hit Points */
    gint16 maxhp;                       /**< Maximum hit points */
    gint16 sp;                          /**< Spell points for casting spells */
    gint16 maxsp;                       /**< Maximum spell points. */
    gint16 grace;                       /**< Spell points for using prayers. */
    gint16 maxgrace;                    /**< Maximum spell points. */
    gint64 exp;                         /**< Experience.  Killers gain 1/10. */
    gint16 food;                        /**< Quantity food in stomach.
                                         *   0 = starved.
                                         */
    gint16 dam;                         /**< How much damage this object does
                                         *   for each hit
                                         */
    gint32 speed;                       /**< Speed (is displayed as a float) */
    gint32 weapon_sp;                   /**< Weapon speed (displayed in client
                                         *   as a float)
                                         */
    guint32 attuned;                     /**< Spell paths to which the player is
                                         *   attuned
                                         */
    guint32 repelled;                    /**< Spell paths to which the player is
                                         *   repelled
                                         */
    guint32 denied;                      /**< Spell paths denied to the player*/
    guint16 flags;                       /**< Contains fire on/run on flags */
    gint16 resists[30];                 /**< Resistant values */
    guint32 resist_change:1;             /**< Resistant value change flag */
    gint16 skill_level[MAX_SKILL];      /**< Level of known skills */
    gint64 skill_exp[MAX_SKILL];        /**< Experience points for skills */
    guint32 weight_limit;                /**< Carrying weight limit */
} Stats;

typedef struct Spell_struct {
    struct Spell_struct *next;
    char name[256];                     /**< One length byte plus data       */
    char message[10000];                /**< This is huge, the packets can't
                                         *   be much bigger than this anyway */
    guint32 tag;                         /**< Unique ID number for a spell so
                                         *   updspell etc can operate on it. */
    guint16 level;                       /**< The casting level of the spell. */
    guint16 time;                        /**< Casting time in server ticks.   */
    guint16 sp;                          /**< Mana per cast; may be zero.     */
    guint16 grace;                       /**< Grace per cast; may be zero.    */
    guint16 dam;                         /**< Damage done by spell though the
                                         *   meaning is spell dependent and
                                         *   actual damage may depend on how
                                         *   the spell works.                */
    guint8 skill_number;                 /**< The index in the skill arrays,
                                         *   plus CS_STAT_SKILLINFO. 0: no
                                         *   skill used for cast.  See also:
                                         *   request_info skill_info         */
    char *skill;                        /**< Pointer to the skill name,
                                         *   derived from the skill number.  */
    guint32 path;                        /**< The bitmask of paths this spell
                                         *   belongs to.  See request_info
                                         *   spell_paths and stats about
                                         *   attunement, repulsion, etc.     */
    gint32 face;                        /**< A face ID that may be used to
                                         *   show a graphic representation
                                         *   of the spell.                   */
    guint8 usage;                        /**< Spellmon 2 data.  Values are:
                                         *   0: No argument required.
                                         *   1: Requires other spell name.
                                         *   2: Freeform string is optional.
                                         *   3: Freeform string is required. */
    char requirements[256];             /**< Spellmon 2 data. One length byte
                                         *   plus data. If the spell requires
                                         *   items to be cast, this is a list
                                         *   of req'd items. Comma-separated,
                                         *   number of items, singular names
                                         *   (like ingredients for alchemy). */
} Spell;

typedef struct Player_Struct {
    item        *ob;                    /**< Player object */
    item        *below;                 /**< Items below the player
                                         *   (pl.below->inv) */
    item        *container;             /**< open container */
    guint16      count_left;             /**< count for commands */
    Input_State input_state;            /**< What the input state is */
    char        last_command[MAX_BUF];  /**< Last command entered */
    char        input_text[MAX_BUF];    /**< keys typed (for long commands) */
    item        *ranges[range_size];    /**< Object that is used for that */
                                        /**< range type */
    guint8       ready_spell;            /**< Index to spell that is readied */
    char        spells[255][40];        /**< List of all the spells the */
                                        /**< player knows */
    Stats       stats;                  /**< Player stats */
    Spell       *spelldata;             /**< List of spells known */
    char        title[MAX_BUF];         /**< Title of character */
    char        range[MAX_BUF];         /**< Range attack chosen */
    guint32      spells_updated;         /**< Whether or not spells updated */
    guint32      fire_on:1;              /**< True if fire key is pressed */
    guint32      run_on:1;               /**< True if run key is on */
    guint32      meta_on:1;              /**< True if fire key is pressed */
    guint32      alt_on:1;               /**< True if fire key is pressed */
    guint32      no_echo:1;              /**< If TRUE, don't echo keystrokes */
    guint32      count;                  /**< Repeat count on command */
    guint16      mmapx, mmapy;           /**< size of magic map */
    guint16      pmapx, pmapy;           /**< Where the player is on the magic
                                         *   map */
    guint8       *magicmap;              /**< Magic map data */
    guint8       showmagic;              /**< If 0, show the normal map,
                                         *   otherwise show the magic map. */
    guint16      mapxres,mapyres;        /**< Resolution to draw on the magic
                                         *   map. Only used in client-specific
                                         *   code, so it should move there. */
    char        *name;                  /**< Name of PC, set and freed in account.c
                                         *   play_character() (using data returned
                                         *   from server to AccountPlayersCmd, via
                                         *   character_choose window,
                                         *   OR in
                                         *   send_create_player_to_server() when
                                         *   new character created. */
} Client_Player;

/**
 * @defgroup MAX_xxx_xxx MAX_xxx_xxx Face and image constants.
 * Faceset information pretty much grabbed right from server/socket/image.c.
 */
/*@{*/
#define MAX_FACE_SETS   20
#define MAX_IMAGE_SIZE 320              /**< Maximum size of image in each
                                         *   direction.  Needed for the X11
                                         *   client, which wants to initialize
                                         *   some data once.  Increasing this
                                         *   would likely only need a bigger
                                         *   footprint.
                                         */
/*@}*/

typedef struct FaceSets_struct {
    guint8   setnum;                     /**<  */
    guint8   fallback;                   /**<  */
    char    *prefix;                    /**<  */
    char    *fullname;                  /**<  */
    char    *size;                      /**<  */
    char    *extension;                 /**<  */
    char    *comment;                   /**<  */
} FaceSets;

/**
 * One struct that holds most of the image related data to reduce danger of
 * namespace collision.
 */
typedef struct Face_Information_struct {
    guint8   faceset;
    char    *want_faceset;
    gint16  num_images;
    guint32  bmaps_checksum, old_bmaps_checksum;
    /**
     * Just for debugging/logging purposes.  This is cleared on each new
     * server connection.  This may not be 100% precise (as we increment
     * cache_hits when we find a suitable image to load - if the data is bad,
     * that would count as both a hit and miss.
     */
    gint16  cache_hits, cache_misses;
    guint8   have_faceset_info;          /**< Simple value to know if there is
                                         *   data in facesets[].
                                         */
    FaceSets    facesets[MAX_FACE_SETS];
} Face_Information;

extern Face_Information face_info;

extern Client_Player cpl;               /**< Player object. */
extern char *skill_names[MAX_SKILL];

extern int last_used_skills[MAX_SKILL+1]; /**< maps position to skill id with
                                           *  trailing zero as stop mark.
                                           */

typedef enum {
    LOG_DEBUG = 0,      ///< Useful debugging information
    LOG_INFO = 1,       ///< Minor, non-harmful issues
    LOG_WARNING = 2,    ///< Warning that something might not work
    LOG_ERROR = 3,      ///< Warning that something definitely didn't work
    LOG_CRITICAL = 4    ///< Fatal crash-worthy error
} LogLevel;

typedef struct PipeLog {
    char* name;
    LogLevel level;
    int log;                            /**< To log or not to log. */
}PipeLog;

#define CHILD_STDIN      1
#define CHILD_STDOUT     2
#define CHILD_STDERR     4
#define CHILD_SILENTFAIL 8
#define CHILD_TUBE       (CHILD_STDIN|CHILD_STDOUT|CHILD_STDERR)
typedef struct ChildProcess{
    char* name;
    int flag;
    int pid;
    int tube[3];
    PipeLog logger[3];
    struct ChildProcess* next;
}ChildProcess;

#define CHILD_PIPEIN(__child)  (__child->tube[0])
#define CHILD_PIPEOUT(__child) (__child->tube[1])
#define CHILD_PIPEERR(__child) (__child->tube[2])

/**
 * Translation of the STAT_RES names into printable names, in matching order.
 */
#define NUM_RESISTS 18

extern const char *const resists_name[NUM_RESISTS];
extern char *meta_server;
extern int serverloginmethod, wantloginmethod;
extern guint32   tick;

/**
 * Holds the names that correspond to skill and resistance numbers.
 */
typedef struct {
    const char *name;
    int         value;
} NameMapping;

extern NameMapping skill_mapping[MAX_SKILL], resist_mapping[NUM_RESISTS];

extern guint64   *exp_table;
extern guint16   exp_table_max;

/**
 * Map size the client will request the map to be.  The bigger it is, more
 * memory it will use.
 */
#define MAP_MAX_SIZE 31

/**
 * This is the smallest the map structure used for the client can be.  It
 * needs to be bigger than the MAP_MAX_SIZE simply because we have to deal
 * with off map big images, Also, the center point is moved around within this
 * map, so that if the player moves one space, we don't have to move around
 * all the data.
 */
#define MIN_ALLOCATED_MAP_SIZE  MAP_MAX_SIZE * 2

/**
 * How many spaces an object might extend off the map.  E.g. For bigimage
 * stuff, the head of the image may be off the the map edge.  This is the most
 * it may be off.  This is needed To cover case of need_recenter_map routines.
 */
#define MAX_MAP_OFFSET  8

/* Start of map handling code.
 *
 * For the most part, this actually is not window system specific, but
 * certainly how the client wants to store this may vary.
 */

#define MAXPIXMAPNUM 10000

/**
 * Used mostly in the cache.c file, however, it can be returned to the graphic
 * side of things so that they can update the image_data field.  Since the
 * common side has no idea what data the graphic side will point to, we use a
 * void pointer for that - it is completely up to the graphic side to
 * allocate/deallocate and cast that pointer as needed.
 */
typedef struct Cache_Entry {
    char    *filename;
    guint32  checksum;
    guint32  ispublic:1;
    void    *image_data;
    struct Cache_Entry  *next;
} Cache_Entry;

/**
 * @defgroup RI_IMAGE_xxx RI_IMAGE_xxx RequestInfo values.
 * Values used for various aspects of the library to hold state on what
 * requestinfo's we have gotten replyinfo for and what data was received.  In
 * this way, common/client.c can loop until it has gotten replies for all the
 * requestinfos it has sent.  This can be useful - we don't want the addme
 * command sent for example if we are going to use a different image set.  The
 * GUI stuff should really never change these variables, but I suppose I could
 * look at them for debugging/ status information.
 */
/*@{*/
#define RI_IMAGE_INFO 0x1
#define RI_IMAGE_SUMS 0x2
/*@}*/

extern int  replyinfo_status, requestinfo_sent, replyinfo_last_face;

typedef struct PlayerPosition {
  int x;
  int y;
} PlayerPosition;

extern PlayerPosition pl_pos;

typedef struct Msg_Type_Names {
    int        type;                    /**< Type of message */
    int        subtype;                 /**< Subtype of message */
    const char *style_name;             /**< Name of this message in the
                                         *   configfile.
                                         */
} Msg_Type_Names;

extern TextManager* firstTextManager;

/* declared/handled in commands.c .  These variables are documented
 * in that file - the data they present is created by the command
 * code, but consumed by the GUI code.
 */
extern char *motd, *news, *rules;
extern char *motd, *news, *rules;       /* Declared/handled in commands.c */
extern int num_races, used_races, num_classes, used_classes;
extern int stat_points, stat_min, stat_maximum;



/*
 * This structure is used to hold race/class adjustment info, as
 * received by the requestinfo command.  We get the same info
 * for both races and class, so it simplifies code to share a structure.
 */
/* This is how many stats (str, dex, con, etc) that are present
 * in the create character window.
 */
#define NUM_NEW_CHAR_STATS  7

/**
 * The usage of the stat_mapping is to simplify the code and make it easier
 * to expand.  Within the character creation, the different stats really
 * do not have any meaning - the handling is pretty basic - value user
 * has selected + race adjust + class adjustment = total stat.
 */
struct Stat_Mapping {
    const char  *widget_suffix; /* within the glade file, suffix used on widget */
    guint8       cs_value;       /* within the protocol, the CS_STAT value */
    guint8       rc_offset;      /* Offset into the stat_adj array */
};

extern struct Stat_Mapping stat_mapping[NUM_NEW_CHAR_STATS];

/**
 * For classes & races, the server can present some number of
 * choices, eg, the character gets to choose 1 skill from a
 * choice of many.  Eg RC_Choice entry represents one of these
 * choices.  However, each race/class may have multiple choices.
 * For example, if the class had the character make 2 choices -
 * one for a skill, and one for an item, 2 RC_Choice structures
 * would be used.
 */
struct RC_Choice {
    char *choice_name;                  /* name to respond, eg, race_choice_1 */
    char *choice_desc;                  /* Longer description of choice */
    int num_values;                     /* How many values we have */
    char **value_arch;    /* Array arch names */
    char **value_desc;    /* Array of description */
};

typedef struct Race_Class_Info {
    char    *arch_name;     /* Name of the archetype this correponds to */
    char    *public_name;   /* Public (human readadable) name */
    char    *description;   /* Description of the race/class */
    gint8   stat_adj[NUM_NEW_CHAR_STATS];   /* Adjustment values */
    int     num_rc_choice;                  /* Size of following array */
    struct RC_Choice    *rc_choice;         /* array of choices */
} Race_Class_Info;

typedef struct Starting_Map_Info {
    char    *arch_name;     /* Name of archetype for this map */
    char    *public_name;   /* Name of the human readable name */
    char    *description;   /* Description of this map */
} Starting_Map_Info;

extern Race_Class_Info *races, *classes;
extern Starting_Map_Info *starting_map_info;
extern int starting_map_number;
extern int maxfd;

/* End of commands.c data, start of other declarations */
#ifndef MIN
#define MIN(X__,Y__) ( (X__)<(Y__)?(X__):(Y__) )
#endif

/**
 * @defgroup INFO_xxx INFO_xxx login information constants.
 * Used for passing in to update_login_info() used instead of passing in the
 * strings.
 */
/*@{*/
#define INFO_NEWS  1
#define INFO_MOTD  2
#define INFO_RULES 3
/*@}*/

/* We need to declare most of the structs before we can include this */
#include "proto.h"

extern void client_mapsize(int width, int height);

/**
 * Write the given data to the server.
 */
extern bool client_write(const void *buf, int len);

extern void beat_init(int interval);
extern void beat_check();
extern void beat_reset();
extern int beat_interval;

#endif
