/*
 * static char *rcsid_gx11_c =
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
 * draw_look, draw_inv:  Update the look and inventory windows.
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

#ifdef __CYGWIN__
#include <errno.h>
#endif

/* gtk */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
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



#ifdef HAVE_SDL
/* These are only used in SDL mode at current time */
extern SDL_Surface* mapsurface;
#endif

static char *colorname[] = {
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


/* All the following are static because these variables should
 * be local only to this file.  Since the idea is to have only
 * this file be replaced for different windowing systems, use of
 * any of these variables anyplace else would not be portable.
 */
typedef enum inventory_show {
  show_all = 0, show_applied = 0x1, show_unapplied = 0x2, show_unpaid = 0x4,
  show_cursed = 0x8, show_magical = 0x10, show_nonmagical = 0x20,
  show_locked = 0x40, show_unlocked=0x80,
  show_mask=0xff
} inventory_show;



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
  GtkWidget *skill_exp[MAX_SKILL];
} StatWindow;

typedef struct {
  gint row;
  GtkWidget *list;
} animview;

typedef struct {
  item *item;
  GList *view;
} animobject;
  
static char **gargv;

#define MAX_HISTORY 50
#define MAX_COMMAND_LEN 256
char history[MAX_HISTORY][MAX_COMMAND_LEN];
static int cur_history_position=0, scroll_history_position=0;

GList *anim_inv_list=NULL, *anim_look_list=NULL;

extern int maxfd;
struct timeval timeout;
gint	csocket_fd=0;

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
GtkWidget *restable;	/* resistance table */
GtkWidget *res_scrolled_window;	/* window the resistances are in */

#define SHOW_RESISTS 24
static GtkWidget *resists[SHOW_RESISTS];
GtkWidget *ckentrytext, *ckeyentrytext, *cmodentrytext, *cnumentrytext;

GdkColor gdk_green =    { 0, 0, 0xcfff, 0 };
GdkColor gdk_red =    { 0, 0xcfff, 0, 0 };
GdkColor gdk_grey = { 0, 0xea60, 0xea60, 0xea60 };
GdkColor gdk_black = { 0, 0, 0, 0 };

GdkColor gdkdiscolor;
static GdkColor map_color[16];
static GdkColor root_color[16];
static GdkPixmap *magicgdkpixmap;
static GdkGC *magic_map_gc;
static GtkWidget *mapvbox;
GdkPixmap   *mapwindow;
GdkBitmap *dark1, *dark2, *dark3;
GdkPixmap *dark;

#define INFOLINELEN 500
#define XPMGCS 100

static GtkWidget *inv_notebook;


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


GtkWidget *entrytext, *counttext;
static GtkObject *text_hadj,*text_vadj;
static GtkObject *text_hadj2,*text_vadj2;
GtkWidget *gameframe, *stat_frame, *message_frame;

/*
 * These are used for inventory and look window
 */
itemlist look_list, inv_list;
static StatWindow statwindow;
/* gtk */
 
GtkWidget *gtkwin_root, *gtkwin_info;
static GtkWidget *gtkwin_info_text2, *gtkwin_info_text;
GtkWidget *gtkwin_stats, *gtkwin_message, *gtkwin_look, *gtkwin_inv;


static GtkWidget *gtkwin_about = NULL;
static GtkWidget *gtkwin_splash = NULL;
static GtkWidget *gtkwin_chelp = NULL;
static GtkWidget *gtkwin_shelp = NULL;
static GtkWidget *gtkwin_magicmap = NULL;

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

static int pickup_mode = 0;

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

uint16 cachelastused=0, cacheloaded=0;

FILE *fcache;

int misses=0,total=0;

/* BPP is byte per pixels we use for drawing to the screen.
 * In my testing, the overall drawing time took pretty close
 * to the same.  BPP 4 saved some time on the generation
 * (able to do full byte copies), but that was lost on the 
 * actual putting of the image to the screen.  Given its a 
 * wash, might as well use 3 BPP - if nothing else, that saves
 * memory.
 */
#define BPP	3

/* Pixels representing entire viewable screen.  This amounts to about <BPP> mb */


void create_windows (void);

	
/* main loop iteration related stuff */
void do_network() {
    fd_set tmp_read, tmp_exceptions;
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
	FD_ZERO(&tmp_exceptions);
	FD_SET(csocket.fd, &tmp_read);
	FD_SET(csocket.fd, &tmp_exceptions);

	pollret = select(maxfd, &tmp_read, NULL, NULL, &timeout);
	if (pollret==-1) {
	    fprintf(stderr, "Got errno %d on select call.\n", errno);
	}
	else if (FD_ISSET(csocket.fd, &tmp_read)) {
	    DoClient(&csocket);
	}
    } else {
	printf ("locked for network recieves.\n");
    }
}


void event_loop()
{
    gint fleep;
    extern int do_timeout();
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
	
    csocket_fd = gdk_input_add ((gint) csocket.fd,
                              GDK_INPUT_READ,
                              (GdkInputFunction) do_network, &csocket);
    tag = csocket_fd;

    gtk_main();
    gtk_timeout_remove(tag);

    fprintf(stderr,"gtk_main exited, returning from event_loop\n");
}




void end_windows()
{
  free(last_str);
}


/* Animation allocations */

static animview *newanimview() {
  animview *op = malloc (sizeof(animview));
  if (! op) 
    exit(0);
  op->row=0;
  op->list = NULL;
  return op;
}

static animobject *newanimobject() {
  animobject *op = malloc (sizeof(animobject));
  if (! op) 
    exit(0);
  op->view = NULL;
  return op;
}


/* Free allocations for animations */

static void freeanimview (gpointer data, gpointer user_data) {
  if (data)
    g_free (data);
}

static void freeanimobject (animobject *data, gpointer user_data) {
  if (data)
    g_list_foreach (data->view, freeanimview, 0);
  g_free (data);
}

/* Update the pixmap */

static void animateview (animview *data, gint user_data) {
    /* If no data (because of cache), use face 0 */
    if (!pixmaps[user_data]->icon_image) user_data=0;

    gtk_clist_set_pixmap (GTK_CLIST (data->list), data->row, 0,
		    (GdkPixmap*)pixmaps[user_data]->icon_image,
		    (GdkBitmap*)pixmaps[user_data]->icon_mask);
}

/* Run through the animations and update them */

static void animate (animobject *data, gpointer user_data) {
  if (data) {
    data->item->last_anim++;
    if (data->item->last_anim>=data->item->anim_speed) {
      data->item->anim_state++;
      if (data->item->anim_state >= animations[data->item->animation_id].num_animations) {
	data->item->anim_state=0;
      }
      data->item->face = animations[data->item->animation_id].faces[data->item->anim_state];
      data->item->last_anim=0;
      g_list_foreach (data->view, (GFunc) animateview, GINT_TO_POINTER((gint)data->item->face));
    }
    
  }
}

/* Run through the lists of animation and do each */

void animate_list () {
  if (anim_inv_list) {
    g_list_foreach     (anim_inv_list, (GFunc) animate, NULL);
  }
  /* If this list needs to be updated, don't try and animated -
   * the contents of the lists are no longer valid.  this should
   * perhaps be done for the inventory list above, but the
   * removal of an animated object with a non animated one within
   * the timeframe if this being called is unlikely.
   */
  if (anim_look_list && !look_list.env->inv_updated) {
    g_list_foreach     (anim_look_list, (GFunc) animate, NULL);
  }
}




/* Handle mouse presses in the game window */


