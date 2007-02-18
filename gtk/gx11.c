const char *rcsid_gtk_gx11_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001-2003,2006 Mark Wedel & Crossfire Development Team

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

/*
 * This file contains the core window code.
 */

/* Most functions in this file are private.  Here is a list of
 * the global functions:
 *
 * draw_color_info(int color, char*buf) - draws text in specified color
 * draw_info - draw info in the info window
 * end_windows - used when exiting
 * init_windows - called when starting up
 * load_images - creates the bitmaps and pixmaps (if being used)
 * create_pixmap - creates a pixmap from given file and assigns to
 *		the given face
 * create_xpm - as create_pixmap, but does an XPM image
 * load_additional_images - loads images that have been downloaded
 *	from the server in prior sessions
 *
 * draw_stats(int) - draws the stat window.  Pass 1 to redraw all
 *	stats, not only those that changed
 *
 * draw_message_window(int) - draws the message window.  Pass 1 to redraw
 *	all the bars, not only those that changed.
 *
 * NOTE: create_pixmap and create_xpm can be empty functions if the
 * client will always use fonts - in that case, it should never
 * request Bitmap or Pixmap data, and thus not need the create
 * functions above
 *
 * Only functions in this file should be calling functions like
 * draw_stats and draw_message_window with redraw set - functions
 * in other files should always pass 0, because they will never have
 * the information of whether a redraw is needed.
 */


#include "config.h"

#include <errno.h>

/* gtk */
#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#endif
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_SDL
#include <SDL.h>
#include <SDL_image.h>
#endif

/* always include our local headers after the system headers are included */
#include "client.h"
/*#include "clientbmap.h"*/
#include "item.h"
#include "pixmaps/crossfiretitle.xpm"
#include "gx11.h"
#include "gtkproto.h"
#include <script.h>
#include <p_cmd.h>
#include <time.h>

#include "mapdata.h"


#ifdef HAVE_SDL
/* These are only used in SDL mode at current time */
extern SDL_Surface* mapsurface;
#endif

static const char *const colorname[] = {
"Black",                /* 0  */
"White",                /* 1  */
"Navy",                 /* 2  */
"Red",                  /* 3  */
"Orange",               /* 4  */
"DodgerBlue",           /* 5  */
"DarkOrange2",          /* 6  */
"SeaGreen",             /* 7  */
"DarkSeaGreen",         /* 8  */        /* Used for window background color */
"Grey50",               /* 9  */
"Sienna",               /* 10 */
"Gold",                 /* 11 */
"Khaki"                 /* 12 */
};

#define DEFAULT_IMAGE_SIZE	32

int image_size=DEFAULT_IMAGE_SIZE;
int map_image_size=DEFAULT_IMAGE_SIZE, map_image_half_size=DEFAULT_IMAGE_SIZE/2;
PixmapInfo *pixmaps[MAXPIXMAPNUM];


  /* copy from server: include/define.h */
#define PU_NOTHING              0x00000000

#define PU_DEBUG                0x10000000
#define PU_INHIBIT              0x20000000
#define PU_STOP                 0x40000000
#define PU_NEWMODE              0x80000000

#define PU_RATIO                0x0000000F

#define PU_FOOD                 0x00000010
#define PU_DRINK                0x00000020
#define PU_VALUABLES            0x00000040
#define PU_BOW                  0x00000080

#define PU_ARROW                0x00000100
#define PU_HELMET               0x00000200
#define PU_SHIELD               0x00000400
#define PU_ARMOUR               0x00000800

#define PU_BOOTS                0x00001000
#define PU_GLOVES               0x00002000
#define PU_CLOAK                0x00004000
#define PU_KEY                  0x00008000

#define PU_MISSILEWEAPON        0x00010000
#define PU_ALLWEAPON            0x00020000
#define PU_MAGICAL              0x00040000
#define PU_POTION               0x00080000

#define PU_SPELLBOOK		0x00100000
#define PU_SKILLSCROLL		0x00200000
#define PU_READABLES		0x00400000
#define PU_MAGIC_DEVICE		0x00800000

#define PU_NOT_CURSED		0x01000000
#define PU_JEWELS		0x02000000
#define PU_FLESH		0x04000000



typedef struct {
  GtkWidget *playername;
  GtkWidget *score;
  GtkWidget *level;
  GtkWidget *hp;
  GtkWidget *sp;
  GtkWidget *gr;
  GtkWidget *Str;
  GtkWidget *Dex;
  GtkWidget *Con;
  GtkWidget *Int;
  GtkWidget *Wis;
  GtkWidget *Cha;
  GtkWidget *Pow;
  GtkWidget *wc;
  GtkWidget *dam;
  GtkWidget *ac;
  GtkWidget *armor;
  GtkWidget *speed;
  GtkWidget *food;
  GtkWidget *skill;
  GtkWidget *skill_exp[MAX_SKILL*2];
} StatWindow;

static char **gargv;

#define MAX_HISTORY 50
#define MAX_COMMAND_LEN 256
static char history[MAX_HISTORY][MAX_COMMAND_LEN];
static int cur_history_position=0, scroll_history_position=0;

extern int maxfd;
struct timeval timeout;
static gint csocket_fd = 0;

static int gargc;

static uint8
    bigmap=FALSE;	/* True if we've moved some windows around for big maps */

uint8
    time_map_redraw=FALSE,
    updatekeycodes=FALSE,
    redraw_needed=FALSE;


/* Default size of scroll buffers is 100 K */
static int info1_num_chars=0, info2_num_chars=0, info1_max_chars=100000,
    info2_max_chars=100000;


typedef struct {
  int x;
  int y;
} MapPos;


/* vitals window */

typedef struct {
  GtkWidget *bar;
  GtkStyle *style[2];
  int state;
} Vitals;

static Vitals vitals[4];
GtkWidget *run_label, *fire_label;
static GtkWidget *restable;	/* resistance table */
static GtkWidget *res_scrolled_window;	/* window the resistances are in */
static GtkWidget *skill_scrolled_window; /* window the skills are in */


static GtkWidget *resists[NUM_RESISTS];
GtkWidget *ckentrytext, *ckeyentrytext, *cmodentrytext, *cnumentrytext;

GdkColor gdk_green =    { 0, 0, 0xcfff, 0 };
GdkColor gdk_red =    { 0, 0xcfff, 0, 0 };
GdkColor gdk_grey = { 0, 0xea60, 0xea60, 0xea60 };
GdkColor gdk_black = { 0, 0, 0, 0 };

static GdkColor map_color[16];
/* Not static so it can be used in inventory.c for highlighting. */
GdkColor root_color[16];
static GdkPixmap *magicgdkpixmap;
static GdkGC *magic_map_gc;
static GtkWidget *mapvbox;
GdkPixmap   *mapwindow;
GdkBitmap *dark1, *dark2, *dark3;
GdkPixmap *dark;

GtkTooltips *tooltips;

static GtkWidget *dialogtext;
static GtkWidget *dialog_window;
GtkWidget *drawingarea;

GdkGC *mapgc;

GtkWidget *cclist;
static gboolean draw_info_freeze1=FALSE, draw_info_freeze2=FALSE;

enum {
    locked_icon = 1, applied_icon, unpaid_icon,
    damned_icon, cursed_icon, magic_icon, close_icon,
    stipple1_icon, stipple2_icon, max_icons
};


GtkWidget *entrytext; /* "Command-line" frame, built in get_info_display(). */
static GtkObject *text_hadj,*text_vadj;
static GtkObject *text_hadj2,*text_vadj2;
static GtkWidget *gameframe, *stat_frame, *message_frame;

static StatWindow statwindow;
/* gtk */

GtkWidget *gtkwin_root, *gtkwin_info;
GtkWidget *gtkwin_info_text; /* Referenced by inventory::count_callback. */
GtkWidget *gtkwin_info_text2; /* Used when CONFIG_SPLITINFO */
GtkWidget *gtkwin_stats, *gtkwin_message, *gtkwin_look, *gtkwin_inv;


static GtkWidget *gtkwin_about = NULL;
static GtkWidget *gtkwin_bug = NULL;
static GtkWidget *gtkwin_splash = NULL;
static GtkWidget *gtkwin_shelp = NULL;
static GtkWidget *gtkwin_magicmap = NULL;

static GtkWidget *bugtrack = NULL;

/* these are the panes used in splitting up the window in non root
 * windows mode.  Need to be globals so we can get/set the
 * information when loading/saving the positions.
 */

static GtkWidget
    *inv_hpane,		/* Split between inv,message window and stats/game/.. window */
    *stat_info_hpane,	/* game/stats on left, info windows on right */
    *stat_game_vpane,	/* status window/game window split */
    *game_bar_vpane,	/* game/scroll split */
    *inv_look_vpane,	/* inventory/look split */
    *info_vpane;	/* split for 2 info windows */

static char *last_str;

/** Pickup mode. */
static unsigned int pickup_mode = 0;

int updatelock = 0;

/* this is used for caching the images across runs.  When we get a face
 * command from the server, we check the facecache for that name.  If
 * so, we can then use the num to find out what face number it is on the
 * local side.
 */
struct FaceCache {
    char    *name;
    uint16  num;
} facecache[MAXPIXMAPNUM];

int misses=0,total=0;

static void disconnect(GtkWidget *widget);

/* Called from disconnect command - that closes the socket -
 * we just need to do the gtk cleanup.
 */
void cleanup_connection() {
    if (csocket_fd) {
	gdk_input_remove(csocket_fd);
	csocket_fd=0;
	gtk_main_quit();
    }
    cleanup_textmanagers();
}

/* main loop iteration related stuff */
static void do_network(void) {
    fd_set tmp_read;
    int pollret;
    extern int updatelock;

    if (csocket.fd==-1) {
	if (csocket_fd) {
	    gdk_input_remove(csocket_fd);
	    csocket_fd=0;
	    gtk_main_quit();
	}
	return;
    }

    if (updatelock < 20) {
	FD_ZERO(&tmp_read);
	FD_SET(csocket.fd, &tmp_read);
	script_fdset(&maxfd,&tmp_read);
	pollret = select(maxfd, &tmp_read, NULL, NULL, &timeout);
	if (pollret==-1) {
	    LOG(LOG_WARNING,"gtk::do_network", "Got errno %d on select call.", errno);
	}
	else if ( pollret>0 ) {
	    if (FD_ISSET(csocket.fd, &tmp_read)) {
		DoClient(&csocket);
#ifndef WIN32
		if ( pollret > 1 ) script_process(&tmp_read);
#endif
	    }
	    else {
		script_process(&tmp_read);
	    }
	}
    } else {
	LOG(LOG_INFO,"gtk::do_network","locked for network recieves.\n");
    }
    if (csocket.fd==-1) {
	if (csocket_fd) {
	    gdk_input_remove(csocket_fd);
	    csocket_fd=0;
	    gtk_main_quit();
	}
	return;
    }

}

#ifdef WIN32 /* Win32 scripting support */
int do_scriptout()
{
  script_process(NULL);
  return(TRUE);
}
#endif /* WIN32 */

static void event_loop(void)
{
    gint fleep;
    extern int do_timeout(void); /* forward */
    int tag;

    if (MAX_TIME==0) {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
    }
    maxfd = csocket.fd + 1;

    if (MAX_TIME!=0) {
	timeout.tv_sec = 0;/* MAX_TIME / 1000000;*/
	timeout.tv_usec = 0;/* MAX_TIME % 1000000;*/
    }

    fleep =  gtk_timeout_add (100,
			  (GtkFunction) do_timeout,
			  NULL);

#ifdef WIN32
	gtk_timeout_add (25, (GtkFunction) do_scriptout, NULL);
#endif

    csocket_fd = gdk_input_add ((gint) csocket.fd,
                              GDK_INPUT_READ,
                              (GdkInputFunction) do_network, &csocket);
    tag = csocket_fd;

    gtk_main();
    gtk_timeout_remove(tag);

    cleanup_textmanagers();
    LOG(LOG_INFO,"gtk::event_loop","gtk_main exited, returning from event_loop");
}




/* Handle mouse presses in the game window */

static void button_map_event(GtkWidget *widget, GdkEventButton *event) {
    int dx, dy, i, x, y, xmidl, xmidh, ymidl, ymidh;

    x=(int)event->x;
    y=(int)event->y;
    dx=(x-2)/map_image_size-(use_config[CONFIG_MAPWIDTH]/2);
    dy=(y-2)/map_image_size-(use_config[CONFIG_MAPHEIGHT]/2);
    xmidl=(use_config[CONFIG_MAPWIDTH]/2) * map_image_size;
    xmidh=(use_config[CONFIG_MAPWIDTH]/2 + 1) * map_image_size;
    ymidl=(use_config[CONFIG_MAPHEIGHT]/2) * map_image_size;
    ymidh=(use_config[CONFIG_MAPHEIGHT]/2 + 1) * map_image_size;

    switch (event->button) {
	case 1:
	    look_at(dx,dy);
	    break;

	case 2:
	case 3:
	    if (x<xmidl)
		i = 0;
	    else if (x>xmidh)
		i = 6;
	    else i =3;

	    if (y>ymidh)
		i += 2;
	    else if (y>ymidl)
		i++;

	    if (event->button==2) {
		switch (i) {
		    case 0: fire_dir (8);break;
		    case 1: fire_dir (7);break;
		    case 2: fire_dir (6);break;
		    case 3: fire_dir (1);break;
		    case 5: fire_dir (5);break;
		    case 6: fire_dir (2);break;
		    case 7: fire_dir (3);break;
		    case 8: fire_dir (4);break;
		}
		/* Only want to fire once */
		clear_fire();
	    }
	    else switch (i) {
		case 0: move_player (8);break;
		case 1: move_player (7);break;
		case 2: move_player (6);break;
		case 3: move_player (1);break;
		case 5: move_player (5);break;
		case 6: move_player (2);break;
		case 7: move_player (3);break;
		case 8: move_player (4);break;
	    }
    }
}





/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

/* Initializes the data for image caching */
static void init_cache_data(void)
{
    int i;
    GtkStyle *style;
#include "pixmaps/question.xpm"


    LOG(LOG_INFO,"gtk::init_cache_data","Init Cache");

    style = gtk_widget_get_style(gtkwin_root);
    pixmaps[0] = malloc(sizeof(PixmapInfo));
    pixmaps[0]->icon_image = gdk_pixmap_create_from_xpm_d(gtkwin_root->window,
							(GdkBitmap**)&pixmaps[0]->icon_mask,
							&style->bg[GTK_STATE_NORMAL],
							(gchar **)question);
#ifdef HAVE_SDL
    if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL) {
	/* make a semi transparent question mark symbol to
	 * use for the cached images.
	 */
#include "pixmaps/question.sdl"
	pixmaps[0]->map_image = SDL_CreateRGBSurfaceFrom(question_sdl,
		32, 32, 1, 4, 1, 1, 1, 1);
	SDL_SetAlpha(pixmaps[0]->map_image, SDL_SRCALPHA, 70);
	pixmaps[0]->fog_image = SDL_CreateRGBSurfaceFrom(question_sdl,
		32, 32, 1, 4, 1, 1, 1, 1);
	SDL_SetAlpha(pixmaps[0]->fog_image, SDL_SRCALPHA, 70);
    }
    else
#endif
    {
	pixmaps[0]->map_image =  pixmaps[0]->icon_image;
	pixmaps[0]->fog_image =  pixmaps[0]->icon_image;
	pixmaps[0]->map_mask =  pixmaps[0]->icon_mask;
    }
    pixmaps[0]->icon_width = pixmaps[0]->icon_height = pixmaps[0]->map_width = pixmaps[0]->map_height = map_image_size;
    pixmaps[0]->smooth_face = 0;

    /* Don't do anything special for SDL image - rather, that drawing
     * code will check to see if there is no data
     */

    /* Initialize all the images to be of the same value. */
    for (i=1; i<MAXPIXMAPNUM; i++)  {
	pixmaps[i] = pixmaps[0];
    }

    init_common_cache_data();
}

/* Deals with command history.  if direction is 0, we are going backwards,
 * if 1, we are moving forward.
 */

void gtk_command_history(int direction)
{
    int i=scroll_history_position;
    if (direction) {
	i--;
	if (i<0) i+=MAX_HISTORY;
	if (i == cur_history_position) return;
    } else {
	i++;
	if (i>=MAX_HISTORY) i = 0;
	if (i == cur_history_position) {
	    /* User has forwarded to what should be current entry - reset it now. */
	    gtk_entry_set_text(GTK_ENTRY(entrytext), "");
	    gtk_entry_set_position(GTK_ENTRY(entrytext), 0);
	    scroll_history_position=cur_history_position;
	    return;
	}
    }

    if (history[i][0] == 0) return;

    scroll_history_position=i;
/*    fprintf(stderr,"resetting postion to %d, data = %s\n", i, history[i]);*/
    gtk_entry_set_text(GTK_ENTRY(entrytext), history[i]);
    gtk_entry_set_position(GTK_ENTRY(entrytext), strlen(history[i]));
    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
    cpl.input_state = Command_Mode;
}

void gtk_complete_command(void)
{
    const gchar *entry_text, *newcommand;

    entry_text = gtk_entry_get_text(GTK_ENTRY(entrytext));
    newcommand = complete_command(entry_text);
    /* value differ, so update window */
    if (newcommand != NULL) {
        /* Set position to last character */
        gtk_entry_set_text(GTK_ENTRY(entrytext), newcommand);
    }
    else
    /* grab focus anyway, key can be used somewhere, prevent other handlers */
        gtk_widget_grab_focus (GTK_WIDGET(entrytext));
    gtk_entry_set_position(GTK_ENTRY(entrytext), -1);
}


/* Event handlers for map drawing area */
/* For a reason I don't know, this gets called a whole bunch.
 * every time a monster is killed, this gets called for a reason
 * I can't figure out.
 */

static gint
configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
    static sint16  ox=-1, oy=-1;

    /* Handle the surplus number of events that this causes to be generated.
     * basically, if the size of the window hasn't changed, we really don't
     * care - position of the window isn't important.
     * note that we could be more clever and free up the other data even on
     * requests that do change the size,
     * but this will fix the most horrendous memory leak
     */
    if (event->type == GDK_CONFIGURE) {
	if (((GdkEventConfigure*)event)->width == ox && ((GdkEventConfigure*)event)->height == oy)
	    return TRUE;
	else {
#if 0
	    fprintf(stderr, "ox=%d != %d, oy=%d != %d\n", ox, ((GdkEventConfigure*)event)->width,
		    oy, ((GdkEventConfigure*)event)->height);
#endif
	    ox = ((GdkEventConfigure*)event)->width;
	    oy = ((GdkEventConfigure*)event)->height;
	}
    }

#ifdef HAVE_SDL
    if(use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL) {
	/* When program first runs, mapsurface can be null.
	 * either way, we want to catch it here.
	 */
	if (mapsurface)
	    SDL_UpdateRect( mapsurface, 0, 0, 0, 0);
	return TRUE;
    }
#endif

    mapgc = gdk_gc_new (drawingarea->window);

    if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_PIXMAP) {
	int x,y,count;
	GdkGC	*darkgc;

	/* this is used when drawing with GdkPixmaps.  Create another surface,
         * as well as some light/dark images
         */
	dark = gdk_pixmap_new(drawingarea->window, map_image_size, map_image_size, -1);
	gdk_draw_rectangle(dark, drawingarea->style->black_gc, TRUE, 0, 0, map_image_size, map_image_size);
	dark1 = gdk_pixmap_new(drawingarea->window, map_image_size, map_image_size, 1);
	dark2 = gdk_pixmap_new(drawingarea->window, map_image_size, map_image_size, 1);
	dark3 = gdk_pixmap_new(drawingarea->window, map_image_size, map_image_size, 1);

	/* We need our own GC here because we are working with single bit depth images */
	darkgc = gdk_gc_new(dark1);
	gdk_gc_set_foreground(darkgc, &root_color[NDI_WHITE]);
	/* Clear any garbage values we get when we create the bitmaps */
	gdk_draw_rectangle(dark1, darkgc, TRUE, 0, 0, map_image_size, map_image_size);
	gdk_draw_rectangle(dark2, darkgc, TRUE, 0, 0, map_image_size, map_image_size);
	gdk_draw_rectangle(dark3, darkgc, TRUE, 0, 0, map_image_size, map_image_size);
	gdk_gc_set_foreground(darkgc, &root_color[NDI_BLACK]);
	count=0;
	for (x=0; x<map_image_size; x++) {
	    for (y=0; y<map_image_size; y++) {

		/* we just fill in points every X pixels - dark1 is the darkest, dark3 is the lightest.
		 * dark1 has 50% of the pixels filled in, dark2 has 33%, dark3 has 25%
		 * The formula's here are not perfect - dark2 will not match perfectly with an
		 * adjacent dark2 image.  dark3 results in diagonal stripes.  OTOH, these will
		 * change depending on the image size.
		 */
		if ((x+y) % 2) {
		    gdk_draw_point(dark1, darkgc, x, y);
		}
		if ((x+y) %3) {
		    gdk_draw_point(dark2, darkgc, x, y);
		}
		if ((x+y) % 4) {
		    gdk_draw_point(dark3, darkgc, x, y);
		}
		/* dark1 gets filled on 0x01, 0x11, 0x10, only leaving 0x00 empty */
	    }
	    /* if the row size is even, we put an extra value in count - in this
	     * way, the pixels will be even on one line, odd on the next, etc
	     * instead of vertical lines - at least for datk1 and dark3
	     */
	}
	mapwindow = gdk_pixmap_new(gtkwin_root->window, use_config[CONFIG_MAPWIDTH] * map_image_size, use_config[CONFIG_MAPHEIGHT] * map_image_size, -1);
	gdk_gc_unref(darkgc);
    }
    display_map_doneupdate(TRUE, FALSE);
    return TRUE;
}



/* Redraw the screen from the backing pixmap */
static gint
expose_event (GtkWidget *widget, GdkEventExpose *event)
{
#ifdef HAVE_SDL
    if(use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL && mapsurface) {
	SDL_UpdateRect( mapsurface, 0, 0, 0, 0);
	return FALSE;
    }
#endif
    display_map_doneupdate(FALSE, FALSE);
    return FALSE;
}

/*
 * Sets up player game view window, implemented as a gtk table. Cells are initialized
 * with the bg.xpm pixmap to avoid resizes and to initialize GC's and everything for the
 * actual drawing routines later.
 */

static int get_game_display(GtkWidget *frame) {
    GtkWidget *gtvbox, *gthbox;

    gtvbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (frame), gtvbox);
    gthbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (gtvbox), gthbox, FALSE, FALSE, 1);

    drawingarea = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawingarea), map_image_size*use_config[CONFIG_MAPWIDTH],map_image_size*use_config[CONFIG_MAPHEIGHT]);
    /* Add mouseclick events to the drawing area */

    gtk_widget_set_events (drawingarea, GDK_BUTTON_PRESS_MASK);

    /* Set up X redraw routine signalling */
    gtk_signal_connect (GTK_OBJECT (drawingarea), "expose_event",
		      (GtkSignalFunc) expose_event, NULL);
    gtk_signal_connect (GTK_OBJECT(drawingarea),"configure_event",
		      (GtkSignalFunc) configure_event, NULL);
    /* Set up handling of mouseclicks in map */

    gtk_signal_connect (GTK_OBJECT(drawingarea),
		      "button_press_event",
		      GTK_SIGNAL_FUNC(button_map_event),
		      NULL);

    /* Pack it up and show it */

    gtk_box_pack_start (GTK_BOX (gthbox), drawingarea, FALSE, FALSE, 1);

    gtk_widget_show(drawingarea);

    gtk_widget_show(gthbox);
    gtk_widget_show(gtvbox);


    gtk_signal_connect (GTK_OBJECT (frame), "expose_event",
		      (GtkSignalFunc) expose_event, NULL);
    gtk_signal_connect (GTK_OBJECT(frame),"configure_event",
		      (GtkSignalFunc) configure_event, NULL);

    gtk_widget_show (frame);
  return 0;
}



