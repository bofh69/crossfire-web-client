/*
 * static char *rcsid_client_h =
 *   "$Id$";
 */
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001 Mark Wedel & Crossfire Development Team

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    The author can be reached via e-mail to crossfire-devel@real-time.com
*/

/* This file includes the various dependencies header files needed
 * by most everything.  It also declares structures and other variables
 * that the gui portion needs
 */

#include <config.h>
#include <version.h>
#include <client-types.h>
#include <newclient.h>
#include <item.h>

#ifdef HAVE_DMALLOC_H
#  include <dmalloc.h>
#endif

#define MULTKEYS

#define VERSION_CS 1023
#define VERSION_SC 1027

char VERSION_INFO[256];

/* Don't send more than this many outstanding commands to the server
 * this is only a default value.
 */
#define COMMAND_WINDOW 10

#define STRINGCOMMAND 0

/* How many skill types server supports/client will get sent to it.
 * If more skills are added to server, this needs to get increased.
 */
#define MAX_SKILL   CS_NUM_SKILLS

#define MAXANIM 2000

/* Values for send_command option */

#define SC_NORMAL 0
#define SC_FIRERUN 1
#define SC_ALWAYS 2

typedef struct Animations {
    uint16  flags;
    uint8   num_animations; /* number of animations.  Value of 2 means
			     * only faces[0],[1] have meaningfull values.
			     */
    uint16  *faces;
    uint8   speed;
    uint8   speed_left;
    uint8   phase;
} Animations;

extern Animations animations[MAXANIM];

#define CAN_SMOOTH(__SQUARE,__LEVEL) ( (                                       \
                                            (__SQUARE.heads[__LEVEL].face==0)  \
                                            && (__LEVEL >0)                    \
                                        )                                      \
                                        || (__SQUARE.smooth[layer])            \
                                     )

/* ClientSocket could probably hold more of the global values - it could
 * probably hold most all socket/communication related values instead
 * of globals.
 */
typedef struct ClientSocket {
    int	fd;
    SockList	inbuf;
    int	cs_version, sc_version;	/* Server versions of these */
    /* These are used for the newer 'windowing' method of commands -
     * number of last command sent, number of received confirmation
     */
    int command_sent, command_received;
    /* Time (in ms) players commands currently take to execute */
    int command_time;
    char* servername;
} ClientSocket;

extern ClientSocket csocket;

extern char *server, *client_libdir,*image_file, *sound_server;

typedef enum Input_State {Playing, Reply_One, Reply_Many,
	Configure_Keys, Command_Mode, Metaserver_Select} Input_State;

typedef enum rangetype {
  range_bottom = -1, range_none = 0, range_bow = 1, range_magic = 2,
  range_wand = 3, range_rod = 4, range_scroll = 5, range_horn = 6,
  range_steal = 7,
  range_size = 8
} rangetype;

/* This is a structure that contains most all of the
 * configuration options.  Instead of having a
 * whole mess of variables of different names, instead use
 * a common 16 bit signed array, and index into these -
 * this makes processing in the gui aspect of the GTK
 * client much easier.  There are also 2 elements -
 * want options, and use_options.  The former is what the
 * player wants to use, the later is what is currently
 * in use.  There are many options that can not be
 * switched between during actual play, but we want to
 * record what the player has changed them to so that
 * when we save them out, we save what the player wants,
 * and not what is currently being used.  Note that all the gui
 * interfaces may not use all these values, but making them
 * available here makes it easy for the GUI to present a
 * nice interface.
 * 0 is intentially skipped so the index into this doesn't
 * get a default if a table has a blank value
 *
 * CONFIG_NUMS is the number of configuration options; don't
 * forget to add to some of
 *   common/init.c config_names,
 *                 init_client_vars,
 *   x11/x11.c load_defaults
 *             save_defaults
 *   gtk/config.c load_defaults
 *                save_defaults
 * probably among other places, if you add a new option.
 */