void button_map_event(GtkWidget *widget, GdkEventButton *event) {
  int dx, dy, i, x, y, xmidl, xmidh, ymidl, ymidh;
  
  x=(int)event->x;
  y=(int)event->y;
  dx=(x-2)/map_image_size-(use_config[CONFIG_MAPWIDTH]/2);
  dy=(y-2)/map_image_size-(use_config[CONFIG_MAPHEIGHT]/2);
  xmidl=5*map_image_size-(use_config[CONFIG_MAPWIDTH]/2);
  xmidh=6*map_image_size+(use_config[CONFIG_MAPWIDTH]/2);
  ymidl=5*map_image_size-(use_config[CONFIG_MAPHEIGHT]/2);
  ymidh=6*map_image_size+(use_config[CONFIG_MAPHEIGHT]/2);
  
  switch (event->button) {
  case 1:
    {
      look_at(dx,dy);
    }
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


/* This holds the name we recieve with the 'face' command so we know what
 * to save it as when we actually get the face.
 */
char *facetoname[MAXPIXMAPNUM];

/* Initializes the data for image caching */
static void init_cache_data()
{
    int i;
    GtkStyle *style;
#include "pixmaps/question.xpm"


    printf ("Init Cache\n");
    
    style = gtk_widget_get_style(gtkwin_root);
    pixmaps[0] = malloc(sizeof(PixmapInfo));
    pixmaps[0]->icon_image = gdk_pixmap_create_from_xpm_d(gtkwin_root->window,
							(GdkBitmap**)&pixmaps[0]->icon_mask,
							&style->bg[GTK_STATE_NORMAL],
							(gchar **)question);
#ifdef HAVE_SDL
    if (use_config[CONFIG_SDL]) {
	/* make a semi transparent question mark symbol to
	 * use for the cached images.
	 */
#include "pixmaps/question.sdl"
	pixmaps[0]->map_image = SDL_CreateRGBSurfaceFrom(question_sdl,
		32, 32, 1, 4, 1, 1, 1, 1);
	SDL_SetAlpha(pixmaps[0]->map_image, SDL_SRCALPHA, 70);
    }
    else
#endif
    {
	pixmaps[0]->map_image =  pixmaps[0]->icon_image;
	pixmaps[0]->map_mask =  pixmaps[0]->icon_mask;
    }
    pixmaps[0]->icon_width = pixmaps[0]->icon_height = pixmaps[0]->map_width = pixmaps[0]->map_height = map_image_size;
    facetoname[0]=NULL;

    /* Don't do anything special for SDL image - rather, that drawing
     * code will check to see if there is no data
     */

    /* Initialize all the images to be of the same value. */
    for (i=1; i<MAXPIXMAPNUM; i++)  {
	pixmaps[i] = pixmaps[0];
	facetoname[i]=NULL;
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

void gtk_complete_command()
{
    gchar *entry_text, *newcommand;
	
    entry_text = gtk_entry_get_text(GTK_ENTRY(entrytext));
    newcommand = complete_command(entry_text);
    /* value differ, so update window */ 
    if (strcmp(entry_text, newcommand)) {
	gtk_entry_set_text(GTK_ENTRY(entrytext), newcommand);
	gtk_entry_set_position(GTK_ENTRY(entrytext), strlen(newcommand));
	/* regrab focus, since we've just updated this */
	gtk_widget_grab_focus (GTK_WIDGET(entrytext));
    }
}


/* Event handlers for map drawing area */


static gint
configure_event (GtkWidget *widget, GdkEventConfigure *event)
{

#ifdef HAVE_SDL
    if(use_config[CONFIG_SDL]) {
	/* When program first runs, mapsruface can be null.
	 * either way, we want to catch it here.
	 */
	if (mapsurface)
	    SDL_UpdateRect( mapsurface, 0, 0, 0, 0);
	return TRUE;
    }
#endif
    mapgc = gdk_gc_new (drawingarea->window);

    if (!use_config[CONFIG_SDL]) {
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
    display_map_doneupdate(TRUE);
    return TRUE;
}



/* Redraw the screen from the backing pixmap */
static gint
expose_event (GtkWidget *widget, GdkEventExpose *event)
{
#ifdef HAVE_SDL
    if(use_config[CONFIG_SDL] &&  mapsurface) {
	SDL_UpdateRect( mapsurface, 0, 0, 0, 0);
	return FALSE;
    }
#endif
    display_map_doneupdate(TRUE);
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



/* ----------------------------------- inventory ---------------------------------------------*/

/*
 * Ugh. Ok, this is a Really Nasty Function From Hell (tm). Dont ask me what it does. 
 * Well, actually, it attempts to keep track of the different inventory object views.
 * This should only be called if an actual object is updated. It handles both the look
 * and inventory object window.
 */


static void draw_list (itemlist *l)
{

    gint tmprow;
    item *tmp;
    animobject *tmpanim=NULL;
    animview *tmpanimview;
    char buf[MAX_BUF];
    char buffer[3][MAX_BUF];
    char *buffers[3];
    gint list;
    int mw=image_size, mh=image_size;	    /* max height/width of any image */
    

    /* Is it the inventory or look list? */
    if (l->multi_list) {
    
	/* Ok, inventory list. Animations are handled in client code. First do the nice thing and
	* free all allocated animation lists.
	*/
	if (anim_inv_list) {
	    g_list_foreach (anim_inv_list, (GFunc) freeanimobject, NULL);
	    g_list_free (anim_inv_list);
	    anim_inv_list=NULL;
	}
	/* Freeze the CLists to avoid flickering (and to speed up the processing) */
	for (list=0; list < TYPE_LISTS; list++) {
	    l->pos[list]=GTK_RANGE (GTK_SCROLLED_WINDOW(l->gtk_lists[list])->vscrollbar)->adjustment->value;
	    gtk_clist_freeze (GTK_CLIST(l->gtk_list[list]));
	    gtk_clist_clear (GTK_CLIST(l->gtk_list[list]));
	}
    } else {
	if (anim_look_list) {
	    g_list_foreach (anim_look_list, (GFunc) freeanimobject, NULL);
	    g_list_free (anim_look_list);
	    anim_look_list=NULL;
	}
	/* Just freeze the lists and clear them */
	l->pos[0]=GTK_RANGE (GTK_SCROLLED_WINDOW(l->gtk_lists[0])->vscrollbar)->adjustment->value;
	gtk_clist_freeze (GTK_CLIST(l->gtk_list[0]));
	gtk_clist_clear (GTK_CLIST(l->gtk_list[0]));
    }
  
    /* draw title and put stuff in widgets */
  
    if(l->env->weight < 0 || l->show_weight == 0) {
	strcpy(buf, l->title);
	gtk_label_set (GTK_LABEL(l->label), buf);
	gtk_label_set (GTK_LABEL(l->weightlabel), " ");
	gtk_label_set (GTK_LABEL(l->maxweightlabel), " ");
	gtk_widget_draw (l->label, NULL);
	gtk_widget_draw (l->weightlabel, NULL);
	gtk_widget_draw (l->maxweightlabel, NULL);
    }
    else if (!l->weight_limit) {
	strcpy(buf, l->title);
	gtk_label_set (GTK_LABEL(l->label), buf);
	sprintf (buf, "%6.1f",l->env->weight);
	gtk_label_set (GTK_LABEL(l->weightlabel), buf);
	gtk_label_set (GTK_LABEL(l->maxweightlabel), " ");
	gtk_widget_draw (l->label, NULL);
	gtk_widget_draw (l->weightlabel, NULL);
	gtk_widget_draw (l->maxweightlabel, NULL);
    } else {
	strcpy(buf, l->title);
	gtk_label_set (GTK_LABEL(l->label), buf);
	sprintf (buf, "%6.1f",l->env->weight);
	gtk_label_set (GTK_LABEL(l->weightlabel), buf);
	sprintf (buf, "/ %4d",l->weight_limit/1000);
	gtk_label_set (GTK_LABEL(l->maxweightlabel), buf);
	gtk_widget_draw (l->label, NULL);
	gtk_widget_draw (l->weightlabel, NULL);
	gtk_widget_draw (l->maxweightlabel, NULL);
    }

    /* Ok, go through the objects and start appending rows to the lists */
    for(tmp = l->env->inv; tmp ; tmp=tmp->next) {      

	strcpy (buffer[0]," "); 
	strcpy (buffer[1], tmp->d_name);

	if (l->show_icon == 0)
	    strcat (buffer[1], tmp->flags);
    
	if(tmp->weight < 0 || l->show_weight == 0) {
	    strcpy (buffer[2]," "); 
	} else {
	    sprintf (buffer[2],"%6.1f" ,tmp->nrof * tmp->weight);
	}
    
	buffers[0] = buffer[0];
	buffers[1] = buffer[1];
	buffers[2] = buffer[2];
    
	if (l->multi_list) {
	    tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[0]), buffers);
	    /* Set original pixmap */
	    gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[0]), tmprow, 0,
			    (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			    (GdkBitmap*)pixmaps[tmp->face]->icon_mask); 
	    if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
	    if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

	    gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[0]), tmprow, tmp);
	    if (use_config[CONFIG_COLORINV]) { 
		if (tmp->cursed || tmp->damned) {
		    gtk_clist_set_background (GTK_CLIST(l->gtk_list[0]), tmprow,
				    &root_color[NDI_RED]);
		}
		if (tmp->magical) {
		    gtk_clist_set_background (GTK_CLIST(l->gtk_list[0]), tmprow,
				    &root_color[NDI_BLUE]);
		}
		if ((tmp->cursed || tmp->damned) && tmp->magical) {
		    gtk_clist_set_background (GTK_CLIST(l->gtk_list[0]), tmprow,
				    &root_color[NDI_NAVY]);
		}
	    }
	    /* If it's an animation, zap in an animation object to the list of
	     animations to be done */

	    if (tmp->animation_id>0 && tmp->anim_speed) {
		tmpanim = newanimobject();
		tmpanim->item=tmp;
		tmpanimview = newanimview();
		tmpanimview->row=tmprow;
		tmpanimview->list=l->gtk_list[0];
		tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		anim_inv_list = g_list_append (anim_inv_list, tmpanim);
	    }

	    if (tmp->applied) {
		tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[1]), buffers);
		gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[1]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
		if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
		if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

		gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[1]), tmprow, tmp); 
		if (tmp->animation_id>0 && tmp->anim_speed) {
		    tmpanimview = newanimview();
		    tmpanimview->row=tmprow;
		    tmpanimview->list=l->gtk_list[1];
		    tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		}

	    }
	    if (!tmp->applied) {
		tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[2]), buffers);
		gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[2]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
		if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
		if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

		gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[2]), tmprow, tmp);
		if (tmp->animation_id>0 && tmp->anim_speed) {
		    tmpanimview = newanimview();
		    tmpanimview->row=tmprow;
		    tmpanimview->list=l->gtk_list[2];
		    tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		}
	    }
	    if (tmp->unpaid) {
		tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[3]), buffers);
		gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[3]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
		if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
		if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

		gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[3]), tmprow, tmp);
		if (tmp->animation_id>0 && tmp->anim_speed) {
		    tmpanimview = newanimview();
		    tmpanimview->row=tmprow;
		    tmpanimview->list=l->gtk_list[3];
		    tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		}
	    }
	    if (tmp->cursed || tmp->damned) {
		tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[4]), buffers);
		gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[4]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
		if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
		if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

		gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[4]), tmprow, tmp);
		if (tmp->animation_id>0 && tmp->anim_speed) {
		    tmpanimview = newanimview();
		    tmpanimview->row=tmprow;
		    tmpanimview->list=l->gtk_list[4];
		    tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		}
	    }
	    if (tmp->magical) {
		tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[5]), buffers);
		gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[5]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
		if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
		if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

		gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[5]), tmprow, tmp);
		if (tmp->animation_id>0 && tmp->anim_speed) {
		    tmpanimview = newanimview();
		    tmpanimview->row=tmprow;
		    tmpanimview->list=l->gtk_list[5];
		    tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		}
	    }
	    if (!tmp->magical) {
		tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[6]), buffers);
		gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[6]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
		if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
		if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

		gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[6]), tmprow, tmp);
		if (tmp->animation_id>0 && tmp->anim_speed) {
		    tmpanimview = newanimview();
		    tmpanimview->row=tmprow;
		    tmpanimview->list=l->gtk_list[6];
		    tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		}
	    }
	    if (tmp->locked) {
		tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[7]), buffers);
		gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[7]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
		if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
		if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

		gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[7]), tmprow, tmp);
		if (tmp->animation_id>0 && tmp->anim_speed) {
		    tmpanimview = newanimview();
		    tmpanimview->row=tmprow;
		    tmpanimview->list=l->gtk_list[7];
		    tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		}
	    }
	    if (!tmp->locked) {
		tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[8]), buffers);
		gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[8]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
		if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
		if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

		gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[8]), tmprow, tmp);
		if (tmp->animation_id>0 && tmp->anim_speed) {
		    tmpanimview = newanimview();
		    tmpanimview->row=tmprow;
		    tmpanimview->list=l->gtk_list[8];
		    tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		}
		if (use_config[CONFIG_COLORINV]) { 
		    if (tmp->cursed || tmp->damned) {
			gtk_clist_set_background (GTK_CLIST(l->gtk_list[8]), tmprow,
				    &root_color[NDI_RED]);
		    }
		    if (tmp->magical) {
			gtk_clist_set_background (GTK_CLIST(l->gtk_list[8]), tmprow,
				    &root_color[NDI_BLUE]);
		    }
		    if ((tmp->cursed || tmp->damned) && tmp->magical) {
			gtk_clist_set_background (GTK_CLIST(l->gtk_list[8]), tmprow,
				    &root_color[NDI_NAVY]);
		    }
		}
	    }
	} else {
	    tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[0]), buffers);
	    gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[0]), tmprow, 0,
			      (GdkPixmap*)pixmaps[tmp->face]->icon_image,
			      (GdkBitmap*)pixmaps[tmp->face]->icon_mask);
	    if (pixmaps[tmp->face]->icon_width > mw) mw = pixmaps[tmp->face]->icon_width;
	    if (pixmaps[tmp->face]->icon_height > mh) mh = pixmaps[tmp->face]->icon_height;

	    gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[0]), tmprow, tmp);
	    if (tmp->animation_id>0 && tmp->anim_speed) {
		tmpanim = newanimobject();
		tmpanim->item=tmp;
		tmpanimview = newanimview();
		tmpanimview->row=tmprow;
		tmpanimview->list=l->gtk_list[0];
		tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
		anim_look_list = g_list_append (anim_look_list, tmpanim);
	    }
	    if (use_config[CONFIG_COLORINV]) { 
		if (tmp->cursed || tmp->damned) {
		    gtk_clist_set_background (GTK_CLIST(l->gtk_list[0]), tmprow,
				    &root_color[NDI_RED]);
		}
		if (tmp->magical) {
		    gtk_clist_set_background (GTK_CLIST(l->gtk_list[0]), tmprow,
				    &root_color[NDI_BLUE]);
		}
		if ((tmp->cursed || tmp->damned) && tmp->magical) {
		    gtk_clist_set_background (GTK_CLIST(l->gtk_list[0]), tmprow,
				    &root_color[NDI_NAVY]);
		}
	    }
	}
    }


    /* Ok, stuff is drawn, now replace the scrollbar positioning as far as possible */
    if (l->multi_list) {
	for (list=0; list < TYPE_LISTS; list++) {
	    gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_RANGE (GTK_SCROLLED_WINDOW(l->gtk_lists[list])->vscrollbar)->adjustment), l->pos[list]);
	    gtk_clist_thaw (GTK_CLIST(l->gtk_list[list]));
	}
    } else {
	if (l->column_width != mw) {
	    gtk_clist_set_column_width(GTK_CLIST(l->gtk_list[0]), 0, mw);
	    l->column_width = mw;
	}
	if (l->row_height != mh) {
	    gtk_clist_set_row_height(GTK_CLIST(l->gtk_list[0]), mh);
	    l->row_height = mh;
	}
	gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_RANGE (GTK_SCROLLED_WINDOW(l->gtk_lists[0])->vscrollbar)->adjustment), l->pos[0]);
	gtk_clist_thaw (GTK_CLIST(l->gtk_list[0]));
    }
}