/******************************************************************************
 *
 * The functions dealing with the info window follow
 *
 *****************************************************************************/


static void enter_callback(GtkWidget *widget, GtkWidget *entry)
{
    const gchar *entry_text;

    /* Next reply will reset this as necessary */
    gtk_entry_set_visibility(GTK_ENTRY(entrytext), TRUE);

    entry_text = gtk_entry_get_text(GTK_ENTRY(entrytext));
	 /*         printf("Entry contents: %s\n", entry_text);*/

    if (cpl.input_state==Metaserver_Select) {
	strcpy(cpl.input_text, entry_text);
    } else if (cpl.input_state == Reply_One ||
	       cpl.input_state == Reply_Many) {
	cpl.input_state = Playing;
	strcpy(cpl.input_text, entry_text);
	if (cpl.input_state == Reply_One)
	    cpl.input_text[1] = 0;

        send_reply(cpl.input_text);

    } else {
	cpl.input_state = Playing;
	/* No reason to do anything for a null string */
	if (entry_text[0] != 0) {
	    strncpy(history[cur_history_position], entry_text, MAX_COMMAND_LEN);
	    history[cur_history_position][MAX_COMMAND_LEN-1] = 0;
	    cur_history_position++;
	    cur_history_position %= MAX_HISTORY;
	    scroll_history_position = cur_history_position;
	    extended_command(entry_text);
	}
    }
    gtk_entry_set_text(GTK_ENTRY(entrytext),"");
    gtk_widget_grab_focus (GTK_WIDGET(gtkwin_info_text));

    if( cpl.input_state == Metaserver_Select)
    {
      cpl.input_state= Playing;
      /* This is the gtk_main that is started up by get_metaserver
       * The client will start another one once it is connected
       * to a crossfire server
       */
      gtk_main_quit();
    }
}

/* Can use a wheeled mouse to scroll the info window */
static gboolean
info_text_button_press_event (GtkWidget *widget, GdkEventButton *event,
                              gpointer user_data)
{
  GtkAdjustment *vadj;
  gboolean shifted;
  gfloat v_value;

  vadj = GTK_TEXT (widget)->vadj;
  v_value = vadj->value;

  shifted = (event->state & GDK_SHIFT_MASK) != 0;

  switch (event->button)
  {
    case 4:
      if (shifted)
        v_value -= vadj->page_size;
      else
        v_value -= vadj->step_increment * 5;
      break;

    case 5:
      if (shifted)
        v_value += vadj->page_size;
      else
        v_value += vadj->step_increment * 5;
      break;

    default:
      return FALSE;
  }

  v_value = CLAMP (v_value, vadj->lower, vadj->upper - vadj->page_size);

  gtk_adjustment_set_value (vadj, v_value);

  return TRUE;
}

