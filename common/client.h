/* Header file for new client. */

#include <config.h>
#include <client-types.h>
#include <newclient.h>
#include <item.h>

#ifdef HAVE_DMALLOC_H
#  include <dmalloc.h>
#endif


#define VERSION_CS 1022
#define VERSION_SC 1026

char VERSION_INFO[256];

/* Don't send more than this many outstanding commands to the server 
 * this is only a default value.
 */
#define COMMAND_WINDOW 10

#define STRINGCOMMAND 0

/* How many skill types server supports/client will get sent to it.
 * If more skills are added to server, this needs to get increased.
 */
#define MAX_SKILL   6

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
} Animations;

extern Animations animations[MAXANIM];


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
	    
} ClientSocket;

extern ClientSocket csocket;

extern int port_num,
	basenrofpixmaps;	/* needed so that we know where to
				 * start when creating the additional
				 * images in x11.c
				 */

extern char *server, *client_libdir,*image_file;

typedef enum Input_State {Playing, Reply_One, Reply_Many,
	Configure_Keys, Command_Mode, Metaserver_Select} Input_State;

typedef enum rangetype {
  range_bottom = -1, range_none = 0, range_bow = 1, range_magic = 2,
  range_wand = 3, range_rod = 4, range_scroll = 5, range_horn = 6,
  range_steal = 7,
  range_size = 8
} rangetype;

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
    sint32 exp;		    /* Experience.  Killers gain 1/10. */
    sint16 food;	    /* How much food in stomach.  0 = starved. */
    sint16 dam;		    /* How much damage this object does when hitting */
    sint32 speed;	    /* Gets converted to a float for display*/
    sint32 weapon_sp;	    /* Gets converted to a float for display */
    uint16 flags;	    /* contains fire on/run on flags */
    sint16 resists[30];	    /* Resistant values */
    uint32 resist_change:1; /* Resistant value has changed */
    sint16 skill_level[MAX_SKILL];  /* Level and experience totals for */
    sint32 skill_exp[MAX_SKILL];    /* skills */
} Stats;


typedef struct Player_Struct {
    item	*ob;		/* Player object */
    item	*below;		/* Items below the player (pl.below->inv) */
    item	*container;	/* open container */
    uint16	count_left;	/* count for commands */
    Input_State input_state;	/* What the input state is */
    char	last_command[MAX_BUF];	/* Last command entered */
    uint32	no_echo:1;	/* If TRUE, don't echo keystrokes */
    char	input_text[MAX_BUF];	/* keys typed (for long commands) */
    char	name[40];	/* name and password.  Only used while */
    char	password[40];	/* logging in. */
    rangetype	shoottype;	/* What type of range attack player has */
    item	*ranges[range_size];	/* Object that is used for that */
				/* range type */
    uint8	ready_spell;	/* Index to spell that is readied */
    char	spells[255][40];	/* List of all the spells the */
				/* player knows */
    uint8	map_x, map_y;	/* These are offset values.  See object.c */
				/* for more details */
    Stats	stats;		/* Player stats */
    char	title[MAX_BUF];	/* Title of character */
    char	range[MAX_BUF];	/* Range attack chosen */
    uint32	fire_on:1;	/* True if fire key is pressed */
    uint32	run_on:1;	/* True if run key is on */
    uint32	echo_bindings:1;/* If true, echo the command that the key */
				/* is bound to */
    uint32	food_beep:1;	/* if TRUE, then beep when food is low (red) */
    uint32	count;		/* Repeat count on command */
    uint16	mmapx, mmapy;	/* size of magic map */
    uint16	pmapx, pmapy;	/* Where the player is on the magic map */
    uint8	*magicmap;	/* Magic map data */
    uint8	showmagic;	/* If 0, show normal map, otherwise, show
				 * magic map.
				 */
    uint8	command_window;	/* How many outstanding commands to allow */
    uint16	mapxres,mapyres;/* resolution to draw on the magic map */

} Client_Player;

extern Client_Player cpl;		/* Player object. */
extern char *skill_names[MAX_SKILL];


/* To handle XPM display mode, #ifdef Xpm_Pix are only used in areas
 * that make XPM function calls, or areas where using certain display
 * methods is a lot more efficient.
 *
 * Xpm_Display can only be set if Xpm_Pix is defined.  Thus, a lot
 * of the #ifdefs can be removed - those functions will never be called,
 * or values used, because Display_Mode will never be set to Xpm_Display
 */

typedef enum Display_Mode {Pix_Display, Xpm_Display, Png_Display}
	Display_Mode;

extern Display_Mode display_mode;

extern int nosound; 

/* WE need to declare most of the structs before we can include this */
#include <proto.h>

extern int errno;

/* translation of the STAT_RES names into printable names,
 * in matching order.
 */
#define NUM_RESISTS 18
extern char *resists_name[NUM_RESISTS];
extern char *meta_server;
extern int meta_port,want_skill_exp, want_mapx, want_mapy;
extern int map1cmd,metaserver_on, want_darkness;
extern int mapx, mapy;

/* Map size the client will request the map to be.  Bigger it is,
 * more memory it will use
 */
#define MAP_MAX_SIZE	31

/* Fog of war stuff */
#define FOG_MAP_SIZE 512   /* Default size of virtual map */


/* Start of map handling code.
 * For the most part, this actually is not window system specific,
 * but certainly how the client wants to store this may vary.
 */

#define MAXFACES 5
#define MAXPIXMAPNUM 10000
struct MapCell {
  short faces[MAXFACES];
  int count;
  uint8 darkness;
  uint8 need_update:1;
  uint8 have_darkness:1;
  uint8 cleared:1; /* Used for fog of war code only */
};


struct Map {
  struct MapCell **cells;
  /* Store size of map so we know if map_size has changed
   * since the last time we allocated this;
   */
  int x;
  int y;
};

extern struct Map the_map;