/******************************************************************************
 *
 * The functions dealing with the info window follow
 *
 *****************************************************************************/


static void enter_callback(GtkWidget *widget, GtkWidget *entry)
{
    gchar *entry_text;

    /* Next reply will reset this as necessary */
    if (!use_config[CONFIG_POPUPS])
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
	    history[cur_history_position][MAX_COMMAND_LEN] = 0;
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
  gchar *dialog_text;
  dialog_text = gtk_entry_get_text(GTK_ENTRY(dialogtext));
  
  gtk_widget_destroy (dialog_window);
  send_reply(dialog_text);
  cpl.input_state = Playing;
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

			    dialoglabel =
				gtk_label_new ("What is your name?");
			    gtk_box_pack_start (GTK_BOX (dbox), dialoglabel,
						FALSE, TRUE, 6);
			    gtk_widget_show (dialoglabel);

			    hbox = gtk_hbox_new (FALSE, 6);
			    dialogtext = gtk_entry_new ();
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
			    continue;
			}

		      if (!strcmp (last_str, "What is your password?"))
			{

			    dialoglabel =
				gtk_label_new ("What is your password?");
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
		      if (!strcmp
			  (last_str, "Please type your password again."))
			{

			    dialoglabel =
				gtk_label_new
				("Please type your password again.");
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
			    continue;
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

		if (!found)
		  {
		      dialoglabel = gtk_label_new (str);
		      gtk_box_pack_start (GTK_BOX (dbox), dialoglabel, FALSE,
					  TRUE, 6);
		      gtk_widget_show (dialoglabel);

		      hbox = gtk_hbox_new (FALSE, 6);
		      dialogtext = gtk_entry_new ();

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
  
    if (ncolor==NDI_WHITE) {
	ncolor=NDI_BLACK;
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
		fprintf(stderr,"reduced output buffer2 to %d chars\n", info1_num_chars);
	    }
	}
	gtk_text_insert (GTK_TEXT (gtkwin_info_text2), NULL, &root_color[ncolor], NULL, str , -1);
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
		fprintf(stderr,"trim_info_window, deleted %d characters, %d remaining\n", to_delete, info1_num_chars);
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

	gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[ncolor], NULL, str , -1);
	gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[ncolor], NULL, "\n" , -1);
    }
}