static int get_info_display(GtkWidget *frame) {
    GtkWidget *box1;
    GtkWidget *box2;
    GtkWidget *tablet;
    GtkWidget *vscrollbar;
    FILE *infile;

    box1 = gtk_vbox_new (FALSE, 0);
    if (use_config[CONFIG_SPLITINFO]) {
	info_vpane = gtk_vpaned_new();
	gtk_container_add (GTK_CONTAINER (frame), info_vpane);
	gtk_widget_show(info_vpane);
	gtk_paned_add2(GTK_PANED(info_vpane), box1);
    } else {
	gtk_container_add (GTK_CONTAINER (frame), box1);
    }
    gtk_widget_show (box1);

    box2 = gtk_vbox_new (FALSE, 3);
    gtk_container_border_width (GTK_CONTAINER (box2), 3);
    gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
    gtk_widget_show (box2);


    tablet = gtk_table_new (2, 2, FALSE);
    gtk_table_set_row_spacing (GTK_TABLE (tablet), 0, 2);
    gtk_table_set_col_spacing (GTK_TABLE (tablet), 0, 2);
    gtk_box_pack_start (GTK_BOX (box2), tablet, TRUE, TRUE, 0);
    gtk_widget_show (tablet);

    text_hadj = gtk_adjustment_new(1, 0, 1, 0.01, 0.1, 40);
    text_vadj = gtk_adjustment_new(1, 0, 1, 0.01, 0.1, 40);

    gtkwin_info_text = gtk_text_new (GTK_ADJUSTMENT(text_hadj),GTK_ADJUSTMENT(text_vadj));
    gtk_text_set_editable (GTK_TEXT (gtkwin_info_text), FALSE);
    gtk_table_attach (GTK_TABLE (tablet), gtkwin_info_text, 0, 1, 0, 1,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
    gtk_widget_show (gtkwin_info_text);


    vscrollbar = gtk_vscrollbar_new (GTK_TEXT (gtkwin_info_text)->vadj);
    gtk_table_attach (GTK_TABLE (tablet), vscrollbar, 1, 2, 0, 1,
		     GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
    gtk_widget_show (vscrollbar);

    gtk_signal_connect (GTK_OBJECT (gtkwin_info_text), "button_press_event",
                      GTK_SIGNAL_FUNC (info_text_button_press_event),
                      vscrollbar);

    gtk_text_freeze (GTK_TEXT (gtkwin_info_text));

    gtk_widget_realize (gtkwin_info_text);

    if (use_config[CONFIG_SPLITINFO]) {

	box1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (box1);
	gtk_paned_add1(GTK_PANED(info_vpane), box1);

	tablet = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacing (GTK_TABLE (tablet), 0, 2);
	gtk_table_set_col_spacing (GTK_TABLE (tablet), 0, 2);
	gtk_box_pack_start (GTK_BOX (box1), tablet, TRUE, TRUE, 0);
	gtk_widget_show (tablet);

	text_hadj2 = gtk_adjustment_new(1, 0, 1, 0.01, 0.1, 40);
	text_vadj2 = gtk_adjustment_new(1, 0, 1, 0.01, 0.1, 40);

	gtkwin_info_text2 = gtk_text_new (GTK_ADJUSTMENT(text_hadj2),GTK_ADJUSTMENT(text_vadj2));

	gtk_text_set_editable (GTK_TEXT (gtkwin_info_text2), FALSE);
	gtk_table_attach (GTK_TABLE (tablet), gtkwin_info_text2, 0, 1, 0, 1,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
	gtk_widget_show (gtkwin_info_text2);

	vscrollbar = gtk_vscrollbar_new (GTK_TEXT (gtkwin_info_text2)->vadj);
	gtk_table_attach (GTK_TABLE (tablet), vscrollbar, 1, 2, 0, 1,
		     GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
	gtk_widget_show (vscrollbar);
	gtk_signal_connect (GTK_OBJECT (gtkwin_info_text2), "button_press_event",
                      GTK_SIGNAL_FUNC (info_text_button_press_event),
                      vscrollbar);

	gtk_widget_realize (gtkwin_info_text2);
    }

  infile = fopen("Welcome", "r");

  if (infile)
    {
      char buffer[1024];
      int nchars;

      while (1)
	{
	  nchars = fread(buffer, 1, 1024, infile);
	  gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, NULL,
			   NULL, buffer, nchars);

	  if (nchars < 1024)
	    break;
	}

      fclose (infile);
    }

  gtk_text_thaw (GTK_TEXT (gtkwin_info_text));


  entrytext = gtk_entry_new ();
  gtk_signal_connect(GTK_OBJECT(entrytext), "activate",
		     GTK_SIGNAL_FUNC(enter_callback),
		     entrytext);
  gtk_box_pack_start (GTK_BOX (box2),entrytext, FALSE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS (entrytext, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (entrytext);
  gtk_widget_show (entrytext);

  return 0;
}

/* Various replies */

static void sendstr(char *sendstr)
{
  gtk_widget_destroy (dialog_window);
  send_reply(sendstr);
  cpl.input_state = Playing;
}


/* This is similar to draw_info below, but doesn't advance to a new
 * line.  Generally, queries use this function to draw the prompt for
 * the name, password, etc.
 */



static void dialog_callback(GtkWidget *dialog)
{
  const gchar *dialog_text;
  dialog_text = gtk_entry_get_text(GTK_ENTRY(dialogtext));
  send_reply(dialog_text);
  gtk_widget_destroy (dialog_window);
  cpl.input_state = Playing;
}
static GtkWidget *userText = NULL;
static GtkWidget *passwordText = NULL;
static GtkWidget *passwordText2 = NULL;
static GtkWidget *loginWindow = NULL;
static GtkWidget *motdText = NULL;
static GtkWidget *rulesText = NULL;
static GtkWidget *newsText = NULL;
static GtkWidget *loginTabs = NULL;
static GtkWidget *loginButtonOk = NULL;
static GtkWidget *loginButtonCancel = NULL;
static GtkWidget *loginMessage = NULL;

char password[64]="";
static void setUserPass(GtkButton *button, gpointer func_data) {
    gchar* user;
    gchar* pass;
    user=gtk_editable_get_chars (GTK_EDITABLE(userText),0,-1);
    pass=gtk_editable_get_chars (GTK_EDITABLE(passwordText),0,-1);
    strncpy(password,pass,sizeof(password));
    send_reply(user);
#ifdef MULTKEYS
  /* Now is a good time to load player's specific key bindings */
    if (csocket.servername != NULL)
        sprintf(cpl.name, "%s.%s", user, csocket.servername);
    else
        strcpy( cpl.name, user );
    init_keys( );
#endif
    cpl.input_state = Playing;
    g_free(user);
    g_free(pass);
    gtk_widget_hide(loginWindow);
}
static void confirmUserPass(GtkButton *button, gpointer func_data) {
    gchar* pass;
    pass=gtk_editable_get_chars (GTK_EDITABLE(passwordText2),0,-1);
    send_reply(pass);
    cpl.input_state = Playing;
    g_free(pass);
    gtk_widget_hide(loginWindow);
}
static void cancelConnection(GtkButton *button, gpointer func_data) {
    gtk_widget_hide(loginWindow);
    cpl.input_state = Metaserver_Select;
    disconnect(GTK_WIDGET(button));
}

static void disable_ok_if_empty(gpointer button, GtkEditable *entry) {
    gchar *passcontent,*txtcontent;
    txtcontent = gtk_editable_get_chars(GTK_EDITABLE(userText),0,-1);
    passcontent= gtk_editable_get_chars(GTK_EDITABLE(passwordText)  ,0,-1);
    if ( passcontent && txtcontent &&
         (strlen(txtcontent)>=1)&&
         (strlen(passcontent)>=1))
        gtk_widget_set_sensitive(GTK_WIDGET(loginButtonOk),TRUE);
    else
        gtk_widget_set_sensitive(GTK_WIDGET(loginButtonOk),FALSE);
    if (txtcontent && (strlen (txtcontent)>0))
        gtk_widget_show(passwordText);
    else
        gtk_widget_hide(passwordText);
    g_free(txtcontent);
    g_free(passcontent);
}
static void change_focus(GtkWidget *focusTo, GtkEditable *entry) {

    char *txtcontent = gtk_editable_get_chars(entry,0,-1);
/*    printf("switch focus\n"); */
    if (txtcontent && (strlen(txtcontent)>0))
            gtk_widget_grab_focus(focusTo);
}
static void activate_ok_if_not_empty(GtkWidget *button, GtkEditable *entry) {
    char *txtcontent = gtk_editable_get_chars(entry,0,-1);
    if (txtcontent && (strlen(txtcontent)>0))
        gtk_widget_activate(button);
}
static void fill_news(GtkWidget *o, news_entry *news) {
    media_state state;
    while(news){
        state = write_media(GTK_TEXT(o),"[b]");
        write_media_with_state(GTK_TEXT(o),news->title,state);
        write_media(GTK_TEXT(o),"\n\n");
        write_media(GTK_TEXT(o),news->content);
        write_media(GTK_TEXT(o),"\n\n");
        news=news->next;
    }
}
static gint dialog_delete_event_callback(GtkWidget *widget, GdkEvent *event, gpointer data)
    {
    loginWindow = NULL;
    return FALSE;
    }
static void buildLoginDialog(void) {
    if (loginWindow==NULL){
        /* build window */
        GtkWidget *vbox, *table, *label, *hbox, *vscroll;
        loginWindow= gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_policy (GTK_WINDOW (loginWindow), TRUE, TRUE,
                 FALSE);
        gtk_window_set_transient_for (GTK_WINDOW (loginWindow),
                 GTK_WINDOW (gtkwin_root));
        gtk_window_set_title (GTK_WINDOW (loginWindow), "Login");
        gtk_window_set_transient_for(GTK_WINDOW (loginWindow),
                    GTK_WINDOW (gtkwin_root));
        vbox=gtk_vbox_new(FALSE,4);

        /* build it's notebook */
        loginTabs = gtk_notebook_new();
        /* notebook -> news */
        hbox=gtk_hbox_new(FALSE,2);
        newsText = gtk_text_new(NULL,NULL);
        gtk_text_set_word_wrap(GTK_TEXT(newsText),TRUE);
        gtk_text_set_line_wrap(GTK_TEXT(newsText),TRUE);
        vscroll = gtk_vscrollbar_new (GTK_TEXT (newsText)->vadj);
        gtk_box_pack_start(GTK_BOX(hbox),newsText,TRUE,TRUE,0);
        gtk_box_pack_start(GTK_BOX(hbox),vscroll,FALSE,TRUE,0);
        label = gtk_label_new("News");
        gtk_notebook_append_page(GTK_NOTEBOOK(loginTabs),hbox,label);
        gtk_widget_show(vscroll);
        gtk_widget_show(newsText);
        gtk_widget_show(hbox);
        /* notebook -> rules */
        hbox=gtk_hbox_new(FALSE,2);
        rulesText = gtk_text_new(NULL,NULL);
        gtk_text_set_word_wrap(GTK_TEXT(rulesText),TRUE);
        gtk_text_set_line_wrap(GTK_TEXT(rulesText),TRUE);
        vscroll = gtk_vscrollbar_new (GTK_TEXT (rulesText)->vadj);
        gtk_box_pack_start(GTK_BOX(hbox),rulesText,TRUE,TRUE,0);
        gtk_box_pack_start(GTK_BOX(hbox),vscroll,FALSE,TRUE,0);
        label = gtk_label_new("Rules");
        gtk_notebook_append_page(GTK_NOTEBOOK(loginTabs),hbox,label);
        gtk_widget_show(vscroll);
        gtk_widget_show(rulesText);
        gtk_widget_show(hbox);

        /*notebook -> login*/
        hbox=gtk_hbox_new(FALSE,2);
        motdText = gtk_text_new(NULL,NULL);
        vscroll = gtk_vscrollbar_new (GTK_TEXT (motdText)->vadj);
        gtk_box_pack_start(GTK_BOX(hbox),motdText,TRUE,TRUE,0);
        gtk_box_pack_start(GTK_BOX(hbox),vscroll,FALSE,TRUE,0);
        gtk_widget_show(hbox);
        gtk_widget_show(motdText);
        gtk_widget_show(vscroll);
        gtk_box_pack_start(GTK_BOX(vbox),hbox,TRUE,TRUE,0);


        /* message information */
        loginMessage = gtk_label_new(NULL);
        gtk_box_pack_start(GTK_BOX(vbox),loginMessage,FALSE,FALSE,0);
        gtk_widget_show(loginMessage);

        /* user-pass table*/
        table=gtk_table_new(3,2,FALSE);
           /* TODO for strange reason justify do not work.
            * May someone fix this?*/
        label=gtk_label_new("User:");
        gtk_table_attach(GTK_TABLE(table),label,0,1,0,1,GTK_EXPAND|GTK_FILL,0,2,2);
        gtk_label_set_justify(GTK_LABEL(label),GTK_JUSTIFY_RIGHT);
        gtk_widget_show(label);
        label=gtk_label_new("Password:");
        gtk_table_attach(GTK_TABLE(table),label,0,1,1,2,GTK_EXPAND|GTK_FILL,0,2,2);
        gtk_label_set_justify(GTK_LABEL(label),GTK_JUSTIFY_RIGHT);
        gtk_widget_show(label);
        label=gtk_label_new("Re-type password:");
        gtk_table_attach(GTK_TABLE(table),label,0,1,2,3,GTK_EXPAND|GTK_FILL,0,2,2);
        gtk_label_set_justify(GTK_LABEL(label),GTK_JUSTIFY_RIGHT);
        gtk_widget_show(label);
        userText=gtk_entry_new();
        gtk_widget_show(userText);
        gtk_table_attach(GTK_TABLE(table),userText,1,2,0,1,GTK_EXPAND|GTK_FILL,0,2,2);
        passwordText= gtk_entry_new();
        gtk_entry_set_visibility(GTK_ENTRY(passwordText),FALSE);
        gtk_widget_show(passwordText);
        gtk_table_attach(GTK_TABLE(table),passwordText,1,2,1,2,GTK_EXPAND|GTK_FILL,0,2,2);
        passwordText2= gtk_entry_new();
        gtk_entry_set_visibility(GTK_ENTRY(passwordText2),FALSE);
        gtk_entry_set_editable(GTK_ENTRY(passwordText2),FALSE);
        gtk_table_attach(GTK_TABLE(table),passwordText2,1,2,2,3,GTK_EXPAND|GTK_FILL,0,2,2);
        gtk_widget_show(passwordText2);
        gtk_box_pack_start(GTK_BOX(vbox),table,FALSE,FALSE,0);


        hbox=gtk_hbox_new(FALSE,2);
        loginButtonOk = gtk_button_new_with_label("Ok");
        loginButtonCancel = gtk_button_new_with_label("Cancel");
        gtk_box_pack_start(GTK_BOX(hbox),loginButtonOk,TRUE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(hbox),loginButtonCancel,TRUE,FALSE,0);
        gtk_widget_show(hbox);
        gtk_widget_show(loginButtonOk);
        gtk_widget_show(loginButtonCancel);

        /*manage events on login widgets*/
        gtk_signal_connect_object (GTK_OBJECT (loginWindow),
                "delete_event",
                GTK_SIGNAL_FUNC (dialog_delete_event_callback),
                NULL);
        gtk_signal_connect_object (GTK_OBJECT (loginButtonCancel),
                "clicked",
                GTK_SIGNAL_FUNC (cancelConnection),
                NULL);
        gtk_signal_connect_object (GTK_OBJECT (userText),
                "changed",
                GTK_SIGNAL_FUNC (disable_ok_if_empty),
                GINT_TO_POINTER(loginButtonOk));
        gtk_signal_connect_object (GTK_OBJECT (passwordText),
                "changed",
                GTK_SIGNAL_FUNC (disable_ok_if_empty),
                GINT_TO_POINTER(loginButtonOk));
        gtk_signal_connect_object (GTK_OBJECT (userText),
                "activate",
                GTK_SIGNAL_FUNC (change_focus),
                GINT_TO_POINTER(passwordText));
        gtk_signal_connect_object (GTK_OBJECT (userText),
                "activate",
                GTK_SIGNAL_FUNC (change_focus),
                GINT_TO_POINTER(passwordText));
        gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
        gtk_signal_connect_object (GTK_OBJECT (passwordText),
                "activate",
                GTK_SIGNAL_FUNC (activate_ok_if_not_empty),
                GINT_TO_POINTER(loginButtonOk));
        gtk_signal_connect_object (GTK_OBJECT (passwordText2),
                "activate",
                GTK_SIGNAL_FUNC (activate_ok_if_not_empty),
                GINT_TO_POINTER(loginButtonOk));
        gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
        gtk_widget_show(table);
        gtk_widget_show(vbox);
        label=gtk_label_new("login");
        gtk_notebook_append_page(GTK_NOTEBOOK(loginTabs),vbox,label);
        gtk_container_add(GTK_CONTAINER(loginWindow),loginTabs);
        gtk_widget_show(loginTabs);
        gtk_window_set_default_size(GTK_WINDOW(loginWindow),500,400);
        gtk_window_set_position(GTK_WINDOW(loginWindow),GTK_WIN_POS_CENTER);
    }
    gtk_editable_delete_text(GTK_EDITABLE(motdText),0,-1);
    write_media(GTK_TEXT(motdText), getMOTD());
    gtk_editable_delete_text(GTK_EDITABLE(rulesText),0,-1);
    write_media(GTK_TEXT(rulesText),get_rules());
    gtk_editable_delete_text(GTK_EDITABLE(newsText),0,-1);
    fill_news(newsText,get_news());
    gtk_widget_show(loginWindow);
}
guint signalLoginDialogClicked = -1;
static void logUserIn(void) {
    buildLoginDialog();
    gtk_label_set_text(GTK_LABEL(loginMessage),"Type in user name and password");
    gtk_entry_set_editable(GTK_ENTRY(userText),TRUE);
    gtk_entry_set_editable(GTK_ENTRY(passwordText),TRUE);
    gtk_entry_set_editable(GTK_ENTRY(passwordText2),FALSE);
    gtk_widget_show(GTK_WIDGET(passwordText));
    gtk_widget_hide(GTK_WIDGET(passwordText2));
    if (signalLoginDialogClicked!=-1)
        gtk_signal_disconnect(GTK_OBJECT (loginButtonOk),
                signalLoginDialogClicked);
    signalLoginDialogClicked = gtk_signal_connect_object (GTK_OBJECT (loginButtonOk),
            "clicked",
            GTK_SIGNAL_FUNC (setUserPass),
            NULL);
    gtk_widget_set_sensitive(GTK_WIDGET(loginButtonOk),TRUE);
    gtk_entry_set_text(GTK_ENTRY(userText),"");
    gtk_entry_set_text(GTK_ENTRY(passwordText),"");
    gtk_entry_set_text(GTK_ENTRY(passwordText2),"");
    gtk_widget_grab_focus(userText);
}
static void sendPassword(void) {
    send_reply(password);
	cpl.input_state = Playing;
}
static void confirmPassword(void) {
    buildLoginDialog();
    gtk_label_set_text(GTK_LABEL(loginMessage),"Creating new user, please confirm password");
    gtk_entry_set_editable(GTK_ENTRY(userText),FALSE);
    gtk_entry_set_editable(GTK_ENTRY(passwordText),FALSE);
    gtk_entry_set_editable(GTK_ENTRY(passwordText2),TRUE);
    gtk_widget_hide(GTK_WIDGET(passwordText));
    gtk_widget_show(GTK_WIDGET(passwordText2));
    if (signalLoginDialogClicked!=-1)
        gtk_signal_disconnect(GTK_OBJECT (loginButtonOk),
                signalLoginDialogClicked);
    signalLoginDialogClicked = gtk_signal_connect_object (GTK_OBJECT (loginButtonOk),
            "clicked",
            GTK_SIGNAL_FUNC (confirmUserPass),
            NULL);
    gtk_widget_grab_focus(passwordText2);
}
/* Draw a prompt dialog window */
/* Ok, now this is trying to be smart and decide what sort of dialog is
 * wanted.
 */

void
draw_prompt (const char *str)
{
    GtkWidget *dbox;
    GtkWidget *hbox;
    GtkWidget *dialoglabel;
    GtkWidget *yesbutton, *nobutton;
    GtkWidget *strbutton, *dexbutton, *conbutton, *intbutton, *wisbutton,
	*powbutton, *chabutton;

    gint    found = FALSE;

    if (!use_config[CONFIG_POPUPS])
      {
	draw_info(str, NDI_BLACK);
      }
    else
      {
	  dialog_window = gtk_window_new (GTK_WINDOW_DIALOG);

	  gtk_window_set_policy (GTK_WINDOW (dialog_window), TRUE, TRUE,
				 FALSE);
	  gtk_window_set_title (GTK_WINDOW (dialog_window), "Dialog");
	  gtk_window_set_transient_for (GTK_WINDOW (dialog_window),
					GTK_WINDOW (gtkwin_root));

	  dbox = gtk_vbox_new (FALSE, 6);
	  gtk_container_add (GTK_CONTAINER (dialog_window), dbox);

	  /* Ok, here we start generating the contents */

	  /*  printf ("Last info draw: %s\n", last_str); */
	  while (!found)
	    {
		if (!strcmp (str, ":"))
		  {
		      if (!strcmp (last_str, "What is your name?"))
			{
                logUserIn();
                return;
			}

		      if (!strcmp (last_str, "What is your password?"))
			{
                sendPassword();
                return;
			}
		      if (!strcmp
			  (last_str, "Please type your password again."))
			{
                confirmPassword();
                return;
			}
		  }
		/* Ok, tricky ones. */
		if (!strcmp (last_str, "[1-7] [1-7] to swap stats.")
		    || !strncmp (last_str, "Str d", 5)
		    || !strncmp (last_str, "Dex d", 5)
		    || !strncmp (last_str, "Con d", 5)
		    || !strncmp (last_str, "Int d", 5)
		    || !strncmp (last_str, "Wis d", 5)
		    || !strncmp (last_str, "Pow d", 5)
		    || !strncmp (last_str, "Cha d", 5))
		  {

		      dialoglabel =
			  gtk_label_new ("Roll again or exchange ability.");
		      gtk_box_pack_start (GTK_BOX (dbox), dialoglabel, FALSE,
					  TRUE, 6);
		      gtk_widget_show (dialoglabel);

		      hbox = gtk_hbox_new (TRUE, 2);
		      strbutton = gtk_button_new_with_label ("Str");
		      gtk_box_pack_start (GTK_BOX (hbox), strbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (strbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("1"));


		      dexbutton = gtk_button_new_with_label ("Dex");
		      gtk_box_pack_start (GTK_BOX (hbox), dexbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (dexbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("2"));

		      conbutton = gtk_button_new_with_label ("Con");
		      gtk_box_pack_start (GTK_BOX (hbox), conbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (conbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("3"));

		      intbutton = gtk_button_new_with_label ("Int");
		      gtk_box_pack_start (GTK_BOX (hbox), intbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (intbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("4"));

		      wisbutton = gtk_button_new_with_label ("Wis");
		      gtk_box_pack_start (GTK_BOX (hbox), wisbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (wisbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("5"));

		      powbutton = gtk_button_new_with_label ("Pow");
		      gtk_box_pack_start (GTK_BOX (hbox), powbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (powbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("6"));

		      chabutton = gtk_button_new_with_label ("Cha");
		      gtk_box_pack_start (GTK_BOX (hbox), chabutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (chabutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("7"));

		      gtk_widget_show (strbutton);
		      gtk_widget_show (dexbutton);
		      gtk_widget_show (conbutton);
		      gtk_widget_show (intbutton);
		      gtk_widget_show (wisbutton);
		      gtk_widget_show (powbutton);
		      gtk_widget_show (chabutton);



		      gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, TRUE,
					  6);
		      gtk_widget_show (hbox);

		      hbox = gtk_hbox_new (FALSE, 6);

		      yesbutton = gtk_button_new_with_label ("Roll again");
		      gtk_box_pack_start (GTK_BOX (hbox), yesbutton, TRUE,
					  TRUE, 6);
		      gtk_signal_connect_object (GTK_OBJECT (yesbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("y"));

		      nobutton = gtk_button_new_with_label ("Keep this");
		      gtk_box_pack_start (GTK_BOX (hbox), nobutton, TRUE,
					  TRUE, 6);
		      gtk_signal_connect_object (GTK_OBJECT (nobutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("n"));

		      gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, TRUE,
					  6);

		      gtk_widget_show (yesbutton);
		      gtk_widget_show (nobutton);
		      gtk_widget_show (hbox);

		      found = TRUE;
		      continue;
		  }
		if (!strncmp (last_str, "Str -", 5) ||
		    !strncmp (last_str, "Dex -", 5)
		    || !strncmp (last_str, "Con -", 5)
		    || !strncmp (last_str, "Int -", 5)
		    || !strncmp (last_str, "Wis -", 5)
		    || !strncmp (last_str, "Pow -", 5)
		    || !strncmp (last_str, "Cha -", 5))
		  {


		      dialoglabel =
			  gtk_label_new ("Exchange with which ability?");
		      gtk_box_pack_start (GTK_BOX (dbox), dialoglabel, FALSE,
					  TRUE, 6);
		      gtk_widget_show (dialoglabel);

		      hbox = gtk_hbox_new (TRUE, 2);
		      strbutton = gtk_button_new_with_label ("Str");
		      gtk_box_pack_start (GTK_BOX (hbox), strbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (strbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("1"));


		      dexbutton = gtk_button_new_with_label ("Dex");
		      gtk_box_pack_start (GTK_BOX (hbox), dexbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (dexbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("2"));

		      conbutton = gtk_button_new_with_label ("Con");
		      gtk_box_pack_start (GTK_BOX (hbox), conbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (conbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("3"));

		      intbutton = gtk_button_new_with_label ("Int");
		      gtk_box_pack_start (GTK_BOX (hbox), intbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (intbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("4"));

		      wisbutton = gtk_button_new_with_label ("Wis");
		      gtk_box_pack_start (GTK_BOX (hbox), wisbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (wisbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("5"));

		      powbutton = gtk_button_new_with_label ("Pow");
		      gtk_box_pack_start (GTK_BOX (hbox), powbutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (powbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("6"));

		      chabutton = gtk_button_new_with_label ("Cha");
		      gtk_box_pack_start (GTK_BOX (hbox), chabutton, TRUE,
					  TRUE, 1);
		      gtk_signal_connect_object (GTK_OBJECT (chabutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("7"));

		      gtk_widget_show (strbutton);
		      gtk_widget_show (dexbutton);
		      gtk_widget_show (conbutton);
		      gtk_widget_show (intbutton);
		      gtk_widget_show (wisbutton);
		      gtk_widget_show (powbutton);
		      gtk_widget_show (chabutton);


		      gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, TRUE,
					  6);
		      gtk_widget_show (hbox);

		      found = TRUE;
		      continue;
		  }

		if (!strncmp (last_str, "Press `d'", 9))
		  {


		      dialoglabel = gtk_label_new ("Choose a character.");
		      gtk_box_pack_start (GTK_BOX (dbox), dialoglabel, FALSE,
					  TRUE, 6);
		      gtk_widget_show (dialoglabel);

		      hbox = gtk_hbox_new (FALSE, 6);

		      yesbutton = gtk_button_new_with_label ("Show next");
		      gtk_box_pack_start (GTK_BOX (hbox), yesbutton, TRUE,
					  TRUE, 6);
		      gtk_signal_connect_object (GTK_OBJECT (yesbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER (" "));

		      nobutton = gtk_button_new_with_label ("Keep this");
		      gtk_box_pack_start (GTK_BOX (hbox), nobutton, TRUE,
					  TRUE, 6);
		      gtk_signal_connect_object (GTK_OBJECT (nobutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("d"));

		      gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, TRUE,
					  6);

		      gtk_widget_show (yesbutton);
		      gtk_widget_show (nobutton);
		      gtk_widget_show (hbox);

		      found = TRUE;
		      continue;
		  }

		if (!strncmp (str, "Do you want to play", 18))
		  {


		      dialoglabel =
			  gtk_label_new ("Do you want to play again?");
		      gtk_box_pack_start (GTK_BOX (dbox), dialoglabel, FALSE,
					  TRUE, 6);
		      gtk_widget_show (dialoglabel);

		      hbox = gtk_hbox_new (FALSE, 6);

		      yesbutton = gtk_button_new_with_label ("Play again");
		      gtk_box_pack_start (GTK_BOX (hbox), yesbutton, TRUE,
					  TRUE, 6);
		      gtk_signal_connect_object (GTK_OBJECT (yesbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("a"));

		      nobutton = gtk_button_new_with_label ("Quit");
		      gtk_box_pack_start (GTK_BOX (hbox), nobutton, TRUE,
					  TRUE, 6);
		      gtk_signal_connect_object (GTK_OBJECT (nobutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("q"));

		      gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, TRUE,
					  6);

		      gtk_widget_show (yesbutton);
		      gtk_widget_show (nobutton);
		      gtk_widget_show (hbox);

		      found = TRUE;
		      continue;
		  }

		if (!strncmp (str, "Are you sure you want", 21))
		  {


		      dialoglabel =
			  gtk_label_new ("Are you sure you want to quit?");
		      gtk_box_pack_start (GTK_BOX (dbox), dialoglabel, FALSE,
					  TRUE, 6);
		      gtk_widget_show (dialoglabel);

		      hbox = gtk_hbox_new (FALSE, 6);

		      yesbutton = gtk_button_new_with_label ("Yes, quit");
		      gtk_box_pack_start (GTK_BOX (hbox), yesbutton, TRUE,
					  TRUE, 6);
		      gtk_signal_connect_object (GTK_OBJECT (yesbutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("y"));

		      nobutton = gtk_button_new_with_label ("Don't quit");
		      gtk_box_pack_start (GTK_BOX (hbox), nobutton, TRUE,
					  TRUE, 6);
		      gtk_signal_connect_object (GTK_OBJECT (nobutton),
						 "clicked",
						 GTK_SIGNAL_FUNC (sendstr),
						 GINT_TO_POINTER ("n"));

		      gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, TRUE,
					  6);

		      gtk_widget_show (yesbutton);
		      gtk_widget_show (nobutton);
		      gtk_widget_show (hbox);

		      found = TRUE;
		      continue;
		  }

        if (!strcmp (last_str, "What is the password?"))
			{

			    dialoglabel =
				gtk_label_new ("What is the party password?");
			    gtk_box_pack_start (GTK_BOX (dbox), dialoglabel,
						FALSE, TRUE, 6);
			    gtk_widget_show (dialoglabel);

			    hbox = gtk_hbox_new (FALSE, 6);
			    dialogtext = gtk_entry_new ();
			    gtk_entry_set_visibility (GTK_ENTRY (dialogtext),
						      FALSE);
			    gtk_signal_connect (GTK_OBJECT (dialogtext),
						"activate",
						GTK_SIGNAL_FUNC
						(dialog_callback),
						dialog_window);
			    gtk_box_pack_start (GTK_BOX (hbox), dialogtext,
						TRUE, TRUE, 6);
			    gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE,
						TRUE, 6);

			    gtk_widget_show (hbox);

			    gtk_widget_show (dialogtext);
			    gtk_widget_grab_focus (dialogtext);
			    found = TRUE;
			    continue;;
			}

		if (!found)
		  {
		      dialoglabel = gtk_label_new (str);
		      gtk_box_pack_start (GTK_BOX (dbox), dialoglabel, FALSE,
					  TRUE, 6);
		      gtk_widget_show (dialoglabel);

		      hbox = gtk_hbox_new (FALSE, 6);
		      dialogtext = gtk_entry_new ();
		      if (cpl.no_echo == 1)
		          gtk_entry_set_visibility(GTK_ENTRY (dialogtext), FALSE);

		      gtk_signal_connect (GTK_OBJECT (dialogtext), "activate",
					  GTK_SIGNAL_FUNC (dialog_callback),
					  dialog_window);
		      gtk_box_pack_start (GTK_BOX (hbox), dialogtext, TRUE,
					  TRUE, 6);
		      gtk_box_pack_start (GTK_BOX (dbox), hbox, FALSE, TRUE,
					  6);

		      gtk_widget_show (hbox);
		      gtk_widget_show (dialogtext);
		      gtk_widget_grab_focus (dialogtext);
		      found = TRUE;
		      continue;
		  }
	    }

	  /* Finished with the contents. */


	  gtk_widget_show (dbox);
	  gtk_widget_show (dialog_window);
      }

}

/* draw_info adds a line to the info window. For speed reasons it will
 * automatically freeze the info window when adding text to it, set the
 * draw_info_freeze variable true and the actual drawing will take place
 * during the next do_timeout at which point it is unfrozen again. That way
 * we handle massive amounts of text addition with a single gui event, which
 * results in a serious speed improvement for slow client machines (and
 * above all it avoids a gui lockup when the client becomes congested with
 * updates (which is often when you're in the middle of fighting something
 * serious and not a good time to get slow reaction time)).
 *
 * MSW 2001-05-25: The removal of input from the text windows should
 * work, and in fact does about 90% of the time.  But that 10% it
 * doesn't, the client crashes.  The error itself is in the gtk library,
 * to hopefully they will fix it someday.  The reason to do this is
 * to keep these buffers a reasonable size so that performance stays
 * good - otherewise, performance slowly degrades.
 */

void draw_info(const char *str, int color) {
    int ncolor = color;
    char timestamp[11]="";

    if (ncolor==NDI_WHITE) {
	ncolor=NDI_BLACK;
    }
    if (use_config[CONFIG_TIMESTAMP] && color != NDI_BLACK) {
	struct tm *now;
	time_t currenttime = time(0);
	now = localtime(&currenttime);
	strftime(timestamp, 10, "%I:%M: ", now);
    }
    strcpy (last_str, str);
    if (use_config[CONFIG_SPLITINFO] && color != NDI_BLACK) {
	if (!draw_info_freeze2){
	    gtk_text_freeze (GTK_TEXT (gtkwin_info_text2));
	    draw_info_freeze2=TRUE;
	}
	if (use_config[CONFIG_TRIMINFO]) {
	    info2_num_chars += strlen(str) + 1;
	    /* Limit size of scrollback buffer. To be more efficient, delete a good
	     * blob (5000) characters at a time - in that way, there will be some
	     * time between needing to delete.
	     */
	    if (info2_num_chars > info2_max_chars ) {
		gtk_text_set_point(GTK_TEXT(gtkwin_info_text2),0);
		gtk_text_forward_delete(GTK_TEXT(gtkwin_info_text2), (info2_num_chars - info2_max_chars) + 5000);
		info2_num_chars = gtk_text_get_length(GTK_TEXT(gtkwin_info_text2));
		gtk_text_set_point(GTK_TEXT(gtkwin_info_text2), info2_num_chars);
		LOG(LOG_INFO,"gtk::draw_info","reduced output buffer2 to %d chars", info1_num_chars);
	    }
	}
	if (use_config[CONFIG_TIMESTAMP])
	    gtk_text_insert(GTK_TEXT (gtkwin_info_text2), NULL, &root_color[NDI_GREY], NULL, timestamp, -1);
	gtk_text_insert (GTK_TEXT (gtkwin_info_text2), NULL, &root_color[ncolor], NULL, str, -1);
	gtk_text_insert (GTK_TEXT (gtkwin_info_text2), NULL, &root_color[ncolor], NULL, "\n" , -1);

    } else {
	/* all nootes in the above section apply here also */
	if (!draw_info_freeze1){
	    gtk_text_freeze (GTK_TEXT (gtkwin_info_text));
	    draw_info_freeze1=TRUE;
	}
	if (use_config[CONFIG_TRIMINFO]) {
	    info1_num_chars += strlen(str) + 1;
	    if (info1_num_chars > info1_max_chars ) {
#if 1
		int to_delete = (info1_num_chars - info1_max_chars) + 5000;
		/* Delete on newline boundaries */
		while (GTK_TEXT_INDEX(GTK_TEXT(gtkwin_info_text), to_delete)!='\n')
		    to_delete++;
		gtk_text_set_point(GTK_TEXT(gtkwin_info_text),0);
		gtk_text_forward_delete(GTK_TEXT(gtkwin_info_text), to_delete);
		info1_num_chars = gtk_text_get_length(GTK_TEXT(gtkwin_info_text));
		gtk_text_set_point(GTK_TEXT(gtkwin_info_text), info1_num_chars);
		LOG(LOG_INFO,"gtk::draw_info",
                "trim_info_window, deleted %d characters, %d remaining",
                to_delete, info1_num_chars);
#else
		/* This works, so it is possible to completely clear the window */
		info1_num_chars = gtk_text_get_length(GTK_TEXT (gtkwin_info_text));
		gtk_text_set_point(GTK_TEXT (gtkwin_info_text), 0);
		gtk_text_forward_delete (GTK_TEXT (gtkwin_info_text), info1_num_chars );
		gtk_text_thaw (GTK_TEXT (gtkwin_info_text));
		info1_num_chars=0;
#endif
	    }

	}
	if (use_config[CONFIG_TIMESTAMP])
	    gtk_text_insert (GTK_TEXT (gtkwin_info_text2), NULL, &root_color[NDI_GREY], NULL, timestamp, -1);
	gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[ncolor], NULL, str , -1);
	gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[ncolor], NULL, "\n" , -1);
    }
}


void draw_color_info(int colr, const char *buf){
	draw_info(buf,colr);
}

/***********************************************************************
 *
 * Stats window functions follow
 *
 ***********************************************************************/

static int get_stats_display(GtkWidget *frame) {
    GtkWidget *stats_vbox;
    GtkWidget *stats_box_1;
    GtkWidget *stats_box_2;
    GtkWidget *stats_box_4;
    GtkWidget *stats_box_5;
    GtkWidget *stats_box_6;
    GtkWidget *stats_box_7;
    GtkWidget *table;
    int i,x,y;


    stats_vbox = gtk_vbox_new (FALSE, 0);

    /* 1st row  - Player name */
    stats_box_1 = gtk_hbox_new (FALSE, 0);

    statwindow.playername = gtk_label_new("Player: ");
    gtk_box_pack_start (GTK_BOX (stats_box_1), statwindow.playername, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.playername);

    gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_1, FALSE, FALSE, 0);
    gtk_widget_show (stats_box_1);

    /* 2nd row - score and level */
    stats_box_2 = gtk_hbox_new (FALSE, 0);
    statwindow.score = gtk_label_new("Score: 0");
    gtk_box_pack_start (GTK_BOX (stats_box_2), statwindow.score, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.score);

    statwindow.level = gtk_label_new("Level: 0");
    gtk_box_pack_start (GTK_BOX (stats_box_2), statwindow.level, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.level);

    gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_2, FALSE, FALSE, 0);
    gtk_widget_show (stats_box_2);


    /* 4th row (really the thrid) - the stats - str, dex, con, etc */
    stats_box_4 = gtk_hbox_new (FALSE, 0);

    statwindow.Str = gtk_label_new("S 0");
    gtk_box_pack_start (GTK_BOX (stats_box_4), statwindow.Str, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.Str);

    statwindow.Dex = gtk_label_new("D 0");
    gtk_box_pack_start (GTK_BOX (stats_box_4), statwindow.Dex, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.Dex);

    statwindow.Con = gtk_label_new("Co 0");
    gtk_box_pack_start (GTK_BOX (stats_box_4), statwindow.Con, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.Con);

    statwindow.Int = gtk_label_new("I 0");
    gtk_box_pack_start (GTK_BOX (stats_box_4), statwindow.Int, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.Int);

    statwindow.Wis = gtk_label_new("W 0");
    gtk_box_pack_start (GTK_BOX (stats_box_4), statwindow.Wis, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.Wis);

    statwindow.Pow = gtk_label_new("P 0");
    gtk_box_pack_start (GTK_BOX (stats_box_4), statwindow.Pow, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.Pow);

    statwindow.Cha = gtk_label_new("Ch 0");
    gtk_box_pack_start (GTK_BOX (stats_box_4), statwindow.Cha, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.Cha);

    gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_4, FALSE, FALSE, 0);
    gtk_widget_show (stats_box_4);

    /* 5th row wc, dam, ac, armor*/

    stats_box_5 = gtk_hbox_new (FALSE, 0);

    statwindow.wc = gtk_label_new("Wc: 0");
    gtk_box_pack_start (GTK_BOX (stats_box_5), statwindow.wc, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.wc);

    statwindow.dam = gtk_label_new("Dam: 0");
    gtk_box_pack_start (GTK_BOX (stats_box_5), statwindow.dam, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.dam);

    statwindow.ac = gtk_label_new("Ac: 0");
    gtk_box_pack_start (GTK_BOX (stats_box_5), statwindow.ac, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.ac);

    statwindow.armor = gtk_label_new("Armor: 0");
    gtk_box_pack_start (GTK_BOX (stats_box_5), statwindow.armor, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.armor);

    gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_5, FALSE, FALSE, 0);
    gtk_widget_show (stats_box_5);

    /* 6th row speed and weapon speed */

    stats_box_6 = gtk_hbox_new (FALSE, 0);

    statwindow.speed = gtk_label_new("Speed: 0");
    gtk_box_pack_start (GTK_BOX (stats_box_6), statwindow.speed, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.speed);

    gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_6, FALSE, FALSE, 0);
    gtk_widget_show (stats_box_6);

    /* 7th row - range */

    stats_box_7 = gtk_hbox_new (FALSE, 0);

    statwindow.skill = gtk_label_new("Skill: 0");
    gtk_box_pack_start (GTK_BOX (stats_box_7), statwindow.skill, FALSE, FALSE, 5);
    gtk_widget_show (statwindow.skill);
    gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_7, FALSE, FALSE, 0);
    gtk_widget_show (stats_box_7);


    /* Start of experience display - we do it in a 4 x 4 array.  Use a table
     * so that spacing is uniform - this should look better.
     * Change it so we use one field for the name, and the other for exp -
     * in this way, the values line up better.
     */

    table = gtk_table_new (4, 4, FALSE);
    x=0;
    y=0;
    /* this is all the same - we just pack it in different places */
    for (i=0; i<MAX_SKILL*2; i++) {
	statwindow.skill_exp[i] = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(table), statwindow.skill_exp[i], x, x+1, y, y+1, GTK_FILL  | GTK_EXPAND, 0, 10, 0);
	x++;
	if (x==4) { x=0; y++; }
	gtk_widget_show(statwindow.skill_exp[i]);
    }
    skill_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (res_scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (skill_scrolled_window),GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(stats_vbox), skill_scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW (skill_scrolled_window), table);

    gtk_widget_show(table);

    gtk_container_add (GTK_CONTAINER (frame), stats_vbox);
    gtk_widget_show (stats_vbox);
    gtk_widget_show (skill_scrolled_window);

    return 0;
}

/* This draws the stats window.  If redraw is true, it means
 * we need to redraw the entire thing, and not just do an
 * updated.
 */

void draw_stats(int redraw) {
  static Stats last_stats;
  static char last_name[MAX_BUF]="", last_range[MAX_BUF]="";
  static int init_before=0, lastbeep=0, max_drawn_skill=0;

  float weap_sp;
  char buff[MAX_BUF];
  int i, on_skill;

  if (!init_before) {
    init_before=1;
    memset(&last_stats, 0, sizeof(Stats));
  }

  if (updatelock < 25) {
    updatelock++;
    if (strcmp(cpl.title, last_name) || redraw) {
      strcpy(last_name,cpl.title);
      strcpy(buff,cpl.title);
      gtk_label_set (GTK_LABEL(statwindow.playername), cpl.title);
      gtk_widget_draw (statwindow.playername, NULL);
    }

    if(redraw || cpl.stats.exp!=last_stats.exp) {
      last_stats.exp = cpl.stats.exp;
      sprintf(buff,"Score: %5" FMT64 ,cpl.stats.exp);

      gtk_label_set (GTK_LABEL(statwindow.score), buff);
      gtk_widget_draw (statwindow.score, NULL);
    }

    if(redraw || cpl.stats.level!=last_stats.level) {
      last_stats.level = cpl.stats.level;
      sprintf(buff,"Level: %d",cpl.stats.level);
      gtk_label_set (GTK_LABEL(statwindow.level), buff);
      gtk_widget_draw (statwindow.level, NULL);
    }

    if(redraw ||
       cpl.stats.hp!=last_stats.hp || cpl.stats.maxhp!=last_stats.maxhp) {
      last_stats.hp=cpl.stats.hp;
      last_stats.maxhp=cpl.stats.maxhp;
      sprintf(buff,"Hp: %d/%d",cpl.stats.hp, cpl.stats.maxhp);
      gtk_label_set (GTK_LABEL(statwindow.hp), buff);
      gtk_widget_draw (statwindow.hp, NULL);
    }

    if(redraw ||
       cpl.stats.sp!=last_stats.sp || cpl.stats.maxsp!=last_stats.maxsp) {
      last_stats.sp=cpl.stats.sp;
      last_stats.maxsp=cpl.stats.maxsp;
      sprintf(buff,"Sp: %d/%d",cpl.stats.sp, cpl.stats.maxsp);
      gtk_label_set (GTK_LABEL(statwindow.sp), buff);
      gtk_widget_draw (statwindow.sp, NULL);
    }

    if(redraw ||
       cpl.stats.grace!=last_stats.grace || cpl.stats.maxgrace!=last_stats.maxgrace) {
      last_stats.grace=cpl.stats.grace;
      last_stats.maxgrace=cpl.stats.maxgrace;
      sprintf(buff,"Gr: %d/%d",cpl.stats.grace, cpl.stats.maxgrace);
      gtk_label_set (GTK_LABEL(statwindow.gr), buff);
      gtk_widget_draw (statwindow.gr, NULL);
    }

    if(redraw || cpl.stats.Str!=last_stats.Str) {
      last_stats.Str=cpl.stats.Str;
      sprintf(buff,"S%2d",cpl.stats.Str);
      gtk_label_set (GTK_LABEL(statwindow.Str), buff);
      gtk_widget_draw (statwindow.Str, NULL);
    }

    if(redraw || cpl.stats.Dex!=last_stats.Dex) {
      last_stats.Dex=cpl.stats.Dex;
      sprintf(buff,"D%2d",cpl.stats.Dex);
      gtk_label_set (GTK_LABEL(statwindow.Dex), buff);
      gtk_widget_draw (statwindow.Dex, NULL);
    }

    if(redraw || cpl.stats.Con!=last_stats.Con) {
      last_stats.Con=cpl.stats.Con;
      sprintf(buff,"Co%2d",cpl.stats.Con);
      gtk_label_set (GTK_LABEL(statwindow.Con), buff);
      gtk_widget_draw (statwindow.Con, NULL);
    }

    if(redraw || cpl.stats.Int!=last_stats.Int) {
      last_stats.Int=cpl.stats.Int;
      sprintf(buff,"I%2d",cpl.stats.Int);
      gtk_label_set (GTK_LABEL(statwindow.Int), buff);
      gtk_widget_draw (statwindow.Int, NULL);
    }

    if(redraw || cpl.stats.Wis!=last_stats.Wis) {
      last_stats.Wis=cpl.stats.Wis;
      sprintf(buff,"W%2d",cpl.stats.Wis);
      gtk_label_set (GTK_LABEL(statwindow.Wis), buff);
      gtk_widget_draw (statwindow.Wis, NULL);
    }

    if(redraw || cpl.stats.Pow!=last_stats.Pow) {
      last_stats.Pow=cpl.stats.Pow;
      sprintf(buff,"P%2d",cpl.stats.Pow);
      gtk_label_set (GTK_LABEL(statwindow.Pow), buff);
      gtk_widget_draw (statwindow.Pow, NULL);
    }

    if(redraw || cpl.stats.Cha!=last_stats.Cha) {
      last_stats.Cha=cpl.stats.Cha;
      sprintf(buff,"Ch%2d",cpl.stats.Cha);
      gtk_label_set (GTK_LABEL(statwindow.Cha), buff);
      gtk_widget_draw (statwindow.Cha, NULL);
    }

    if(redraw || cpl.stats.wc!=last_stats.wc) {
      last_stats.wc=cpl.stats.wc;
      sprintf(buff,"Wc%3d",cpl.stats.wc);
      gtk_label_set (GTK_LABEL(statwindow.wc), buff);
      gtk_widget_draw (statwindow.wc, NULL);
    }

    if(redraw || cpl.stats.dam!=last_stats.dam) {
      last_stats.dam=cpl.stats.dam;
      sprintf(buff,"Dam%3d",cpl.stats.dam);
      gtk_label_set (GTK_LABEL(statwindow.dam), buff);
      gtk_widget_draw (statwindow.dam, NULL);
    }

    if(redraw || cpl.stats.ac!=last_stats.ac) {
      last_stats.ac=cpl.stats.ac;
      sprintf(buff,"Ac%3d",cpl.stats.ac);
      gtk_label_set (GTK_LABEL(statwindow.ac), buff);
      gtk_widget_draw (statwindow.ac, NULL);
    }

    if(redraw || cpl.stats.resists[0]!=last_stats.resists[0]) {
      last_stats.resists[0]=cpl.stats.resists[0];
      sprintf(buff,"Arm%3d",cpl.stats.resists[0]);
      gtk_label_set (GTK_LABEL(statwindow.armor), buff);
      gtk_widget_draw (statwindow.armor, NULL);
    }

    if(redraw || cpl.stats.speed!=last_stats.speed ||
       cpl.stats.weapon_sp != last_stats.weapon_sp) {
      last_stats.speed=cpl.stats.speed;
      last_stats.weapon_sp=cpl.stats.weapon_sp;
      weap_sp = (float) cpl.stats.speed/((float)cpl.stats.weapon_sp);
      sprintf(buff,"Speed: %3.2f (%1.2f)",(float)cpl.stats.speed/FLOAT_MULTF,weap_sp);
      gtk_label_set (GTK_LABEL(statwindow.speed), buff);
      gtk_widget_draw (statwindow.speed, NULL);
    }

    if(redraw || cpl.stats.food!=last_stats.food) {
      last_stats.food=cpl.stats.food;
      sprintf(buff,"Food: %3d",cpl.stats.food);
      gtk_label_set (GTK_LABEL(statwindow.food), buff);
      gtk_widget_draw (statwindow.food, NULL);
      if (use_config[CONFIG_FOODBEEP] && (cpl.stats.food%4==3) && (cpl.stats.food < 200))
#ifndef WIN32
        XBell(GDK_DISPLAY(), 0);
#else
	    gdk_beep( );
#endif
    } else if (use_config[CONFIG_FOODBEEP] && cpl.stats.food == 0 && ++lastbeep == 5) {
	lastbeep = 0;
#ifndef WIN32
    XBell(GDK_DISPLAY(), 0);
#else
    gdk_beep( );
#endif
    }

    if(redraw || strcmp(cpl.range, last_range)) {
      strcpy(last_range, cpl.range);
      gtk_label_set (GTK_LABEL(statwindow.skill), cpl.range);
      gtk_widget_draw (statwindow.skill, NULL);
    }
    on_skill=0;
    for (i=0; i<MAX_SKILL; i++) {
	/* Drawing a particular skill entry is tricky - only draw if
	 * different, and only draw if we have a name for the skill
	 * and the player has some exp in the skill - don't draw
	 * all 30 skills for no reason.
	 */
	if ((redraw || cpl.stats.skill_exp[i] != last_stats.skill_exp[i]) &&
	    skill_names[i] && cpl.stats.skill_level[i]) {
	    gtk_label_set(GTK_LABEL(statwindow.skill_exp[on_skill++]), skill_names[i]);
	    sprintf(buff,"%" FMT64 " (%d)", cpl.stats.skill_exp[i], cpl.stats.skill_level[i]);
	    gtk_label_set(GTK_LABEL(statwindow.skill_exp[on_skill++]), buff);
	    last_stats.skill_level[i] = cpl.stats.skill_level[i];
	    last_stats.skill_exp[i] = cpl.stats.skill_exp[i];
	} else if (cpl.stats.skill_level[i]) {
	    /* don't need to draw the skill, but need to update the position
	     * of where to draw the next one.
	     */
	    on_skill+=2;
	}
    }
    /* Since the number of skills we draw come and go, basically we want
     * to erase any extra.  This shows up when switching characters, eg, character
     * #1 knows 10 skills, #2 knows 5 - need to erase those 5 extra.
     */
    if (on_skill < max_drawn_skill) {
	int k;

	for (k = on_skill; k <= max_drawn_skill; k++)
	    gtk_label_set(GTK_LABEL(statwindow.skill_exp[k]), "");
    }
    max_drawn_skill = on_skill;
  } /* updatelock < 25 */
}


/***********************************************************************
*
* Handles the message window
*
***********************************************************************/


static void create_stat_bar(GtkWidget *mtable, gint row, const gchar *label, gint bar, GtkWidget **plabel) {
  /*  GtkWidget *plabel;*/

  *plabel = gtk_label_new (label);
  gtk_table_attach (GTK_TABLE (mtable), *plabel, 0, 1, row, row+1,/*GTK_FILL |*/ GTK_EXPAND,GTK_FILL | GTK_EXPAND,0,0);
  gtk_widget_show (*plabel);

  vitals[bar].bar = gtk_progress_bar_new ();
  gtk_table_attach(GTK_TABLE(mtable), vitals[bar].bar, 0,1,row+1,row+2,GTK_FILL | GTK_EXPAND, 0 ,3,0);
  gtk_widget_set_usize (vitals[bar].bar,100,15);


  gtk_widget_show (vitals[bar].bar);



  vitals[bar].state=1;

  vitals[bar].style[0] = gtk_style_new ();
  vitals[bar].style[0]->bg[GTK_STATE_PRELIGHT] = gdk_green;
  gtk_widget_set_style (vitals[bar].bar, vitals[bar].style[0]);
  vitals[bar].style[1] = gtk_style_new ();
  vitals[bar].style[1]->bg[GTK_STATE_PRELIGHT] = gdk_red;

}

/* This is used when going from gradiated color stat bars back
 * to the normal - we need to reset the colors.
 */
void reset_stat_bars(void) {
    int i;

    for (i=0; i<4; i++) {
	vitals[i].style[0]->bg[GTK_STATE_PRELIGHT] = gdk_green;
	vitals[i].style[1]->bg[GTK_STATE_PRELIGHT] = gdk_red;
	/* need to do this double switch so that the color gets updated. Otherwise,
	 * if we are currently using style[0] to draw, the update above won't
	 * have any effect.
	 */
	gtk_widget_set_style(vitals[i].bar, vitals[i].style[1]);
	gtk_widget_set_style(vitals[i].bar, vitals[i].style[0]);
	vitals[i].state = 0;

    }
    draw_message_window(1);
}

static int get_message_display(GtkWidget *frame) {
    GtkWidget *mtable;
    GtkWidget *vbox;
    GtkWidget *res_mainbox;
    GtkWidget *reswindow;

    /* initialize the main hbox */
    res_mainbox = gtk_hbox_new (TRUE,0);
    gtk_container_add (GTK_CONTAINER(frame), res_mainbox);

    /* stat bar part - start */

    /* initialize the vbox for the stat bars (Hp,Mana,Grace,Food)
     * and pack it into the main hbox
     */
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(res_mainbox), vbox, FALSE, TRUE, 0);

    /* initialize the table and pack this into the vbox */
    mtable = gtk_table_new (2,4,FALSE);
    gtk_box_pack_start (GTK_BOX(vbox),mtable,FALSE,FALSE,0);

    /* create the stat bars and place them in the table */
    create_stat_bar (mtable, 1,"Hp: 0",0, &statwindow.hp);
    create_stat_bar (mtable, 3,"Mana: 0",1, &statwindow.sp);
    create_stat_bar (mtable, 5,"Grace: 0",2, &statwindow.gr);
    create_stat_bar (mtable, 7,"Food: 0",3, &statwindow.food);

    /* stat bar part - end */


    /* resistances table part - start */

    /* initialize the hbox for the resistances table */
    reswindow = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start(GTK_BOX(res_mainbox), reswindow, FALSE, TRUE, 0);

    /* create the resistance table*/
    restable = gtk_table_new (4,12,FALSE);

    /* packing the restable in a scrollable window*/
    res_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (res_scrolled_window), 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (res_scrolled_window),
	GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(reswindow), res_scrolled_window, TRUE, TRUE, 0);
    gtk_widget_show (res_scrolled_window);
    gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW (res_scrolled_window), restable);

    /* finally, draw the resistances table */
    resize_resistance_table(use_config[CONFIG_RESISTS]);

    /* resistances table part - end */

    /* now showing all not already showed widgets */
    gtk_widget_show (res_mainbox);
    gtk_widget_show (reswindow);
    gtk_widget_show (restable);
    gtk_widget_show (mtable);
    gtk_widget_show (vbox);
    return 0;
}

/* This handles layout of the resistance window.
 * We end up just removing (and thus freeing) all the data and
 * then create new entries.  This keeps things simpler, because
 * in basic mode, not all the resist[] widgets are attached,
 */
void resize_resistance_table(int resists_show) {
    int i, left=0, right=0;

    while (GTK_TABLE(restable)->children) {
	GtkTableChild *child;
	child = GTK_TABLE(restable)->children->data;

	gtk_container_remove(GTK_CONTAINER(restable),
			     child->widget);
    }

    /* initialize labels for all modes of CONFIG_RESISTS */
    fire_label = gtk_label_new ("    ");
    run_label = gtk_label_new ("   ");

    /* place labels for dual-column mode of CONFIG_RESISTS */
    if (resists_show) {
	gtk_table_resize(GTK_TABLE(restable), 4,12);
	gtk_table_attach (GTK_TABLE(restable), fire_label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE(restable), run_label, 3, 4, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
    } 
    else { /* single column mode */
	gtk_table_resize(GTK_TABLE(restable), 2,24);
	gtk_table_attach (GTK_TABLE(restable), fire_label, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE(restable), run_label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
    }
    /* show labels for all modes of CONFIG_RESISTS */
    gtk_widget_show (fire_label);
    gtk_widget_show (run_label);
    /* make and place labels for showing the resistances - start */

    for (i=0; i< NUM_RESISTS; i++) {
	resists[i] = gtk_label_new("          ");

	/* place the labels for dual columns in the table restable */
	if (resists_show) {
	    if ((i/2)*2 != i) {
		left++;
		gtk_table_attach (GTK_TABLE(restable), resists[i], 1, 2, 3+left, 4+left, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    } else {
		right++;
		gtk_table_attach (GTK_TABLE(restable), resists[i], 3, 4, 3+right, 4+right, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    }
	    gtk_widget_show (resists[i]);
	}
	else { /* single column style */
	    gtk_table_attach (GTK_TABLE(restable), resists[i], 0, 2, 3+i, 4+i, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    gtk_widget_show (resists[i]);
	}
    }
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (res_scrolled_window),GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
}

static void draw_stat_bar(int bar_pos, float bar, int is_alert)
{
    if (use_config[CONFIG_GRAD_COLOR]) {
	/* In this mode, the color of the stat bar were go between red and green
	 * in a gradual style.  This, at 50% of the value, the stat bar will be
	 * drawn in yellow.  Pure fluff I know.
	 */
	int nstyle;
	GdkColor ncolor;
	/* We need to figure out what style to use.  We can't call gtk_widget_set_style
         * on the widget currently in use - doing so results in no effect.
	 * 53247 is float value of 0xcfff, which is the value used in the gdk_red
	 * and gdk_green values.  We double the values, so that it scales properly -
	 * at .5, it then matches 53247, so the scaling appears proper.
         */
	if (gtk_widget_get_style(vitals[bar_pos].bar) == vitals[bar_pos].style[0]) nstyle=1;
	else nstyle=0;
	/* We are 'supercharged' - scale to max of 2.0 for pure blue */
	if (bar > 1.0) {
	    if (bar>2.0) bar=2.0;   /* Doesn't affect display, just are calculations */
	    ncolor.blue = 65535.0 * (bar - 1.0);
	    ncolor.green = 53247.0 * (2.0 - bar);
	    ncolor.red = 0;
	    bar=1.0;
	} else {
	    /* Use 0.5 as the adjustment - basically, if greater than 0.5,
	     * we have pure green with lesser amounts of red.  If less than
	     * 0.5, we have pure red with lesser amounts of green.
	     */
	    if (bar < 0.0) bar=0.0;  /* Like above, doesn't affect display */
	    if (bar >= 0.5) ncolor.green = 0xcfff;
	    else ncolor.green = 106494.0 * bar;
	    if (bar <= 0.5) ncolor.red = 0xcfff;
	    else ncolor.red = 106494.0 * (1.0 - bar);
	    ncolor.blue = 0;
	}
	vitals[bar_pos].style[nstyle]->bg[GTK_STATE_PRELIGHT] = ncolor;
	gtk_widget_set_style(vitals[bar_pos].bar, vitals[bar_pos].style[nstyle]);
	vitals[bar_pos].state=is_alert;
    } else {
	if (bar>1.0) bar=1.0;
	if (is_alert) is_alert=1;	/* Safety check */
	if (vitals[bar_pos].state!=is_alert) {
	    gtk_widget_set_style (vitals[bar_pos].bar, vitals[bar_pos].style[is_alert]);
	    vitals[bar_pos].state=is_alert;
	}
    }
    gtk_progress_bar_update (GTK_PROGRESS_BAR (vitals[bar_pos].bar),bar );
    gtk_widget_draw (vitals[bar_pos].bar, NULL);
}

/* This updates the status bars.  If redraw, then redraw them
 * even if they have not changed
 */

void draw_message_window(int redraw) {
    float bar;
    int is_alert,flags;
    static uint16 scrollsize_hp=0, scrollsize_sp=0, scrollsize_food=0,
	scrollsize_grace=0;
    static uint8 scrollhp_alert=FALSE, scrollsp_alert=FALSE,
	scrollfood_alert=FALSE, scrollgrace_alert=FALSE;

    if (updatelock < 25) {
	updatelock++;
	/* draw hp bar */
	if(cpl.stats.maxhp>0)
	{
	    bar=(float)cpl.stats.hp/cpl.stats.maxhp;
	    if(bar<=0)
		bar=(float)0.01;
	    is_alert=(cpl.stats.hp <= cpl.stats.maxhp/4);
	}
	else
	{
	    bar=(float)0.01;
	    is_alert=0;
	}

	if (redraw || scrollsize_hp!=bar || scrollhp_alert!=is_alert)
	    draw_stat_bar(0, bar, is_alert);

	scrollsize_hp=bar;
	scrollhp_alert=is_alert;

	/* draw sp bar.  Let draw_stats_bar handle high values */
	bar=(float)cpl.stats.sp/cpl.stats.maxsp;
	if(bar<=0)
	    bar=(float)0.01;

	is_alert=(cpl.stats.sp <= cpl.stats.maxsp/4);

	if (redraw || scrollsize_sp!=bar || scrollsp_alert!=is_alert)
	    draw_stat_bar(1, bar, is_alert);

	scrollsize_sp=bar;
	scrollsp_alert=is_alert;

	/* draw grace bar. grace can go above max or below min */
	bar=(float)cpl.stats.grace/cpl.stats.maxgrace;
	if(bar<=0)
	    bar=(float)0.01;

	is_alert=(cpl.stats.grace <= cpl.stats.maxgrace/4);

	if (redraw || scrollsize_grace!=bar || scrollgrace_alert!=is_alert)
	    draw_stat_bar(2, bar, is_alert);

	scrollsize_grace=bar;
	scrollgrace_alert=is_alert;

	/* draw food bar */
	bar=(float)cpl.stats.food/999;
	if(bar<=0)
	    bar=(float)0.01;
	is_alert=(cpl.stats.food <= 999/4);

	if (redraw || scrollsize_food!=bar || scrollfood_alert!=is_alert)
	    draw_stat_bar(3, bar, is_alert);

	scrollsize_food=bar;
	scrollfood_alert=is_alert;

	flags = cpl.stats.flags;

	if (redraw || cpl.stats.resist_change) {
	    int i,j=0;
	    char buf[40];

	    cpl.stats.resist_change=0;
	    for (i=0; i<NUM_RESISTS; i++) {
		if (cpl.stats.resists[i]) {
		    sprintf(buf,"%-10s %+4d",
			resists_name[i], cpl.stats.resists[i]);
		    gtk_label_set(GTK_LABEL(resists[j]), buf);
		    gtk_widget_draw(resists[j], NULL);
		    j++;
		    if (j >= NUM_RESISTS) break;
		}
	    }
	    /* Erase old/unused resistances */
	    while (j<NUM_RESISTS) {
		gtk_label_set(GTK_LABEL(resists[j]), "              ");
		gtk_widget_draw(resists[j], NULL);
		j++;
	    }
	} /* if we draw the resists */
    }
    else {
	/*    printf ("WARNING -- RACE. Frozen updates until updatelock is cleared!\n");*/
    }
}







/****************************************************************************
 *
 * Dialogue boxes and the menus that raise them.
 *
 ****************************************************************************/

static void aboutdialog(GtkWidget *widget) {
#include "help/about.h"
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *aboutlabel;
  GtkWidget *vscrollbar;
  GtkWidget *aboutbutton;
  GtkWidget *aboutgtkpixmap;
  GdkPixmap *aboutgdkpixmap;
  GdkBitmap *aboutgdkmask;

  GtkStyle *style;

  if(!gtkwin_about) {

    gtkwin_about = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_position (GTK_WINDOW (gtkwin_about), GTK_WIN_POS_CENTER);
    gtk_widget_set_usize (gtkwin_about,500,210);
    gtk_window_set_title (GTK_WINDOW (gtkwin_about), "About Crossfire");

    gtk_signal_connect (GTK_OBJECT (gtkwin_about), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_about);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_about), 0);
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER(gtkwin_about),vbox);
    style = gtk_widget_get_style(gtkwin_about);
    gtk_widget_realize(gtkwin_about);
    aboutgdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_about->window,
						  &aboutgdkmask,
						  &style->bg[GTK_STATE_NORMAL],
						  (gchar **)crossfiretitle);
    aboutgtkpixmap= gtk_pixmap_new (aboutgdkpixmap, aboutgdkmask);
    gtk_box_pack_start (GTK_BOX (vbox),aboutgtkpixmap, FALSE, TRUE, 0);
    gtk_widget_show (aboutgtkpixmap);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    aboutlabel = gtk_text_new (NULL, NULL);
    gtk_text_set_editable (GTK_TEXT (aboutlabel), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox),aboutlabel, TRUE, TRUE, 0);
    gtk_widget_show (aboutlabel);

    vscrollbar = gtk_vscrollbar_new (GTK_TEXT (aboutlabel)->vadj);
    gtk_box_pack_start (GTK_BOX (hbox),vscrollbar, FALSE, FALSE, 0);

    gtk_widget_show (vscrollbar);

    gtk_widget_show (hbox);

    hbox = gtk_hbox_new(FALSE, 2);

    aboutbutton = gtk_button_new_with_label ("Close");
    gtk_signal_connect_object (GTK_OBJECT (aboutbutton), "clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT (gtkwin_about));
    gtk_box_pack_start (GTK_BOX (hbox), aboutbutton, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (aboutbutton);
    gtk_widget_show (hbox);

    gtk_widget_show (vbox);
    gtk_widget_show (gtkwin_about);
    gtk_text_insert (GTK_TEXT (aboutlabel), NULL, &aboutlabel->style->black,
		     NULL, VERSION_INFO , -1);
    gtk_text_insert (GTK_TEXT (aboutlabel), NULL, &aboutlabel->style->black,
		     NULL, text , -1);
    gtk_adjustment_set_value(GTK_TEXT(aboutlabel)->vadj, 0.0);
  }
  else {
    gdk_window_raise (gtkwin_about->window);
  }
}

static void createBugTracker(void) {
    if (bugtrack ==NULL){
        LogEntry* le;
        bugtrack = gtk_text_new (NULL, NULL);
        gtk_signal_connect (GTK_OBJECT (bugtrack), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &bugtrack);
        gtk_text_set_editable (GTK_TEXT (bugtrack), FALSE);
        gtk_text_insert (GTK_TEXT (bugtrack), NULL, &bugtrack->style->black,NULL, "MESSAGES TRACK:\n" , -1);
        for (le=LogFirst;le;le=le->next)
            gtk_text_insert (GTK_TEXT (bugtrack), NULL, &bugtrack->style->black,NULL, getLogText(le) , -1);

    }
}

static void bugdialog(GtkWidget *widget) {
#include "help/bugreport.h"
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *buglabel;
  GtkWidget *vscrollbar;
  GtkWidget *bugbutton;
  GtkWidget *buggtkpixmap;
  GdkPixmap *buggdkpixmap;
  GdkBitmap *buggdkmask;

  GtkStyle *style;
#ifndef CFGTK2
  GdkFont* font;
#endif
    int i;

  if(!gtkwin_bug) {

    gtkwin_bug = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_position (GTK_WINDOW (gtkwin_bug), GTK_WIN_POS_CENTER);
    gtk_widget_set_usize (gtkwin_bug,500,450);
    gtk_window_set_title (GTK_WINDOW (gtkwin_bug), "Report a bug in Crossfire");

    gtk_signal_connect (GTK_OBJECT (gtkwin_bug), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_bug);
    /*gtk_signal_connect (GTK_OBJECT (gtkwin_bug), "destroy", GTK_SIGNAL_FUNC(bugreportdestroy), &gtkwin_bug);*/

    gtk_container_border_width (GTK_CONTAINER (gtkwin_bug), 0);
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER(gtkwin_bug),vbox);
    style = gtk_widget_get_style(gtkwin_bug);
    gtk_widget_realize(gtkwin_bug);
    buggdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_bug->window,
						  &buggdkmask,
						  &style->bg[GTK_STATE_NORMAL],
						  (gchar **)crossfiretitle);
    buggtkpixmap= gtk_pixmap_new (buggdkpixmap, buggdkmask);
    gtk_box_pack_start (GTK_BOX (vbox),buggtkpixmap, FALSE, TRUE, 0);
    gtk_widget_show (buggtkpixmap);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    buglabel = gtk_text_new (NULL, NULL);
    gtk_widget_set_style(buglabel,style);
    /*GtkStyle*   gtk_widget_get_style            (GtkWidget *widget);*/
    gtk_text_set_editable (GTK_TEXT (buglabel), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox),buglabel, TRUE, TRUE, 0);
    gtk_widget_show (buglabel);

    vscrollbar = gtk_vscrollbar_new (GTK_TEXT (buglabel)->vadj);
    gtk_box_pack_start (GTK_BOX (hbox),vscrollbar, FALSE, FALSE, 0);

    gtk_widget_show (vscrollbar);

    gtk_widget_show (hbox);
    hbox = gtk_hbox_new(FALSE, 2);
    createBugTracker();
#ifndef CFGTK2
    /* Win32 uses GTK2, this apparently doesn't work... */
    font = gdk_font_load ("-*-fixed-*-*-*-*-12-*-*-*-*-*-*-*");
    if (font){
        style = gtk_style_copy(gtk_widget_get_style (bugtrack));
        gdk_font_unref(style->font);
        style->font=font; /*no ref since transfert*/
        font=NULL;
        gtk_widget_set_style(bugtrack,style);
    }
#endif
    gtk_box_pack_start (GTK_BOX (hbox),bugtrack, TRUE, TRUE, 0);
    gtk_widget_show (bugtrack);
    vscrollbar = gtk_vscrollbar_new (GTK_TEXT (bugtrack)->vadj);
    gtk_box_pack_start (GTK_BOX (hbox),vscrollbar, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show (vscrollbar);
    gtk_widget_show (hbox);
    hbox = gtk_hbox_new(FALSE, 2);
    bugbutton = gtk_button_new_with_label ("Close");
    gtk_signal_connect_object (GTK_OBJECT (bugbutton), "clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT (gtkwin_bug));
    gtk_box_pack_start (GTK_BOX (hbox), bugbutton, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (bugbutton);
    gtk_widget_show (hbox);

    gtk_widget_show (vbox);
    gtk_widget_show (gtkwin_bug);
    gtk_text_insert (GTK_TEXT (buglabel), NULL, &buglabel->style->black,
		     NULL, VERSION_INFO , -1);
    gtk_text_insert (GTK_TEXT (buglabel), NULL, &buglabel->style->black,
		     NULL, text , -1);

    gtk_text_insert (GTK_TEXT (buglabel), NULL, &buglabel->style->black,
		     NULL, "\n\nVersion Information\n" , -1);


    gtk_adjustment_set_value(GTK_TEXT(buglabel)->vadj, 0.0);
  }
  else {
    gdk_window_raise (gtkwin_bug->window);
  }
}

void cclist_button_event(GtkWidget *gtklist, gint row, gint column, GdkEventButton *event) {
  gchar *buf;
  if (event->button==1) {
    gtk_clist_get_text (GTK_CLIST(cclist), row, 0, &buf);
    gtk_label_set (GTK_LABEL(cnumentrytext), buf);
    gtk_clist_get_text (GTK_CLIST(cclist), row, 1, &buf);
    gtk_entry_set_text (GTK_ENTRY(ckeyentrytext), buf);
    gtk_clist_get_text (GTK_CLIST(cclist), row, 3, &buf);
    gtk_entry_set_text (GTK_ENTRY(cmodentrytext), buf);
    gtk_clist_get_text (GTK_CLIST(cclist), row, 4, &buf);
    gtk_entry_set_text (GTK_ENTRY(ckentrytext), buf);
  }
}


static void disconnect(GtkWidget *widget) {
#ifdef WIN32
    closesocket(csocket.fd);
#else
    close(csocket.fd);
#endif
    csocket.fd = -1;
    if (csocket_fd) {
	gdk_input_remove(csocket_fd);
	csocket_fd=0;
	gtk_main_quit();
    }
    cleanup_textmanagers();
}

/* Ok, simplistic help system. Just put the text file up in a scrollable window */

static void shelpdialog(GtkWidget *widget) {
#include "help/shelp.h"
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *shelptext;
  GtkWidget *helpbutton;
  GtkWidget *vscrollbar;
  /*  GtkStyle *style;*/

  if(!gtkwin_shelp) {

    gtkwin_shelp = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_position (GTK_WINDOW (gtkwin_shelp), GTK_WIN_POS_CENTER);
    gtk_widget_set_usize (gtkwin_shelp,400,300);
    gtk_window_set_title (GTK_WINDOW (gtkwin_shelp), "Crossfire Server Help");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_shelp), TRUE, TRUE, FALSE);

    gtk_signal_connect (GTK_OBJECT (gtkwin_shelp), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_shelp);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_shelp), 0);
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER(gtkwin_shelp),vbox);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    shelptext = gtk_text_new (NULL, NULL);
    gtk_text_set_editable (GTK_TEXT (shelptext), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox),shelptext, TRUE, TRUE, 0);
    gtk_widget_show (shelptext);

    vscrollbar = gtk_vscrollbar_new (GTK_TEXT (shelptext)->vadj);
    gtk_box_pack_start (GTK_BOX (hbox),vscrollbar, FALSE, FALSE, 0);

    gtk_widget_show (vscrollbar);
    gtk_widget_show (hbox);

    hbox = gtk_hbox_new(FALSE, 2);

    helpbutton = gtk_button_new_with_label ("Close");
    gtk_signal_connect_object (GTK_OBJECT (helpbutton), "clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT (gtkwin_shelp));
    gtk_box_pack_start (GTK_BOX (hbox), helpbutton, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (helpbutton);
    gtk_widget_show (hbox);

    gtk_widget_show (vbox);
    gtk_widget_show (gtkwin_shelp);
    gtk_text_insert (GTK_TEXT (shelptext), NULL, &shelptext->style->black, NULL, text , -1);
  }
  else {
    gdk_window_raise (gtkwin_shelp->window);
  }
}

/* Various routines for setting modes by menu choices. */
static void new_menu_pickup(GtkWidget *button, int val)
{
  char modestr[128];
  unsigned int old_pickup = pickup_mode;

  /* widget is GtkCheckMenuItem */
  if(GTK_CHECK_MENU_ITEM (button)->active) {
      pickup_mode=pickup_mode|val;
      if (val != PU_NEWMODE)
          pickup_mode = pickup_mode | PU_NEWMODE;
  } else pickup_mode=pickup_mode&~val;


  if (old_pickup == pickup_mode)
      return;

#if 0
  fprintf(stderr,"val=0x%8x\n",val);
  fprintf(stderr,"mode=0x%8x\n",pmode);
#endif

  sprintf(modestr,"bind pickup %u",pickup_mode);
  draw_info("To set this pickup mode to a key, use:",NDI_BLACK);
  draw_info(modestr,NDI_BLACK);
  sprintf(modestr,"pickup %u",pickup_mode);
  send_command(modestr, -1, 0);
}


static void menu_pickup0(void) {
  pickup_mode = 0;
  send_command("pickup 0", -1, 0);
}

static void menu_pickup1(void) {
  pickup_mode = 1;
  send_command("pickup 1", -1, 0);
}

static void menu_pickup2(void) {
  pickup_mode = 2;
  send_command("pickup 2", -1, 0);
}

static void menu_pickup3(void) {
  pickup_mode = 3;
  send_command("pickup 3", -1, 0);
}

static void menu_pickup4(void) {
  pickup_mode = 4;
  send_command("pickup 4", -1, 0);
}

static void menu_pickup5(void) {
  pickup_mode = 5;
  send_command("pickup 5", -1, 0);

}

static void menu_pickup6(void) {
  pickup_mode = 6;
  send_command("pickup 6", -1, 0);
}

static void menu_pickup7(void) {
  pickup_mode = 7;
  send_command("pickup 7", -1, 0);
}

static void menu_pickup10(void) {
  pickup_mode = 10;
  send_command("pickup 10", -1, 0);
}



static void menu_who(void) {
  extended_command("who");
}

static void menu_apply(void) {
  extended_command("apply");
}

static void menu_cast(void) {
    gtk_entry_set_text(GTK_ENTRY(entrytext),"cast ");
    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
}

static void menu_search(void) {
  extended_command("search");
}

static void menu_disarm(void) {
  extended_command("disarm");
}


static GtkWidget *gtkwin_spell = NULL; /* spell window */
static GtkWidget *description  = NULL; /* the text box containing spell description */
static GtkWidget *list         = NULL;
static GtkWidget *spelloptions = NULL; /* text box with extra options to pass to the spell */

static void select_spell_event(GtkWidget *gtklist, gint row, gint column, 
    GdkEventButton *event) {

    char command[MAX_BUF], message[MAX_BUF];
    Spell *spell = gtk_clist_get_row_data (GTK_CLIST(gtklist), row);
    char *options = NULL;

    if (!event) return; /* we have nothing to do */
    /* any click will select the spell, and show it's description */
    gtk_text_freeze(GTK_TEXT(description));
    gtk_text_set_point(GTK_TEXT(description), 0);
    gtk_text_forward_delete(GTK_TEXT(description), gtk_text_get_length(GTK_TEXT(description)));
    sprintf(message, "%s - level %d %s spell\n\n%s", spell->name, spell->level, 
	spell->skill?spell->skill:"generic", spell->message);
    gtk_text_insert(GTK_TEXT(description), NULL, NULL, NULL, message, -1);
    gtk_text_thaw(GTK_TEXT(description));
    if (event->button==2) { /* on middle click, also invoke the spell */
	options = gtk_editable_get_chars(GTK_EDITABLE(spelloptions), 0, -1);
	sprintf(command, "invoke %d %s", spell->tag, options);
	send_command(command, -1, 1);
	g_free(options);
    }
    else if (event->button==3) { /* on right click, also cast the spell */
	options = gtk_editable_get_chars(GTK_EDITABLE(spelloptions), 0, -1);
	sprintf(command, "cast %d %s", spell->tag, options);
	send_command(command, -1, 1);
	g_free(options);
    }
}

static void update_spell_list(int force) {
    gint row; 
    char buffer[3][MAX_BUF];
    char *columns[3];
    Spell *spell;
    PixmapInfo * pixmap;

    /* only update if we have to */
    if (!force && !cpl.spells_updated) return;
    if (!gtkwin_spell || !GTK_IS_CLIST(list) || !GTK_WIDGET_VISIBLE(gtkwin_spell)) return; 

    gtk_clist_freeze(GTK_CLIST(list));

    /* we are about to recreate the entire spell list, so remove the existing one first */
    gtk_clist_clear(GTK_CLIST(list));

    for (spell = cpl.spelldata; spell; spell=spell->next) {
	if (!spell) break;
	pixmap = pixmaps[spell->face];
	buffer[2][0]='\0';
	buffer[0][0]='\0';
	strcpy(buffer[1], spell->name);
	columns[0] = buffer[0];
	columns[1] = buffer[1];
	columns[2] = buffer[2];
	if (spell->sp) sprintf(buffer[2], "%d mana ", spell->sp);
	if (spell->grace) sprintf(buffer[2]+strlen(buffer[2]), "%d grace ", spell->grace);
	if (spell->dam) sprintf(buffer[2]+strlen(buffer[2]), "%d damage ", spell->dam);

	/* the columns array doesn't yet contain the data we need, but we can't set the 
	 * row colour until we create the row, so we create the row with gtk_clist_append()
	 *  set the colour, then reset the text in the second column 
	 */
	row = gtk_clist_append(GTK_CLIST(list), columns);
	gtk_clist_set_row_data(GTK_CLIST(list), row, spell); 
	if (spell->path & cpl.stats.denied) {
	    gtk_clist_set_background (GTK_CLIST(list), row, &root_color[NDI_RED]);
	    strcat(buffer[2], "(DENIED) ");
	}
	else if (spell->path & cpl.stats.attuned) {
	    gtk_clist_set_background (GTK_CLIST(list), row, &root_color[NDI_GREEN]);
	    strcat(buffer[2], "(attuned) ");
	}
	else if (spell->path & cpl.stats.repelled) {
	    gtk_clist_set_background (GTK_CLIST(list), row, &root_color[NDI_ORANGE]);
	    strcat(buffer[2], "(repelled) ");
	}
	gtk_clist_set_text(GTK_CLIST(list), row, 2, columns[2]);
	gtk_clist_set_pixmap (GTK_CLIST (list), row, 0,
	    (GdkPixmap*)pixmap->icon_image, (GdkBitmap*)pixmap->icon_mask);
    }
    gtk_clist_thaw(GTK_CLIST(list));
    cpl.spells_updated =0;
}

static void menu_spells(void) {
    GtkWidget * scroll_window;
    GtkStyle * liststyle;
    GtkWidget *cancelbutton;
    GtkWidget * vbox;
    GtkWidget * optionsbox;
    GtkWidget * spelloptionslabel;
    gchar *titles[] = {" ", "Name", "Cost"};

    if (gtkwin_spell && GTK_IS_CLIST(list)) {
	  /* the window is already created, re-present it */
	if (GTK_WIDGET_VISIBLE(gtkwin_spell)) { 
	    gdk_window_raise(gtkwin_spell->window);
	    return; 
	}

	/* the window is hidden at the moment, we don't need to recreate it, 
         * we can merely reshow it, but the spell list won't have updated whilst
         * it was hidden so we have to force an update */
	gtk_widget_show_all(gtkwin_spell);
	update_spell_list(1);
	return;
    }

    /* we can't use an existing version, so we must create a new one, first we 
     * will deal with the window itself */
    gtkwin_spell = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_set_default_size(GTK_WINDOW(gtkwin_spell), 400+image_size, 400+image_size);
    gtk_window_set_title(GTK_WINDOW (gtkwin_spell), "Cast Spell");

    /* Now for its contents: first we'll deal with the options widget */
    spelloptions = gtk_entry_new();
    spelloptionslabel = gtk_label_new("Spell Options:");
    optionsbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(optionsbox), spelloptionslabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(optionsbox), spelloptions, TRUE, TRUE, 0);

    /* now the list scroll window */
    scroll_window = gtk_scrolled_window_new (0,0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
	GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* and the spell list itself */
    list = gtk_clist_new_with_titles(3, titles);
    gtk_clist_set_column_width(GTK_CLIST(list), 1, image_size);
    gtk_clist_set_column_width(GTK_CLIST(list), 1, 200);
    gtk_clist_set_column_width(GTK_CLIST(list), 2, 200);
    gtk_clist_set_selection_mode(GTK_CLIST(list) , GTK_SELECTION_BROWSE);
    gtk_clist_set_row_height (GTK_CLIST(list), image_size); 
    liststyle = gtk_rc_get_style(list);
    if (liststyle) {
	liststyle->bg[GTK_STATE_SELECTED] = gdk_grey;
	liststyle->fg[GTK_STATE_SELECTED] = gdk_black;
	gtk_widget_set_style (list, liststyle);
    }
    /* set the actions for the mouse buttons to trigger the callback function */
    gtk_clist_set_button_actions(GTK_CLIST(list), 1, GTK_BUTTON_SELECTS);
    gtk_clist_set_button_actions(GTK_CLIST(list), 2, GTK_BUTTON_SELECTS);
    gtk_signal_connect(GTK_OBJECT(list), "select_row",
	GTK_SIGNAL_FUNC(select_spell_event), NULL);

    /* with all that done, we can now add it to the scroll window */
    gtk_container_add(GTK_CONTAINER(scroll_window), list);

    /* now we'll create the description box */
    description = gtk_text_new(NULL, NULL);
    gtk_text_set_editable(GTK_TEXT (description), FALSE);

    /* finally add a close button to the window */
    cancelbutton = gtk_button_new_with_label("Close");
    gtk_signal_connect_object (GTK_OBJECT (cancelbutton), "clicked",
	GTK_SIGNAL_FUNC(gtk_widget_hide_all), GTK_OBJECT (gtkwin_spell));

    /* vbox holds all the widgets we just created, in order */
    vbox = gtk_vbox_new(FALSE, 2);

    /* ok, time to pack it all up */
    gtk_container_add(GTK_CONTAINER(gtkwin_spell), vbox);
    gtk_box_pack_start(GTK_BOX(vbox), optionsbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_window, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), description, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), cancelbutton, FALSE, FALSE, 0);

    gtk_widget_show_all(gtkwin_spell);

    /* let's add the spells to the list now */
    update_spell_list(1);
}

void menu_clear(void) {
  guint size;

  size = gtk_text_get_length(GTK_TEXT (gtkwin_info_text));
  gtk_text_freeze (GTK_TEXT (gtkwin_info_text));
  gtk_text_set_point(GTK_TEXT (gtkwin_info_text), 0);
  gtk_text_forward_delete (GTK_TEXT (gtkwin_info_text), size );
  gtk_text_thaw (GTK_TEXT (gtkwin_info_text));

#ifdef WIN32
  if ( gtkwin_info_text2 )
      {
#endif
  size = gtk_text_get_length(GTK_TEXT (gtkwin_info_text2));
  gtk_text_freeze (GTK_TEXT (gtkwin_info_text2));
  gtk_text_set_point(GTK_TEXT (gtkwin_info_text2), 0);
  gtk_text_forward_delete (GTK_TEXT (gtkwin_info_text2), size );
  gtk_text_thaw (GTK_TEXT (gtkwin_info_text2));
#ifdef WIN32
      }
#endif
}

static void sexit(void)
{
    extended_command("quit");
}

void client_exit(void) {
    LOG(LOG_INFO,"gtk::client_exit","Exiting with return value 0.");
#ifdef WIN32
   	script_killall();
#endif
    exit(0);
}

/* To keep trace of pickup menus, and be able to check/uncheck them. */
static GtkWidget* pickup_menus[32];
static int pickup_value[32];
static int pickup_count = 0;

/* get_menu_display
 * This sets up menus
 */

static int get_menu_display (GtkWidget *box) {
  GtkWidget *filemenu;
  GtkWidget *actionmenu;
  GtkWidget *pickupmenu;
  GtkWidget *newpickupmenu;
  GtkWidget *ratiopickupmenu;
  GtkWidget *weaponpickupmenu;
  GtkWidget *armourpickupmenu;
  GtkWidget *bookspickupmenu;
  GtkWidget *clientmenu;
  GtkWidget *helpmenu;
  GtkWidget *menu_bar;
  GtkWidget *root_filemenu;
  GtkWidget *root_helpmenu;
  GtkWidget *root_actionmenu;
  /*  GtkWidget *sub_pickupmenu;*/
  GtkWidget *root_clientmenu;
  GtkWidget *menu_items;
  GtkWidget *pickup_menu_item;
  GtkWidget *newpickup_menu_item;
  GtkWidget *ratiopickup_menu_item;
  GtkWidget *weaponpickup_menu_item;
  GtkWidget *armourpickup_menu_item;
  GtkWidget *bookspickup_menu_item;
  GSList *pickupgroup;
  GSList *ratiopickupgroup;
  int i;
  char menustring[128];


  /* Init the menu-widget, and remember -- never
   * gtk_show_widget() the menu widget!!
   * This is the menu that holds the menu items, the one that
   * will pop up when you click on the "Root Menu" in the app */
  filemenu = gtk_menu_new();

  /* Next we make a little loop that makes three menu-entries for "test-menu".
   * Notice the call to gtk_menu_append.  Here we are adding a list of
   * menu items to our menu.  Normally, we'd also catch the "clicked"
   * signal on each of the menu items and setup a callback for it,
   * but it's omitted here to save space. */

  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (filemenu), menu_items);
  gtk_widget_show (menu_items);

  menu_items = gtk_menu_item_new_with_label("Save config");
  gtk_menu_append(GTK_MENU (filemenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(save_defaults), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new_with_label("Save window positions");
  gtk_menu_append(GTK_MENU (filemenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(save_winpos), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new ();
  gtk_menu_append(GTK_MENU (filemenu), menu_items);
  gtk_widget_show(menu_items);


  menu_items = gtk_menu_item_new_with_label("Quit character");
  gtk_menu_append(GTK_MENU (filemenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(sexit), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new_with_label("Quit client");
  gtk_menu_append(GTK_MENU (filemenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(client_exit), NULL);
  gtk_widget_show(menu_items);

  /* This is the root menu, and will be the label
   * displayed on the menu bar.  There won't be a signal handler attached,
   * as it only pops up the rest of the menu when pressed. */
  root_filemenu = gtk_menu_item_new_with_label("File");

  gtk_widget_show(root_filemenu);

  /* Now we specify that we want our newly created "menu" to be the menu
   * for the "root menu" */
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_filemenu), filemenu);

 /* Do the clientmenu */

  clientmenu = gtk_menu_new();

  /*  menu_items = gtk_menu_item_new_with_label("Navigator");
  gtk_menu_append(GTK_MENU (clientmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(navbut), NULL);
			    gtk_widget_show(menu_items);*/

  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (clientmenu), menu_items);
  gtk_widget_show (menu_items);

  menu_items = gtk_menu_item_new_with_label("Clear info");
  gtk_menu_append(GTK_MENU (clientmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_clear), NULL);
  gtk_widget_show(menu_items);


  menu_items = gtk_menu_item_new_with_label("Spells");
  gtk_menu_append(GTK_MENU (clientmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_spells), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new_with_label("Configure");
  gtk_menu_append(GTK_MENU (clientmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(configdialog), NULL);
  gtk_widget_show(menu_items);


  menu_items = gtk_menu_item_new_with_label("Disconnect");
  gtk_menu_append(GTK_MENU (clientmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(disconnect), NULL);
  gtk_widget_show(menu_items);


  root_clientmenu = gtk_menu_item_new_with_label("Client");

  gtk_widget_show(root_clientmenu);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_clientmenu), clientmenu);

  /* Do the actionmenu */

  actionmenu = gtk_menu_new();

  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (actionmenu), menu_items);
  gtk_widget_show (menu_items);

  menu_items = gtk_menu_item_new_with_label("Who");
  gtk_menu_append(GTK_MENU (actionmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_who), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new_with_label("Cast...");
  gtk_menu_append(GTK_MENU (actionmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_cast), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new_with_label("Apply");
  gtk_menu_append(GTK_MENU (actionmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_apply), NULL);
  gtk_widget_show(menu_items);

  pickup_menu_item = gtk_menu_item_new_with_label("Pickup");
  gtk_menu_append(GTK_MENU (actionmenu), pickup_menu_item);
  /*  gtk_signal_connect_object(GTK_OBJECT(pickup_menu_item), "activate",
			    GTK_SIGNAL_FUNC(menu_apply), NULL);*/
  gtk_widget_show(pickup_menu_item);

  newpickup_menu_item = gtk_menu_item_new_with_label("NEWPickup");
  gtk_menu_append(GTK_MENU (actionmenu), newpickup_menu_item);
  gtk_widget_show(newpickup_menu_item);

  menu_items = gtk_menu_item_new_with_label("Search");
  gtk_menu_append(GTK_MENU (actionmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_search), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new_with_label("Disarm");
  gtk_menu_append(GTK_MENU (actionmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_disarm), NULL);
  gtk_widget_show(menu_items);


  root_actionmenu = gtk_menu_item_new_with_label("Action");

  gtk_widget_show(root_actionmenu);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_actionmenu), actionmenu);

 /* Do the submenu */

  pickupmenu = gtk_menu_new();

  /*  This allows you to change your pickup status. Eight different modes for pick up exist: ``don't pick up'',``pick up 1
item'', ``pick up 1 item and stop'', ``stop before picking up'', ``pick up all items'', pick up all items and stop'',
``pick up all magic items'', ``pick up all coins and gems''. Whenever you move over a pile of stuff your pickup*/
  pickupgroup=NULL;

  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (pickupmenu), menu_items);
  gtk_widget_show (menu_items);

  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Don't pick up");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup0), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Pick up 1 item");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup1), NULL);
  gtk_widget_show(menu_items);


  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Pick up 1 item and stop");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup2), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Stop before picking up.");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup3), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Pick up all items.");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup4), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Pick up all items and stop.");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup5), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Pick up all magic items.");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup6), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Pick up all coins and gems.");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup7), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_radio_menu_item_new_with_label(pickupgroup, "Pickup silver and higher value/weight.");
  pickupgroup = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (menu_items));
  gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_items), TRUE);
  gtk_menu_append(GTK_MENU (pickupmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_pickup10), NULL);
  gtk_widget_show(menu_items);


  /*  sub_pickupmenu = gtk_menu_item_new_with_label("Action");

  gtk_widget_show(sub_pickupmenu);*/
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (pickup_menu_item), pickupmenu);
/* ENDPICKUP */