#define CONFIG_DOWNLOAD	    1
#define CONFIG_ECHO	    2
#define CONFIG_FASTTCP	    3
#define CONFIG_CWINDOW	    4
#define CONFIG_CACHE	    5
#define CONFIG_FOGWAR	    6
#define CONFIG_ICONSCALE    7
#define CONFIG_MAPSCALE	    8
#define CONFIG_POPUPS	    9
#define CONFIG_DISPLAYMODE  10
#define CONFIG_SHOWICON	    11
#define CONFIG_TOOLTIPS	    12
#define CONFIG_SOUND	    13
#define CONFIG_SPLITINFO    14
#define CONFIG_SPLITWIN	    15
#define CONFIG_SHOWGRID	    16
#define CONFIG_LIGHTING	    17
#define CONFIG_TRIMINFO	    18
#define CONFIG_MAPWIDTH	    19
#define CONFIG_MAPHEIGHT    20
#define CONFIG_FOODBEEP	    21
#define CONFIG_DARKNESS	    22
#define CONFIG_PORT	    23		/* Not sure if useful at all anymore */
#define CONFIG_GRAD_COLOR   24
#define CONFIG_RESISTS      25
#define CONFIG_SMOOTH       26
#define CONFIG_SPLASH	    27
#define CONFIG_APPLY_CONTAINER	28	/* Reapply container */
#define CONFIG_MAPSCROLL    29		/* Use bitmap operations for map scrolling */
#define CONFIG_SIGNPOPUP    30
#define CONFIG_TIMESTAMP    31
#define CONFIG_NUMS	    32		/* This should always be the last value in the list */

/* CONFIG_LIGHTING can have several possible values - set them accordingly */
#define CFG_LT_NONE	    0
#define CFG_LT_TILE	    1
#define CFG_LT_PIXEL	    2
#define CFG_LT_PIXEL_BEST   3

/* CONFIG_DISPLAYMODE can have several possible values.  Give
 * defines for the possibilities.
 */
#define CFG_DM_PIXMAP	    0
#define CFG_DM_SDL	    1
#define CFG_DM_OPENGL	    2



extern sint16 want_config[CONFIG_NUMS], use_config[CONFIG_NUMS];
/* see common/init.c - basically, this is a string to number
 * mapping that is used when loading/saving the values.
 */
extern const char *const config_names[CONFIG_NUMS];


typedef struct Stat_struct {
    sint8 Str,Dex,Con,Wis,Cha,Int,Pow;
    sint8 wc,ac;	    /* Weapon Class and Armour Class */
    sint8 level;
    sint16 hp;		    /* Hit Points. */
    sint16 maxhp;
    sint16 sp;		    /* Spell points.  Used to cast spells. */
    sint16 maxsp;	    /* Max spell points. */
    sint16 grace;	    /* Spell points.  Used to cast spells. */
    sint16 maxgrace;	    /* Max spell points. */
    sint64 exp;		    /* Experience.  Killers gain 1/10. */
    sint16 food;	    /* How much food in stomach.  0 = starved. */
    sint16 dam;		    /* How much damage this object does when hitting */
    sint32 speed;	    /* Gets converted to a float for display*/
    sint32 weapon_sp;	    /* Gets converted to a float for display */
    uint32 attuned;         /* The spell paths to which the player is attuned */
    uint32 repelled;        /* The spell paths to which the player is repelled */
    uint32 denied;          /* The spell paths denied to the player*/
    uint16 flags;	    /* contains fire on/run on flags */
    sint16 resists[30];	    /* Resistant values */
    uint32 resist_change:1; /* Resistant value has changed */
    sint16 skill_level[MAX_SKILL];  /* Level and experience totals for */
    sint64 skill_exp[MAX_SKILL];    /* skills */
    uint32 weight_limit;    /* weight limit */
} Stats;