void draw_color_info(int colr, const char *buf){
    if (use_config[CONFIG_COLORTXT]){
	draw_info(buf,colr);
    }
    else {
	draw_info("==========================================",NDI_BLACK);
	draw_info(buf,NDI_BLACK);
	draw_info("==========================================",NDI_BLACK);
    }
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
    char    buf[MAX_BUF];
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


    /* Start of experience display - we do it in a 2 X 3 array.  Use a table
     * so that spacing is uniform - this should look better.
     */

    table = gtk_table_new (2, 3, TRUE);
    x=0;
    y=0;
    /* this is all the same - we just pack it in different places */
    for (i=0; i<MAX_SKILL; i++) {
	sprintf(buf,"%s: %d (%d)", skill_names[i], 0, 0);
	statwindow.skill_exp[i] = gtk_label_new(buf);
	gtk_table_attach(GTK_TABLE(table), statwindow.skill_exp[i], x, x+1, y, y+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	x++;
	if (x==2) { x=0; y++; }
	gtk_widget_show(statwindow.skill_exp[i]);
    }
    gtk_box_pack_start (GTK_BOX (stats_vbox), table, TRUE, TRUE, 0);
    gtk_widget_show(table);

    gtk_container_add (GTK_CONTAINER (frame), stats_vbox);
    gtk_widget_show (stats_vbox);

    return 0;
}

/* This draws the stats window.  If redraw is true, it means
 * we need to redraw the entire thing, and not just do an
 * updated.
 */

void draw_stats(int redraw) {
  static Stats last_stats;
  static char last_name[MAX_BUF]="", last_range[MAX_BUF]="";
  static int init_before=0, lastbeep=0;

  float weap_sp;
  char buff[MAX_BUF];
  int i;

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
      sprintf(buff,"Score: %5d",cpl.stats.exp);
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
	XBell(GDK_DISPLAY(), 0);
    } else if (use_config[CONFIG_FOODBEEP] && cpl.stats.food == 0 && ++lastbeep == 5) {
	lastbeep = 0;
	XBell(GDK_DISPLAY(), 0);
    }
    
    if(redraw || strcmp(cpl.range, last_range)) {
      strcpy(last_range, cpl.range);
      gtk_label_set (GTK_LABEL(statwindow.skill), cpl.range);
      gtk_widget_draw (statwindow.skill, NULL);
    }
    for (i=0; i<MAX_SKILL; i++) {
	if (redraw || cpl.stats.skill_level[i] != last_stats.skill_level[i] ||
	    cpl.stats.skill_exp[i] != last_stats.skill_exp[i]) {
	    sprintf(buff,"%s: %d (%d)", skill_names[i], cpl.stats.skill_exp[i], cpl.stats.skill_level[i]);
	    gtk_label_set(GTK_LABEL(statwindow.skill_exp[i]), buff);
	    last_stats.skill_level[i] = cpl.stats.skill_level[i];
	    last_stats.skill_exp[i] = cpl.stats.skill_exp[i];
	}
    }
  } /* updatelock < 25 */
}


/***********************************************************************
*
* Handles the message window
*
***********************************************************************/