/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

  /* root of the NEWPickup menu */
  newpickupmenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(newpickup_menu_item), newpickupmenu);

  menu_items = gtk_tearoff_menu_item_new();
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Enable NEW autopickup");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_NEWMODE));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_NEWMODE;

  menu_items = gtk_check_menu_item_new_with_label("Inhibit autopickup");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_INHIBIT));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_INHIBIT;

  menu_items = gtk_check_menu_item_new_with_label("Stop before pickup");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_STOP));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_STOP;

  menu_items = gtk_check_menu_item_new_with_label("Debug autopickup");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_DEBUG));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_DEBUG;


  /* the ratio pickup submenu */
  ratiopickupmenu = gtk_menu_new();
  ratiopickup_menu_item = gtk_menu_item_new_with_label("Weight/Value Ratio");
  gtk_menu_append(GTK_MENU(newpickupmenu), ratiopickup_menu_item);
  gtk_widget_show(ratiopickup_menu_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(ratiopickup_menu_item), ratiopickupmenu);

  ratiopickupgroup=NULL;

  menu_items = gtk_tearoff_menu_item_new();
  gtk_menu_append(GTK_MENU(ratiopickupmenu), menu_items);
  gtk_widget_show(menu_items);

  for (i=0;i<16;i++)
  {
    if (i==0) sprintf(menustring,"Ratio pickup OFF");
    else sprintf(menustring,"Ratio >= %d",i*5);
    menu_items = gtk_radio_menu_item_new_with_label(ratiopickupgroup, menustring);
    ratiopickupgroup = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_items));
    gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
    gtk_menu_append(GTK_MENU(ratiopickupmenu), menu_items);
    gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(i));
    gtk_widget_show(menu_items);
    pickup_menus[pickup_count] = menu_items;
    pickup_value[pickup_count++] = i;
  }


  /* weapon pickup menu */
  weaponpickupmenu = gtk_menu_new();
  weaponpickup_menu_item = gtk_menu_item_new_with_label("Weapons");
  gtk_menu_append(GTK_MENU(newpickupmenu), weaponpickup_menu_item);
  gtk_widget_show(weaponpickup_menu_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(weaponpickup_menu_item), weaponpickupmenu);

  menu_items = gtk_tearoff_menu_item_new();
  gtk_menu_append(GTK_MENU(weaponpickupmenu), menu_items);
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("All weapons");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(weaponpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_ALLWEAPON));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_ALLWEAPON;

  menu_items = gtk_check_menu_item_new_with_label("Missile Weapons");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(weaponpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_MISSILEWEAPON));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_MISSILEWEAPON;

  menu_items = gtk_check_menu_item_new_with_label("Bows");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(weaponpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_BOW));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_BOW;

  menu_items = gtk_check_menu_item_new_with_label("Arrows");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(weaponpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_ARROW));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_ARROW;


  /* armour pickup menu */
  armourpickupmenu = gtk_menu_new();
  armourpickup_menu_item = gtk_menu_item_new_with_label("Armour");
  gtk_menu_append(GTK_MENU(newpickupmenu), armourpickup_menu_item);
  gtk_widget_show(armourpickup_menu_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(armourpickup_menu_item), armourpickupmenu);

  menu_items = gtk_tearoff_menu_item_new();
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Helmets");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_HELMET));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_HELMET;

  menu_items = gtk_check_menu_item_new_with_label("Shields");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_SHIELD));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_SHIELD;

  menu_items = gtk_check_menu_item_new_with_label("Body Armour");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_ARMOUR));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_ARMOUR;

  menu_items = gtk_check_menu_item_new_with_label("Boots");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_BOOTS));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_BOOTS;

  menu_items = gtk_check_menu_item_new_with_label("Gloves");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_GLOVES));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_GLOVES;

  menu_items = gtk_check_menu_item_new_with_label("Cloaks");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_CLOAK));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_CLOAK;


  /* books pickup menu */
  bookspickupmenu = gtk_menu_new();
  bookspickup_menu_item = gtk_menu_item_new_with_label("Books");
  gtk_menu_append(GTK_MENU(newpickupmenu), bookspickup_menu_item);
  gtk_widget_show(bookspickup_menu_item);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(bookspickup_menu_item), bookspickupmenu);

  menu_items = gtk_tearoff_menu_item_new();
  gtk_menu_append(GTK_MENU(bookspickupmenu), menu_items);
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Spellbooks");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(bookspickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_SPELLBOOK));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_SPELLBOOK;

  menu_items = gtk_check_menu_item_new_with_label("Skillscrolls");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(bookspickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_SKILLSCROLL));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_SKILLSCROLL;

  menu_items = gtk_check_menu_item_new_with_label("Normal Books/Scrolls");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(bookspickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_READABLES));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_READABLES;


  /* continue with the rest of the stuff... */

  menu_items = gtk_check_menu_item_new_with_label("Food");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_FOOD));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_FOOD;

  menu_items = gtk_check_menu_item_new_with_label("Drinks");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_DRINK));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_DRINK;

  menu_items = gtk_check_menu_item_new_with_label("Flesh");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_FLESH));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_FLESH;

  menu_items = gtk_check_menu_item_new_with_label("Valuables (Money, Gems)");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_VALUABLES));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_VALUABLES;

  menu_items = gtk_check_menu_item_new_with_label("Keys");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_KEY));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_KEY;

  menu_items = gtk_check_menu_item_new_with_label("Magical Items");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_MAGICAL));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_MAGICAL;

  menu_items = gtk_check_menu_item_new_with_label("Potions");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_POTION));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_POTION;

  menu_items = gtk_check_menu_item_new_with_label("Magic Devices");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
  GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_MAGIC_DEVICE));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_MAGIC_DEVICE;

  menu_items = gtk_check_menu_item_new_with_label("Ignore cursed");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_NOT_CURSED));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_NOT_CURSED;

  menu_items = gtk_check_menu_item_new_with_label("Jewelry");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);
  gtk_signal_connect(GTK_OBJECT(menu_items),"activate",
    GTK_SIGNAL_FUNC(new_menu_pickup),GINT_TO_POINTER(PU_JEWELS));
  gtk_widget_show(menu_items);
  pickup_menus[pickup_count] = menu_items;
  pickup_value[pickup_count++] = PU_JEWELS;
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

  /*Do the helpmenu */
  helpmenu = gtk_menu_new();

  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (helpmenu), menu_items);
  gtk_widget_show (menu_items);

  menu_items = gtk_menu_item_new_with_label("Client commands");
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(chelpdialog), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new_with_label("Server help");
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(shelpdialog), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new ();
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);
  gtk_widget_show(menu_items);

  /* Link to things like the client-walkthrough and the playbook? */

  menu_items = gtk_menu_item_new_with_label("Report a bug");
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(bugdialog), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new ();
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);
  gtk_widget_show(menu_items);


  menu_items = gtk_menu_item_new_with_label("About");
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(aboutdialog), NULL);
  gtk_widget_show(menu_items);

  root_helpmenu = gtk_menu_item_new_with_label("Help");

  gtk_widget_show(root_helpmenu);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_helpmenu), helpmenu);

  /* Create a menu-bar to hold the menus and add it to our main window */


  menu_bar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(box), menu_bar, FALSE, FALSE, 2);
  gtk_widget_show(menu_bar);

  /* Create a button to which to attach menu as a popup */

  /* And finally we append the menu-item to the menu-bar -- this is the
   * "root" menu-item I have been raving about =) */
  gtk_menu_bar_append(GTK_MENU_BAR (menu_bar), root_filemenu);
  gtk_menu_bar_append(GTK_MENU_BAR (menu_bar), root_clientmenu);
  gtk_menu_bar_append(GTK_MENU_BAR (menu_bar), root_actionmenu);
  gtk_menu_item_right_justify (GTK_MENU_ITEM(root_helpmenu));
  gtk_menu_bar_append(GTK_MENU_BAR (menu_bar), root_helpmenu);

  /* always display the window as the last step so it all splashes on
   * the screen at once. */

  return 0;
}