typedef struct Spell_struct {
    struct Spell_struct *next;
    char name[256]; /* the protocol allows one length bit, so 256 is the maximum name length */
    char message[10000]; /* this is plenty, the packets can't be much bigger than this anyway */
    uint32 tag; /* used to identify the spell by updspell */
    uint16 level;
    uint16 time; /* number of ticks to cast */
    uint16 sp;
    uint16 grace;
    uint16 dam;
    uint8 skill_number; /* the index in the skill arrays, plus CS_STAT_SKILLINFO */
    char *skill; /* pointer to the skill name, derived from the skill number */
    uint32 path; /* the bitmask of paths this spell belongs to */
    sint32 face; 
} Spell;

typedef struct Player_Struct {
    item	*ob;		/* Player object */
    item	*below;		/* Items below the player (pl.below->inv) */
    item	*container;	/* open container */
    uint16	count_left;	/* count for commands */
    Input_State input_state;	/* What the input state is */
    char	last_command[MAX_BUF];	/* Last command entered */
    char	input_text[MAX_BUF];	/* keys typed (for long commands) */
    item	*ranges[range_size];	/* Object that is used for that */
				/* range type */
    uint8	ready_spell;	/* Index to spell that is readied */
    char	spells[255][40];	/* List of all the spells the */
				/* player knows */
    Stats	stats;		/* Player stats */
    Spell	*spelldata;	/* linked list of spells known to the player */
    char	title[MAX_BUF];	/* Title of character */
    char	range[MAX_BUF];	/* Range attack chosen */
    uint32	spells_updated; /* tracks whether the spells updated*/ 
    uint32	fire_on:1;	/* True if fire key is pressed */
    uint32	run_on:1;	/* True if run key is on */
    uint32	meta_on:1;	/* True if fire key is pressed */
    uint32	alt_on:1;	/* True if fire key is pressed */
    uint32	no_echo:1;	/* If TRUE, don't echo keystrokes */
    uint32	count;		/* Repeat count on command */
    uint16	mmapx, mmapy;	/* size of magic map */
    uint16	pmapx, pmapy;	/* Where the player is on the magic map */
    uint8	*magicmap;	/* Magic map data */
    uint8	showmagic;	/* If 0, show normal map, otherwise, show
				 * magic map.
				 */
    uint16	mapxres,mapyres;/* Resolution to draw on the magic map
                                 * Only used in client-specific code, so it should move there. */

#ifdef MULTKEYS
    char    name[ 40 ]; /* Player's name, for player-specific key files */
#endif
} Client_Player;


/* This faceset information is pretty much grabbed right from
 * server/socket/image.c
 */

#define MAX_FACE_SETS   20

/* Max size of image in each direction.  This
 * is needed for the x11 client, which wants to
 * initalize some data once.  Increasing this would
 * likely only need a bigger footrpint
 */
#define MAX_IMAGE_SIZE 320
typedef struct {
    uint8   setnum;
    char    *prefix;
    char    *fullname;
    uint8   fallback;
    char    *size;
    char    *extension;
    char    *comment;
} FaceSets;


/* Make one struct that holds most of the image related data.
 * reduces danger of namespace collision.
 */
typedef struct {
    uint8   faceset;
    char    *want_faceset;
    sint16  num_images;
    uint32  bmaps_checksum, old_bmaps_checksum;
    /* Just for debugging/logging purposes.  This is cleared
     * on each new server connection.  This may not be
     * 100% precise (as we increment cache_hits when we
     * find a suitable image to load - if the data is bad,
     * that would count as both a hit and miss
     */
    sint16  cache_hits, cache_misses;
    uint8	have_faceset_info;	/* Simple value to know if there is data in facesets[] */
    FaceSets	facesets[MAX_FACE_SETS];
} Face_Information;

extern Face_Information face_info;


extern Client_Player cpl;		/* Player object. */
extern char *skill_names[MAX_SKILL];


typedef enum LogLevel {
  LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARNING = 2, LOG_ERROR = 3, LOG_CRITICAL = 4
} LogLevel;