void create_stat_bar (GtkWidget *mtable, gint row, gchar *label, gint bar, GtkWidget **plabel) {
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
void reset_stat_bars() {
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

    int i, left=0, right=0;

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

    /* initialize labels for all modes of CONFIG_RESISTS */
    fire_label = gtk_label_new ("    ");
    run_label = gtk_label_new ("   ");
  
    /* place labels for mode 2 of CONFIG_RESISTS */
    if (use_config[CONFIG_RESISTS]==2) {
	restable = gtk_table_new (4,12,FALSE);
	gtk_table_attach (GTK_TABLE(restable), fire_label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE(restable), run_label, 3, 4, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
    }

    /* place labels for mode 1 and 0 of CONFIG_RESISTS */  
    if (use_config[CONFIG_RESISTS]<=1) {
	restable = gtk_table_new (2,24,FALSE);
	gtk_table_attach (GTK_TABLE(restable), fire_label, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE(restable), run_label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
    }
  
    /* show labels for all modes of CONFIG_RESISTS */
    gtk_widget_show (fire_label);
    gtk_widget_show (run_label);

    /* make and place labels for showing the resistances - start */
    for (i=0; i< SHOW_RESISTS ; i++) {
	resists[i] = gtk_label_new("          ");    
  
	/* place the labels for mode 2 in the table restable */
	if (use_config[CONFIG_RESISTS]==2) {
	    if ((i/2)*2 != i) {
		left++;
		gtk_table_attach (GTK_TABLE(restable), resists[i], 1, 2, 3+left, 4+left, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    } else {
		right++;
		gtk_table_attach (GTK_TABLE(restable), resists[i], 3, 4, 3+right, 4+right, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    }
	    gtk_widget_show (resists[i]);
	}

	/* place the labels for mode 1 in the table restable */
	else if (use_config[CONFIG_RESISTS]==1) {
	    gtk_table_attach (GTK_TABLE(restable), resists[i], 0, 2, 3+i, 4+i, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    gtk_widget_show (resists[i]);
	}

	/* place the labels for mode 0 in the table restable - only seven - old style */  
	else if ( (use_config[CONFIG_RESISTS]==0) && (i <= 6) ) {
	    gtk_table_attach (GTK_TABLE(restable), resists[i], 0, 2, 3+i, 4+i, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    gtk_widget_show (resists[i]);
	}
    }
    /* make and place labels for showing the resistances - stop */

    /* packing the restable for mode not 0 - scrollable*/  
    res_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (res_scrolled_window), 0);
    if (want_config[CONFIG_RESISTS] ==0) 
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (res_scrolled_window),GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    else
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (res_scrolled_window),GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start(GTK_BOX(reswindow), res_scrolled_window, TRUE, TRUE, 0);
    gtk_widget_show (res_scrolled_window);
    gtk_scrolled_window_add_with_viewport ( GTK_SCROLLED_WINDOW (res_scrolled_window), restable);
    /* resistances table part - end */

    /* now showing all not already showed widgets */
    gtk_widget_show (res_mainbox);
    gtk_widget_show (reswindow);
    gtk_widget_show (restable);
    gtk_widget_show (mtable);
    gtk_widget_show (vbox);
    return 0;
}

/* This changes the layout of the resistance window.
 * We end up just removing (and thus freeing) all the data and
 * then create new entries.  This keeps things simpler, because
 * in basic mode, not all the resist[] widgets are attached,
 */
void resize_resistance_table(int resists_show)
{
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

    /* place labels for mode 2 of CONFIG_RESISTS */
    if (resists_show == 2) {
	gtk_table_resize(GTK_TABLE(restable), 4,12);
	gtk_table_attach (GTK_TABLE(restable), fire_label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE(restable), run_label, 3, 4, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
    }

    /* place labels for mode 1 and 0 of CONFIG_RESISTS */  
    if (resists_show<=1) {
	gtk_table_resize(GTK_TABLE(restable), 2,24);
	gtk_table_attach (GTK_TABLE(restable), fire_label, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE(restable), run_label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

    }
    /* show labels for all modes of CONFIG_RESISTS */
    gtk_widget_show (fire_label);
    gtk_widget_show (run_label);
    /* make and place labels for showing the resistances - start */

    for (i=0; i< SHOW_RESISTS ; i++) {
	resists[i] = gtk_label_new("          ");    
  
	/* place the labels for mode 2 in the table restable */
	if (resists_show==2) {
	    if ((i/2)*2 != i) {
		left++;
		gtk_table_attach (GTK_TABLE(restable), resists[i], 1, 2, 3+left, 4+left, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    } else {
		right++;
		gtk_table_attach (GTK_TABLE(restable), resists[i], 3, 4, 3+right, 4+right, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    }
	    gtk_widget_show (resists[i]);
	}

	/* place the labels for mode 1 in the table restable */
	else if (resists_show==1) {
	    gtk_table_attach (GTK_TABLE(restable), resists[i], 0, 2, 3+i, 4+i, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    gtk_widget_show (resists[i]);
	}

	/* place the labels for mode 0 in the table restable - only seven - old style */  
	else if ( (resists_show==0) && (i <= 6) ) {
	    gtk_table_attach (GTK_TABLE(restable), resists[i], 0, 2, 3+i, 4+i, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	    gtk_widget_show (resists[i]);
	}
    }

    if (resists_show ==0) 
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (res_scrolled_window),GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    else
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
		    if (j >= SHOW_RESISTS) break;
		}
	    }
	    /* Erase old/unused resistances */
	    while (j<SHOW_RESISTS) {
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
 * Inventory window functions follow
 *
 ****************************************************************************/


/*
 * draw_all_list clears a window and after that draws all objects 
 * and a scrollbar
 */
void draw_all_list(itemlist *l)
{
    int i;

    strcpy (l->old_title, "");

    for(i=0; i<l->size; i++) {
	copy_name(l->names[i], "");
	l->faces[i] = 0;
	l->icon1[i] = 0;
	l->icon2[i] = 0;
	l->icon3[i] = 0;
	l->icon4[i] = 0;
    }

    l->bar_size = 1;    /* so scroll bar is drawn */
    draw_list (l);

}

void open_container (item *op) 
{
  look_list.env = op;
  sprintf (look_list.title, "%s:", op->d_name);
  draw_list (&look_list);
}

void close_container (item *op) 
{
  if (look_list.env != cpl.below) {
    client_send_apply (look_list.env->tag);
    look_list.env = cpl.below;
    strcpy (look_list.title, "You see:");
    draw_list (&look_list);
  }
}


/* Handle mouse presses in the lists */
static void list_button_event (GtkWidget *gtklist, gint row, gint column, GdkEventButton *event, itemlist *l)
{
  item *tmp;
  if (event->button==1) {
      tmp = gtk_clist_get_row_data (GTK_CLIST(gtklist), row); 
      gtk_clist_unselect_row (GTK_CLIST(gtklist), row, 0);
      if (event->state & GDK_SHIFT_MASK)
	toggle_locked(tmp);
      else
	client_send_examine (tmp->tag);     

  }
  if (event->button==2) {
      tmp = gtk_clist_get_row_data (GTK_CLIST(gtklist), row); 

      gtk_clist_unselect_row (GTK_CLIST(gtklist), row, 0);
      if (event->state & GDK_SHIFT_MASK)
	send_mark_obj(tmp);
      else
	client_send_apply (tmp->tag);

  }
  if (event->button==3) {

    tmp = gtk_clist_get_row_data (GTK_CLIST(gtklist), row);
    gtk_clist_unselect_row (GTK_CLIST(gtklist), row, 0);
    
    if (tmp->locked) {
      draw_info ("This item is locked.",NDI_BLACK);
    } else if (l == &inv_list) {
      cpl.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(counttext));
      client_send_move (look_list.env->tag, tmp->tag, cpl.count);
      if (!use_config[CONFIG_POPUPS]) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(counttext),0.0);
        cpl.count=0;
      }
    }
    else {
      cpl.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(counttext));
      client_send_move (inv_list.env->tag, tmp->tag, cpl.count);
      if (!use_config[CONFIG_POPUPS]) {
         gtk_spin_button_set_value(GTK_SPIN_BUTTON(counttext),0.0);
         cpl.count=0;
      }
    }
    
  }
  
}


static void resize_notebook_event (GtkWidget *widget, GtkAllocation *event) {
    int i, newwidth;
    static int oldwidth=0;

    newwidth = GTK_CLIST(inv_list.gtk_list[0])->clist_window_width - image_size - 75;

    if (newwidth != oldwidth) {
	oldwidth = newwidth;
	for (i=0; i<TYPE_LISTS; i++) {
	    gtk_clist_set_column_width (GTK_CLIST(inv_list.gtk_list[i]), 0, image_size);
	    gtk_clist_set_column_width (GTK_CLIST(inv_list.gtk_list[i]), 1, newwidth);
	    gtk_clist_set_column_width (GTK_CLIST(inv_list.gtk_list[i]), 2, 50);
	}
	inv_list.column_width = image_size;

	gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 0, image_size);
	gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 1, newwidth);
	gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 2, 50);
	look_list.column_width = image_size;

    }
}

void count_callback(GtkWidget *widget, GtkWidget *entry)
{
    gchar *count_text;

    count_text = gtk_entry_get_text(GTK_ENTRY(counttext));
    cpl.count = atoi (count_text);
    gtk_widget_grab_focus (GTK_WIDGET(gtkwin_info_text)); 
}


/* Create tabbed notebook page */
void create_notebook_page (GtkWidget *notebook, GtkWidget **list, GtkWidget **lists, gchar **label) {
  GtkWidget *vbox1;
  GtkStyle *liststyle, *tabstyle;
  GdkPixmap *labelgdkpixmap;
  GdkBitmap *labelgdkmask;
  GtkWidget *tablabel;

  gchar *titles[] ={"?","Name","Weight"};	   

  tabstyle = gtk_widget_get_style(gtkwin_root);

  labelgdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_root->window,
					    &labelgdkmask,
					    &tabstyle->bg[GTK_STATE_NORMAL],
					    (gchar **) label );

 
  tablabel = gtk_pixmap_new (labelgdkpixmap, labelgdkmask);
  gtk_widget_show (tablabel);

  vbox1 = gtk_vbox_new(FALSE, 0);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox1, tablabel);
  *lists = gtk_scrolled_window_new (0,0);
  *list = gtk_clist_new_with_titles (3, titles);

  gtk_clist_set_column_width (GTK_CLIST(*list), 0, image_size);
  gtk_clist_set_column_width (GTK_CLIST(*list), 1, 150);
  gtk_clist_set_column_width (GTK_CLIST(*list), 2, 50);
  /* Since the program will automatically adjust these, any changes
   * the user makes can get obliterated, so just don't let the user
   * make changes.
   */
  gtk_clist_set_column_resizeable(GTK_CLIST(*list), 0, FALSE);
  gtk_clist_set_column_resizeable(GTK_CLIST(*list), 1, FALSE);
  gtk_clist_set_column_resizeable(GTK_CLIST(*list), 2, FALSE);

  gtk_clist_set_selection_mode (GTK_CLIST(*list) , GTK_SELECTION_SINGLE);
  gtk_clist_set_row_height (GTK_CLIST(*list), image_size); 
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(*lists),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  liststyle = gtk_rc_get_style(*list);
  if (liststyle) {
	liststyle->bg[GTK_STATE_SELECTED] = gdk_grey;
	liststyle->fg[GTK_STATE_SELECTED] = gdk_black;
	gtk_widget_set_style (*list, liststyle);
  }
  gtk_clist_set_button_actions (GTK_CLIST(*list),
				1,
				GTK_BUTTON_SELECTS);
  gtk_clist_set_button_actions (GTK_CLIST(*list),
				2,
				GTK_BUTTON_SELECTS);
  gtk_signal_connect (GTK_OBJECT(*list),
		      "select_row",
		      GTK_SIGNAL_FUNC(list_button_event),
		      &inv_list);
  gtk_widget_show (*list);
  gtk_container_add (GTK_CONTAINER (*lists), *list);
  gtk_box_pack_start (GTK_BOX(vbox1),*lists, TRUE, TRUE, 0);
  gtk_widget_show (*lists);

  gtk_signal_connect (GTK_OBJECT(*list),"size-allocate",
		      (GtkSignalFunc) resize_notebook_event, NULL);

  gtk_widget_show (vbox1);
}


static int get_inv_display(GtkWidget *frame)
{
  /*  GtkWidget *vbox1;*/
#include "pixmaps/all.xpm" 
#include "pixmaps/hand.xpm" 
#include "pixmaps/hand2.xpm" 
#include "pixmaps/coin.xpm"
#include "pixmaps/skull.xpm"
#include "pixmaps/mag.xpm"
#include "pixmaps/nonmag.xpm"
#include "pixmaps/lock.xpm"
#include "pixmaps/unlock.xpm"
  
  GtkWidget *vbox2;
  GtkWidget *hbox1;
  GtkWidget *invlabel;
  GtkAdjustment *adj;

  strcpy (inv_list.title, "Inventory:");
  inv_list.env = cpl.ob;
  inv_list.show_weight = 1;
  inv_list.weight_limit=0;
  
  vbox2 = gtk_vbox_new(FALSE, 0); /* separation here */
  
  gtk_container_add (GTK_CONTAINER(frame), vbox2); 

  hbox1 = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start (GTK_BOX(vbox2),hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);


  inv_list.label = gtk_label_new ("Inventory:");
  gtk_box_pack_start (GTK_BOX(hbox1),inv_list.label, TRUE, FALSE, 2);
  gtk_widget_show (inv_list.label);

  inv_list.weightlabel = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX(hbox1),inv_list.weightlabel, TRUE, FALSE, 2);
  gtk_widget_show (inv_list.weightlabel);


  inv_list.maxweightlabel = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX(hbox1),inv_list.maxweightlabel, TRUE, FALSE, 2);
  gtk_widget_show (inv_list.maxweightlabel);

  invlabel = gtk_label_new ("Count:");
  gtk_box_pack_start (GTK_BOX(hbox1),invlabel, FALSE, FALSE, 5);
  gtk_widget_show (invlabel);

  adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 100000.0, 1.0,
                                                  100.0, 0.0);
  counttext = gtk_spin_button_new (adj, 1.0, 0);

  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (counttext), FALSE);
  gtk_widget_set_usize (counttext, 65, 0);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (counttext),
				     GTK_UPDATE_ALWAYS);
   gtk_signal_connect(GTK_OBJECT(counttext), "activate",
		     GTK_SIGNAL_FUNC(count_callback),
		     counttext);


  gtk_box_pack_start (GTK_BOX (hbox1),counttext, FALSE, FALSE, 0);

  gtk_widget_show (counttext);
  gtk_tooltips_set_tip (tooltips, counttext, "This sets the number of items you wish to pickup or drop. You can also use the keys 0-9 to set it.", NULL);

  inv_notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (inv_notebook), GTK_POS_TOP );


  gtk_box_pack_start (GTK_BOX(vbox2),inv_notebook, TRUE, TRUE, 0);

  create_notebook_page (inv_notebook, &inv_list.gtk_list[0], &inv_list.gtk_lists[0], all_xpm); 
  create_notebook_page (inv_notebook, &inv_list.gtk_list[1], &inv_list.gtk_lists[1], hand_xpm); 
  create_notebook_page (inv_notebook, &inv_list.gtk_list[2], &inv_list.gtk_lists[2], hand2_xpm); 
  create_notebook_page (inv_notebook, &inv_list.gtk_list[3], &inv_list.gtk_lists[3], coin_xpm);
  create_notebook_page (inv_notebook, &inv_list.gtk_list[4], &inv_list.gtk_lists[4], skull_xpm);
  create_notebook_page (inv_notebook, &inv_list.gtk_list[5], &inv_list.gtk_lists[5], mag_xpm);
  create_notebook_page (inv_notebook, &inv_list.gtk_list[6], &inv_list.gtk_lists[6], nonmag_xpm);
  create_notebook_page (inv_notebook, &inv_list.gtk_list[7], &inv_list.gtk_lists[7], lock_xpm);
  create_notebook_page (inv_notebook, &inv_list.gtk_list[8], &inv_list.gtk_lists[8], unlock_xpm);

  gtk_widget_show (vbox2);
  gtk_widget_show (inv_notebook);

  inv_list.multi_list=1;
  inv_list.row_height = image_size;
  draw_all_list(&inv_list);
 
  return 0;
}