/* get_root_display:
 * this sets up the root window (or none, if in split
 * windows mode, and also scans for any Xdefaults.  Right now, only
 * splitwindow and image are used.  image is the display
 * mechanism to use.  I thought having one type that is set
 * to font, xpm, or pixmap was better than using xpm and pixmap
 * resources with on/off values (which gets pretty weird
 * if one of this is set to off.
 */


/* Create the splash window at startup */

static void create_splash(void) {
    GtkWidget *vbox;
    GtkWidget *aboutgtkpixmap;
    GdkPixmap *aboutgdkpixmap;
    GdkBitmap *aboutgdkmask;
    GtkStyle *style;

    gtkwin_splash = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_position (GTK_WINDOW (gtkwin_splash), GTK_WIN_POS_CENTER);
    gtk_widget_set_usize (gtkwin_splash,346,87);
    gtk_window_set_title (GTK_WINDOW (gtkwin_splash), "Welcome to Crossfire");
    gtk_signal_connect (GTK_OBJECT (gtkwin_splash), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_splash);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_splash), 0);
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER(gtkwin_splash),vbox);
    style = gtk_widget_get_style(gtkwin_splash);
    gtk_widget_realize(gtkwin_splash);
    aboutgdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_splash->window,
					       &aboutgdkmask,
						  &style->bg[GTK_STATE_NORMAL],
					       (gchar **)crossfiretitle);
    aboutgtkpixmap= gtk_pixmap_new (aboutgdkpixmap, aboutgdkmask);
    gtk_box_pack_start (GTK_BOX (vbox),aboutgtkpixmap, FALSE, TRUE, 0);
    gtk_widget_show (aboutgtkpixmap);

    gtk_widget_show (vbox);
    gtk_widget_show (gtkwin_splash);


    while ( gtk_events_pending() ) {
	gtk_main_iteration();
    }
    sleep (1);
    while ( gtk_events_pending() ) {
	gtk_main_iteration();
    }

}