typedef struct LogEntry{
    char* message;
    char* origin;
    LogLevel level;
    struct LogEntry* next;
} LogEntry;
extern LogEntry* LogFirst;
extern LogEntry* LogLast;
typedef void (*LogListener)(LogEntry*);
#define LOG_APPEND(_Entry) if (LogLast) {LogLast->next=_Entry;} else {LogFirst=_Entry;};LogLast=_Entry;_Entry->next=NULL
#define LOG_NEW_ENTRY (LogEntry*)calloc(1,sizeof(LogEntry))
#define LOG_SETMESSAGE(_Entry,_msg) free(_Entry->message);_Entry->message=strdup(_msg)
#define LOG_SETORIGIN(_Entry,_orig) free(_Entry->origin);_Entry->origin=strdup(_orig)

typedef struct{
    char* name;
    LogLevel level;
    int log; /*yes or no to log*/
}PipeLog;
#define CHILD_STDIN 1
#define CHILD_STDOUT 2
#define CHILD_STDERR 4
#define CHILD_SILENTFAIL 8
#define CHILD_TUBE (CHILD_STDIN|CHILD_STDOUT|CHILD_STDERR)
typedef struct ChildProcess{
    char* name;
    int flag;
    int pid;
    int tube[3];
    PipeLog logger[3];
    struct ChildProcess* next;
}ChildProcess;
#define CHILD_PIPEIN(__child) (__child->tube[0])
#define CHILD_PIPEOUT(__child) (__child->tube[1])
#define CHILD_PIPEERR(__child) (__child->tube[2])


#ifndef CPROTO
/* We need to declare most of the structs before we can include this */
#include <proto.h>
#endif

/* translation of the STAT_RES names into printable names,
 * in matching order.
 */
#define NUM_RESISTS 18
extern const char *const resists_name[NUM_RESISTS];
extern char *meta_server;
extern int meta_port,want_skill_exp;
extern int map1cmd,metaserver_on;
extern uint32	tick;

/* Map size the client will request the map to be.  Bigger it is,
 * more memory it will use
 */
#define MAP_MAX_SIZE	31

/* This is basically the smallest the map structure
 * used for the client can be.  It needs to be bigger than
 * the min map size above simply because we have to deal with
 * off map big images, Also, we move the center point
 * around within this map, so that if the player moves one space,
 * we don't have to move around all the data.
 */
#define MIN_ALLOCATED_MAP_SIZE	MAP_MAX_SIZE * 2

/* This is how many spaces an object might extend off the map.
 * Eg, for bigimage stuff, the head of the image may be off the
 * the map edge.  This is the most it may be off.  This is needed
 * To cover case of need_recenter_map routines.
 */
#define MAX_MAP_OFFSET	8

/* Start of map handling code.
 * For the most part, this actually is not window system specific,
 * but certainly how the client wants to store this may vary.
 */

#define MAXPIXMAPNUM 10000

/* This is used mostly in the cache.c file, however, it
 * can be returned to the graphic side of things so that they
 * can update the image_data field.  Since the common side
 * has no idea what data the graphic side will point to,
 * we use a void pointer for that - it is completely up to
 * the graphic side to allocate/deallocate and cast that
 * pointer as needed.
 */
typedef struct Cache_Entry {
    char    *filename;
    uint32  checksum;
    uint32  ispublic:1;
    void    *image_data;
    struct Cache_Entry	*next;
} Cache_Entry;

/* These values are used for various aspects of the library to
 * hold state on what requestinfo's we have gotten replyinfo
 * for and what data was received.  In this way, common/client.c
 * can loop until it has gotten replies for all the requestinfos
 * it has sent.  This can be useful - we don't want the addme
 * command sent for example if we are going to use a different
 * image set.  The GUI stuff should really never chnage these
 * variables, but could I suppose look at them for debugging/
 * status information.
 */

#define RI_IMAGE_INFO	    0x1
#define RI_IMAGE_SUMS	    0x2
extern int  replyinfo_status, requestinfo_sent, replyinfo_last_face;

typedef struct
{
  int x;
  int y;
} PlayerPosition;

extern PlayerPosition pl_pos;

extern TextManager* firstTextManager;

#ifndef MIN
#define MIN(X__,Y__) ( (X__)<(Y__)?(X__):(Y__) )
#endif