static int get_look_display(GtkWidget *frame) 
{
  GtkWidget *vbox1;
  GtkWidget *hbox1;
  GtkWidget *closebutton;
  GtkStyle *liststyle;
  
  /*  gchar *test[] ={"testamer","testa","test"};*/
  gchar *titles[] ={"?","Name","Weight"};
  
  look_list.env = cpl.below;
  strcpy (look_list.title, "You see:");
  look_list.show_weight = 1;
  look_list.weight_limit = 0;
    

  vbox1 = gtk_vbox_new(FALSE, 0);/*separation here*/
  gtk_container_add (GTK_CONTAINER(frame), vbox1);

  hbox1 = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start (GTK_BOX(vbox1),hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);

  closebutton = gtk_button_new_with_label ("Close");
  gtk_signal_connect_object (GTK_OBJECT (closebutton), "clicked",
			       GTK_SIGNAL_FUNC(close_container),
			       NULL);
  gtk_box_pack_start (GTK_BOX(hbox1),closebutton, FALSE, FALSE, 2);
  gtk_widget_show (closebutton);
  gtk_tooltips_set_tip (tooltips,closebutton , "This will close an item if you have one open.", NULL);

  look_list.label = gtk_label_new ("You see:");
  gtk_box_pack_start (GTK_BOX(hbox1),look_list.label, TRUE, FALSE, 2);
  gtk_widget_show (look_list.label);

  look_list.weightlabel = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX(hbox1),look_list.weightlabel, TRUE, FALSE, 2);
  gtk_widget_show (look_list.weightlabel);

  look_list.maxweightlabel = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX(hbox1),look_list.maxweightlabel, TRUE, FALSE, 2);
  gtk_widget_show (look_list.maxweightlabel);

  look_list.gtk_lists[0] = gtk_scrolled_window_new (0,0);
  look_list.gtk_list[0] = gtk_clist_new_with_titles (3,titles);;
  gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 0, image_size);
  gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 1, 150);
  gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 2, 50);
  gtk_clist_set_selection_mode (GTK_CLIST(look_list.gtk_list[0]) , GTK_SELECTION_SINGLE);
  gtk_clist_set_row_height (GTK_CLIST(look_list.gtk_list[0]), image_size); 
  look_list.row_height = image_size;
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(look_list.gtk_lists[0]),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  liststyle = gtk_rc_get_style (look_list.gtk_list[0]);
  if (liststyle) {
    liststyle->bg[GTK_STATE_SELECTED] = gdk_grey;
    liststyle->fg[GTK_STATE_SELECTED] = gdk_black;
    gtk_widget_set_style (look_list.gtk_list[0], liststyle);
  }
  gtk_clist_set_button_actions (GTK_CLIST(look_list.gtk_list[0]),
				1,
				GTK_BUTTON_SELECTS);
  gtk_clist_set_button_actions (GTK_CLIST(look_list.gtk_list[0]),
				2,
				GTK_BUTTON_SELECTS);
  gtk_signal_connect (GTK_OBJECT(look_list.gtk_list[0]),
		      "select_row",
		      GTK_SIGNAL_FUNC(list_button_event),
		      &look_list);

  gtk_widget_show (look_list.gtk_list[0]);
  gtk_container_add (GTK_CONTAINER (look_list.gtk_lists[0]), look_list.gtk_list[0]);
  gtk_box_pack_start (GTK_BOX(vbox1),look_list.gtk_lists[0], TRUE, TRUE, 0);
  gtk_widget_show (look_list.gtk_lists[0]);
  gtk_widget_show (vbox1);
  look_list.multi_list=0;
  draw_all_list(&look_list);
  return 0;
}


/*
 *  draw_lists() redraws inventory and look windows when necessary
 */
void draw_lists ()
{
  if (inv_list.env->inv_updated) {

    draw_list (&inv_list);
    inv_list.env->inv_updated = 0;
  } else {
    if (look_list.env->inv_updated) {
      draw_list (&look_list);
      look_list.env->inv_updated = 0;
    }
  }
}

void set_show_icon (char *s)
{
    if (s == NULL || *s == 0 || strncmp ("inventory", s, strlen(s)) == 0) {
	inv_list.show_icon = ! inv_list.show_icon; /* toggle */
	draw_all_list(&inv_list);
    } else if (strncmp ("look", s, strlen(s)) == 0) {
	look_list.show_icon = ! look_list.show_icon; /* toggle */
	draw_all_list(&look_list);
    }
}

void set_show_weight (char *s)
{
    if (s == NULL || *s == 0 || strncmp ("inventory", s, strlen(s)) == 0) {
	inv_list.show_weight = ! inv_list.show_weight; /* toggle */
	draw_list (&inv_list);
    } else if (strncmp ("look", s, strlen(s)) == 0) {
	look_list.show_weight = ! look_list.show_weight; /* toggle */
	draw_list (&look_list);
    }
}

void aboutdialog(GtkWidget *widget) {
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


void disconnect(GtkWidget *widget) {
    close(csocket.fd);
    csocket.fd = -1;
    if (csocket_fd) {
	gdk_input_remove(csocket_fd);
	csocket_fd=0;
	gtk_main_quit();
    }
}

/* Ok, simplistic help system. Just put the text file up in a scrollable window */

void chelpdialog(GtkWidget *widget) {
#include "help/chelp.h"
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *chelptext;
  GtkWidget *helpbutton;
  GtkWidget *vscrollbar;
  /*  GtkStyle *style;*/


  if(!gtkwin_chelp) {
    
    gtkwin_chelp = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_position (GTK_WINDOW (gtkwin_chelp), GTK_WIN_POS_CENTER);
    gtk_widget_set_usize (gtkwin_chelp,400,300);
    gtk_window_set_title (GTK_WINDOW (gtkwin_chelp), "Crossfire Client Help");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_chelp), TRUE, TRUE, FALSE);

    gtk_signal_connect (GTK_OBJECT (gtkwin_chelp), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_chelp);
    
    gtk_container_border_width (GTK_CONTAINER (gtkwin_chelp), 0);
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER(gtkwin_chelp),vbox);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    chelptext = gtk_text_new (NULL, NULL);
    gtk_text_set_editable (GTK_TEXT (chelptext), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox),chelptext, TRUE, TRUE, 0);
    gtk_widget_show (chelptext);

    vscrollbar = gtk_vscrollbar_new (GTK_TEXT (chelptext)->vadj);
    gtk_box_pack_start (GTK_BOX (hbox),vscrollbar, FALSE, FALSE, 0);
 
    gtk_widget_show (vscrollbar);
    gtk_widget_show (hbox);

    hbox = gtk_hbox_new(FALSE, 2);
    
    helpbutton = gtk_button_new_with_label ("Close");
    gtk_signal_connect_object (GTK_OBJECT (helpbutton), "clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT (gtkwin_chelp));
    gtk_box_pack_start (GTK_BOX (hbox), helpbutton, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (helpbutton);
    gtk_widget_show (hbox);

    gtk_widget_show (vbox);
    gtk_widget_show (gtkwin_chelp);
    gtk_text_insert (GTK_TEXT (chelptext), NULL, &chelptext->style->black, NULL, text , -1);    
  }
  else { 
    gdk_window_raise (gtkwin_chelp->window);
  }
}