static void destroy_splash(void) {
  gtk_widget_destroy(gtkwin_splash);
}

/* Error handlers removed.  Right now, there is nothing for
 * the client to do if it gets a fatal error - it doesn't have
 * any information to save.  And we might as well let the standard
 * X11 error handler handle non fatal errors.
 */


void create_windows(void) {
  GtkWidget *rootvbox;
  GtkWidget *frame;
  int i;

  tooltips = gtk_tooltips_new();

  if (want_config[CONFIG_SPLITWIN]==FALSE) {
    GtkStyle	*style;
    int gcw;
    int gch;
    int rootwin_width;
    int rootwin_height;

    gtkwin_root = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    style = gtk_rc_get_style(gtkwin_root);
    if (style) {
#ifdef CFGTK2 /* GTK 2.2 stuff */
	gcw = gdk_char_width(gdk_font_from_description(style->font_desc), '0') + 4;
	gch = gdk_char_height(gdk_font_from_description(style->font_desc), '0') + 2;
#else
	gcw = gdk_char_width(style->font, '0') + 4;
	gch = gdk_char_height(style->font, '0') + 2;
#endif
    } else {
	/* These are what the old defaults values were */
	gcw = 11;
	gch = 10;
    }

    gtk_widget_set_events (gtkwin_root, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_root, 0, 0);

    if ((55*gcw)+(map_image_size*use_config[CONFIG_MAPWIDTH]) >= gdk_screen_width())
    	rootwin_width = gdk_screen_width() - 30;
    else
        rootwin_width = (55*gcw)+(map_image_size*use_config[CONFIG_MAPWIDTH]);
    if ((33*gch)+(map_image_size*use_config[CONFIG_MAPHEIGHT]) >= gdk_screen_height())
    	rootwin_height = gdk_screen_height() - 50;
    else
        rootwin_height = (33*gch)+(map_image_size*use_config[CONFIG_MAPHEIGHT]);
    gtk_widget_set_usize (gtkwin_root,rootwin_width,rootwin_height);
    gtk_window_set_title (GTK_WINDOW (gtkwin_root), "Crossfire GTK Client");
    gtk_signal_connect_object(GTK_OBJECT(gtkwin_root), "destroy",GTK_SIGNAL_FUNC(main_window_destroyed), NULL);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_root), 0);

    /* Alloc colors.  colorname[] comes from xutil.c */
    for (i=0; i<=12; i++ ) {
	if ( !gdk_color_parse(colorname[i], &root_color[i])) {
	    printf ("cparse failed (%s)\n",colorname[i]);
	}
	if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[i])) {
	    printf ("calloc failed\n");
	}
    }

    /* menu / windows division */
    rootvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (gtkwin_root), rootvbox);
    gtk_widget_show (rootvbox);

    get_menu_display(rootvbox);

    /* first horizontal division. inv+obj on left, rest on right */

    inv_hpane = gtk_hpaned_new ();

    gtk_box_pack_start (GTK_BOX (rootvbox), inv_hpane, TRUE, TRUE, 0);
    gtk_container_border_width (GTK_CONTAINER(inv_hpane), 5);
    gtk_widget_show (inv_hpane);

    /* Divisior game+stats | text */

    stat_info_hpane = gtk_hpaned_new ();
    gtk_paned_add2 (GTK_PANED (inv_hpane), stat_info_hpane);

    /* text frame */

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, (25*gcw), (30*gch));
    gtk_paned_add2 (GTK_PANED (stat_info_hpane), frame);

    get_info_display (frame);
    gtk_widget_show (frame);

    /* game & statbars below, stats above */
    stat_game_vpane = gtk_vpaned_new ();
    gtk_paned_add1 (GTK_PANED (stat_info_hpane), stat_game_vpane);

#if 0
    /* game - statbars */
    if (want_config[CONFIG_MAPWIDTH]>15) {
	bigmap=TRUE;

	game_bar_vpane = gtk_hpaned_new ();
	gtk_paned_add1 (GTK_PANED (stat_game_vpane), game_bar_vpane);
    } else {
	game_bar_vpane = gtk_vpaned_new ();
	gtk_paned_add2 (GTK_PANED (stat_game_vpane), game_bar_vpane);
    }
#else
    game_bar_vpane = gtk_vpaned_new ();
    gtk_paned_add2 (GTK_PANED (stat_game_vpane), game_bar_vpane);
#endif


    /* Statbars frame */
    message_frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(message_frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (message_frame, (22*gcw)+6,  (map_image_size*use_config[CONFIG_MAPHEIGHT])+6);
    gtk_paned_add2 (GTK_PANED (game_bar_vpane), message_frame);

    get_message_display(message_frame);

    gtk_widget_show (message_frame);

    /* Game frame */
    gameframe = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(gameframe), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (gameframe, (map_image_size*use_config[CONFIG_MAPWIDTH])+6, (map_image_size*use_config[CONFIG_MAPHEIGHT])+6);

    if (bigmap)
	gtk_paned_add2 (GTK_PANED (stat_game_vpane), gameframe);
    else
	gtk_paned_add1 (GTK_PANED (game_bar_vpane), gameframe);

    get_game_display (gameframe);

    gtk_widget_show (gameframe);

    /* stats frame */
    stat_frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(stat_frame), GTK_SHADOW_ETCHED_IN);
    if (bigmap)
	gtk_paned_add1 (GTK_PANED (game_bar_vpane), stat_frame);
    else
	gtk_paned_add1 (GTK_PANED (stat_game_vpane), stat_frame);

    get_stats_display (stat_frame);

    gtk_widget_show (stat_frame);

    gtk_widget_show (game_bar_vpane);
    gtk_widget_show (stat_game_vpane);

    inv_look_vpane = gtk_vpaned_new ();
    gtk_paned_add1 (GTK_PANED (inv_hpane), inv_look_vpane);

    /* inventory frame */
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, (24*gcw), (33*gch));
    gtk_paned_add1 (GTK_PANED (inv_look_vpane), frame);

    get_inv_display (frame);

    gtk_widget_show (frame);

    /* look frame */
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, (24*gcw), (12*gch));
    gtk_paned_add2 (GTK_PANED (inv_look_vpane), frame);

    get_look_display (frame);

    gtk_widget_show (frame);

    gtk_widget_show (inv_look_vpane);

    gtk_widget_show (stat_info_hpane);

    gtk_widget_show (inv_hpane);


    /* Connect signals */

    gtk_signal_connect_object (GTK_OBJECT (gtkwin_root), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_root));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_root), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_root));
    gtk_widget_show (gtkwin_root);

#ifdef HAVE_SDL
    if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL)
	init_SDL( drawingarea, 0);
#endif

  } else { /* split window mode */


  /* game window */

    gtkwin_root = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_root, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_root, 300, 160);
    gtk_widget_set_usize (gtkwin_root,(map_image_size*use_config[CONFIG_MAPWIDTH])+6,(map_image_size*use_config[CONFIG_MAPHEIGHT])+6);
    gtk_window_set_title (GTK_WINDOW (gtkwin_root), "Crossfire - view");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_root), TRUE, TRUE, FALSE);
    gtk_signal_connect_object(GTK_OBJECT(gtkwin_root), "destroy",GTK_SIGNAL_FUNC(main_window_destroyed), NULL);


    gtk_container_border_width (GTK_CONTAINER (gtkwin_root), 0);


    rootvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (gtkwin_root), rootvbox);

    gtk_widget_realize (rootvbox);

    gtk_widget_realize (gtkwin_root);



    gtk_widget_show (rootvbox);
    gtk_widget_show (gtkwin_root);
    gtk_widget_draw (gtkwin_root,NULL);
    gtk_widget_draw (rootvbox,NULL);

    get_game_display(rootvbox);



  /* Stats and menu window */
    gtkwin_stats = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_stats, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_stats, 300, 0);
    gtk_widget_set_usize (gtkwin_stats,(map_image_size*use_config[CONFIG_MAPWIDTH])+6,140);
    gtk_window_set_title (GTK_WINDOW (gtkwin_stats), "Crossfire GTK Client");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_stats), TRUE, TRUE, FALSE);
    gtk_signal_connect (GTK_OBJECT (gtkwin_stats), "destroy", GTK_SIGNAL_FUNC(main_window_destroyed), &gtkwin_stats);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_stats), 0);


    rootvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (gtkwin_stats), rootvbox);
    gtk_widget_show (rootvbox);

    get_menu_display(rootvbox);
    get_stats_display (rootvbox);
    gtk_widget_realize (gtkwin_stats);
    gdk_window_set_group (gtkwin_stats->window, gtkwin_root->window);
    gtk_widget_show (gtkwin_stats);


   /* info window - text and messages */
    gtkwin_info = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_info, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_info, 570, 0);
    gtk_widget_set_usize (gtkwin_info,400,600);
    gtk_window_set_title (GTK_WINDOW (gtkwin_info), "Crossfire - info");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_info), TRUE, TRUE, FALSE);
    gtk_signal_connect (GTK_OBJECT (gtkwin_info), "destroy", GTK_SIGNAL_FUNC(main_window_destroyed), &gtkwin_info);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_info), 0);

    /* Alloc colors - not entirely necessary, really, since GTK should do this */
    /* colorname[] comes from xutil.c */
    for (i=0; i<=12; i++ ) {
	if ( !gdk_color_parse(colorname[i], &root_color[i])) {
	    printf ("cparse failed (%s)\n",colorname[i]);
	}
	if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[i])) {
	    printf ("calloc failed\n");
	}
    }

    rootvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (gtkwin_info), rootvbox);
    gtk_widget_show (rootvbox);

    get_info_display(rootvbox);

    gtk_widget_show (gtkwin_info);
    gtk_widget_realize (gtkwin_info);
    gdk_window_set_group (gtkwin_info->window, gtkwin_root->window);

 /* statbars window */
    gtkwin_message = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_message, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_message, 300, 450);
    gtk_widget_set_usize (gtkwin_message,(map_image_size*use_config[CONFIG_MAPWIDTH])+6,170);
    gtk_window_set_title (GTK_WINDOW (gtkwin_message), "Crossfire - vitals");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_message), TRUE, TRUE, FALSE);
    gtk_signal_connect (GTK_OBJECT (gtkwin_message), "destroy", GTK_SIGNAL_FUNC(main_window_destroyed), &gtkwin_message);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_message), 0);


    rootvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (gtkwin_message), rootvbox);
    gtk_widget_show (rootvbox);

    get_message_display(rootvbox);

    gtk_widget_show (gtkwin_message);
    gtk_widget_realize (gtkwin_message);
    gdk_window_set_group (gtkwin_message->window, gtkwin_root->window);

 /* inventory window */
    gtkwin_inv = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_inv, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_inv, 0, 0);
    gtk_widget_set_usize (gtkwin_inv,290,400);
    gtk_window_set_title (GTK_WINDOW (gtkwin_inv), "Crossfire - inventory");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_inv), TRUE, TRUE, FALSE);
    gtk_signal_connect (GTK_OBJECT (gtkwin_inv), "destroy", GTK_SIGNAL_FUNC(main_window_destroyed), &gtkwin_inv);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_inv), 0);


    rootvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (gtkwin_inv), rootvbox);
    gtk_widget_show (rootvbox);

    get_inv_display(rootvbox);

    gtk_widget_show (gtkwin_inv);
    gtk_widget_realize (gtkwin_inv);
    gdk_window_set_group (gtkwin_inv->window, gtkwin_root->window);
 /* look window */
    gtkwin_look = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_look, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_look, 0, 420);
    gtk_widget_set_usize (gtkwin_look,290,150);
    gtk_window_set_title (GTK_WINDOW (gtkwin_look), "Crossfire - look");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_look), TRUE, TRUE, FALSE);
    gtk_signal_connect (GTK_OBJECT (gtkwin_look), "destroy", GTK_SIGNAL_FUNC(main_window_destroyed), &gtkwin_look);

    gtk_container_border_width (GTK_CONTAINER (gtkwin_look), 0);


    rootvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (gtkwin_look), rootvbox);
    gtk_widget_show (rootvbox);

    get_look_display(rootvbox);

    gtk_widget_show (gtkwin_look);
    gtk_widget_realize (gtkwin_look);
    gdk_window_set_group (gtkwin_look->window, gtkwin_root->window);
    /* Setup key events */

    gtk_signal_connect_object (GTK_OBJECT (gtkwin_message), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_message));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_message), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_message));

    gtk_signal_connect_object (GTK_OBJECT (gtkwin_root), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_root));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_root), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_root));

    gtk_signal_connect_object (GTK_OBJECT (gtkwin_info), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_info));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_info), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_info));

    gtk_signal_connect_object (GTK_OBJECT (gtkwin_look), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_look));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_look), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_look));

    gtk_signal_connect_object (GTK_OBJECT (gtkwin_inv), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_inv));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_inv), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_inv));

    gtk_signal_connect_object (GTK_OBJECT (gtkwin_stats), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_stats));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_stats), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_stats));

#ifdef HAVE_SDL
    if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL)
	init_SDL( drawingarea, 0);
#endif

  } /* else split windows */

  /* load window positions from file */
  set_window_pos();
  gtk_tooltips_set_delay(tooltips, 1000 );
  if (use_config[CONFIG_TOOLTIPS]) {
    gtk_tooltips_enable(tooltips);
  }

}

static int get_root_display(char *display_name,int gargc, char **gargv) {
    gtk_init (&gargc,&gargv);
    last_str=malloc(32767);

    if (want_config[CONFIG_SPLASH]) create_splash();
    /* we need to call gdk_rgb_init very early on, as some of the
     * create window functions may do call backs in which case we try
     * to draw the game window.
     */
    gdk_rgb_init();
    create_windows();

    return 0;
}


/* null procedures. gtk does this for us. */

/* TODO Make these commands specific to x11 toolkit. */
void set_scroll(const char *s)
{
}


void set_autorepeat(const char *s) /* ...and what does this one *do*, anyway? */
{
}


int get_info_width(void)
{
    /*
     * TODO Have crossfire-server send paragraphs rather than lines, so to
     * speak. Except for ASCII maps and things. Then this can go away
     * completely.
     */
    return 40;
}



/***********************************************************************
 *
 * Here is the start of event handling functions
 *
 ***********************************************************************/



/* This function handles the reading of the X Events and then
 * doing the appropriate action.  For most input events, it is calling
 * another function.
 *
 * It can also call display functions to make sure the information is
 * correct - in this way, updates will not be done so often (like
 * for every ITEM command received), but less frequently but still
 * update the display fully.  All the functions above are optimized to
 * only draw stuff that needs drawing.  So calling them a lot is not
 * going to draw stuff too much.
 */

void do_clearlock(void) {
}

void x_set_echo(void) {
  gtk_entry_set_visibility(GTK_ENTRY(entrytext), !cpl.no_echo);
}

void draw_info_windows(void)
    {
    if (draw_info_freeze1) {
        gtk_text_thaw (GTK_TEXT (gtkwin_info_text));
        gtk_adjustment_set_value(GTK_ADJUSTMENT(text_vadj), GTK_ADJUSTMENT(text_vadj)->upper-GTK_ADJUSTMENT(text_vadj)->page_size);
        gtk_text_set_adjustments(GTK_TEXT (gtkwin_info_text),GTK_ADJUSTMENT(text_hadj),GTK_ADJUSTMENT(text_vadj));
        draw_info_freeze1=FALSE;
    }
    if (draw_info_freeze2) {
        gtk_text_thaw (GTK_TEXT (gtkwin_info_text2));
        gtk_adjustment_set_value(GTK_ADJUSTMENT(text_vadj2), GTK_ADJUSTMENT(text_vadj2)->upper-GTK_ADJUSTMENT(text_vadj2)->page_size);
        gtk_text_set_adjustments(GTK_TEXT (gtkwin_info_text2),GTK_ADJUSTMENT(text_hadj2),GTK_ADJUSTMENT(text_vadj2));
        draw_info_freeze2=FALSE;
    }
    }


/* X11 client doesn't care about this */
void client_tick(uint32 tick)
{
    inventory_tick();
    mapdata_animation();
}

/**
 * We get pickup information from server, update our status.
 */
void client_pickup(uint32 pickup)
{
    int menu;

    /* Update value, so handling function won't resend info. */
    pickup_mode = pickup;

    for (menu = 0; menu < pickup_count; menu++)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(pickup_menus[menu]), (pickup & pickup_value[menu]) ? 1 : 0);
}

int do_timeout(void) {

    updatelock=0;

    if (!tick) {
	inventory_tick();
	mapdata_animation();
    }
    update_spell_list(0);
    draw_info_windows();
    if (redraw_needed) {
	display_map_doneupdate(TRUE, FALSE);
	redraw_needed=FALSE;
    }
    if (cpl.showmagic) magic_map_flash_pos();
    return TRUE;
}

int gtk_checkchilds(void) {
    monitorChilds();
    return FALSE;
}



/* Here are the old Xutil commands needed. */
/* ----------------------------------------------------------------------------*/


/* This function draws the magic map in the game window.  I guess if
 * we wanted to get clever, we could open up some other window or
 * something.
 *
 * A lot of this code was taken from server/xio.c  But being all
 * the map data has been figured, it tends to be much simpler.
 */
void draw_magic_map(void)
{

  int x=0;
  int y=0;

  GtkWidget *hbox;
  GtkWidget *closebutton;
  GtkStyle *style;

  static GtkWidget *magicgtkpixmap;


  static GdkBitmap *magicgdkmask;


  if (!cpl.magicmap) {
    draw_info ("You have yet to cast magic map.",NDI_BLACK);
    return;
  }

  if(!gtkwin_magicmap) {

    gtkwin_magicmap = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_position (GTK_WINDOW (gtkwin_magicmap), GTK_WIN_POS_CENTER);
    gtk_widget_set_usize (gtkwin_magicmap,264,300);
    gtk_window_set_title (GTK_WINDOW (gtkwin_magicmap), "Magic map");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_magicmap), FALSE, FALSE, FALSE);

    gtk_signal_connect (GTK_OBJECT (gtkwin_magicmap), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_magicmap);

    mapvbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_set_usize (mapvbox,264,300);
    gtk_container_add (GTK_CONTAINER(gtkwin_magicmap),mapvbox);

    style = gtk_widget_get_style(gtkwin_magicmap);
    gtk_widget_realize(mapvbox);


    magicgdkpixmap = gdk_pixmap_new(gtkwin_magicmap->window,
				    264,
				    264,
				    -1);
    magicgtkpixmap= gtk_pixmap_new (magicgdkpixmap, magicgdkmask);
    gtk_box_pack_start (GTK_BOX (mapvbox),magicgtkpixmap, FALSE, FALSE, 0);
    gtk_widget_show (magicgtkpixmap);

    hbox = gtk_hbox_new(FALSE, 2);

    closebutton = gtk_button_new_with_label ("Close");
    gtk_signal_connect_object (GTK_OBJECT (closebutton), "clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT (gtkwin_magicmap));
    gtk_box_pack_start (GTK_BOX (hbox), closebutton, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (mapvbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (closebutton);
    gtk_widget_show (hbox);

    gtk_widget_show (mapvbox);
    gtk_widget_show (gtkwin_magicmap);


    gdk_color_parse("Black", &map_color[0]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[0]);
    gdk_color_parse("White", &map_color[1]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[1]);
    gdk_color_parse("Navy", &map_color[2]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[2]);
    gdk_color_parse("Red", &map_color[3]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[3]);
    gdk_color_parse("Orange", &map_color[4]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[4]);
    gdk_color_parse("DodgerBlue", &map_color[5]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[5]);
    gdk_color_parse("DarkOrange2", &map_color[6]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[6]);
    gdk_color_parse("SeaGreen", &map_color[7]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[7]);
    gdk_color_parse("DarkSeaGreen", &map_color[8]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[8]);
    gdk_color_parse("Grey50", &map_color[9]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[9]);
    gdk_color_parse("Sienna", &map_color[10]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[10]);
    gdk_color_parse("Gold", &map_color[11]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[11]);
    gdk_color_parse("Khaki", &map_color[12]);
    gdk_color_alloc (gtk_widget_get_colormap (magicgtkpixmap), &map_color[12]);


    magic_map_gc = gdk_gc_new (magicgdkpixmap);


   gdk_gc_set_foreground (magic_map_gc, &map_color[0]);
   gdk_draw_rectangle (magicgdkpixmap, magic_map_gc,
		       TRUE,
		       0,
		       0,
		       264,
		       264);
    cpl.mapxres = (262)/cpl.mmapx;
    cpl.mapyres = (262)/cpl.mmapy;
    if (cpl.mapxres < 1 || cpl.mapyres<1) {
      LOG(LOG_WARNING,"gtk::draw_magic_map","magic map resolution less than 1, map is %dx%d",
	      cpl.mmapx, cpl.mmapy);
      return;
    }
    /* In theory, cpl.mapxres and cpl.mapyres do not have to be the same.  However,
     * it probably makes sense to keep them the same value.
     * Need to take the smaller value.
     */
    if (cpl.mapxres>cpl.mapyres) cpl.mapxres=cpl.mapyres;
    else cpl.mapyres=cpl.mapxres;

    if (cpl.mapxres>24) {
      cpl.mapxres=24;
      cpl.mapyres=24;
    }
    /* this is keeping the same unpacking scheme that the server uses
     * to pack it up.
     */
    for (y = 0; y < cpl.mmapy; y++) {
      for (x = 0; x < cpl.mmapx; x++) {
	uint8 val = cpl.magicmap[y*cpl.mmapx + x];

    gdk_gc_set_foreground (magic_map_gc, &map_color[val&FACE_COLOR_MASK]);

	gdk_draw_rectangle (magicgdkpixmap, magic_map_gc,
			    TRUE,
			    2+cpl.mapxres*x,
			    2+cpl.mapyres*y,
			    cpl.mapxres,
			    cpl.mapyres);
      } /* Saw into this space */
    }
    /*    gdk_gc_destroy (magic_map_gc);*/
    gtk_widget_draw (mapvbox,NULL);
  }

  else {
    /* ------------------ There is already a magic map up - replace it ---------*/

    gdk_window_raise (gtkwin_magicmap->window);
    /* --------------------------- */

   gdk_gc_set_foreground (magic_map_gc, &map_color[0]);
   gdk_draw_rectangle (magicgdkpixmap, magic_map_gc,
		       TRUE,
		       0,
		       0,
		       264,
		       264);

    cpl.mapxres = (262)/cpl.mmapx;
    cpl.mapyres = (262)/cpl.mmapy;
    if (cpl.mapxres < 1 || cpl.mapyres<1) {
      LOG(LOG_WARNING,"gtk::draw_magic_map","magic map resolution less than 1, map is %dx%d\n",
	      cpl.mmapx, cpl.mmapy);
      return;
    }

    if (cpl.mapxres>cpl.mapyres) cpl.mapxres=cpl.mapyres;
    else cpl.mapyres=cpl.mapxres;



    if (cpl.mapxres>24) {
      cpl.mapxres=24;
      cpl.mapyres=24;
    }



    for (y = 0; y < cpl.mmapy; y++) {
      for (x = 0; x < cpl.mmapx; x++) {
	uint8 val = cpl.magicmap[y*cpl.mmapx + x];

    gdk_gc_set_foreground (magic_map_gc, &map_color[val&FACE_COLOR_MASK]);

	gdk_draw_rectangle (magicgdkpixmap, magic_map_gc,
			    TRUE,
			    2+cpl.mapxres*x,
			    2+cpl.mapyres*y,
			    cpl.mapxres,
			    cpl.mapyres);

      }

    }
 gtk_widget_draw (mapvbox,NULL);
  }
}

/* Basically, this just flashes the player position on the magic map */

void magic_map_flash_pos()
{
  if (!cpl.showmagic) return;
  if (!gtkwin_magicmap) return;
  cpl.showmagic ^=2;
  if (cpl.showmagic & 2) {
    gdk_gc_set_foreground (magic_map_gc, &map_color[0]);
  } else {
    gdk_gc_set_foreground (magic_map_gc, &map_color[1]);
  }
  gdk_draw_rectangle (magicgdkpixmap, magic_map_gc,
		      TRUE,
		      2+cpl.mapxres*cpl.pmapx,
		      2+cpl.mapyres*cpl.pmapy,
		      cpl.mapxres,
		      cpl.mapyres);
  gtk_widget_draw (mapvbox,NULL);
}

/* Gets a specified windows coordinates.  This function is pretty much
 * an exact copy out of the server.
 */

void get_window_coord(GtkWidget *win,
                 int *x,int *y,
                 int *wx,int *wy,
                 int *w,int *h)
{
  int tmp;
  gdk_window_get_geometry (win->window, x, y, w, h, &tmp);
/*  gdk_window_get_root_origin (win->window, wx, wy); */
/*  gdk_window_get_deskrelative_origin (win->window, wx, wy); */
  gdk_window_get_origin (win->window, wx, wy);
  *wx -= *x;
  *wy -= *y;
}


void save_winpos(void)
{
    char savename[MAX_BUF],buf[MAX_BUF];
    FILE    *fp;
    int	    x,y,w,h,wx,wy;

    if (!want_config[CONFIG_SPLITWIN])
	sprintf(savename,"%s/.crossfire/gwinpos", getenv("HOME"));
    else
	sprintf(savename,"%s/.crossfire/winpos", getenv("HOME"));

    if (!(fp=fopen(savename,"w"))) {
	sprintf(buf,"Unable to open %s, window positions not saved",savename);
	draw_info(buf,NDI_BLUE);
	return;
    }

    get_window_coord(gtkwin_root, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_game: %d %d %d %d\n", wx,wy, w, h);
    if (want_config[CONFIG_SPLITWIN]) {
	get_window_coord(gtkwin_stats, &x,&y, &wx,&wy,&w,&h);
	fprintf(fp,"win_stats: %d %d %d %d\n", wx,wy, w, h);
	get_window_coord(gtkwin_info, &x,&y, &wx,&wy,&w,&h);
	fprintf(fp,"win_info: %d %d %d %d\n", wx,wy, w, h);
	get_window_coord(gtkwin_inv, &x,&y, &wx,&wy,&w,&h);
	fprintf(fp,"win_inv: %d %d %d %d\n", wx,wy, w, h);
	get_window_coord(gtkwin_look, &x,&y, &wx,&wy,&w,&h);
	fprintf(fp,"win_look: %d %d %d %d\n", wx,wy, w, h);
	get_window_coord(gtkwin_message, &x,&y, &wx,&wy,&w,&h);
	fprintf(fp,"win_message: %d %d %d %d\n", wx,wy, w, h);
    } else {
	/* in non split mode, we really want the position of the
	    * various panes.  Current versions do not have a proper
	 * way (ie, function call/macro) to do this - future
	 * versions of gtk will have a gtk_paned_get_position.
	 * That code basically does the same thing as what we
	 * are doing below (version 1.3)
	 */
	fprintf(fp,"inv_hpane: %d\n",
		GTK_PANED(inv_hpane)->child1_size);
	fprintf(fp,"stat_info_hpane: %d\n",
		GTK_PANED(stat_info_hpane)->child1_size);
	fprintf(fp,"stat_game_vpane: %d\n",
		GTK_PANED(stat_game_vpane)->child1_size);
	fprintf(fp,"game_bar_vpane: %d\n",
		GTK_PANED(game_bar_vpane)->child1_size);
	fprintf(fp,"inv_look_vpane: %d\n",
		GTK_PANED(inv_look_vpane)->child1_size);
	if (use_config[CONFIG_SPLITINFO])
	    fprintf(fp,"info_vpane: %d\n",
		GTK_PANED(info_vpane)->child1_size);
    }
    fclose(fp);
    sprintf(buf,"Window positions saved to %s",savename);
    draw_info(buf,NDI_BLUE);
}


/* Reads in the winpos file created by the above function and sets the
 * the window positions appropriately.
 */
void set_window_pos(void)
{
    gint wx=0;
    gint wy=0;
    gint w=0;
    gint h=0;

    char buf[MAX_BUF],*cp;
    FILE *fp;

    if (want_config[CONFIG_SPLITWIN])
	sprintf(buf,"%s/.crossfire/winpos", getenv("HOME"));
    else
	sprintf(buf,"%s/.crossfire/gwinpos", getenv("HOME"));

    if (!(fp=fopen(buf,"r"))) return;

    while(fgets(buf,MAX_BUF-1, fp)!=NULL) {
	buf[MAX_BUF-1]='\0';
	if (!(cp=strchr(buf,' '))) continue;
	*cp++='\0';
	if (sscanf(cp,"%d %d %d %d",&wx,&wy,&w,&h)!=4) {
	    gint pos = atoi(cp);
	    if (pos == 0) continue;

	    if (!strcmp(buf, "inv_hpane:")) gtk_paned_set_position(GTK_PANED(inv_hpane),pos);
	    else if (!strcmp(buf, "stat_info_hpane:")) gtk_paned_set_position(GTK_PANED(stat_info_hpane),pos);
	    else if (!strcmp(buf, "stat_game_vpane:")) gtk_paned_set_position(GTK_PANED(stat_game_vpane),pos);
	    else if (!strcmp(buf, "game_bar_vpane:")) gtk_paned_set_position(GTK_PANED(game_bar_vpane),pos);
	    else if (!strcmp(buf, "inv_look_vpane:")) gtk_paned_set_position(GTK_PANED(inv_look_vpane),pos);
	    else if (use_config[CONFIG_SPLITINFO] && !strcmp(buf, "info_vpane:")) gtk_paned_set_position(GTK_PANED(info_vpane),pos);
	    else LOG(LOG_ERROR,"gtk::set_window_pos","Found bogus line in window position file:\n/%s/ /%s/", buf, cp);
	} else {
	    if (!strcmp(buf,"win_game:")) {
                gdk_window_move_resize(gtkwin_root->window, wx, wy, w, h);
		continue;
	    }
	    if (!want_config[CONFIG_SPLITWIN]) {
		LOG(LOG_ERROR,"gtk::set_window_pos","Found bogus line in window position file:\n%s %s", buf, cp);
		continue;
	    }
	    if (!strcmp(buf,"win_stats:")) {
                gdk_window_move_resize(gtkwin_stats->window, wx, wy, w, h);
	    }
	    if (!strcmp(buf,"win_info:")) {
                gdk_window_move_resize(gtkwin_info->window, wx, wy, w, h);
	    }
	    if (!strcmp(buf,"win_inv:")) {
                gdk_window_move_resize(gtkwin_inv->window, wx, wy, w, h);
	    }
	    if (!strcmp(buf,"win_look:")) {
                gdk_window_move_resize(gtkwin_look->window, wx, wy, w, h);
	    }
	    if (!strcmp(buf,"win_message:")) {
               gdk_window_move_resize(gtkwin_message->window, wx, wy, w, h);
	    }
	} /* else if split windows */
    } /* while fgets */
    fclose(fp);
}




/***********************************************************************
 *
 * Here starts the X11 init functions.  It will call other
 * functions that were grouped previously by window
 *
 ***********************************************************************/

/* Usage routine.  All clients should support server, port and
 * display options, with -pix and -xpm also suggested.  -split
 * does not need to be supported - it is in this copy because
 * the old code supported it.
 */

static void usage(const char *progname)
{
    puts("Usage of gcfclient:\n\n");
    puts("-cache           - Cache images for future use.");
    puts("-nocache         - Do not cache images (default action).");
    puts("-darkness        - Enables darkness code (default)");
    puts("-nodarkness      - Disables darkness code");
    puts("-display <name>  - Use <name> instead if DISPLAY environment variable.");
    puts("-download_all_faces - Download all needed faces before play starts");
    puts("-echo            - Echo the bound commands");
    puts("-noecho          - Do not echo the bound commands (default)");
    puts("-faceset <name>  - Use faceset <name> if available");
    puts("-fasttcpsend     - Send data immediately to server, may increase bandwidth");
    puts("-nofasttcpsend   - Disables fasttcpsend");
    puts("-fog             - Enable fog of war code");
    puts("-help            - Display this message.");
    puts("-loglevel <val>  - Set default logging level (0 is most verbose)");
    puts("-iconscale %%    - Set icon scale percentage");
    puts("-mapscale %%     - Set map scale percentage");
    puts("-mapsize xXy     - Set the mapsize to be X by Y spaces. (default 11x11)");
    puts("-splash          - Display the splash screen (default)");
    puts("-nosplash        - Don't display the splash screen (startup logo)");
    puts("-popups          - Use pop up windows for input (default)");
    puts("-nopopups        - Don't use pop up windows for input");
    puts("-port <number>   - Use port <number> instead of the standard port number");
    puts("-sdl             - Use sdl for drawing png (may not work on all hardware");
    puts("-server <name>   - Connect to <name> instead of localhost.");
    puts("-showicon        - Print status icons in inventory window");
    puts("-smooth          - Enable smooth");
    puts("-nosmooth        - Disable smooth");
    puts("-mapscroll       - Enable mapscrolling by bitmap operations");
    puts("-nomapscroll     - Disable mapscrolling by bitmap operations");
    puts("-sound           - Enable sound output (default).");
    puts("-nosound         - Disable sound output.");
    puts("-sound_server <path> - Executable to use to play sounds.");
    puts("-resists <val>   - Control look of resistances.");
    puts("-split           - Use split windows.");
    puts("-splitinfo       - Use two information windows, segregated by information type.");
    puts("-timemapredraw   - Print out timing information for map generation");
    puts("-triminfowindow  - Trims size of information window(s)");
    puts("-notriminfowindow  - Do not trims size of information window(s) (default)");
    puts("-updatekeycodes  - Update the saved bindings for this keyboard.");

    exit(0);
}

/* init_windows:  This initiliazes all the windows - it is an
 * interface routine.  The command line arguments are passed to
 * this function to interpert.  Note that it is not in fact
 * required to parse anything, but doing at least -server and
 * -port would be a good idea.
 *
 * This function returns 0 on success, nonzero on failure.
 */

int init_windows(int argc, char **argv)
{
    int on_arg=1;
    char *display_name="";
    load_defaults();

#ifndef WIN32
    strcpy(VERSION_INFO,"GTK Unix Client " FULL_VERSION);
#else
    strcpy(VERSION_INFO,"GTK Win32 Client " FULL_VERSION);
#endif
    /* Set this global so we get skill experience - gtk client can display
     * it, so lets get the info.
     */
    want_skill_exp=1;
    for (on_arg=1; on_arg<argc; on_arg++) {
	if (!strcmp(argv[on_arg],"-cache")) {
	    want_config[CONFIG_CACHE]= TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nocache")) {
	    want_config[CONFIG_CACHE]= FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-darkness")) {
	    want_config[CONFIG_DARKNESS]= TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nodarkness")) {
	    want_config[CONFIG_DARKNESS]= FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-display")) {
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-display requires a display name");
		return 1;
	    }
	    display_name = argv[on_arg];
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-download_all_faces")) {
	    want_config[CONFIG_DOWNLOAD]= TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-echo")) {
	    want_config[CONFIG_ECHO]= TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-noecho")) {
	    want_config[CONFIG_ECHO]= FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-faceset")) {
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-faceset requires a faceset name/number");
		return 1;
	    }
	    face_info.want_faceset = argv[on_arg];
	    continue;
	}
	else if( !strcmp( argv[on_arg],"-fog")) {
	    want_config[CONFIG_FOGWAR]= TRUE;
	    continue;
	}
	else if( !strcmp( argv[on_arg],"-nofog")) {
	    want_config[CONFIG_FOGWAR]= FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-help")) {
	    usage(argv[0]);
	    continue;
	}
	else if( !strcmp( argv[on_arg],"-iconscale")) {
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-iconscale requires a percentage value");
		return 1;
	    }
	    want_config[CONFIG_ICONSCALE] = atoi(argv[on_arg]);
	    if (want_config[CONFIG_ICONSCALE] < 25 || want_config[CONFIG_ICONSCALE]>200) {
		LOG(LOG_WARNING,"gtk::init_windows","Valid range for -iconscale is 25 through 200");
		want_config[CONFIG_ICONSCALE]=100;
		return 1;
	    }
	    continue;
	}
	else if( !strcmp( argv[on_arg],"-mapscale")) {
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-mapscale requires a percentage value");
		return 1;
	    }
	    want_config[CONFIG_MAPSCALE] = atoi(argv[on_arg]);
	    if (want_config[CONFIG_MAPSCALE] < 25 || want_config[CONFIG_MAPSCALE]>200) {
		LOG(LOG_WARNING,"gtk::init_windows","Valid range for -mapscale is 25 through 200");
		want_config[CONFIG_MAPSCALE]=100;
		return 1;
	    }
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-mapsize")) {
	    char *cp, x, y=0;
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-mapsize requires a XxY value");
		return 1;
	    }
	    x = atoi(argv[on_arg]);
	    for (cp = argv[on_arg]; *cp!='\0'; cp++)
		if (*cp == 'x' || *cp == 'X') break;

	    if (*cp==0) {
		LOG(LOG_WARNING,"gtk::init_windows","-mapsize requires both and X and Y value (ie, XxY - note the\nx in between.");
	    } else {
		y = atoi(cp+1);
	    }
	    if (x<9 || y<9) {
		LOG(LOG_WARNING,"gtk::init_windows","map size must be positive values of at least 9");
	    } else if (x>MAP_MAX_SIZE || y>MAP_MAX_SIZE) {
		LOG(LOG_WARNING,"gtk::init_windows","Map size can not be larger than %d x %d", MAP_MAX_SIZE, MAP_MAX_SIZE);

	    } else {
		want_config[CONFIG_MAPWIDTH]=x;
		want_config[CONFIG_MAPHEIGHT]=y;
	    }
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-fasttcpsend")) {
	    want_config[CONFIG_FASTTCP] = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nofasttcpsend")) {
	    want_config[CONFIG_FASTTCP] = FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-popups")) {
	    want_config[CONFIG_POPUPS] = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nopopups")) {
	    want_config[CONFIG_POPUPS] = FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-port")) {
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-port requires a port number");
		return 1;
	    }
	    want_config[CONFIG_PORT] = atoi(argv[on_arg]);
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-sdl")) {
#ifndef HAVE_SDL
	    LOG(LOG_WARNING,"gtk::init_windows","client not compiled with sdl support.  Ignoring -sdl");