/* Same simplistic help system. Serverside help this time. */

void shelpdialog(GtkWidget *widget) {
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
void new_menu_pickup(GtkWidget *button, int val)
{
  static unsigned int pmode=0;
  char modestr[128];

  /* widget is GtkCheckMenuItem */
  if(GTK_CHECK_MENU_ITEM (button)->active) pmode=pmode|val;
  else pmode=pmode&~val;

#if 0
  fprintf(stderr,"val=0x%8x\n",val);
  fprintf(stderr,"mode=0x%8x\n",pmode);
#endif

  sprintf(modestr,"bind pickup %u",pmode);
  draw_info("To set this pickup mode to a key, use:",NDI_BLACK);
  draw_info(modestr,NDI_BLACK);
  sprintf(modestr,"pickup %u",pmode);
  send_command(modestr, -1, 0);
}


void menu_pickup0 () {
  pickup_mode = 0;
  send_command("pickup 0", -1, 0);
}

void menu_pickup1 () {
  pickup_mode = 1;
  send_command("pickup 1", -1, 0);
}

void menu_pickup2 () {
  pickup_mode = 2;
  send_command("pickup 2", -1, 0);
}

void menu_pickup3 () {
  pickup_mode = 3;
  send_command("pickup 3", -1, 0);
}

void menu_pickup4 () {
  pickup_mode = 4;
  send_command("pickup 4", -1, 0);
}

void menu_pickup5 () {
  pickup_mode = 5;
  send_command("pickup 5", -1, 0);
  
}

void menu_pickup6 () {
  pickup_mode = 6;
  send_command("pickup 6", -1, 0);
}

void menu_pickup7 () {
  pickup_mode = 7;
  send_command("pickup 7", -1, 0);
}

void menu_pickup10 () {
  pickup_mode = 10;
  send_command("pickup 10", -1, 0);
}



void menu_who () {
  extended_command("who");
}

void menu_apply () {
  extended_command("apply");
}

void menu_cast () {
  gtk_entry_set_text(GTK_ENTRY(entrytext),"cast ");
  gtk_widget_grab_focus (GTK_WIDGET(entrytext));
}

void menu_search () {
  extended_command("search");
}

void menu_disarm () {
  extended_command("disarm");
}


void menu_spells () {
  char buf[MAX_BUF];
  int i;
  for (i=0; i<25 ; i++) {
    sprintf(buf,"Range: spell (%s)", cpl.spells[cpl.ready_spell]);

    /*    strcpy (buf, cpl.spells[i]);*/
    printf ("Spell: %s\n", cpl.spells[cpl.ready_spell]);
  }
}

void menu_clear () {
  guint size;
  
  size = gtk_text_get_length(GTK_TEXT (gtkwin_info_text));
  gtk_text_freeze (GTK_TEXT (gtkwin_info_text));
  gtk_text_set_point(GTK_TEXT (gtkwin_info_text), 0);
  gtk_text_forward_delete (GTK_TEXT (gtkwin_info_text), size );
  gtk_text_thaw (GTK_TEXT (gtkwin_info_text));

  size = gtk_text_get_length(GTK_TEXT (gtkwin_info_text2));
  gtk_text_freeze (GTK_TEXT (gtkwin_info_text2));
  gtk_text_set_point(GTK_TEXT (gtkwin_info_text2), 0);
  gtk_text_forward_delete (GTK_TEXT (gtkwin_info_text2), size );
  gtk_text_thaw (GTK_TEXT (gtkwin_info_text2));
}

void sexit()
{
    extended_command("quit");
}

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
			    GTK_SIGNAL_FUNC(exit), NULL);
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

  menu_items = gtk_check_menu_item_new_with_label("Inhibit autopickup");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_INHIBIT));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Stop before pickup");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_STOP));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Debug autopickup");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_DEBUG));
  gtk_widget_show(menu_items);


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

  menu_items = gtk_check_menu_item_new_with_label("Missile Weapons");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(weaponpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_MISSILEWEAPON));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Bows");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(weaponpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_BOW));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Arrows");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(weaponpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_ARROW));
  gtk_widget_show(menu_items);


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

  menu_items = gtk_check_menu_item_new_with_label("Shields");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_SHIELD));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Body Armour");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_ARMOUR));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Boots");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_BOOTS));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Gloves");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_GLOVES));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Cloaks");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(armourpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_CLOAK));
  gtk_widget_show(menu_items);


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

  menu_items = gtk_check_menu_item_new_with_label("Skillscrolls");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(bookspickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_SKILLSCROLL));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Normal Books/Scrolls");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(bookspickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_READABLES));
  gtk_widget_show(menu_items);


  /* continue with the rest of the stuff... */

  menu_items = gtk_check_menu_item_new_with_label("Food");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_FOOD));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Drinks");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_DRINK));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Valuables (Money, Gems)");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_VALUABLES));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Keys");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_KEY));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Magical Items");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_MAGICAL));
  gtk_widget_show(menu_items);

  menu_items = gtk_check_menu_item_new_with_label("Potions");
  gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(menu_items), TRUE);
  gtk_menu_append(GTK_MENU(newpickupmenu), menu_items);   
  gtk_signal_connect(GTK_OBJECT(menu_items), "activate",
	GTK_SIGNAL_FUNC(new_menu_pickup), GINT_TO_POINTER(PU_POTION));
  gtk_widget_show(menu_items);


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

  menu_items = gtk_menu_item_new_with_label("About");
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);   
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(aboutdialog), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new ();
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);   
  gtk_widget_show(menu_items);


  menu_items = gtk_menu_item_new_with_label("Client help");
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);   
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(chelpdialog), NULL);
  gtk_widget_show(menu_items);

  menu_items = gtk_menu_item_new_with_label("Server help");
  gtk_menu_append(GTK_MENU (helpmenu), menu_items);   
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(shelpdialog), NULL);
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

void create_splash() {
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


void destroy_splash() {
  gtk_widget_destroy(gtkwin_splash);
}

/* Error handlers removed.  Right now, there is nothing for
 * the client to do if it gets a fatal error - it doesn't have
 * any information to save.  And we might as well let the standard
 * X11 error handler handle non fatal errors.
 */

 
void create_windows() {
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
	gcw = gdk_char_width(style->font, '0') + 4;
	gch = gdk_char_height(style->font, '0') + 2;
    } else {
	/* These are what the old defaults values were */
	gcw = 11;
	gch = 10;
    }

    gtk_widget_set_events (gtkwin_root, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_root, 0, 0);

    fprintf(stderr, "Character Width : %d\n", gcw);
    fprintf(stderr, "Character Height: %d\n", gch);
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
    gtk_signal_connect (GTK_OBJECT (gtkwin_root), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_root);
    
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
    if (use_config[CONFIG_SDL]) 
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
    gtk_signal_connect (GTK_OBJECT (gtkwin_root), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_root);


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
    gtk_signal_connect (GTK_OBJECT (gtkwin_stats), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_stats);
    
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
    gtk_signal_connect (GTK_OBJECT (gtkwin_info), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_info);
    
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
    gtk_signal_connect (GTK_OBJECT (gtkwin_message), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_message);
    
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
    gtk_signal_connect (GTK_OBJECT (gtkwin_inv), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_inv);
    
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
    gtk_signal_connect (GTK_OBJECT (gtkwin_look), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_look);
    
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
    if (use_config[CONFIG_SDL])
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

    create_splash();
    /* we need to call gdk_rgb_init very early on, as some of the
     * create window functions may do call backs in which case we try
     * to draw the game window.
     */
    gdk_rgb_init();
    create_windows();

    return 0;
}
 
/* null procedures. gtk does this for us. */


void set_weight_limit (uint32 wlim)
{
    inv_list.weight_limit = wlim;
}


void set_scroll(char *s)
{
}


void set_autorepeat(char *s)
{
}


int get_info_width()
{
    return 40;	/* would be better to return some real info here  - I'll have to look at it later
		 * to see how easy it is to get that */
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

void do_clearlock () {
}

void x_set_echo() {
  if (!use_config[CONFIG_POPUPS]) {
    gtk_entry_set_visibility(GTK_ENTRY(entrytext), !cpl.no_echo);
  }
}

int do_timeout() {

  updatelock=0;
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
  if (redraw_needed) {
    display_map_doneupdate(TRUE);
    draw_all_list(&inv_list);
    draw_all_list(&look_list);
    redraw_needed=FALSE;
  }
  if (!inv_list.env->inv_updated) {
    animate_list();
  }
  if (cpl.showmagic) magic_map_flash_pos();
  draw_lists();
  return TRUE;
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
void draw_magic_map()
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
      fprintf(stderr,"magic map resolution less than 1, map is %dx%d\n",
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
      fprintf(stderr,"magic map resolution less than 1, map is %dx%d\n",
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


void save_winpos()
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

void command_show (char *params)
{
    if(!params)  {
	/* Shouldn't need to get current page, but next_page call is not wrapping
	 * like the docs claim it should.
	 */
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(inv_notebook))==8)
	    gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 0);
	else 
	    gtk_notebook_next_page(GTK_NOTEBOOK(inv_notebook));

    } else if (!strncmp(params, "all", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 0);
    else if (!strncmp(params, "applied", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 1);

    else if (!strncmp(params, "unapplied", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 2);

    else if (!strncmp(params, "unpaid", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 3);

    else if (!strncmp(params, "cursed", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 4);

    else if (!strncmp(params, "magical", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 5);

    else if (!strncmp(params, "nonmagical", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 6);

    else if (!strncmp(params, "locked", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 7);

    else if (!strncmp(params, "unlocked", strlen(params)))
	gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 8);


}

/* Reads in the winpos file created by the above function and sets the
 * the window positions appropriately.
 */
void set_window_pos()
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
	    else fprintf(stderr,"Found bogus line in window position file:\n%s %s\n", buf, cp);
	} else {
	    if (!strcmp(buf,"win_game:")) {
                gdk_window_move_resize(gtkwin_root->window, wx, wy, w, h);
		continue;
	    }
	    if (!want_config[CONFIG_SPLITWIN]) {
		fprintf(stderr,"Found bogus line in window position file:\n%s %s\n", buf, cp);
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

static void usage(char *progname)
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
    puts("-fog             - Enable for of war code");
    puts("-help            - Display this message.");
    puts("-iconscale %%    - Set icon scale percentage");
    puts("-mapscale %%     - Set map scale percentage");
    puts("-mapsize xXy     - Set the mapsize to be X by Y spaces. (default 11x11)");
    puts("-popups          - Use pop up windows for input (default)");
    puts("-nopopups        - Don't use pop up windows for input");
    puts("-port <number>   - Use port <number> instead of the standard port number");
    puts("-sdl             - Use sdl for drawing png (may not work on all hardware");
    puts("-server <name>   - Connect to <name> instead of localhost.");
    puts("-showicon        - Print status icons in inventory window");
    puts("-sound           - Enable sound output (default).");
    puts("-nosound         - Disable sound output.");
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

    strcpy(VERSION_INFO,"GTK Unix Client " VERSION);

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
		fprintf(stderr,"-display requires a display name\n");
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
		fprintf(stderr,"-faceset requires a faceset name/number\n");
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
		fprintf(stderr,"-iconscale requires a percentage value\n");
		return 1;
	    }
	    want_config[CONFIG_ICONSCALE] = atoi(argv[on_arg]);
	    if (want_config[CONFIG_ICONSCALE] < 25 || want_config[CONFIG_ICONSCALE]>200) {
		fprintf(stderr,"Valid range for -iconscale is 25 through 200\n");
		want_config[CONFIG_ICONSCALE]=100;
		return 1;
	    }
	    continue;
	}
	else if( !strcmp( argv[on_arg],"-mapscale")) {
	    if (++on_arg == argc) {
		fprintf(stderr,"-mapscale requires a percentage value\n");
		return 1;
	    }
	    want_config[CONFIG_MAPSCALE] = atoi(argv[on_arg]);
	    if (want_config[CONFIG_MAPSCALE] < 25 || want_config[CONFIG_MAPSCALE]>200) {
		fprintf(stderr,"Valid range for -mapscale is 25 through 200\n");
		want_config[CONFIG_MAPSCALE]=100;
		return 1;
	    }
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-mapsize")) {
	    char *cp, x, y=0;
	    if (++on_arg == argc) {
		fprintf(stderr,"-mapsize requires a XxY value\n");
		return 1;
	    }
	    x = atoi(argv[on_arg]);
	    for (cp = argv[on_arg]; *cp!='\0'; cp++)
		if (*cp == 'x' || *cp == 'X') break;

	    if (*cp==0) {
		fprintf(stderr,"-mapsize requires both and X and Y value (ie, XxY - note the\nx in between.\n");
	    } else {
		y = atoi(cp+1);
	    }
	    if (x<9 || y<9) {
		fprintf(stderr,"map size must be positive values of at least 9\n");
	    } else if (x>MAP_MAX_SIZE || y>MAP_MAX_SIZE) {
		fprintf(stderr,"Map size can not be larger than %d x %d \n", MAP_MAX_SIZE, MAP_MAX_SIZE);

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
		fprintf(stderr,"-port requires a port number\n");
		return 1;
	    }
	    want_config[CONFIG_PORT] = atoi(argv[on_arg]);
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-sdl")) {
#ifndef HAVE_SDL
	    fprintf(stderr,"client not compiled with sdl support.  Ignoring -sdl\n");
#else
	    want_config[CONFIG_SDL] = TRUE;
#endif
	    continue;
	}
	else if (!strcmp(argv[on_arg],"+sdl")) {
	    want_config[CONFIG_SDL] = FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-server")) {
	    if (++on_arg == argc) {
		fprintf(stderr,"-server requires a host name\n");
		return 1;
	    }
	    server = argv[on_arg];
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-showicon")) {
	    want_config[CONFIG_SHOWICON] = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-sound")) {
	    want_config[CONFIG_SOUND] = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nosound")) {
	    want_config[CONFIG_SOUND] = FALSE;
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
		fprintf(stderr,"-resists requires a value\n");
		return 1;
	    }
	    want_config[CONFIG_RESISTS]=atoi(argv[on_arg]);
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
	else {
	    fprintf(stderr,"Do not understand option %s\n", argv[on_arg]);
	    usage(argv[0]);
	    return 1;
	}
    }

    /* Now copy over the values just loaded */
    for (on_arg=0; on_arg<CONFIG_NUMS; on_arg++) {
        use_config[on_arg] = want_config[on_arg];
    }

    image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_ICONSCALE] / 100;
    map_image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 100;
    map_image_half_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 200;
    inv_list.show_icon = use_config[CONFIG_SHOWICON];
    if (!use_config[CONFIG_CACHE]) use_config[CONFIG_DOWNLOAD] = FALSE;

    allocate_map( &the_map, FOG_MAP_SIZE, FOG_MAP_SIZE);
    pl_pos.x= the_map.x / 2;
    pl_pos.y= the_map.y / 2;


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
    if (want_config[CONFIG_CACHE]) init_cache_data();
    destroy_splash();

    return 0;
}


/* Do the map drawing */
void display_map_doneupdate(int redraw)
{

    if (updatelock < 30) {
	updatelock++;

#ifdef HAVE_SDL
	if (use_config[CONFIG_SDL]) sdl_gen_map(redraw);
	else
#endif
	gtk_draw_map(redraw);

    } /* if updatelock */

}

void display_map_newmap()
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
    if (use_config[CONFIG_SDL])
	init_SDL( drawingarea, FALSE);
#endif

}


void display_map_startupdate()
{
}

char *get_metaserver()
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


int main(int argc, char *argv[])
{
    int got_one=0;

    /* This needs to be done first.  In addition to being quite quick,
     * it also sets up some paths (client_libdir) that are needed by
     * the other functions.
     */

    init_client_vars();
    
    /* Call this very early.  It should parse all command
     * line arguments and set the pertinent ones up in
     * globals.  Also call it early so that if it can't set up
     * the windowing system, we get an error before trying to
     * to connect to the server.  And command line options will
     * likely change on the server we connect to.
     */
    if (init_windows(argc, argv)) {	/* x11.c */
	fprintf(stderr,"Failure to init windows.\n");
	exit(1);
    }
    csocket.inbuf.buf=malloc(MAXSOCKBUF);

#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX);
#else
    maxfd = getdtablesize();
#endif

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

	if (!strcmp(server, SERVER) || got_one) {
	    char *ms;
	    metaserver_get_info(meta_server, meta_port);
	    metaserver_show(TRUE);
	    do {
		ms=get_metaserver();
	    } while (metaserver_select(ms));
	    negotiate_connection(use_config[CONFIG_SOUND]);
	} else {
	    csocket.fd=init_connection(server, use_config[CONFIG_PORT]);
	    if (csocket.fd == -1) { /* specified server no longer valid */
		server = SERVER;
		continue;
	    }
	    negotiate_connection(use_config[CONFIG_SOUND]);
	}

	got_one=1;
	event_loop();
	/* if event_loop has exited, we most of lost our connection, so we
	 * loop again to establish a new one.
	 */

	/* Need to reset the images so they match up properly and prevent
	 * memory leaks.
	 */
	reset_image_data();
	remove_item_inventory(cpl.ob);
	/* We know the following is the private map structure in
	 * item.c.  But we don't have direct access to it, so
	 * we still use locate.
	 */
	remove_item_inventory(locate_item(0));
	reset_map_data();
	look_list.env=cpl.below;
    }
    exit(0);	/* never reached */
}