#else
	    want_config[CONFIG_DISPLAYMODE] = CFG_DM_SDL;
#endif
	    continue;
	}
	else if (!strcmp(argv[on_arg],"+sdl")) {
	    want_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-server")) {
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-server requires a host name");
		return 1;
	    }
	    server = argv[on_arg];
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-showicon")) {
	    want_config[CONFIG_SHOWICON] = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-smooth")) {
	    want_config[CONFIG_SMOOTH] = TRUE;
	}
	else if (!strcmp(argv[on_arg],"-nosmooth")) {
	    want_config[CONFIG_SMOOTH] = FALSE;
	}
	else if (!strcmp(argv[on_arg],"-mapscroll")) {
	    want_config[CONFIG_MAPSCROLL] = TRUE;
	}
	else if (!strcmp(argv[on_arg],"-nomapscroll")) {
	    want_config[CONFIG_MAPSCROLL] = FALSE;
	}
	else if (!strcmp(argv[on_arg],"-sound")) {
	    want_config[CONFIG_SOUND] = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nosound")) {
	    want_config[CONFIG_SOUND] = FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-sound_server")) {
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-sound_server requires an executable pathname");
		return 1;
	    }
	    sound_server = argv[on_arg];
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-split")) {
	    want_config[CONFIG_SPLITWIN]=TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nosplit")) {
	    want_config[CONFIG_SPLITWIN]=FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-resists")) {
	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-resists requires a value");
		return 1;
	    }
	    want_config[CONFIG_RESISTS]=atoi(argv[on_arg]);
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-loglevel")) {
	    extern int MINLOG;

	    if (++on_arg == argc) {
		LOG(LOG_WARNING,"gtk::init_windows","-loglevel requires a value");
		return 1;
	    }
	    MINLOG = atoi(argv[on_arg]);
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-splitinfo")) {
	    want_config[CONFIG_SPLITINFO]=TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-timemapredraw")) {
	    time_map_redraw=TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-triminfowindow")) {
	    want_config[CONFIG_TRIMINFO] = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-notriminfowindow")) {
	    want_config[CONFIG_TRIMINFO] = FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-updatekeycodes")) {
	    updatekeycodes=TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-splash")) {
	    want_config[CONFIG_SPLASH] = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nosplash")) {
	    want_config[CONFIG_SPLASH] = FALSE;
	    continue;
	}
	else {
	    LOG(LOG_WARNING,"gtk::init_windows","Do not understand option %s", argv[on_arg]);
	    usage(argv[0]);
	    return 1;
	}
    }

    LOG(LOG_INFO,"Client Version",VERSION_INFO);

    /* Now copy over the values just loaded */
    for (on_arg=0; on_arg<CONFIG_NUMS; on_arg++) {
        use_config[on_arg] = want_config[on_arg];
    }

    image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_ICONSCALE] / 100;
    map_image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 100;
    map_image_half_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 200;
    itemlist_set_show_icon(&inv_list, use_config[CONFIG_SHOWICON]);
    if (!use_config[CONFIG_CACHE]) use_config[CONFIG_DOWNLOAD] = FALSE;

    mapdata_init();

    /* Finished parsing all the command line options.  Now start
     * working on the display.
     */
    gargc=argc;
    gargv=argv;

    for (on_arg = 0; on_arg<MAX_HISTORY; on_arg++)
	history[on_arg][0]=0;


    if (get_root_display(display_name,gargc,gargv))
		return 1;

    init_keys();
    init_cache_data();
    if (want_config[CONFIG_SPLASH]) destroy_splash();
    gtk_timeout_add (10,(GtkFunction)gtk_checkchilds,NULL);
    return 0;
}


/** Do the map drawing
 *
 * If redraw is set, force redraw of all tiles.
 *
 * If notice is set, another call will follow soon.
 */
void display_map_doneupdate(int redraw, int notice)
{
    if (notice)
	return;

    if (updatelock < 30) {
	updatelock++;

#ifdef HAVE_SDL
	if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL) sdl_gen_map(redraw);
	else
#endif
	gtk_draw_map(redraw);
    } /* if updatelock */
    else {
	redraw_needed = TRUE;
    }
}

void display_map_newmap(void)
{
    reset_map();
}

void resize_map_window(int x, int y)
{
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawingarea), map_image_size * x, map_image_size * y);
    if (!want_config[CONFIG_SPLITWIN]) {
#if 0
	/* 15 it is a purely arbitary value.  But basically, if the map window is
	 * narrow, we then will have the stats on top, with message down below
	 * the map window.  IF the map window is wide, we put these side
	 * by side at the top
	 */
	if (bigmap && x<15) {

	    /* reverse of below basically. */
	    GtkWidget	*newpane;   /* will take place of game_bar_vpane */

	    bigmap =FALSE;

	    newpane = gtk_vpaned_new(); /*game in pane1, bars in pane2 */

	    /* Remove referance to the split - the stats frame takes its place */
	    gtk_widget_ref(game_bar_vpane);
	    gtk_container_remove(GTK_CONTAINER(stat_game_vpane), game_bar_vpane);

	    /* Stat now gets the entire top pane to itself */
	    gtk_widget_ref(stat_frame);
	    gtk_container_remove(GTK_CONTAINER(game_bar_vpane), stat_frame);
	    gtk_paned_add1(GTK_PANED(stat_game_vpane), stat_frame);
	    gtk_widget_unref(stat_frame);

	    /* statbars now second part of bottom frame */
	    gtk_widget_ref(message_frame);
	    gtk_container_remove(GTK_CONTAINER(game_bar_vpane), message_frame);
	    gtk_paned_add2(GTK_PANED(newpane), message_frame);
	    gtk_widget_unref(message_frame);

	    /* game now first part of bottom frame */
	    gtk_widget_ref(gameframe);
	    gtk_container_remove(GTK_CONTAINER(stat_game_vpane), gameframe);
	    gtk_paned_add1(GTK_PANED(newpane), gameframe);
	    gtk_widget_unref(gameframe);

	    gtk_paned_add2(GTK_PANED(stat_game_vpane), newpane);

	    gtk_widget_show(newpane);
	    /* This should also destroy it */
	    gtk_widget_unref(game_bar_vpane);
	    game_bar_vpane = newpane;

	} else if (!bigmap && x>=15) {

	    GtkWidget	*newpane;   /* will take place of game_bar_vpane */
	    bigmap=TRUE;
	    newpane = gtk_hpaned_new();

	    /* We need to remove this here - the game pane is goind to
	     * take game_bar_vpane as second position in the stat_game_vpane.
	     * add a refcount so it isn't destroyed.
	     */
	    gtk_widget_ref(game_bar_vpane);
	    gtk_container_remove(GTK_CONTAINER(stat_game_vpane), game_bar_vpane);


	    /* Stat and message are now split on 'newpane' */
	    gtk_widget_ref(stat_frame);
	    gtk_container_remove(GTK_CONTAINER(stat_game_vpane), stat_frame);
	    gtk_paned_add1(GTK_PANED(newpane), stat_frame);
	    gtk_widget_unref(stat_frame);

	    gtk_widget_ref(message_frame);
	    gtk_container_remove(GTK_CONTAINER(game_bar_vpane), message_frame);
	    gtk_paned_add2(GTK_PANED(newpane), message_frame);
	    gtk_widget_unref(message_frame);

	    /* the game is now part 2 of stat_game_vpane, and not part of
	     * game_bar_vpane
	     */

	    gtk_widget_ref(gameframe);
	    gtk_container_remove(GTK_CONTAINER(game_bar_vpane), gameframe);
	    gtk_paned_add2(GTK_PANED(stat_game_vpane), gameframe);
	    gtk_widget_unref(gameframe);

	    /* Newpane (split stat/message) is now part one of stat_game_vpane */
	    gtk_paned_add1(GTK_PANED(stat_game_vpane), newpane);

	    gtk_widget_show(newpane);
	    /* This should also destroy it */
	    gtk_widget_unref(game_bar_vpane);
	    game_bar_vpane = newpane;
	}
#endif
	gtk_widget_set_usize (gameframe, (map_image_size*use_config[CONFIG_MAPWIDTH])+6, (map_image_size*use_config[CONFIG_MAPHEIGHT])+6);
    } else {
      gtk_widget_set_usize (gtkwin_root,(map_image_size*use_config[CONFIG_MAPWIDTH])+6,(map_image_size*use_config[CONFIG_MAPHEIGHT])+6);
    }

#ifdef HAVE_SDL
    if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL)
	init_SDL( drawingarea, FALSE);
#endif

}


void display_map_startupdate(void)
{
}

char *get_metaserver(void)
{
    cpl.input_state = Metaserver_Select;


    while(cpl.input_state==Metaserver_Select) {
        /*
         * This gtk_main will be quit inside of event_callback
         * when the user enters data into the input_text box
         * at which point the input_state will change.
         */
        gtk_main();
	usleep(10*1000);    /* 10 milliseconds */
    }
    return cpl.input_text;
}
void gtkLogListener (LogEntry *le){
    if (bugtrack)
        gtk_text_insert (GTK_TEXT (bugtrack), NULL, &bugtrack->style->black,NULL, getLogText(le), -1);
}
#define MAX_RECURSE 50
/* a handler for the glib error logging. Used so we care ourself of the gtk/gdk warnings
 * and make them appear in message window using the logging facility
 */
void gLogHandler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data){
    static char LogOrigin[4096];
    static int recurse = 0;
    LogLevel level;
    gboolean in_recursion;
    gboolean is_fatal;
    /*printf ("hi i received a g error on %s with severity %d and message %s\n",log_domain,log_level,message);*/
    if (recurse > MAX_RECURSE)
        return; /*don't Log*/
    if (recurse == MAX_RECURSE){
        recurse++;
        LOG(LOG_ERROR,"gtk::gLogHandler","Too many recurse, reached limit: %d",recurse);
        recurse--;
        return;
    }
    LogOrigin[0]='\0';
    strcat (LogOrigin,"Library::");
    in_recursion = (log_level & G_LOG_FLAG_RECURSION) != 0;
    is_fatal = (log_level & G_LOG_FLAG_FATAL) != 0;
    log_level &= G_LOG_LEVEL_MASK;

    if (!message)
        message = "gLogHandler: (NULL) message";
    if (log_domain){
        strcat(LogOrigin,log_domain);
        strcat(LogOrigin,"-");
    }else
        strcat(LogOrigin, "** ");


    switch (log_level)
    {
    case G_LOG_LEVEL_ERROR:/*Our critical*/
        strcat (LogOrigin,"ERROR");
        level=LOG_CRITICAL;
        break;
    case G_LOG_LEVEL_CRITICAL:/*our error*/
        strcat (LogOrigin,"CRITICAL");
        level=LOG_ERROR;
        break;
    case G_LOG_LEVEL_WARNING:/*our warning*/
        strcat (LogOrigin,"WARNING");
        level=LOG_WARNING;
        break;
    case G_LOG_LEVEL_MESSAGE:/* message */
        strcat (LogOrigin,"Message");
        level=LOG_INFO;
        break;
    case G_LOG_LEVEL_INFO:/* message */
        strcat (LogOrigin,"Info");
        level=LOG_INFO;
        break;
    case G_LOG_LEVEL_DEBUG:/*our debug*/
        strcat (LogOrigin,"DEBUG");
        level=LOG_DEBUG;
        break;
    default:
        strcat (LogOrigin,"LOG");
        level=LOG_WARNING;
      break;
    }
    if (in_recursion)
        strcat(LogOrigin," (recursed)");
    /*else
        strcat(LogOrigin,"**: ");*/
    recurse++;
    LOG(level,LogOrigin,"%s",message);
    if (is_fatal)
            LOG(level,LogOrigin,"Last Message was fatal, aborting...");
    recurse--;
}


extern char* cached_server_file;

int main(int argc, char *argv[])
{
    int got_one=0;
    int i;
    static char file_cache[ MAX_BUF ];

    g_log_set_handler (NULL,G_LOG_FLAG_RECURSION|G_LOG_FLAG_FATAL|G_LOG_LEVEL_ERROR|
            G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING |G_LOG_LEVEL_MESSAGE|G_LOG_LEVEL_INFO|
            G_LOG_LEVEL_DEBUG,(GLogFunc)gLogHandler,NULL);
    g_log_set_handler ("Gtk",G_LOG_FLAG_RECURSION|G_LOG_FLAG_FATAL|G_LOG_LEVEL_ERROR|
            G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING |G_LOG_LEVEL_MESSAGE|G_LOG_LEVEL_INFO|
            G_LOG_LEVEL_DEBUG,(GLogFunc)gLogHandler,NULL);
    g_log_set_handler ("Gdk",G_LOG_FLAG_RECURSION|G_LOG_FLAG_FATAL|G_LOG_LEVEL_ERROR|
            G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING |G_LOG_LEVEL_MESSAGE|G_LOG_LEVEL_INFO|
            G_LOG_LEVEL_DEBUG,(GLogFunc)gLogHandler,NULL);

    /* This needs to be done first.  In addition to being quite quick,
     * it also sets up some paths (client_libdir) that are needed by
     * the other functions.
     */
    init_client_vars();
    snprintf( file_cache, MAX_BUF, "%s/.crossfire/servers.cache", getenv( "HOME" ) );
    cached_server_file = file_cache;
    init_text_callbacks();
    setLogListener(gtkLogListener);
    /* Call this very early.  It should parse all command
     * line arguments and set the pertinent ones up in
     * globals.  Also call it early so that if it can't set up
     * the windowing system, we get an error before trying to
     * to connect to the server.  And command line options will
     * likely change on the server we connect to.
     */
    if (init_windows(argc, argv)) {	/* x11.c */
	LOG(LOG_CRITICAL,"gtk::main","Failure to init windows.");
	exit(1);
    }

    csocket.inbuf.buf=malloc(MAXSOCKBUF);

#ifdef WIN32 /* def WIN32 */
	maxfd = 0; /* This is ignored on win32 platforms */

	/* This is required for sockets to be used under win32 */
	{
		WORD Version = 0x0202;
		WSADATA wsaData;
		if (WSAStartup( Version, &wsaData ) != 0) {
			LOG(LOG_CRITICAL,"gtk::main", "Couldn't load winsock!");
			exit(1);
		}
	}
#else /* def WIN32 */
#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX);
#else
    maxfd = getdtablesize();
#endif
#endif /* def WIN32 */

    if (init_sounds() == -1)
	use_config[CONFIG_SOUND] = FALSE;
    else use_config[CONFIG_SOUND] = TRUE;

    /* Loop to connect to server/metaserver and play the game */
    while (1) {
	reset_client_vars();
	csocket.inbuf.len=0;
	csocket.cs_version=0;

	/* Perhaps not the best assumption, but we are taking it that
	 * if the player has not specified a server (ie, server
	 * matches compiled in default), we use the meta server.
	 * otherwise, use the server provided, bypassing metaserver.
	 * Also, if the player has already played on a server once (defined
	 * by got_one), go to the metaserver.  That gives them the oppurtunity
	 * to quit the client or select another server.  We should really add
	 * an entry for the last server there also.
	 */

	if (!server || got_one) {
	    char *ms;
	    metaserver_get_info(meta_server, meta_port);
	    metaserver_show(TRUE);
	    do {
            draw_info_windows();
            ms=get_metaserver();
	    } while (metaserver_select(ms));
	    negotiate_connection(use_config[CONFIG_SOUND]);
	} else {
	    csocket.fd=init_connection(server, use_config[CONFIG_PORT]);
	    if (csocket.fd == -1) { /* specified server no longer valid */
		server = NULL;
		continue;
	    }
	    negotiate_connection(use_config[CONFIG_SOUND]);
	}

	got_one=1;
	event_loop();
	/* if event_loop has exited, we most of lost our connection, so we
	 * loop again to establish a new one.
	 */

	mapdata_reset();
	/* Need to reset the images so they match up properly and prevent
	 * memory leaks.
	 */
	reset_image_data();
	remove_item_inventory(cpl.ob);
	remove_item_inventory(cpl.below);
	set_look_list_env(cpl.below);
    }
    exit(0);	/* never reached */
}
