/*
 * static char *rcsid_xio_c =
 *   "$Id$";
 *
 * This file handles all the windowing stuff.  The idea is
 * that all of it is in one file, so to port to different systems
 * or toolkits, only this file needs to be updated.  All windowing
 * variables (display, gc's, windows, etc), should be stored in
 * this file as statics.
 *
 * This file is largely a combination of the common/xutil.c and server/xio.c
 * file.  While I don't think this was a particulary great interface, the code
 * was there, and I figured it was probably easier to re-use that
 * code instead of writing new code, plus the old code worked.
 *
 */

/*
    CrossFire, A Multiplayer game for X-windows

    Copyright (C) 1994 Mark Wedel
    Copyright (C) 1992 Frank Tore Johansen

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

    The author can be reached via e-mail to mark@pyramid.com
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "client.h"
/*#include "clientbmap.h"*/
#include "item.h"
#include "def-keys.h"
#include <X11/keysym.h>
#include "pixmaps/crossfiretitle.xpm"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>

/* gtk */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#define MAX_BUF 256

/* All the following are static because these variables should
 * be local only to this file.  Since the idea is to have only
 * this file be replaced for different windowing systems, use of
 * any of these variables anyplace else would not be portable.
 */
typedef enum inventory_show {
  show_all = 0, show_applied = 0x1, show_unapplied = 0x2, show_unpaid = 0x4,
  show_cursed = 0x8, show_magical = 0x10, show_nonmagical = 0x20,
  show_locked = 0x40,
  show_mask=0x7f
} inventory_show;


/*
 *  This is similar obwin, but totally redone for client
 */
typedef struct {
  item *env;		  /* Environment shown in window */
  char title[MAX_BUF];  /* title of item list */
  char old_title[MAX_BUF];  /* previos title (avoid redrawns) */
  
  Window win;		  /* for X-windows */
  GtkWidget *label;
  GtkWidget *weightlabel;
  GtkWidget *maxweightlabel;

  /*  gint invrows;
  gint appliedrows;
  gint unappliedrows;
  gint unpaidrows;
  gint cursedrows;
  gint magicalrows;
  gint nonmagicalrows;
  gint lockedrows;*/

  float pos[8];

  GtkWidget *gtk_list[8];
  GtkWidget *gtk_lists[8];

  GC gc_text;
  GC gc_icon;
  GC gc_status;
  
  uint8 multi_list:1;     /* view is multi type */
  uint8 show_icon:1;	  /* show status icons */
  uint8 show_weight:1;  /* show item's weight */
  
  char format_nw[20];	  /* sprintf-format for text (name and weight) */
  char format_nwl[20];    /* sprintf-format for text (name, weight, limit) */
  char format_n[20];	  /* sprintf-format for text (only name) */
  sint16 text_len;	  /* How wide the text-field is */
  
  sint16 width;	  /* How wide the window is in pixels */
  sint16 height;	  /* How height the window is in pixels */
   
  sint16 item_pos;	  /* The sequence number of the first drawn item */
  sint16 item_used;	  /* How many items actually drawn. (0 - size) */
  
  sint16 size;	  /* How many items there is room to display */
  sint16 *faces;	  /* [size] */
  sint8 *icon1;	  /* status icon : locked */
  sint8 *icon2;	  /* status icon : applied / unpaid */
  sint8 *icon3;	  /* status icon : magic */
  sint8 *icon4;	  /* status icon : damned / cursed */
  char **names;	  /* [size][NAME_LEN] */
  
  /* The scrollbar */
  sint16 bar_length; 	  /* the length of scrollbar in pixels */
  sint16 bar_size;	  /* the current size of scrollbar in pixels */
  sint16 bar_pos;	  /* the starting position of scrollbar in pixels */
  inventory_show show_what;   /* What to show in inventory */
  uint32 weight_limit;   /* Weight limit for this list - used for title */
} itemlist;


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

/*typedef struct {
  gchar *command;
} historyitem;
*/


GList *anim_list=NULL;
/*GList *history=NULL;*/

extern int maxfd;
struct timeval timeout;


 /* Defined in global.h */
#define SCROLLBAR_WIDTH	16	/* +2+2 for border on each side */
#define INFOCHARS 50
#define INFOLINES 36
#define FONTWIDTH 8
#define FONTHEIGHT 13
#define MAX_INFO_WIDTH 80
#define MAXNAMELENGTH 50
#define WINUPPER (-5)
#define WINLOWER 5
#define WINLEFT (-5)
#define WINRIGHT 5

static int gargc;

Display_Mode display_mode = Xpm_Display;
static char cache_images=FALSE;

/* This struct contains the information to draw 1 line of data. */
typedef struct {
    char	*info;		/* Actual character data for a line */
    uint8	color;		/* Color to draw that line */
} InfoLine;

/* This contains all other information for the info window */
typedef struct {
    uint16	info_chars;	/* width in chars of info window */
    uint16	infopos;	/* Where in the info arry to put new data */
    uint16	infoline;	/* Where on the window to draw the line */
    uint16	scroll_info_window:1;  /* True if we should scroll the window */
    uint16	numlines;	/* How many have been stored the array */
    uint16	maxlines;	/* Maxlines (how large the array below is) */
    uint16	maxdisp;	/* How many lines can be displayed at once */
    uint8	lastcolor;	/* Last color text was drawn in */
    InfoLine	*data;		/* An array of lines */
    Window	win_info;	/* Actual info window */
    GC		gc_info;	/* GC for this window */
    /* The scrollbar */
    sint16 bar_length; 	  /* the max length of scrollbar in pixels */
    sint16 bar_size;	  /* the current size (length) of scrollbar in pixels */
    sint16 bar_pos;	  /* the starting position of scrollbar.  This is
			   * an offset, which is the number of lines from
			   * 0 for the text to end out.*/
    sint16 bar_y;	  /* X starting position of scrollbar */
    uint16	has_scrollbar:1;/* True if there is a scrollbar in the window */
    sint16	width,height; /* Width and height of window */
} InfoData;

static InfoData infodata = {0, 0, 0, 0, 0, INFOLINES, INFOLINES, NDI_BLACK,
	NULL, 0, 0,0,0,0,0,0,0,0};

static uint8	
	split_windows=FALSE;
/*	iscolor = TRUE;*/

static uint8 color_inv=FALSE;
static uint8 color_text=FALSE;
static uint8 tool_tips=FALSE;

#define MAXFACES 5
#define MAXPIXMAPNUM 10000
struct MapCell {
  short faces[MAXFACES];
  int count;
};

struct Map {
  struct MapCell cells[11][11];
};

struct PixmapInfo {
  Pixmap pixmap,mask;
  Pixmap bitmap;
  long fg,bg;
  GdkPixmap *gdkpixmap;
  GdkBitmap *gdkmask;
  GdkPixmap *gdkbitmap;
};

struct GtkMap {
  GtkWidget *pixmap;
  GdkPixmap *gdkpixmap;
  GdkBitmap *gdkmask;
  GdkGC *gc;
};


typedef struct {
  int x;
  int y;
} MapPos;

static struct GtkMap gtkmap [11][11];

static GdkPixmap *gdkmask[11][11];
static MapPos mappos [11][11];

/* vitals window */

typedef struct {
  GtkWidget *bar;
  GtkStyle *style;
  int state;
} Vitals;

static Vitals vitals[4];
static GtkWidget *run_label, *fire_label;
static GtkWidget *ckentrytext, *ckeyentrytext, *cmodentrytext, *cnumentrytext;

GdkColor gdk_green =    { 0, 0, 0xcfff, 0 };
GdkColor gdk_red =    { 0, 0xcfff, 0, 0 };
GdkColor gdk_grey = { 0, 0xea60, 0xea60, 0xea60 };
GdkColor gdk_black = { 0, 0, 0, 0 };

GdkColor gdkdiscolor;
static GdkColor map_color[16];
static GdkColor root_color[16];
static GdkPixmap *magicgdkpixmap;
static GdkGC *map_gc;
static GtkWidget *mapvbox;

static struct Map the_map;

static struct PixmapInfo pixmaps[MAXPIXMAPNUM];
#define INFOLINELEN 500
#define XPMGCS 100

static GtkWidget *ccheckbutton1;
static GtkWidget *ccheckbutton2;
static GtkWidget *ccheckbutton3;
static GtkWidget *ccheckbutton4;
static GtkWidget *ccheckbutton5;
static GtkWidget *ccheckbutton6;

static GtkTooltips *tooltips;

static GtkWidget *dialogtext;
static GtkWidget *dialog_window;
static GtkWidget *table;
	  
static GtkWidget *cclist;
static gboolean draw_info_freeze=FALSE;

enum {
    locked_icon = 1, applied_icon, unpaid_icon,
    damned_icon, cursed_icon, magic_icon, close_icon, 
    stipple1_icon, stipple2_icon, max_icons
};


static GtkWidget *entrytext, *counttext;
static gint redraw_needed=FALSE;

/*
 * These are used for inventory and look window
 */
static itemlist look_list, inv_list;
static StatWindow statwindow;
/* gtk */
 
static GtkWidget *gtkwin_root,  *gtkwin_info_text;
static GtkWidget *gtkwin_stats, *gtkwin_message, *gtkwin_info, *gtkwin_look,*gtkwin_info_text, *gtkwin_inv;


/*static GtkWidget *gtkwin_history = NULL;*/
static GtkWidget *gtkwin_about = NULL;
static GtkWidget *gtkwin_splash = NULL;
static GtkWidget *gtkwin_chelp = NULL;
static GtkWidget *gtkwin_shelp = NULL;
static GtkWidget *gtkwin_magicmap = NULL;
/*static GtkWidget *history_list = NULL;*/
static GtkWidget *gtkwin_config = NULL;

static char *last_str;

static int pickup_mode = 0;

int updatelock = 0;

/* info win */
#define INFOCHARS 50
#define FONTWIDTH 8

/* this is used for caching the images across runs.  When we get a face
 * command from the server, we check the facecache for that name.  If
 * so, we can then use the num to find out what face number it is on the
 * local side.  the facecachemap does something similar - when the
 * server sends us saying to draw face XXX (server num), the facecacehmap
 * can then be used to determine what XXX is locally.  If not caching,
 * facecachemap is just a 1:1 mapping.
 */
struct FaceCache {
    char    *name;
    uint16  num;
} facecache[MAXPIXMAPNUM];

uint16 facecachemap[MAXPIXMAPNUM], cachelastused=0, cacheloaded=0;

FILE *fcache;

/*#include <xutil.c>*/

int misses=0,total=0;

void create_windows (void);


/* Convert xpm memory buffer to xpm data (needed since GTK/GDK doesnt have a
 * function to create from buffer rather than data
*/

char **xpmbuffertodata (char *buffer) {
  char *buf=NULL;
  char **strings=NULL;
  int i=0,q=0,z=0;

  for (i=1; buffer[i]!=';' ;i++) {
    if (buffer[i]=='"') {
      z=0;
      for (i++; buffer[i]!='"';i++) {
        buf=(char *)realloc(buf, (z+2)*sizeof(char));
        buf[z]=buffer[i];
        z++; 
      }
      buf[z]='\0';
      strings=(char **)realloc(strings, (q+2)*sizeof(char *));
      strings[q]=(char *)strdup(buf);    
      q++;
    }
  }
  strings=(char **)realloc(strings, (q+2)*sizeof(char *));
  strings[q]=(char *)NULL;
  free(buf);
  buf=NULL;
  return (strings);
} 

/* free the memeory allocated for the xpm data */

void freexpmdata (char **strings) {
  int q=0;
  for (q=0; strings[q]!=NULL ; q++) {
    free (strings[q]);
  }
  free (strings);
}
	
/* main loop iteration related stuff */
void do_network() {
  fd_set tmp_read, tmp_exceptions;
  int pollret;
  extern int updatelock;
  
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
    fd_set tmp_read, tmp_exceptions;  
    gint flerp,fleep;
    extern int do_timeout();

    if (MAX_TIME==0) {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
    }
    maxfd = csocket.fd + 1;
    while (1) {
	FD_ZERO(&tmp_read);
	FD_ZERO(&tmp_exceptions);
	FD_SET(csocket.fd, &tmp_read);
	FD_SET(csocket.fd, &tmp_exceptions);

	if (MAX_TIME!=0) {
 	    timeout.tv_sec = 0;/* MAX_TIME / 1000000;*/
	    timeout.tv_usec = 0;/* MAX_TIME % 1000000;*/
	}

	fleep =  gtk_timeout_add (100,
				  (GtkFunction) do_timeout,
				  NULL);
	
        flerp = gdk_input_add ((gint) csocket.fd,
                              GDK_INPUT_READ,
                              (GdkInputFunction) do_network, &csocket);
	gtk_main();
    }
}

/* Do the pixmap copy with gc to tile it onto the stack in the cell */



static void gen_draw_face(int face,int x,int y)
{
  gdk_gc_set_clip_mask (gtkmap[x][y].gc, pixmaps[facecachemap[face]].gdkmask);
  gdk_window_copy_area (gtkmap[x][y].gdkpixmap, gtkmap[x][y].gc, 0, 0, pixmaps[facecachemap[face]].gdkpixmap,0,0,24,24);
}

void end_windows()
{
  free(last_str);

  /*    XFreeGC(display, gc_root);
    XFreeGC(display, gc_game);
    XFreeGC(display, gc_stats);
    XFreeGC(display, infodata.gc_info);
    XFreeGC(display, inv_list.gc_text);
    XFreeGC(display, inv_list.gc_icon);
    XFreeGC(display, inv_list.gc_status);
    XFreeGC(display, look_list.gc_text);
    XFreeGC(display, look_list.gc_icon);
    XFreeGC(display, look_list.gc_status);
    XFreeGC(display, gc_message);
    if (display_mode==Xpm_Display) {
	XFreeGC(display, gc_xpm_object);
    }
    XDestroyWindow(display,win_game);
    XCloseDisplay(display);*/
}

/*static historyitem *newhistoryitem(gchar *text) {
  historyitem *op = malloc (sizeof(historyitem));
  gchar *command = malloc (strlen(text));
  strcpy (command, text);
  op->command=command;
  return op;
  }*/


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

/*void printhistory (historyitem *data, gpointer user_data) {
  gchar buf[MAX_BUF];
  strcpy (buf, data->command);
    printf ("History: %s \n", buf);
}

void addhistorylist (historyitem *data, gpointer user_data) {
  gint tmprow;
  tmprow = gtk_clist_append (GTK_CLIST (history_list), &data->command);
  gtk_clist_set_row_data (GTK_CLIST(history_list), tmprow, data);

}


void addhistoryitem(gchar *text) {
  gint tmprow;
  historyitem *tmphist;
  tmphist=newhistoryitem(text);
  history = g_list_append (history, tmphist);
  if(gtkwin_history) {
     tmprow = gtk_clist_append (GTK_CLIST (history_list), &tmphist->command);
     gtk_clist_set_row_data (GTK_CLIST(history_list), tmprow, tmphist);
  }
  
}

*/
/* Free allocations for animations */

void freeanimview (gpointer data, gpointer user_data) {
  if (data)
    g_free (data);
}

void freeanimobject (animobject *data, gpointer user_data) {
  if (data)
    g_list_foreach (data->view, freeanimview, 0);
    g_free (data);
}

/* Update the pixmap */

void animateview (animview *data, gint user_data) {
      gtk_clist_set_pixmap (GTK_CLIST (data->list), data->row, 0,
			    pixmaps[facecachemap[user_data]].gdkpixmap,
			    pixmaps[facecachemap[user_data]].gdkmask);
}

/* Run through the animations and update them */

void animate (animobject *data, gpointer user_data) {
  if (data) {
    data->item->last_anim++;
    if (data->item->last_anim>=data->item->anim_speed) {
      data->item->anim_state++;
      if (data->item->anim_state >= animations[data->item->animation_id].num_animations) {
	data->item->anim_state=0;
      }
      data->item->face = animations[data->item->animation_id].faces[data->item->anim_state];
      data->item->last_anim=0;
      g_list_foreach (data->view, (GFunc) animateview, (gpointer)data->item->face);
      
    }
    
  }
}

/* Run through the lists of animation and do each */

void animate_list () {
  if (anim_list) {
    g_list_foreach     (anim_list, (GFunc) animate, NULL);
  }
}




/*static void history_button_event (GtkWidget *gtklist, GdkEventButton *event)
{
  GList *node;
  historyitem *tmp;
  if (event->type==GDK_BUTTON_PRESS && event->button==1) {
    for (node =  GTK_CLIST(gtklist)->selection ; node ; node = node->next) {
      tmp = gtk_clist_get_row_data (GTK_CLIST(gtklist), (gint)node->data); 
      extended_command(tmp->command);
      printf("History command: %s\n", tmp->command);
    }
  }

  if (event->type==GDK_BUTTON_PRESS && event->button==3) {

    if (GTK_CLIST(gtklist)->selection) {
      node =  GTK_CLIST(gtklist)->selection;
      tmp = gtk_clist_get_row_data (GTK_CLIST(gtklist), (gint)node->data); 
      gtk_clist_remove (GTK_CLIST(gtklist), (gint)node->data);

      free (tmp->command);

      history = g_list_remove (history, tmp); 
      free (tmp);
    }
  }
  
}
*/


/* Handle mouse presses in the game window */



void button_map_event(GtkWidget *widget, GdkEventButton *event, MapPos *pos) {
  int i;
  printf("Button clicked in map, x %d y %d\n", pos->x, pos->y );
  if (event->type==GDK_BUTTON_PRESS && event->button==1) {
    look_at(pos->x, pos->y);
  }
  if (event->type==GDK_BUTTON_PRESS && event->button==2) {
    if  (pos->x<0)
      i=0;
    else if (pos->x>0)
      i=6;
    else i=3;
    if (pos->y>0)
      i+=2;
    else if (pos->y>-1)
      i++;
    switch(i) {
    case 0: fire_dir(8); break;
    case 1: fire_dir(7); break;
    case 2: fire_dir(6); break;
    case 3: fire_dir(1); break;
    case 5: fire_dir(5); break;
    case 6: fire_dir(2); break;
    case 7: fire_dir(3); break;
    case 8: fire_dir(4); break;
    }
    stop_fire();
    clear_fire_run();
  }
  if (event->type==GDK_BUTTON_PRESS && event->button==3) {
    if  (pos->x<0)
      i=0;
    else if (pos->x>0)
      i=6;
    else i=3;
    if (pos->y>0)
      i+=2;
    else if (pos->y>-1)
      i++;
    switch(i) {
    case 0: move_player(8); break;
    case 1: move_player(7); break;
    case 2: move_player(6); break;
    case 3: move_player(1); break;
    case 5: move_player(5); break;
    case 6: move_player(2); break;
    case 7: move_player(3); break;
    case 8: move_player(4); break;
    }
  }
}









/***********************************************************************
 *
 * Key board input translations are handled here.  We don't deal with
 * the events, but rather KeyCodes and KeySyms.
 *
 * It would be nice to deal with only KeySyms, but many keyboards
 * have keys that do not correspond to a KeySym, so we do need to
 * support KeyCodes.
 *
 ***********************************************************************/


static KeyCode firekey[2], runkey[2], commandkey, *bind_keycode;
static KeySym firekeysym[2], runkeysym[2], commandkeysym,*bind_keysym;
static int bind_flags=0;
static char bind_buf[MAX_BUF];

#define KEYF_NORMAL	0x01	/* Used in normal mode */
#define KEYF_FIRE	0x02	/* Used in fire mode */
#define KEYF_RUN	0x04	/* Used in run mode */
#define KEYF_MODIFIERS	0x07	/* Mask for actual keyboard modifiers, */
				/* not action modifiers */
#define KEYF_EDIT	0x08	/* Line editor */
#define KEYF_STANDARD	0x10	/* For standard (built in) key definitions */

extern char *directions[9];


typedef struct Keys {
    uint8	flags;
    sint8	direction;
    KeySym	keysym;
    char	*command;
    struct Keys	*next;
} Key_Entry;

/* Key codes can only be from 8-255 (at least according to
 * the X11 manual.  This is easier than using a hash
 * table, quicker, and doesn't use much more space.
 */

#define MAX_KEYCODE 255
static Key_Entry *keys[256];


void load_defaults()
{
    char path[MAX_BUF],inbuf[MAX_BUF],*cp;
    FILE *fp;

    sprintf(path,"%s/.crossfire/gdefaults", getenv("HOME"));
    if ((fp=fopen(path,"r"))==NULL) return;
    while (fgets(inbuf, MAX_BUF-1, fp)) {
	inbuf[MAX_BUF-1]='\0';
	inbuf[strlen(inbuf)-1]='\0';	/* kill newline */

	if (inbuf[0]=='#') continue;
	/* IF no colon, then we certainly don't have a real value, so just skip */
	if (!(cp=strchr(inbuf,':'))) continue;
	*cp='\0';
	cp+=2;	    /* colon, space, then value */

	if (!strcmp(inbuf, "port")) {
	    port_num = atoi(cp);
	    continue;
	}
	if (!strcmp(inbuf, "server")) {
	    server = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	if (!strcmp(inbuf,"display")) {
	    if (!strcmp(cp,"xpm")) 
		display_mode=Xpm_Display;
	    else if (!strcmp(cp,"pixmap"))
		display_mode = Pix_Display;
	    else fprintf(stderr,"Unknown display specication in %s, %s",
			   path, cp);
	    continue;
	}
	if (!strcmp(inbuf,"cacheimages")) {
	    if (!strcmp(cp,"True")) cache_images=TRUE;
	    else cache_images=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"split")) {
	    if (!strcmp(cp,"True")) split_windows=TRUE;
	    else split_windows=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"showicon")) {
	    if (!strcmp(cp,"True")) inv_list.show_icon=TRUE;
	    else inv_list.show_icon=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"scrolllines")) {
	    infodata.maxlines = atoi(cp);
	    continue;
	}
	if (!strcmp(inbuf,"scrollinfo")) {
	    if (!strcmp(cp,"True")) infodata.scroll_info_window=TRUE;
	    else infodata.scroll_info_window=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"sound")) {
	    if (!strcmp(cp,"True")) nosound=FALSE;
	    else nosound=TRUE;
	    continue;
	}
	if (!strcmp(inbuf,"colorinv")) {
	    if (!strcmp(cp,"True")) color_inv=TRUE;
	    else color_inv=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"colortext")) {
	    if (!strcmp(cp,"True")) color_text=TRUE;
	    else color_text=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"tooltips")) {
	  if (!strcmp(cp,"True")) tool_tips=TRUE;
	  else tool_tips=FALSE;
	  continue;
	}  
	fprintf(stderr,"Got line we did not understand: %s: %s", inbuf, cp);
    }
    fclose(fp);

}

void save_defaults()
{
    char path[MAX_BUF],buf[MAX_BUF];
    FILE *fp;

    sprintf(path,"%s/.crossfire/gdefaults", getenv("HOME"));
    if (make_path_to_file(path)==-1) {
	fprintf(stderr,"Could not create %s\n", path);
	return;
    }
    if ((fp=fopen(path,"w"))==NULL) {
	fprintf(stderr,"Could not open %s\n", path);
	return;
    }
    fprintf(fp,"# This file is generated automatically by cfclient.\n");
    fprintf(fp,"# Manually editing is allowed, however cfclient may be a bit finicky about\n");
    fprintf(fp,"# some of the matching it does.  all comparissons are case sensitive.\n");
    fprintf(fp,"# 'True' and 'False' are the proper cases for those two values.\n");

    fprintf(fp,"port: %d\n", port_num);
    fprintf(fp,"server: %s\n", server);
    if (display_mode==Xpm_Display) {
	fprintf(fp,"display: xpm\n");
    } else if (display_mode==Pix_Display) {
	fprintf(fp,"display: pixmap\n");
    }
    fprintf(fp,"cacheimages: %s\n", cache_images?"True":"False");
    fprintf(fp,"split: %s\n", split_windows?"True":"False");
    fprintf(fp,"showicon: %s\n", inv_list.show_icon?"True":"False");
    fprintf(fp,"scrolllines: %d\n", infodata.maxlines);
    fprintf(fp,"scrollinfo: %s\n", infodata.scroll_info_window?"True":"False");
    fprintf(fp,"sound: %s\n", nosound?"False":"True");
    fprintf(fp,"colorinv: %s\n", color_inv?"True":"False");
    fprintf(fp,"colortext: %s\n", color_text?"True":"False");
    fprintf(fp,"tooltips: %s\n", color_text?"True":"False");
    fclose(fp);
    sprintf(buf,"Defaults saved to %s",path);
    draw_info(buf,NDI_BLUE);
    /* Save the gcfclient specifics */
}


/* Updates the keys array with the keybinding that is passed.  All the
 * arguments are pretty self explanatory.  flags is the various state
 * that the keyboard is in.
 */
static void insert_key(KeySym keysym, KeyCode keycode, int flags, char *command)
{

    Key_Entry *newkey;
    int i, direction=-1;

    if (keycode>MAX_KEYCODE) {
	fprintf(stderr,"Warning insert_key:keycode that is passed is greater than 255.\n");
	keycode=0;	/* hopefully the rest of the data is OK */
    }
    if (keys[keycode]==NULL) {
	keys[keycode]=malloc(sizeof(Key_Entry));
	keys[keycode]->command=NULL;
	keys[keycode]->next=NULL;
    }
    newkey=keys[keycode];

    /* Try to find out if the command is a direction command.  If so, we
     * then want to keep track of this fact, so in fire or run mode,
     * things work correctly.
     */
    for (i=0; i<9; i++)
	if (!strcmp(command, directions[i])) {
		direction=i;
		break;
	}

    if (keys[keycode]->command!=NULL) {
	/* if keys[keycode]->command is not null, then newkey is
	 * the same as keys[keycode]->command.
	 */
	while (newkey->next!=NULL)
	    newkey = newkey->next;
	newkey->next = malloc(sizeof(Key_Entry));
	newkey = newkey->next;
	/* This is the only initializing we need to do - the other fields
	 * will get filled in by the passed parameters
	 */
	newkey->next = NULL;
    }
    newkey->keysym = keysym;
    newkey->flags = flags;
    newkey->command = strdup_local(command);
    newkey->direction = direction;
}


static void parse_keybind_line(char *buf, int line, int standard)
{
    char *cp, *cpnext;
    KeySym keysym;
    KeyCode keycode;
    int flags;

    if (buf[0]=='#' || buf[0]=='\n') return;
    if ((cpnext = strchr(buf,' '))==NULL) {
	fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line,buf);
	return;
    }
    if (standard) standard=KEYF_STANDARD;
    else standard=0;

    *cpnext++ = '\0';
    keysym = XStringToKeysym(buf);
    cp = cpnext;
    if ((cpnext = strchr(cp,' '))==NULL) {
	fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line, cp);
	return;
    }
    *cpnext++ = '\0';

    /* If we can, convert the keysym into a keycode.  */
    keycode = atoi(cp);
    if (keysym!=NoSymbol) {
        keycode = XKeysymToKeycode(GDK_DISPLAY(), keysym);

        /* It is possible that we get a keysym that we can not convert
         * into a keycode (such a case might be binding the key on
         * one system, and later trying to run on another system that
         * doesn't have that key.
         * While the client will not be able to use it this invocation,
         * it may be able to use it in the future.  As such, don't throw
         * it away, but at least print a warning message.
         */
        if (keycode==0) {
	fprintf(stderr,"Warning: could not convert keysym %s into keycode, ignoring\n",
		buf);
        }
    }
    cp = cpnext;
    if ((cpnext = strchr(cp,' '))==NULL) {
	fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line, cp);
	return;
    }
    *cpnext++ = '\0';
    flags = 0;
    while (*cp!='\0') {
        switch (*cp) {
	case 'A':
		flags |= KEYF_NORMAL | KEYF_FIRE | KEYF_RUN;
		break;
	case 'N':
		flags |= KEYF_NORMAL;
		break;
	case 'F':
		flags |= KEYF_FIRE;
		break;
	case 'R':
		flags |= KEYF_RUN;
		break;
	case 'E':
		flags |= KEYF_EDIT;
		break;
	case 'S':
		flags |= KEYF_STANDARD;
		break;
	default:
	    fprintf(stderr,"Warning:  Unknown flag (%c) line %d in key binding file\n",
		*cp, line);
        }
        cp++;
    }
    /* Rest of the line is the actual command.  Lets kill the newline */
    cpnext[strlen(cpnext)-1]='\0';
    insert_key(keysym, keycode, flags | standard, cpnext);
}

static void init_default_keybindings()
{
  char buf[MAX_BUF];
  int i;

  for(i=0;i< sizeof(def_keys)/sizeof(char *);i++) {
    strcpy(buf,def_keys[i]);
    parse_keybind_line(buf,i,1);
  }
}


/* This reads in the keybindings, and initializes any special values.
 * called by init_windows.
 */

static void init_keys()
{
    int i, line=0;
    FILE *fp;
    char buf[MAX_BUF];

    commandkeysym = XK_apostrophe;
    commandkey = XKeysymToKeycode(GDK_DISPLAY(),XK_apostrophe);
    if (!commandkey) {
      commandkeysym =XK_acute;
      commandkey = XKeysymToKeycode(GDK_DISPLAY(), XK_acute);
    }
    firekeysym[0] = XK_Shift_L;
    firekey[0] = XKeysymToKeycode(GDK_DISPLAY(), XK_Shift_L);
    firekeysym[1] = XK_Shift_R;
    firekey[1] = XKeysymToKeycode(GDK_DISPLAY(), XK_Shift_R);
    runkeysym[0] = XK_Control_L;
    runkey[0] = XKeysymToKeycode(GDK_DISPLAY(), XK_Control_L);
    runkeysym[1] = XK_Control_R;
    runkey[1] = XKeysymToKeycode(GDK_DISPLAY(), XK_Control_R);

    for (i=0; i<=MAX_KEYCODE; i++) {
	keys[i] = NULL;
    }

    /* We now try to load the keybindings.  First place to look is the
     * users home directory, "~/.crossfire/keys".  Using a directory
     * seems like a good idea, in the future, additional stuff may be
     * stored.
     *
     * The format is described in the def_keys file.  Note that this file
     * is the same as what it was in the server distribution.  To convert
     * bindings in character files to this format, all that needs to be done
     * is remove the 'key ' at the start of each line.
     *
     * We need at least one of these keybinding files to exist - this is
     * where the various commands are defined.  In theory, we actually
     * don't need to have any of these defined -- the player could just
     * bind everything.  Probably not a good idea, however.
     */

    sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
    if ((fp=fopen(buf,"r"))==NULL) {
	fprintf(stderr,"Could not open ~/.crossfire/keys, trying to load global bindings\n");
	if (client_libdir==NULL) {
	    init_default_keybindings();
	    return;
	}
	sprintf(buf,"%s/def_keys", client_libdir);
	if ((fp=fopen(buf,"r"))==NULL) {
	    init_default_keybindings();
	    return;
	}
    }
    while (fgets(buf, MAX_BUF, fp)) {
	line++;
	parse_keybind_line(buf,line,0);
    }
    fclose(fp);
}


/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

char facecachedir[MAX_BUF];

/* This holds the name we recieve with the 'face' command so we know what
 * to save it as when we actually get the face.
 */
char *facetoname[MAXPIXMAPNUM];

/* Initializes the data for image caching */
static void init_cache_data()
{
    int i;
    char buf[MAX_BUF];
    GtkStyle *style;
#include "pixmaps/question.xpm"


    printf ("Init Cache\n");
    sprintf(buf,"%s/.crossfire/images.xpm", getenv("HOME"));
    
    /*
      pixmaps[0].mask=None;
      pixmaps[0].bitmap=XCreateBitmapFromData(display, 
      RootWindow(display, screen_num), question_bits, 24,24);
    */
    /* In xpm mode, XCopyArea is used from this data, so we need to copy
     * the image into an pixmap of appropriate depth.
     */

    /*    pixmaps[0].pixmap=XCreatePixmap(display, win_root, 24, 24, 
	  DefaultDepth(display,DefaultScreen(display)));
	  XCopyPlane(display, pixmaps[0].bitmap, pixmaps[0].pixmap, gc_game,
	  0,0,24,24,0,0,1);*/
    
    style = gtk_widget_get_style(gtkwin_root);
    pixmaps[0].gdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_root->window,
							&pixmaps[0].gdkmask,
							&style->bg[GTK_STATE_NORMAL],
							(gchar **)question);
    
    pixmaps[0].bg = 0;
    pixmaps[0].fg = 1;
    facetoname[0]=NULL;

    /* Initialize all the images to be of the same value. */
    for (i=1; i<MAXPIXMAPNUM; i++)  {
	pixmaps[i]=pixmaps[0];
	facetoname[i]=NULL;
    }

#ifdef IMAGECACHEDIR
    strcpy(facecachedir, IMAGECACHEDIR);
#else
    sprintf(facecachedir,"%s/.crossfire/images", getenv("HOME"));
#endif

    if (make_path_to_dir(facecachedir)==-1) {
	    fprintf(stderr,"Could not create directory %s, exiting\n", facecachedir);
	    exit(1);
    }

}

static void requestface(int pnum, char *facename, char *facepath)
{
    char buf[MAX_BUF];


    facetoname[pnum] = strdup_local(facepath);
    sprintf(buf,"askface %d", pnum);
    cs_write_string(csocket.fd, buf, strlen(buf));
    /* Need to make sure we have the directory */
    sprintf(buf,"%s/%c%c", facecachedir, facename[0], facename[1]);

    if (access(buf,R_OK)) make_path_to_dir(buf);
}

/* We only get here if the server believes we are caching images. */
/* We rely on the fact that the server will only send a face command for
 * a particular number once - at current time, we have no way of knowing
 * if we have already received a face for a particular number.
 */

void FaceCmd(unsigned char *data,  int len)
{
  int pnum;
  char *face,buf[MAX_BUF];
  
  /* A quick sanity check, since if client isn't caching, all the data
   * structures may not be initialized.
   */

  if (!cache_images) {
    fprintf(stderr,"Received a 'face' command when we are not caching\n");
    return;
  }
  pnum = GetShort_String(data);
  face = (char*)data+2;
  data[len] = '\0';
  /* To prevent having a directory with 2000 images, we do a simple
   * split on the first 2 characters.
   */
  sprintf(buf,"%s/%c%c/%s", facecachedir, face[0], face[1],face);

  strcat(buf,".xpm");

  /* check to see if we already have the file.  IF not, we need to request
   * it from the server.
   */
  if (access(buf,R_OK)) {
    
    requestface(pnum, face, buf);
    
  } else {
    
    GtkStyle *style;
    
    style = gtk_widget_get_style(gtkwin_root);
    pixmaps[pnum].gdkpixmap = gdk_pixmap_create_from_xpm(gtkwin_root->window,
							 &pixmaps[pnum].gdkmask,
							 &style->bg[GTK_STATE_NORMAL],
							 (gchar *) buf );
    if (pixmaps[pnum].gdkpixmap) {
      
    } else {
      requestface(pnum, face, buf);
      
    }
  }
}




/* The only things we actually care about is the run and fire keys.
 * Other key releases are not important.
 * If it is the release of a run or fire key, we tell the client
 * to stop firing or running.  In some cases, it is possible that we
 * actually are not running or firing, and in such cases, the server
 * will just ignore the command.
 */


static void parse_key_release(KeyCode kc, KeySym ks) {

    /* Only send stop firing/running commands if we are in actual
     * play mode.  Something smart does need to be done when the character
     * enters a non play mode with fire or run mode already set, however.
     */
#if 0	/* I think this causes more problems than it solves */
    if (cpl.input_state != Playing) return;
#endif

    if (kc==firekey[0] || ks==firekeysym[0] || 
	kc==firekey[1] || ks==firekeysym[1]) {
#if 0	/* Nice idea, but unfortunately prints too many false results */
		if (cpl.echo_bindings) draw_info("stop fire",NDI_BLACK);
#endif
		cpl.fire_on=0;
		clear_fire();
		/*		draw_message_window(0);*/
		gtk_label_set (GTK_LABEL(fire_label),"    ");
	}
    else if (kc==runkey[0] || ks==runkeysym[0] ||
	kc==runkey[1] || ks==runkeysym[1]) {
		cpl.run_on=0;
		if (cpl.echo_bindings) draw_info("stop run",NDI_BLACK);
		clear_run();
		/*		draw_message_window(0);*/
		gtk_label_set (GTK_LABEL(run_label),"   ");
	}
    /* Firing is handled on server side.  However, to keep more like the
     * old version, if you release the direction key, you want the firing
     * to stop.  This should do that.
     */
    else if (cpl.fire_on) clear_fire();
}

/* This parses a keypress.  It should only be called win in Playing
 * mode.
 */
static void parse_key(char key, KeyCode keycode, KeySym keysym)
{
    Key_Entry *keyentry, *first_match=NULL;
    int present_flags=0;
    char buf[MAX_BUF];

    if (keycode == commandkey || keysym==commandkeysym) {
      /*draw_prompt(">");*/
      if (split_windows) {
	gtk_widget_grab_focus (GTK_WIDGET(gtkwin_info));
	gtk_widget_grab_focus (GTK_WIDGET(entrytext));
      } else {
	gtk_widget_grab_focus (GTK_WIDGET(entrytext));
      }
	cpl.input_state = Command_Mode;
	cpl.no_echo=FALSE;
	return;
    }
    if (keycode == firekey[0] || keysym==firekeysym[0] ||
	keycode == firekey[1] || keysym==firekeysym[1]) {
		cpl.fire_on=1;
		gtk_label_set (GTK_LABEL(fire_label),"Fire");
		/*		draw_message_window(0);*/
		return;
	}
    if (keycode == runkey[0] || keysym==runkeysym[0] ||
	keycode==runkey[1] || keysym==runkeysym[1]) {
		cpl.run_on=1;
		gtk_label_set (GTK_LABEL(run_label),"Run");
		/*draw_message_window(0);*/
		return;
	}

    if (cpl.run_on) present_flags |= KEYF_RUN;
    if (cpl.fire_on) present_flags |= KEYF_FIRE;
    if (present_flags ==0) present_flags = KEYF_NORMAL;

    keyentry = keys[keycode];
    while (keyentry!=NULL) {
	if ((keyentry->keysym!=NoSymbol && keyentry->keysym!=keysym) ||
	    (!(keyentry->flags & present_flags))) {
		keyentry=keyentry->next;
		continue;
	    }
	first_match = keyentry;
	/* Try to find a perfect match */
	if ((keyentry->flags & KEYF_MODIFIERS) != present_flags) {
		keyentry=keyentry->next;
		continue;
	}
	else break;
    }
    if (first_match!=NULL) {
	char buf[MAX_BUF];

	if (first_match->flags & KEYF_EDIT) {
	  	    strcpy(cpl.input_text, first_match->command);
	    cpl.input_state = Command_Mode;
	    sprintf(buf,"%s", cpl.input_text);
   	    gtk_entry_set_text(GTK_ENTRY(entrytext),buf);
	    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
	    return;
	}

	if (first_match->direction>=0) {
	    if (cpl.fire_on) {
		sprintf(buf,"fire %s", first_match->command);
		fire_dir(first_match->direction);
	    }
	    else if (cpl.run_on) {
		run_dir(first_match->direction);
		sprintf(buf,"run %s", first_match->command);
	    }
	    else {
		strcpy(buf,first_match->command);
/*		send_command(first_match->command);*/
		extended_command(first_match->command);
	    }
	    if (cpl.echo_bindings) draw_info(buf,NDI_BLACK);
	}
        else {
	    if (cpl.echo_bindings) draw_info(first_match->command,NDI_BLACK);
	    /*send_command(first_match->command);*/
	    extended_command(first_match->command);
	}
	return;
    }
    if (key>='0' && key<='9') {

	cpl.count = cpl.count*10 + (key-'0');

	if (cpl.count>100000) cpl.count%=100000;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON(counttext), (float) cpl.count );


	return;
    }
    sprintf(buf, "Key unused (%s%s%s)",
          (cpl.fire_on? "Fire&": ""),
          (cpl.run_on ? "Run&" : ""),
          XKeysymToString(keysym));
    draw_info(buf,NDI_BLACK);
    cpl.count=0;
}


/* This returns a character string desribing the key. */
/* If save_mode is true, it means that the format used for saving
 * the information is used, instead of the usual format for displaying
 * the information in a friendly manner.
 */


static char * get_key_info(Key_Entry *key, KeyCode kc, int save_mode)
{
    static char buf[MAX_BUF];
    char buff[MAX_BUF];
    int bi=0;

    if ((key->flags & KEYF_MODIFIERS) == KEYF_MODIFIERS)
	buff[bi++] ='A';
    else {
	if (key->flags & KEYF_NORMAL)
	  buff[bi++] ='N';
	if (key->flags & KEYF_FIRE)
	  buff[bi++] ='F';
	if (key->flags & KEYF_RUN)
	  buff[bi++] ='R';
    }
    if (key->flags & KEYF_EDIT)
	buff[bi++] ='E';
    if (key->flags & KEYF_STANDARD)
	buff[bi++] ='S';

    buff[bi]='\0';
    if (save_mode) {
	if(key->keysym == NoSymbol) {
	  sprintf(buf, "(null) %i %s %s",
		kc,buff, key->command);
	}
	else {
	  sprintf(buf, "%s %i %s %s",
		    XKeysymToString(key->keysym), kc,
		    buff, key->command);
	}
    }
    else {
	if(key->keysym == NoSymbol) {
	  sprintf(buf, "key (null) (%i) %s %s",
		kc,buff, key->command);
	}
	else {
	  sprintf(buf, "key %s (%i) %s %s",
		    XKeysymToString(key->keysym), kc,
		    buff, key->command);
	}
    }
    return buf;
}

/* Shows all the keybindings.  allbindings me we also show the standard
 * (default) keybindings.
 */

static void show_keys(int allbindings)
{
  int i, count=1;
  Key_Entry *key;
  char buf[MAX_BUF];

  sprintf(buf, "Commandkey %s (%d)", XKeysymToString(commandkeysym),
	commandkey);
  draw_info(buf,NDI_BLACK);
  sprintf(buf, "Firekeys 1: %s (%d), 2: %s (%d)",
	  XKeysymToString(firekeysym[0]), firekey[0],
	  XKeysymToString(firekeysym[1]), firekey[1]);
  draw_info(buf,NDI_BLACK);
  sprintf(buf, "Runkeys 1: %s (%d), 2: %s (%d)",
	  XKeysymToString(runkeysym[0]), runkey[0],
	  XKeysymToString(runkeysym[1]), runkey[1]);
  draw_info(buf,NDI_BLACK);


  /* Perhaps we should start at 8, so0 that we only show 'active'
   * keybindings?
   */
  for (i=0; i<=MAX_KEYCODE; i++) {
    for (key=keys[i]; key!=NULL; key =key->next) {
	if (key->flags & KEYF_STANDARD && !allbindings) continue;

	sprintf(buf,"%3d %s",count,  get_key_info(key,i,0));
	draw_info(buf,NDI_BLACK);
	count++;
    }
  }
}




void bind_key(char *params)
{
  char buf[MAX_BUF];

  if (!params) {
    draw_info("Usage: bind [-nfre] {<commandline>/commandkey/firekey{1/2}/runkey{1/2}}",NDI_BLACK);
    return;
  }

  if (!strcmp(params, "commandkey")) {
    bind_keycode = &commandkey;
    bind_keysym = &commandkeysym;
    draw_info("Push key to bind new commandkey.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }
  if (!strcmp(params, "firekey1")) {
    bind_keycode = &firekey[0];
    bind_keysym = & firekeysym[0];
    draw_info("Push key to bind new firekey 1.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }
  if (!strcmp(params, "firekey2")) {
    bind_keycode = &firekey[1];
    bind_keysym = & firekeysym[1];
    draw_info("Push key to bind new firekey 2.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }
  if (!strcmp(params, "runkey1")) {
    bind_keycode = &runkey[0];
    bind_keysym = &runkeysym[0];
    draw_info("Push key to bind new runkey 1.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }
  if (!strcmp(params, "runkey2")) {
    bind_keycode = &runkey[1];
    bind_keysym = &runkeysym[1];
    draw_info("Push key to bind new runkey 2.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }

  /* Skip over any spaces we may have */
  while (*params==' ') params++;

  if (params[0] != '-')
    bind_flags =KEYF_MODIFIERS;
  else {
    bind_flags =0;
    bind_keysym=NULL;
    bind_keycode=NULL;
    for (params++; *params != ' '; params++)
      switch (*params) {
      case 'n':
	bind_flags |= KEYF_NORMAL;
	break;
      case 'f':
	bind_flags |= KEYF_FIRE;
	break;
      case 'r':
	bind_flags |= KEYF_RUN;
	break;
      case 'e':
	bind_flags |= KEYF_EDIT;
	break;
      case '\0':
	draw_info("Try unbind to remove bindings..",NDI_BLACK);
	return;
      default:
	sprintf(buf, "Unknown flag to bind: '%c'", *params);
	draw_info(buf,NDI_BLACK);
	return;
      }
    params++;
  }

  if (!(bind_flags & KEYF_MODIFIERS))
    bind_flags |= KEYF_MODIFIERS;

  if (!params[0]) {
    draw_info("Try unbind to remove bindings..",NDI_BLACK);
    return;
  }

  sprintf(buf, "Push key to bind '%s'.", params);
  draw_info(buf,NDI_BLACK);
  strcpy(bind_buf, params);
  bind_keycode=NULL;
  cpl.input_state = Configure_Keys;
  return;
}


/* This is a recursive function that saves all the entries for a particular
 * entry.  We save the first element first, and then go through
 * and save the rest of the elements.  In this way, the ordering of the key
 * entries in the
 * file remains the same.
 */

static void save_individual_key(FILE *fp, Key_Entry *key, KeyCode kc)
{
    if (key==NULL) return;
    fprintf(fp, "%s\n", get_key_info(key, kc, 1));
    save_individual_key(fp, key->next, kc);
}

static void save_keys()
{
    char buf[MAX_BUF], buf2[MAX_BUF];
    int i;
    FILE *fp;

    sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
    if ((fp=fopen(buf,"w"))==NULL) {
	sprintf(buf2,"Could not open %s, key bindings not saved\n", buf);
	draw_info(buf2,NDI_BLACK);
	return;
    }

    for (i=0; i<=MAX_KEYCODE; i++) {
	save_individual_key(fp, keys[i], i);
    }
    fclose(fp);
    /* Should probably check return value on all writes to be sure, but... */
    draw_info("key bindings successfully saved.",NDI_BLACK);
}

static void configure_keys(KeyCode k, KeySym keysym)
{
  char buf[MAX_BUF];

  if (bind_keycode==NULL) {
    if(k == firekey[0] || k == firekey[1]) {
	cpl.fire_on =1;
	draw_message_window(0);
	return;
    }
    if(k == runkey[0] || k == runkey[1]) {
	cpl.run_on =1;
	draw_message_window(0);
	return;
    }
  }
  cpl.input_state = Playing;
  /* Try to be clever - take into account shift/control keys being
   * held down when binding keys - in this way, player does not have to use
   * -f and -r flags to bind for many simple binds.
   */
	
  if ((cpl.fire_on || cpl.run_on) && (bind_flags & KEYF_MODIFIERS)==KEYF_MODIFIERS) {
	bind_flags &= ~KEYF_MODIFIERS;
	if (cpl.fire_on) bind_flags |= KEYF_FIRE;
	if (cpl.run_on) bind_flags |= KEYF_RUN;
  }

  if (bind_keycode!=NULL) {
	*bind_keycode = k;
	*bind_keysym=keysym;
  }
  else {
	insert_key(keysym, k, bind_flags, bind_buf);
  }

  sprintf(buf, "Binded to key '%s' (%i)", XKeysymToString(keysym), (int)k);
  draw_info(buf,NDI_BLACK);
  cpl.fire_on=0;
  cpl.run_on=0;
  draw_message_window(0);

  /* Do this each time a new key is bound.  This way, we are never actually
   * storing any information that needs to be saved when the connection
   * dies or the player quits.
   */
  save_keys();
  return;
}


void unbind_key(char *params)
{
    int count=0, keyentry, onkey,global=0;
    Key_Entry *key, *tmp;
    char buf[MAX_BUF];

    if (params==NULL || params[0]=='\0') {
	show_keys(0);
	return;
    }
    if (!strcmp(params,"-a")) {
	show_keys(1);
	return;
    }
    if (!strncmp(params,"-g",2)) {
	global=1;
	if (!(params=strchr(params,' ')))  {
	    draw_info("Usage: unbind <entry_number> or",NDI_BLACK);
	    draw_info("Usage: unbind [-a] to show existing bindings (-a shows all bindings)",NDI_BLACK);
	    return;
	}
    }
    if ((keyentry=atoi(params))==0) {
	draw_info("Usage: unbind <entry_number> or",NDI_BLACK);
	draw_info("Usage: unbind [-a] to show existing bindings (-a shows all bindings)",NDI_BLACK);
	return;
    }

    for (onkey=0; onkey<=MAX_KEYCODE; onkey++) {
	for (key=keys[onkey]; key; key =key->next) {
	    if (global || !(key->flags&KEYF_STANDARD)) count++;
	    /* We found the key we want to unbind */
	    if (keyentry==count) {

		/* If it is the first entry, it is easy */
		if (key == keys[onkey]) {
		    keys[onkey] = key->next;
		    goto unbinded;
		}
		/* Otherwise, we need to figure out where in the link list
		 * the entry is.
		 */
		for (tmp=keys[onkey]; tmp->next!=NULL; tmp=tmp->next) {
		    if (tmp->next == key) {
			tmp->next =key->next;
			goto unbinded;
		    }
		}
		fprintf(stderr,"unbind_key - found number entry, but could not find actual key\n");
	    }
	}
    }
    /* Makes things look better to draw the blank line */
    draw_info("",NDI_BLACK);
    draw_info("No such entry. Try 'unbind' with no options to find entry.",NDI_BLACK);
    return;

    /*
     * Found. Now remove it.
     */

unbinded:

    sprintf(buf,"Removed binding: %3d %s", count, get_key_info(key,onkey,0));


    draw_info(buf,NDI_BLACK);
    if (key->command) free(key->command);
    free(key);
    save_keys();
}






/*----------------------------- end key functions ------------------------- */


void keyrelfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
  
  updatelock=0;
  if (GTK_WIDGET_HAS_FOCUS (entrytext) /*|| GTK_WIDGET_HAS_FOCUS(counttext)*/ ) {
  } else {
    parse_key_release(XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
    gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_release_event") ;
  }
}

void keyfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
  char *text;
  updatelock=0;
  if (GTK_WIDGET_HAS_FOCUS (entrytext) /*|| GTK_WIDGET_HAS_FOCUS(counttext)*/) {
  }  else {
       
    switch(cpl.input_state) {
    case Playing:
      if (cpl.run_on) {
	if (!(event->state & GDK_CONTROL_MASK)) {
	  printf ("Run is on while ctrl is not\n");
	  gtk_label_set (GTK_LABEL(run_label),"   ");
	  cpl.run_on=0;
	  stop_run();
	}
      }
      if (cpl.fire_on) {
	if (!(event->state & GDK_SHIFT_MASK)) {
	  printf ("Fire is on while shift is not\n");
	  gtk_label_set (GTK_LABEL(fire_label),"   ");
	  cpl.fire_on=0;
	  stop_fire();
	}
      }
 
      text=XKeysymToString(event->keyval);
 

      parse_key(text[0], XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
      gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event") ;
      break;
    case Configure_Keys:
      configure_keys(XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
      gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event") ;
      break;
    case Command_Mode:
      gtk_widget_grab_focus (GTK_WIDGET(entrytext));
      break;
    default:
      fprintf(stderr,"Unknown input state: %d\n", cpl.input_state);
    }
    
    
    
  }
}




/*void menu_history(GtkWidget *widget) {
  GtkWidget *vbox;
  gchar *titles[] ={"Command"};	   

  if(!gtkwin_history) {
    
    gtkwin_history = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_history, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_usize (gtkwin_history,160,200);
    gtk_window_set_title (GTK_WINDOW (gtkwin_history), "Command history");
    gtk_signal_connect (GTK_OBJECT (gtkwin_history), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_history);
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_history), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_history));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_history), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_history));

    gtk_container_border_width (GTK_CONTAINER (gtkwin_history), 0);
    vbox = gtk_vbox_new(TRUE, 2);
    gtk_container_add (GTK_CONTAINER(gtkwin_history),vbox);
  
    history_list = gtk_clist_new_with_titles (1, titles);
    gtk_clist_set_selection_mode (GTK_CLIST(history_list) , GTK_SELECTION_BROWSE);

    gtk_clist_set_policy (GTK_CLIST (history_list), GTK_POLICY_AUTOMATIC,
			  GTK_POLICY_AUTOMATIC);
    gtk_signal_connect_after (GTK_OBJECT(history_list),
                              "button_press_event",
                              GTK_SIGNAL_FUNC(history_button_event),
                              NULL);

    
    gtk_widget_show (history_list);
    g_list_foreach (history, (GFunc) addhistorylist, NULL);

    gtk_box_pack_start (GTK_BOX(vbox),history_list, TRUE, TRUE, 0);

    gtk_widget_show (vbox);

    gtk_widget_show (gtkwin_history);
  }
  else { 
    gdk_window_raise (gtkwin_history->window);
  }
}
*/




/*
 * Sets up player game view window, implemented as a gtk table. Cells are initialized
 * with the bg.xpm pixmap to avoid resizes and to initialize GC's and everything for the
 * actual drawing routines later.
 */


static int get_game_display(GtkWidget *frame) {
#include "pixmaps/bg.xpm"
  GtkWidget *gtvbox, *gthbox;
  GtkStyle *style;
  GtkWidget *eventbox;
  gint cnt=0;
  int mx,my;
  
  
  gtvbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), gtvbox);
  gthbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (gtvbox), gthbox, FALSE, FALSE, 1);
  
  table = gtk_table_new (11,11,FALSE);
  gtk_box_pack_start (GTK_BOX (gthbox), table, FALSE, FALSE, 1);

  style = gtk_widget_get_style(gtkwin_root);

  /* Set up table cells, connect mouse events and stuff */

  for (my=0 ; my<11 ; my++) {
    for (mx=0 ; mx<11 ; mx++) {
     
      eventbox = gtk_event_box_new ();
     
      gtk_widget_show (eventbox);
      mappos[mx][my].x=mx-5;
      mappos[mx][my].y=my-5;
      gtk_widget_set_events (eventbox, GDK_BUTTON_PRESS_MASK);
      gtk_signal_connect (GTK_OBJECT(eventbox),
			  "button_press_event",
			  GTK_SIGNAL_FUNC(button_map_event),
			  &mappos[mx][my]);
      gtk_table_attach_defaults (GTK_TABLE (table), eventbox, mx, mx+1, my, my+1);
      gtk_widget_realize (eventbox);
      gtkmap[mx][my].gdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_root->window,
							      &gdkmask[mx][my],
							      &style->bg[GTK_STATE_NORMAL],
							      (gchar **)bg_xpm);
      gtkmap[mx][my].pixmap = gtk_pixmap_new (gtkmap[mx][my].gdkpixmap, gtkmap[mx][my].gdkmask);

      gtk_container_add (GTK_CONTAINER(eventbox), gtkmap[mx][my].pixmap); 
      gtk_widget_show(gtkmap[mx][my].pixmap);
      gtkmap[mx][my].gc = gdk_gc_new (gtkmap[mx][my].gdkpixmap);
      cnt++;
    }
  }

  gtk_widget_show(table);
  gtk_widget_show(gthbox);
  gtk_widget_show(gtvbox);
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

  /* Is it the inventory or look list? */
  if (l->multi_list) {
    
    /* Ok, inventory list. Animations are handled in client code. First do the nice thing and
     * free all allocated animation lists.
     */

    if (anim_list) {
      g_list_foreach (anim_list, (GFunc) freeanimobject, NULL);
      g_list_free (anim_list);
      anim_list=NULL;
    }
    /* Freeze the CLists to avoid flickering (and to speed up the processing) */
    for (list=0; list < 8; list++) {
#ifdef GTK_HAVE_FEATURES_1_1_12
      l->pos[list]=GTK_RANGE (GTK_SCROLLED_WINDOW(l->gtk_lists[list])->vscrollbar)->adjustment->value;
#else
      l->pos[list]=GTK_RANGE (GTK_CLIST(l->gtk_list[list])->vscrollbar)->adjustment->value;
#endif
      gtk_clist_freeze (GTK_CLIST(l->gtk_list[list]));
      gtk_clist_clear (GTK_CLIST(l->gtk_list[list]));
    }
  } else {
    /* Just freeze the lists and clear them */
#ifdef GTK_HAVE_FEATURES_1_1_12
    l->pos[0]=GTK_RANGE (GTK_SCROLLED_WINDOW(l->gtk_lists[0])->vscrollbar)->adjustment->value;
#else
    l->pos[0]=GTK_RANGE (GTK_CLIST(l->gtk_list[0])->vscrollbar)->adjustment->value;
#endif
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
    strcpy (buffer[1], tmp->name);

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
			    pixmaps[facecachemap[tmp->face]].gdkpixmap,
			    pixmaps[facecachemap[tmp->face]].gdkmask); 
      gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[0]), tmprow, tmp);
      if (color_inv) { 
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
	anim_list = g_list_append (anim_list, tmpanim);
      }

      if (tmp->applied) {
       	tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[1]), buffers);
	gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[1]), tmprow, 0,
			      pixmaps[facecachemap[tmp->face]].gdkpixmap,
			      pixmaps[facecachemap[tmp->face]].gdkmask);
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
			      pixmaps[facecachemap[tmp->face]].gdkpixmap,
			      pixmaps[facecachemap[tmp->face]].gdkmask);
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
			      pixmaps[facecachemap[tmp->face]].gdkpixmap,
			      pixmaps[facecachemap[tmp->face]].gdkmask);
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
			      pixmaps[facecachemap[tmp->face]].gdkpixmap,
			      pixmaps[facecachemap[tmp->face]].gdkmask);
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
			      pixmaps[facecachemap[tmp->face]].gdkpixmap,
			      pixmaps[facecachemap[tmp->face]].gdkmask);
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
			      pixmaps[facecachemap[tmp->face]].gdkpixmap,
			      pixmaps[facecachemap[tmp->face]].gdkmask);
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
			      pixmaps[facecachemap[tmp->face]].gdkpixmap,
			      pixmaps[facecachemap[tmp->face]].gdkmask);
	gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[7]), tmprow, tmp);
	if (tmp->animation_id>0 && tmp->anim_speed) {
	  tmpanimview = newanimview();
	  tmpanimview->row=tmprow;
	  tmpanimview->list=l->gtk_list[7];
	  tmpanim->view = g_list_append (tmpanim->view, tmpanimview);
	}
	

      }
      
      
    } else {
      tmprow = gtk_clist_append (GTK_CLIST (l->gtk_list[0]), buffers);
      gtk_clist_set_pixmap (GTK_CLIST (l->gtk_list[0]), tmprow, 0,
			    pixmaps[facecachemap[tmp->face]].gdkpixmap,
			    pixmaps[facecachemap[tmp->face]].gdkmask);
      gtk_clist_set_row_data (GTK_CLIST(l->gtk_list[0]), tmprow, tmp);
      if (color_inv) { 
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
    
    for (list=0; list < 8; list++) {
#ifdef GTK_HAVE_FEATURES_1_1_12
      gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_RANGE (GTK_SCROLLED_WINDOW(l->gtk_lists[list])->vscrollbar)->adjustment), l->pos[list]);
#else
      gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_RANGE (GTK_CLIST(l->gtk_list[list])->vscrollbar)->adjustment), l->pos[list]);
#endif
      gtk_clist_thaw (GTK_CLIST(l->gtk_list[list]));
      /*      gtk_widget_draw_children (l->gtk_list[list]);
	      gtk_widget_draw (l->gtk_list[list],NULL);*/
    }
    
  } else {
#ifdef GTK_HAVE_FEATURES_1_1_12
    gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_RANGE (GTK_SCROLLED_WINDOW(l->gtk_lists[0])->vscrollbar)->adjustment), l->pos[0]);
#else
    gtk_adjustment_set_value (GTK_ADJUSTMENT (GTK_RANGE (GTK_CLIST(l->gtk_list[0])->vscrollbar)->adjustment), l->pos[0]);
#endif
    gtk_clist_thaw (GTK_CLIST(l->gtk_list[0]));
    /*    gtk_widget_draw_children (l->gtk_list[0]);
	  gtk_widget_draw (l->gtk_list[0],NULL);*/
  }

}


/******************************************************************************
 *
 * The functions dealing with the info window follow
 *
 *****************************************************************************/


void enter_callback(GtkWidget *widget, GtkWidget *entry)
       {
         gchar *entry_text;
	

         entry_text = gtk_entry_get_text(GTK_ENTRY(entrytext));
	 /*         printf("Entry contents: %s\n", entry_text);*/
	 cpl.input_state = Playing;
	 extended_command(entry_text);

	 gtk_entry_set_text(GTK_ENTRY(entrytext),"");
	 gtk_widget_grab_focus (GTK_WIDGET(gtkwin_info_text));
	 
       }

void dobuttoncmd (GtkWidget *button, gchar *command) {
  printf ("Button command: %s\n",command);
  extended_command(command);
}


static int get_info_display(GtkWidget *frame) {
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *tablet;
  GtkWidget *vscrollbar;


  FILE *infile;

  box1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box1);
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
  
  gtkwin_info_text = gtk_text_new (NULL, NULL);
  gtk_text_set_editable (GTK_TEXT (gtkwin_info_text), FALSE);
  gtk_table_attach (GTK_TABLE (tablet), gtkwin_info_text, 0, 1, 0, 1,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (gtkwin_info_text);
  
  
  vscrollbar = gtk_vscrollbar_new (GTK_TEXT (gtkwin_info_text)->vadj);
  gtk_table_attach (GTK_TABLE (tablet), vscrollbar, 1, 2, 0, 1,
		    GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
  gtk_widget_show (vscrollbar);
  
  gtk_text_freeze (GTK_TEXT (gtkwin_info_text));
  
  gtk_widget_realize (gtkwin_info_text);
  
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

/*static void delete_ch() {
  
}*/

/* Writes one character to the screen.  Used when player is typing
 * stuff we that we want to appear, or used to give prompts.
 */

void write_ch(char key)
{
}

/* Various replies */

void sendstr(char *sendstr)
{
  /*    printf("Entry contents: %s\n", sendstr);*/
 
  gtk_widget_destroy (dialog_window);
  send_reply(sendstr);
  cpl.input_state = Playing;
}


/* This is similar to draw_info below, but doesn't advance to a new
 * line.  Generally, queries use this function to draw the prompt for
 * the name, password, etc.
 */



void dialog_callback(GtkWidget *dialog)
{
  gchar *dialog_text;
  dialog_text = gtk_entry_get_text(GTK_ENTRY(dialogtext));
  /*  printf("Entry contents: %s\n", dialog_text);*/
	 /*send_reply(cpl.input_text);*/
  
  gtk_widget_destroy (dialog_window);
  send_reply(dialog_text);
  cpl.input_state = Playing;
}


/* Draw a prompt dialog window */
/* Ok, now this is trying to be smart and decide what sort of dialog is
 * wanted.
 */

void draw_prompt(const char *str)
{
  GtkWidget *dbox;
  GtkWidget *hbox;
  GtkWidget *dialoglabel;
  GtkWidget *yesbutton, *nobutton;
  GtkWidget *strbutton, *dexbutton, *conbutton, *intbutton, *wisbutton, *powbutton, *chabutton;
  
  gint found=FALSE;

  dialog_window = gtk_window_new (GTK_WINDOW_DIALOG);

  gtk_window_position (GTK_WINDOW (dialog_window), GTK_WIN_POS_CENTER);
  gtk_widget_set_usize (dialog_window, 200, 100);
  gtk_window_set_policy (GTK_WINDOW(dialog_window), TRUE, TRUE, FALSE);

  gtk_window_set_title (GTK_WINDOW (dialog_window), "Dialog");
  gtk_widget_realize (dialog_window);
  XSetTransientForHint (GDK_WINDOW_XDISPLAY (dialog_window->window),
			GDK_WINDOW_XWINDOW (dialog_window->window),
			GDK_WINDOW_XWINDOW (gtkwin_root->window));

  dbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (dialog_window), dbox);

  /* Ok, here we start generating the contents */
 
  /*  printf ("Last info draw: %s\n", last_str);*/
  while (!found) {
    if (!strcmp(str, ":")) {
      if (!strcmp(last_str, "What is your name?")) {

	dialoglabel = gtk_label_new ("What is your name?");
	gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
	gtk_widget_show (dialoglabel);

	hbox = gtk_hbox_new(FALSE, 6);
	dialogtext = gtk_entry_new ();
	gtk_signal_connect(GTK_OBJECT(dialogtext), "activate",
			   GTK_SIGNAL_FUNC(dialog_callback),
			   dialog_window);
	gtk_box_pack_start (GTK_BOX (hbox),dialogtext, TRUE, TRUE, 6);
      	gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);
	
	gtk_widget_show (hbox);
	gtk_widget_show (dialogtext);
	gtk_widget_grab_focus (dialogtext);
	found=TRUE;
	continue;
      }

      if (!strcmp(last_str, "What is your password?")) {

	dialoglabel = gtk_label_new ("What is your password?");
	gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
	gtk_widget_show (dialoglabel);

	hbox = gtk_hbox_new(FALSE, 6);
	dialogtext = gtk_entry_new ();
	gtk_entry_set_visibility(GTK_ENTRY(dialogtext), FALSE);
	gtk_signal_connect(GTK_OBJECT(dialogtext), "activate",
			   GTK_SIGNAL_FUNC(dialog_callback),
			   dialog_window);
	gtk_box_pack_start (GTK_BOX (hbox),dialogtext, TRUE, TRUE, 6);
      	gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);
	
	gtk_widget_show (hbox);
      
	gtk_widget_show (dialogtext);
	gtk_widget_grab_focus (dialogtext);
	found=TRUE;
	continue;;
      }
      if (!strcmp(last_str, "Please type your password again.")) {

	dialoglabel = gtk_label_new ("Please type your password again.");
	gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
	gtk_widget_show (dialoglabel);
	
      	hbox = gtk_hbox_new(FALSE, 6);
	dialogtext = gtk_entry_new ();
	gtk_entry_set_visibility(GTK_ENTRY(dialogtext), FALSE);
	gtk_signal_connect(GTK_OBJECT(dialogtext), "activate",
			   GTK_SIGNAL_FUNC(dialog_callback),
			   dialog_window);
	gtk_box_pack_start (GTK_BOX (hbox),dialogtext, TRUE, TRUE, 6);
      	gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);
	
	gtk_widget_show (hbox);
	gtk_widget_show (dialogtext);
	gtk_widget_grab_focus (dialogtext);
	found=TRUE;
	continue;
      }
    }
    /* Ok, tricky ones. */
    if (!strcmp(last_str, "[1-7] [1-7] to swap stats.") || !strncmp(last_str, "Str d", 5) ||
	!strncmp(last_str, "Dex d", 5) || !strncmp(last_str, "Con d", 5) ||
	!strncmp(last_str, "Int d", 5) || !strncmp(last_str, "Wis d", 5) ||
	!strncmp(last_str, "Pow d", 5) || !strncmp(last_str, "Cha d", 5)) {
      

      gtk_widget_set_usize (dialog_window, 250, 120);
      dialoglabel = gtk_label_new ("Roll again or exchange ability.");
      gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
      gtk_widget_show (dialoglabel);

      hbox = gtk_hbox_new(TRUE, 2);
      strbutton=gtk_button_new_with_label("Str");
      gtk_box_pack_start (GTK_BOX (hbox),strbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (strbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("1"));


      dexbutton=gtk_button_new_with_label("Dex");
      gtk_box_pack_start (GTK_BOX (hbox),dexbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (dexbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			        GINT_TO_POINTER("2"));

      conbutton=gtk_button_new_with_label("Con");
      gtk_box_pack_start (GTK_BOX (hbox),conbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (conbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			        GINT_TO_POINTER("3"));

      intbutton=gtk_button_new_with_label("Int");
      gtk_box_pack_start (GTK_BOX (hbox),intbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (intbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			        GINT_TO_POINTER("4"));

      wisbutton=gtk_button_new_with_label("Wis");
      gtk_box_pack_start (GTK_BOX (hbox),wisbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (wisbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			        GINT_TO_POINTER("5"));

      powbutton=gtk_button_new_with_label("Pow");
      gtk_box_pack_start (GTK_BOX (hbox),powbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (powbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			        GINT_TO_POINTER("6"));

      chabutton=gtk_button_new_with_label("Cha");
      gtk_box_pack_start (GTK_BOX (hbox),chabutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (chabutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			        GINT_TO_POINTER("7"));
    
      gtk_widget_show(strbutton);
      gtk_widget_show(dexbutton);
      gtk_widget_show(conbutton);
      gtk_widget_show(intbutton);
      gtk_widget_show(wisbutton);
      gtk_widget_show(powbutton);
      gtk_widget_show(chabutton);
     
 

      gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);   	
      gtk_widget_show (hbox);      

      hbox = gtk_hbox_new(FALSE, 6);

      yesbutton=gtk_button_new_with_label("Roll again");
      gtk_box_pack_start (GTK_BOX (hbox),yesbutton, TRUE, TRUE, 6);
      gtk_signal_connect_object (GTK_OBJECT (yesbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("y"));

      nobutton=gtk_button_new_with_label("Keep this");
      gtk_box_pack_start (GTK_BOX (hbox),nobutton, TRUE, TRUE, 6);
      gtk_signal_connect_object (GTK_OBJECT (nobutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("n"));

      gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);
	
      gtk_widget_show(yesbutton);
      gtk_widget_show(nobutton);
      gtk_widget_show (hbox);
      
      found=TRUE;
      continue;
    }    
    if (!strncmp(last_str, "Str -", 5) ||
	!strncmp(last_str, "Dex -", 5) || !strncmp(last_str, "Con -", 5) ||
	!strncmp(last_str, "Int -", 5) || !strncmp(last_str, "Wis -", 5) ||
	!strncmp(last_str, "Pow -", 5) || !strncmp(last_str, "Cha -", 5)) {
      

      gtk_widget_set_usize (dialog_window, 250, 120);
      dialoglabel = gtk_label_new ("Exchange with which ability?");
      gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
      gtk_widget_show (dialoglabel);

      hbox = gtk_hbox_new(TRUE, 2);
      strbutton=gtk_button_new_with_label("Str");
      gtk_box_pack_start (GTK_BOX (hbox),strbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (strbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("1"));


      dexbutton=gtk_button_new_with_label("Dex");
      gtk_box_pack_start (GTK_BOX (hbox),dexbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (dexbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("2"));

      conbutton=gtk_button_new_with_label("Con");
      gtk_box_pack_start (GTK_BOX (hbox),conbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (conbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("3"));

      intbutton=gtk_button_new_with_label("Int");
      gtk_box_pack_start (GTK_BOX (hbox),intbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (intbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("4"));

      wisbutton=gtk_button_new_with_label("Wis");
      gtk_box_pack_start (GTK_BOX (hbox),wisbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (wisbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("5"));

      powbutton=gtk_button_new_with_label("Pow");
      gtk_box_pack_start (GTK_BOX (hbox),powbutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (powbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("6"));

      chabutton=gtk_button_new_with_label("Cha");
      gtk_box_pack_start (GTK_BOX (hbox),chabutton, TRUE, TRUE, 1);
      gtk_signal_connect_object (GTK_OBJECT (chabutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("7"));
    
      gtk_widget_show(strbutton);
      gtk_widget_show(dexbutton);
      gtk_widget_show(conbutton);
      gtk_widget_show(intbutton);
      gtk_widget_show(wisbutton);
      gtk_widget_show(powbutton);
      gtk_widget_show(chabutton);
     
 

      gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);   	
      gtk_widget_show (hbox);
     
      found=TRUE;
      continue;
    }    

    if (!strncmp(last_str, "Press `d'", 9)) {
      

      gtk_widget_set_usize (dialog_window, 200, 100);
      dialoglabel = gtk_label_new ("Choose a character.");
      gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
      gtk_widget_show (dialoglabel);

      hbox = gtk_hbox_new(FALSE, 6);

      yesbutton=gtk_button_new_with_label("Show next");
      gtk_box_pack_start (GTK_BOX (hbox),yesbutton, TRUE, TRUE, 6);
      gtk_signal_connect_object (GTK_OBJECT (yesbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER(" "));

      nobutton=gtk_button_new_with_label("Keep this");
      gtk_box_pack_start (GTK_BOX (hbox),nobutton, TRUE, TRUE, 6);
      gtk_signal_connect_object (GTK_OBJECT (nobutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("d"));

      gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);
	
      gtk_widget_show(yesbutton);
      gtk_widget_show(nobutton);
      gtk_widget_show (hbox);
      
      found=TRUE;
      continue;
    }    

   if (!strncmp(str, "Do you want to play", 18)) {
      

      gtk_widget_set_usize (dialog_window, 200, 100);
      dialoglabel = gtk_label_new ("Do you want to play again?");
      gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
      gtk_widget_show (dialoglabel);

      hbox = gtk_hbox_new(FALSE, 6);

      yesbutton=gtk_button_new_with_label("Play again");
      gtk_box_pack_start (GTK_BOX (hbox),yesbutton, TRUE, TRUE, 6);
      gtk_signal_connect_object (GTK_OBJECT (yesbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("a"));

      nobutton=gtk_button_new_with_label("Quit");
      gtk_box_pack_start (GTK_BOX (hbox),nobutton, TRUE, TRUE, 6);
      gtk_signal_connect_object (GTK_OBJECT (nobutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("q"));

      gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);
	
      gtk_widget_show(yesbutton);
      gtk_widget_show(nobutton);
      gtk_widget_show (hbox);
      
      found=TRUE;
      continue;
    }    

  if (!strncmp(str, "Are you sure you want", 21)) {
      

      gtk_widget_set_usize (dialog_window, 200, 100);
      dialoglabel = gtk_label_new ("Are you sure you want to quit?");
      gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
      gtk_widget_show (dialoglabel);

      hbox = gtk_hbox_new(FALSE, 6);

      yesbutton=gtk_button_new_with_label("Yes, quit");
      gtk_box_pack_start (GTK_BOX (hbox),yesbutton, TRUE, TRUE, 6);
      gtk_signal_connect_object (GTK_OBJECT (yesbutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("y"));

      nobutton=gtk_button_new_with_label("Don't quit");
      gtk_box_pack_start (GTK_BOX (hbox),nobutton, TRUE, TRUE, 6);
      gtk_signal_connect_object (GTK_OBJECT (nobutton), "clicked",
			       GTK_SIGNAL_FUNC(sendstr),
			       GINT_TO_POINTER("n"));

      gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);
	
      gtk_widget_show(yesbutton);
      gtk_widget_show(nobutton);
      gtk_widget_show (hbox);
      
      found=TRUE;
      continue;
    }    

    if (!found) {
      dialoglabel = gtk_label_new (str);
      gtk_box_pack_start (GTK_BOX (dbox),dialoglabel, FALSE, TRUE, 6);
      gtk_widget_show (dialoglabel);
      
      hbox = gtk_hbox_new(FALSE, 6);
      dialogtext = gtk_entry_new ();

      gtk_signal_connect(GTK_OBJECT(dialogtext), "activate",
			 GTK_SIGNAL_FUNC(dialog_callback),
			 dialog_window);
      gtk_box_pack_start (GTK_BOX (hbox),dialogtext, TRUE, TRUE, 6);
      gtk_box_pack_start (GTK_BOX (dbox),hbox, FALSE, TRUE, 6);
	
      gtk_widget_show (hbox);      
      gtk_widget_show (dialogtext);
      gtk_widget_grab_focus (dialogtext);
      found=TRUE;
      continue;
    }
  }

  /* Finished with the contents. */


  gtk_widget_show (dbox);
  gtk_widget_show (dialog_window);

}
/* draw_info adds a line to the info window. */

void draw_info(const char *str, int color) {
  guint size;
  if (color==NDI_WHITE) {
    color=NDI_BLACK;
  }

  strcpy (last_str, str);
  if (updatelock < 25) {
    updatelock++;
    
    gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[color], NULL, str , -1);
    gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[color], NULL, "\n" , -1);

    
    size = gtk_text_get_point (GTK_TEXT (gtkwin_info_text));
    gtk_text_set_point (GTK_TEXT (gtkwin_info_text), size);

    gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[color], NULL, " " , -1);
    gtk_text_backward_delete (GTK_TEXT (gtkwin_info_text), 1);
    
  } else {
    /*    printf ("WARNING -- RACE. Frozen text updates until updatelock is cleared!\n");*/
    if (!draw_info_freeze){
      gtk_text_freeze (GTK_TEXT (gtkwin_info_text));
      draw_info_freeze=TRUE;
    }
    gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[color], NULL, str , -1);
    gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &root_color[color], NULL, "\n" , -1);
    
  } 
}


void draw_color_info(int colr, const char *buf){

  if (color_text){

    draw_info(buf,colr);

  }
  else{
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
  /*  GtkWidget *label_playername;*/
  GtkWidget *stats_vbox;
  GtkWidget *stats_box_1;
  GtkWidget *stats_box_2;
  GtkWidget *stats_box_4;
  GtkWidget *stats_box_5;
  GtkWidget *stats_box_6;
  GtkWidget *stats_box_7;
  

      stats_vbox = gtk_vbox_new (FALSE, 0);
      /* 1st row */
      stats_box_1 = gtk_hbox_new (FALSE, 0);

      statwindow.playername = gtk_label_new("Player: ");
      gtk_box_pack_start (GTK_BOX (stats_box_1), statwindow.playername, FALSE, FALSE, 5);
      gtk_widget_show (statwindow.playername);

      gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_1, FALSE, FALSE, 0);
      gtk_widget_show (stats_box_1);

      /* 2nd row */

      stats_box_2 = gtk_hbox_new (FALSE, 0);
      statwindow.score = gtk_label_new("Score: 0");
      gtk_box_pack_start (GTK_BOX (stats_box_2), statwindow.score, FALSE, FALSE, 5);
      gtk_widget_show (statwindow.score);

      statwindow.level = gtk_label_new("Level: 0");
      gtk_box_pack_start (GTK_BOX (stats_box_2), statwindow.level, FALSE, FALSE, 5);
      gtk_widget_show (statwindow.level);

      gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_2, FALSE, FALSE, 0);
      gtk_widget_show (stats_box_2);
      

      /* 4th row */
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

      /* 5th row */

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

      /* 6th row */

      stats_box_6 = gtk_hbox_new (FALSE, 0);

      statwindow.speed = gtk_label_new("Speed: 0");
      gtk_box_pack_start (GTK_BOX (stats_box_6), statwindow.speed, FALSE, FALSE, 5);
      gtk_widget_show (statwindow.speed);

      gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_6, FALSE, FALSE, 0);
      gtk_widget_show (stats_box_6);

      /* 7th row */

      stats_box_7 = gtk_hbox_new (FALSE, 0);

      statwindow.skill = gtk_label_new("Skill: 0");
      gtk_box_pack_start (GTK_BOX (stats_box_7), statwindow.skill, FALSE, FALSE, 5);
      gtk_widget_show (statwindow.skill);


      gtk_box_pack_start (GTK_BOX (stats_vbox), stats_box_7, FALSE, FALSE, 0);
      gtk_widget_show (stats_box_7);



      gtk_container_add (GTK_CONTAINER (frame), stats_vbox);
      gtk_widget_show (stats_vbox);


   return 0;
}

/* This draws the stats window.  If redraw is true, it means
 * we need to redraw the entire thing, and not just do an
 * updated.
 */

void draw_stats(int redraw) {
  float weap_sp;
  char buff[MAX_BUF];

  static Stats last_stats = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  static char last_name[MAX_BUF]="", last_range[MAX_BUF]="";

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
    
    if(redraw || cpl.stats.armor!=last_stats.armor) {
      last_stats.armor=cpl.stats.armor;
      sprintf(buff,"Arm%3d",cpl.stats.armor);
      gtk_label_set (GTK_LABEL(statwindow.armor), buff);
      gtk_widget_draw (statwindow.armor, NULL);
    }
    
    if(redraw || cpl.stats.speed!=last_stats.speed) {
      last_stats.speed=cpl.stats.speed;
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
    }
    
    if(redraw || strcmp(cpl.range, last_range)) {
      strcpy(last_range, cpl.range);
      sprintf(buff,cpl.range);
      gtk_label_set (GTK_LABEL(statwindow.skill), buff);
      gtk_widget_draw (statwindow.skill, NULL);
    }
  }
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

  vitals[bar].style = gtk_style_new ();
  vitals[bar].style->bg[GTK_STATE_PRELIGHT] = gdk_green;
  gtk_widget_set_style (vitals[bar].bar, vitals[bar].style);
}


static int get_message_display(GtkWidget *frame) {
  GtkWidget *plabel;
  GtkWidget *mtable;
  GtkWidget *vbox;
 

  vbox = gtk_vbox_new (TRUE, 0);
  gtk_container_add (GTK_CONTAINER(frame),vbox);

  mtable = gtk_table_new (2,9,FALSE);
  gtk_box_pack_start (GTK_BOX(vbox),mtable,TRUE,FALSE,0);



  create_stat_bar (mtable, 1,"Hp: 0",0, &statwindow.hp);

  create_stat_bar (mtable, 3,"Mana: 0",1, &statwindow.sp);

  create_stat_bar (mtable, 5,"Grace: 0",2, &statwindow.gr);

  create_stat_bar (mtable, 7,"Food: 0",3, &statwindow.food);


  plabel = gtk_label_new ("Status");
  gtk_table_attach(GTK_TABLE(mtable), plabel, 1,2,2,3,GTK_FILL | GTK_EXPAND,GTK_FILL | GTK_EXPAND,0,0);
  gtk_widget_show (plabel);

  fire_label = gtk_label_new ("    ");
  gtk_table_attach(GTK_TABLE(mtable), fire_label, 1,2,4,5,GTK_FILL | GTK_EXPAND,GTK_FILL | GTK_EXPAND,0,0);
  gtk_widget_show (fire_label);

  run_label = gtk_label_new ("   ");
  gtk_table_attach(GTK_TABLE(mtable), run_label, 1,2,6,7,GTK_FILL | GTK_EXPAND,GTK_FILL | GTK_EXPAND,0,0);
  gtk_widget_show (run_label);


  gtk_progress_bar_update (GTK_PROGRESS_BAR (vitals[0].bar), 1);
  gtk_progress_bar_update (GTK_PROGRESS_BAR (vitals[1].bar), 1);
  gtk_progress_bar_update (GTK_PROGRESS_BAR (vitals[2].bar), 1);
  gtk_progress_bar_update (GTK_PROGRESS_BAR (vitals[3].bar), 1);
  gtk_style_unref (vitals[0].style); 
  gtk_style_unref (vitals[1].style); 
  gtk_style_unref (vitals[2].style); 
  gtk_style_unref (vitals[3].style); 
  

  gtk_widget_show (mtable);
  gtk_widget_show (vbox);
   return 0;
}

#define MAX_BARS_MESSAGE 80

static void draw_stat_bar(int bar_pos, float bar, int is_alert)
{
 if (vitals[bar_pos].state!=is_alert) {
    if (is_alert) {
      vitals[bar_pos].style = gtk_style_new ();
      vitals[bar_pos].style->bg[GTK_STATE_PRELIGHT] = gdk_red;
      gtk_widget_set_style (vitals[bar_pos].bar, vitals[bar_pos].style);
      gtk_style_unref (vitals[bar_pos].style); 
      vitals[bar_pos].state=is_alert;
    }
    else {
      vitals[bar_pos].style = gtk_style_new ();
      vitals[bar_pos].style->bg[GTK_STATE_PRELIGHT] = gdk_green;
      gtk_widget_set_style (vitals[bar_pos].bar, vitals[bar_pos].style);
      gtk_style_unref (vitals[bar_pos].style);
      vitals[bar_pos].state=0;
    }
  }
 /* if (bar==0) {
   bar=(float)0.01;
 }*/
 gtk_progress_bar_update (GTK_PROGRESS_BAR (vitals[bar_pos].bar),bar );
 gtk_widget_draw (vitals[bar_pos].bar, NULL);
}


/* This updates the status bars.  If redraw, then redraw them
 * even if they have not changed
 */

void draw_message_window(int redraw) {
  float bar;
    int is_alert,flags;
    /*    static uint16 oldflags=0;*/


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

  /* draw sp bar.  spellpoints can go above max
   * spellpoints via supercharging with the transferrance spell,
   * or taking off items that raise max spellpoints.
   */
  if (cpl.stats.sp>cpl.stats.maxsp)
	bar=(float)1;
  else
	bar=(float)cpl.stats.sp/cpl.stats.maxsp;
  if(bar<=0) 
    bar=(float)0.01;

  is_alert=(cpl.stats.sp <= cpl.stats.maxsp/4);

 

  if (redraw || scrollsize_sp!=bar || scrollsp_alert!=is_alert)
    draw_stat_bar(1, bar, is_alert);

  scrollsize_sp=bar;
  scrollsp_alert=is_alert;

  /* draw grace bar. grace can go above max or below min */
  if (cpl.stats.grace>cpl.stats.maxgrace)
	bar = MAX_BARS_MESSAGE;
  else
	bar=(float)cpl.stats.grace/cpl.stats.maxgrace;
  if(bar<=0)
    bar=(float)0.01;

  if (bar>1.0) {
    bar=(float)1.0;
  }
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
}
  else {
    /*    printf ("WARNING -- RACE. Frozen updates until updatelock is cleared!\n");*/
  }
}

/*static void draw_all_message() {
  draw_message_window(1);
  printf ("draw_all_message\n");
}*/

 


/****************************************************************************
 *
 * Inventory window functions follow
 *
 ****************************************************************************/


#define draw_status_icon(l,x,y,face) \
do { \
    XClearArea(display, l->win, x, y, 24, 6, False); \
    if (face) { \
	XSetClipMask(display, l->gc_status, icons[face].mask), \
	XSetClipOrigin(display, l->gc_status, x, y), \
	XCopyArea(display, icons[face].pixmap, l->win, l->gc_status, \
		  0, 0, 24, 6, x, y); \
    } \
} while (0)


/*
 * draw_all_list clears a window and after that draws all objects 
 * and a scrollbar
 */
static void draw_all_list(itemlist *l)
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
  sprintf (look_list.title, "%s:", op->name);
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

static void resize_list_info(itemlist *l, int w, int h)
{
    draw_all_list(l);	/* this also initializes above allocated tables */
}

/* Handle mouse presses in the lists */
#ifdef GTK_HAVE_FEATURES_1_1_12
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
      }
    else {
      cpl.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(counttext));
      client_send_move (inv_list.env->tag, tmp->tag, cpl.count);
      cpl.count=0;
      
    }
    
  }
  
}
#else
static void list_button_event (GtkWidget *gtklist, GdkEventButton *event, itemlist *l)
{
  GList *node;
  item *tmp;
  if (event->type==GDK_BUTTON_PRESS && event->button==1) {
    if (GTK_CLIST(gtklist)->selection) {
      node =  GTK_CLIST(gtklist)->selection;
      tmp = gtk_clist_get_row_data (GTK_CLIST(gtklist), (gint)node->data); 
      gtk_clist_unselect_row (GTK_CLIST(gtklist), (gint)node->data, 0);
      if (event->state & GDK_SHIFT_MASK)
	toggle_locked(tmp);
      else
	client_send_examine (tmp->tag);     
    }
  }
  if (event->type==GDK_BUTTON_PRESS && event->button==2) {
    if (GTK_CLIST(gtklist)->selection) {
      node =  GTK_CLIST(gtklist)->selection;
      tmp = gtk_clist_get_row_data (GTK_CLIST(gtklist), (gint)node->data); 

      gtk_clist_unselect_row (GTK_CLIST(gtklist), (gint)node->data, 0);
      if (event->state & GDK_SHIFT_MASK)
	send_mark_obj(tmp);
      else
	client_send_apply (tmp->tag);
    }
  }
  if (event->type==GDK_BUTTON_PRESS && event->button==3) {
    if (GTK_CLIST(gtklist)->selection) {
      node =  GTK_CLIST(gtklist)->selection;
      tmp = gtk_clist_get_row_data (GTK_CLIST(gtklist), (gint)node->data);
      gtk_clist_unselect_row (GTK_CLIST(gtklist), (gint)node->data, 0);

      if (tmp->locked) {
	draw_info ("This item is locked.",NDI_BLACK);
      } else if (l == &inv_list) {
	cpl.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(counttext));
	client_send_move (look_list.env->tag, tmp->tag, cpl.count);
      }
      else {
	cpl.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(counttext));
	client_send_move (inv_list.env->tag, tmp->tag, cpl.count);
      cpl.count=0;
      }
    }
    
  }
  
}
#endif

void count_callback(GtkWidget *widget, GtkWidget *entry)
       {
         gchar *count_text;
         count_text = gtk_entry_get_text(GTK_ENTRY(counttext));
	 /*         printf("Entry contents: %s\n", count_text);*/
	 cpl.count = atoi (count_text);
	 /*	 gtk_entry_set_text(GTK_ENTRY(counttext),"");*/
	 gtk_widget_grab_focus (GTK_WIDGET(gtkwin_info_text)); 
       }


/* Create tabbed notebook page */
#ifdef GTK_HAVE_FEATURES_1_1_12
void create_notebook_page (GtkWidget *notebook, GtkWidget **list, GtkWidget **lists, gchar **label) {
#else
void create_notebook_page (GtkWidget *notebook, GtkWidget **list, gchar **label) {
#endif
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
#ifdef GTK_HAVE_FEATURES_1_1_12
  *lists = gtk_scrolled_window_new (0,0);
#endif
  *list = gtk_clist_new_with_titles (3, titles);

  gtk_clist_set_column_width (GTK_CLIST(*list), 0, 24);
  gtk_clist_set_column_width (GTK_CLIST(*list), 1, 150);
  gtk_clist_set_column_width (GTK_CLIST(*list), 2, 20);
  gtk_clist_set_selection_mode (GTK_CLIST(*list) , GTK_SELECTION_SINGLE);
  gtk_clist_set_row_height (GTK_CLIST(*list), 24); 
#ifdef GTK_HAVE_FEATURES_1_1_12
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(*lists),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
#else 
  gtk_clist_set_policy (GTK_CLIST (*list), GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
#endif  
  liststyle = gtk_style_new ();
  liststyle->bg[GTK_STATE_SELECTED] = gdk_grey;
  liststyle->fg[GTK_STATE_SELECTED] = gdk_black;
  gtk_widget_set_style (*list, liststyle);
#ifdef GTK_HAVE_FEATURES_1_1_12
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
#else
  gtk_signal_connect_after (GTK_OBJECT(*list),
                              "button_press_event",
                              GTK_SIGNAL_FUNC(list_button_event),
                              &inv_list);
#endif

  gtk_widget_show (*list);
#ifdef GTK_HAVE_FEATURES_1_1_12
  gtk_container_add (GTK_CONTAINER (*lists), *list);
  gtk_box_pack_start (GTK_BOX(vbox1),*lists, TRUE, TRUE, 0);
  gtk_widget_show (*lists);
#else
  gtk_box_pack_start (GTK_BOX(vbox1),*list, TRUE, TRUE, 0);
#endif
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
  
  GtkWidget *vbox2;
  GtkWidget *hbox1;
  GtkWidget *invlabel;
  GtkWidget *notebook;
  GtkAdjustment *adj;

  strcpy (inv_list.title, "Inventory:");
  inv_list.env = cpl.ob;
  inv_list.show_weight = 1;
  inv_list.show_what = show_all;
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

  notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP );


  gtk_box_pack_start (GTK_BOX(vbox2),notebook, TRUE, TRUE, 0);

#ifdef GTK_HAVE_FEATURES_1_1_12
  create_notebook_page (notebook, &inv_list.gtk_list[0], &inv_list.gtk_lists[0], all_xpm); 
  create_notebook_page (notebook, &inv_list.gtk_list[1], &inv_list.gtk_lists[1], hand_xpm); 
  create_notebook_page (notebook, &inv_list.gtk_list[2], &inv_list.gtk_lists[2], hand2_xpm); 
  create_notebook_page (notebook, &inv_list.gtk_list[3], &inv_list.gtk_lists[3], coin_xpm);
  create_notebook_page (notebook, &inv_list.gtk_list[4], &inv_list.gtk_lists[4], skull_xpm);
  create_notebook_page (notebook, &inv_list.gtk_list[5], &inv_list.gtk_lists[5], mag_xpm);
  create_notebook_page (notebook, &inv_list.gtk_list[6], &inv_list.gtk_lists[6], nonmag_xpm);
  create_notebook_page (notebook, &inv_list.gtk_list[7], &inv_list.gtk_lists[7], lock_xpm);
#else
  create_notebook_page (notebook, &inv_list.gtk_list[0], all_xpm); 
  create_notebook_page (notebook, &inv_list.gtk_list[1], hand_xpm); 
  create_notebook_page (notebook, &inv_list.gtk_list[2], hand2_xpm); 
  create_notebook_page (notebook, &inv_list.gtk_list[3], coin_xpm);
  create_notebook_page (notebook, &inv_list.gtk_list[4], skull_xpm);
  create_notebook_page (notebook, &inv_list.gtk_list[5], mag_xpm);
  create_notebook_page (notebook, &inv_list.gtk_list[6], nonmag_xpm);
  create_notebook_page (notebook, &inv_list.gtk_list[7], lock_xpm);
#endif
  gtk_widget_show (vbox2);
  gtk_widget_show (notebook);

  inv_list.multi_list=1;
  resize_list_info(&inv_list,300,300);
 
  
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
  look_list.show_what = show_all;
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

#ifdef GTK_HAVE_FEATURES_1_1_12
  look_list.gtk_lists[0] = gtk_scrolled_window_new (0,0);
#endif
  look_list.gtk_list[0] = gtk_clist_new_with_titles (3,titles);;
  gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 0, 24);
  gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 1, 150);
  gtk_clist_set_column_width (GTK_CLIST(look_list.gtk_list[0]), 2, 20);
  gtk_clist_set_selection_mode (GTK_CLIST(look_list.gtk_list[0]) , GTK_SELECTION_SINGLE);
  gtk_clist_set_row_height (GTK_CLIST(look_list.gtk_list[0]), 24); 
#ifdef GTK_HAVE_FEATURES_1_1_12
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(look_list.gtk_lists[0]),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
#else
  gtk_clist_set_policy (GTK_CLIST (look_list.gtk_list[0]), GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
#endif
 liststyle = gtk_style_new ();
  liststyle->bg[GTK_STATE_SELECTED] = gdk_grey;
  liststyle->fg[GTK_STATE_SELECTED] = gdk_black;
  gtk_widget_set_style (look_list.gtk_list[0], liststyle);

#ifdef GTK_HAVE_FEATURES_1_1_12
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
#else
  gtk_signal_connect_after (GTK_OBJECT(look_list.gtk_list[0]),
                              "button_press_event",
                              GTK_SIGNAL_FUNC(list_button_event),
                              &look_list);
#endif

  gtk_widget_show (look_list.gtk_list[0]);
#ifdef GTK_HAVE_FEATURES_1_1_12
  gtk_container_add (GTK_CONTAINER (look_list.gtk_lists[0]), look_list.gtk_list[0]);
  gtk_box_pack_start (GTK_BOX(vbox1),look_list.gtk_lists[0], TRUE, TRUE, 0);
  gtk_widget_show (look_list.gtk_lists[0]);
#else
  gtk_box_pack_start (GTK_BOX(vbox1),look_list.gtk_list[0], TRUE, TRUE, 0);
#endif
  gtk_widget_show (vbox1);
  look_list.multi_list=0;
  resize_list_info(&look_list,300,300);

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
	resize_list_info(&inv_list, inv_list.width, inv_list.height);
    } else if (strncmp ("look", s, strlen(s)) == 0) {
	look_list.show_icon = ! look_list.show_icon; /* toggle */
	resize_list_info(&look_list, look_list.width, look_list.height);
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
  /*  gchar *text="GTK Crossfire Client\nGTK porting by David Sundqvist\nOriginal client code by Mark Wedel and others\nLogo and configure scripts by Raphael Quinet\nThis software is licensed according to the terms set forth\nin the GNU General Public License\n";*/

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
    gtk_text_insert (GTK_TEXT (aboutlabel), NULL, &aboutlabel->style->black, NULL, text , -1);    
  }
  else { 
    gdk_window_raise (gtkwin_about->window);
  }
}

/* Ok, here it sets the config and saves it. This is sorta dangerous, and I'm not sure
 * if it's actually possible to do dynamic reconfiguration of everything this way. Something may
 * blow up in our faces.
 */

void applyconfig () {
  int sound;
  if (GTK_TOGGLE_BUTTON (ccheckbutton1)->active)  {
    cache_images=TRUE;
  } else {
    cache_images=FALSE;
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton2)->active) {
    if (!split_windows) {
      gtk_widget_destroy(gtkwin_root);
      split_windows=TRUE;
	create_windows();
	display_map_doneupdate();
	draw_stats (1);
      draw_all_list(&inv_list);
      draw_all_list(&look_list);
    }
  } else {
    if (split_windows) {
      gtk_widget_destroy(gtkwin_root);
      gtk_widget_destroy(gtkwin_info);
      gtk_widget_destroy(gtkwin_stats);
      gtk_widget_destroy(gtkwin_message);
      gtk_widget_destroy(gtkwin_inv);
      gtk_widget_destroy(gtkwin_look);
      split_windows=FALSE;
      create_windows();
      display_map_doneupdate();
      draw_stats (1);
      draw_all_list(&inv_list);
      draw_all_list(&look_list);
    }
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton3)->active) {
    if (nosound) {
      nosound=FALSE;
      sound = init_sounds();
      if (sound<0)
	cs_write_string(csocket.fd,"setsound 0", 10);
      else
	cs_write_string(csocket.fd,"setsound 1", 10);
    }
  } else {
    if (!nosound) {
      nosound=TRUE;
    }
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton4)->active)   {
    if (!color_inv) {
      color_inv=TRUE;
      draw_all_list(&inv_list);
      draw_all_list(&look_list);
    }
  } else {
    if (color_inv) {
      color_inv=FALSE;
      draw_all_list(&inv_list);
      draw_all_list(&look_list);
    }
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton5)->active)   {
    if (!color_text) {
      color_text=TRUE;
    }
  } else {
    if (color_text) {
      color_text=FALSE;
    }
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton6)->active)   {
    if (!tool_tips) {
      gtk_tooltips_enable(tooltips);
      tool_tips=TRUE;
    }
  } else {
    if (tool_tips) {
       gtk_tooltips_disable(tooltips);
      tool_tips=FALSE;
    }
  }
}

/* Ok, here it sets the config and saves it. This is sorta dangerous, and I'm not sure
 * if it's actually possible to do dynamic reconfiguration of everything this way.
 */

void saveconfig () {
  int sound;
  if (GTK_TOGGLE_BUTTON (ccheckbutton1)->active) {
    cache_images=TRUE;
  } else {
    cache_images=FALSE;
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton2)->active) {
    if (!split_windows) {
      gtk_widget_destroy(gtkwin_root);
      split_windows=TRUE;
      create_windows();
      display_map_doneupdate();
      draw_stats (1);
      draw_all_list(&inv_list);
      draw_all_list(&look_list);
      
    }
  } else {
    if (split_windows) {
      gtk_widget_destroy(gtkwin_root);
      gtk_widget_destroy(gtkwin_info);
      gtk_widget_destroy(gtkwin_stats);
      gtk_widget_destroy(gtkwin_message);
      gtk_widget_destroy(gtkwin_inv);
      gtk_widget_destroy(gtkwin_look);
      split_windows=FALSE;
      create_windows();
      display_map_doneupdate();
      draw_stats (1);
      draw_all_list(&inv_list);
      draw_all_list(&look_list);
      
      
    }
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton3)->active) {
    if (nosound) {
      nosound=FALSE;
      sound = init_sounds();
      if (sound<0)
	cs_write_string(csocket.fd,"setsound 0", 10);
      else
	cs_write_string(csocket.fd,"setsound 1", 10);
    }
  } else {
    if (!nosound) {
      nosound=TRUE;
    }
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton4)->active)  {
    if (!color_inv) {
      color_inv=TRUE;
      draw_all_list(&inv_list);
      draw_all_list(&look_list);

    }
  } else {
    if (color_inv) {
      color_inv=FALSE;
      draw_all_list(&inv_list);
      draw_all_list(&look_list);
    }
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton5)->active)   {
    if (!color_text) {
      color_text=TRUE;
    }
  } else {
    if (color_text) {
      color_text=FALSE;
    }
  }
  if (GTK_TOGGLE_BUTTON (ccheckbutton6)->active)   {
    if (!tool_tips) {
      gtk_tooltips_enable(tooltips);
      tool_tips=TRUE;
    }
  } else {
    if (tool_tips) {
      gtk_tooltips_disable(tooltips);
      tool_tips=FALSE;
    }
  }
  save_defaults();
}

/*void cknumentry_callback (GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
  gtk_entry_set_text (GTK_ENTRY(ckeyentrytext),  XKeysymToString(event->keyval));
  gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event"); 
  }*/



void ckeyentry_callback (GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
  /*  configure_keys(XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);*/
  gtk_entry_set_text (GTK_ENTRY(ckeyentrytext),  XKeysymToString(event->keyval));
  
  switch (event->state) {
  case GDK_CONTROL_MASK:
    gtk_entry_set_text (GTK_ENTRY(cmodentrytext),  "R");
    break;
  case GDK_SHIFT_MASK:
    gtk_entry_set_text (GTK_ENTRY(cmodentrytext),  "F");
    break;
  default:
    gtk_entry_set_text (GTK_ENTRY(cmodentrytext),  "A");
  }
  /*  XKeysymToString(event->keyval);*/
  gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event"); 
}

/*void ckeyentry_callback (GtkWidget *entry) {
  KeySym keysym;
  gchar *key;
  key=gtk_entry_get_text(GTK_ENTRY(entry));
  printf ("Text: %s\n", key);
  keysym = XStringToKeysym(key);
  printf ("Keysym: %i\n", keysym);

  }
*/
void ckeyclear () {
  gtk_label_set (GTK_LABEL(cnumentrytext), "0"); 
  gtk_entry_set_text (GTK_ENTRY(ckeyentrytext), "Press key to bind here"); 
  /*  gtk_entry_set_text (GTK_ENTRY(cknumentrytext), ""); */
  gtk_entry_set_text (GTK_ENTRY(cmodentrytext), ""); 
  gtk_entry_set_text (GTK_ENTRY(ckentrytext), ""); 
}
#ifdef GTK_HAVE_FEATURES_1_1_12
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
#else
void cclist_button_event(GtkWidget *gtklist, GdkEventButton *event) {
  gchar *buf;
  GList *node;

  node =  GTK_CLIST(gtklist)->selection;
  if (node) {
    if (event->type==GDK_BUTTON_PRESS && event->button==1) {
      gtk_clist_get_text (GTK_CLIST(cclist), (gint)node->data, 0, &buf); 
      gtk_label_set (GTK_LABEL(cnumentrytext), buf); 
      gtk_clist_get_text (GTK_CLIST(cclist), (gint)node->data, 1, &buf); 
      gtk_entry_set_text (GTK_ENTRY(ckeyentrytext), buf); 
      /*      gtk_clist_get_text (GTK_CLIST(cclist), (gint)node->data, 2, &buf); 
	      gtk_entry_set_text (GTK_ENTRY(cknumentrytext), buf); */
      gtk_clist_get_text (GTK_CLIST(cclist), (gint)node->data, 3, &buf); 
      gtk_entry_set_text (GTK_ENTRY(cmodentrytext), buf); 
      gtk_clist_get_text (GTK_CLIST(cclist), (gint)node->data, 4, &buf); 
      gtk_entry_set_text (GTK_ENTRY(ckentrytext), buf); 
    } 
  } else {
    gtk_label_set (GTK_LABEL(cnumentrytext), "0"); 
    gtk_entry_set_text (GTK_ENTRY(ckeyentrytext), "Press key to bind here"); 
    /*    gtk_entry_set_text (GTK_ENTRY(cknumentrytext), "");*/ 
    gtk_entry_set_text (GTK_ENTRY(cmodentrytext), ""); 
    gtk_entry_set_text (GTK_ENTRY(ckentrytext), ""); 
  }
}
#endif

/*void draw_keybindings (GtkWidget *keylist) {*/
void draw_keybindings (GtkWidget *keylist) {
  int i, count=1;
  Key_Entry *key;
  int allbindings=0;
  /*  static char buf[MAX_BUF];*/
  char buff[MAX_BUF];
  int bi=0;
  char buffer[5][MAX_BUF];
  char *buffers[5];
  gint tmprow; 

  gtk_clist_clear (GTK_CLIST(keylist));
     for (i=0; i<=MAX_KEYCODE; i++) {
      for (key=keys[i]; key!=NULL; key =key->next) {
	if (key->flags & KEYF_STANDARD && !allbindings) continue;
	bi=0;
	
	if ((key->flags & KEYF_MODIFIERS) == KEYF_MODIFIERS)
	  buff[bi++] ='A';
	else {
	  if (key->flags & KEYF_NORMAL)
	    buff[bi++] ='N';
	  if (key->flags & KEYF_FIRE)
	    buff[bi++] ='F';
	  if (key->flags & KEYF_RUN)
	    buff[bi++] ='R';
	}
	if (key->flags & KEYF_EDIT)
	  buff[bi++] ='E';
	if (key->flags & KEYF_STANDARD)
	  buff[bi++] ='S';
	
	buff[bi]='\0';
	
	if(key->keysym == NoSymbol) {
	  /*	  sprintf(buf, "key (null) (%i) %s %s",
		  kc,buff, key->command);
	  */
	}
	  else {
	    sprintf(buffer[0], "%i",count);
	    sprintf(buffer[1], "%s", XKeysymToString(key->keysym));
	    sprintf(buffer[2], "%i",i);
	    sprintf(buffer[3], "%s",buff);
	    sprintf(buffer[4], "%s", key->command);
	    buffers[0] = buffer[0];
	    buffers[1] = buffer[1];
	    buffers[2] = buffer[2];
	    buffers[3] = buffer[3];
	    buffers[4] = buffer[4];
	    tmprow = gtk_clist_append (GTK_CLIST (keylist), buffers);
	  }
	
	/*	sprintf(buf,"%3d %s",count,  get_key_info(key,i,0));
		draw_info(buf,NDI_BLACK);*/
	count++;
      }
    }
}
    
void bind_callback (GtkWidget *gtklist, GdkEventButton *event) {
  KeySym keysym;
  gchar *entry_text;
  gchar *cpnext;
  KeyCode k;
  gchar *mod="";
  char buf[MAX_BUF];
  /*  int flags=0;*/
  /*  int standard=1;
      
      if (standard) standard=KEYF_STANDARD;
      else standard=0;*/
  bind_flags = KEYF_MODIFIERS;
  
  if ((bind_flags & KEYF_MODIFIERS)==KEYF_MODIFIERS) {
    bind_flags &= ~KEYF_MODIFIERS;
    mod=gtk_entry_get_text (GTK_ENTRY(cmodentrytext));
    if (!strcmp(mod, "F")) {
      bind_flags |= KEYF_FIRE;
    }
    if (!strcmp(mod, "R")) {
      bind_flags |= KEYF_RUN;
      }
    if (!strcmp(mod, "A")) {
      bind_flags |= KEYF_MODIFIERS;
    }
  }
  cpnext = gtk_entry_get_text (GTK_ENTRY(ckentrytext));
  entry_text = gtk_entry_get_text (GTK_ENTRY(ckeyentrytext));
  keysym = XStringToKeysym(entry_text);
  k = XKeysymToKeycode(GDK_DISPLAY(), keysym);
  insert_key(keysym, k,  bind_flags, cpnext);
  save_keys();
  draw_keybindings (cclist);
  sprintf(buf, "Binded to key '%s' (%i)", XKeysymToString(keysym), (int)k);
  draw_info(buf,NDI_BLACK);
}

void ckeyunbind (GtkWidget *gtklist, GdkEventButton *event) {
  gchar *buf;
  GList *node;
  node =  GTK_CLIST(cclist)->selection;
  if (node) {
    gtk_clist_get_text (GTK_CLIST(cclist), (gint)node->data, 0, &buf); 

    unbind_key(buf);
    draw_keybindings (cclist);

  }
}

/*
 *  GUI Config dialog. 
 *
 *
 */

void configdialog(GtkWidget *widget) {
  GtkWidget *vbox;
  GtkWidget *hbox;
  /*  GtkWidget *configtext;*/
  /*  GtkStyle *style;*/

  GtkWidget *tablabel;
  GtkWidget *notebook;
  GtkWidget *vbox1;
  GtkWidget *vbox2;
  GtkWidget *hbox1;
  GtkWidget *applybutton;
  GtkWidget *cancelbutton;
  GtkWidget *savebutton;
  GtkWidget *frame1;

  GtkWidget *ehbox;
  GtkWidget *clabel1, *clabel2, *clabel4, *clabel5, *cb1, *cb2, *cb3;
#ifdef GTK_HAVE_FEATURES_1_1_12
  GtkWidget *cclists;
#endif 

  gchar *titles[] ={"#","Key","(#)","Mods","Command"};	   
  /* If the window isnt already up (in which case it's just raised) */

  if(!gtkwin_config) {
    
    gtkwin_config = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_position (GTK_WINDOW (gtkwin_config), GTK_WIN_POS_CENTER);
    gtk_widget_set_usize (gtkwin_config,450,360);
    gtk_window_set_title (GTK_WINDOW (gtkwin_config), "Crossfire Configure");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_config), TRUE, TRUE, FALSE);

    gtk_signal_connect (GTK_OBJECT (gtkwin_config), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_config);
    
    gtk_container_border_width (GTK_CONTAINER (gtkwin_config), 0);
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER(gtkwin_config),vbox);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP );
    gtk_box_pack_start (GTK_BOX(hbox),notebook, TRUE, TRUE, 0);

    tablabel = gtk_label_new ("General");
    gtk_widget_show (tablabel);
    vbox2 = gtk_vbox_new(FALSE, 0);
  

    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox2, tablabel);
    frame1 = gtk_frame_new("General options");  
    gtk_frame_set_shadow_type (GTK_FRAME(frame1), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start (GTK_BOX (vbox2), frame1, TRUE, TRUE, 0);
    vbox1 = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER(frame1), vbox1);
    /* Add the checkbuttons to the notebook and set them according to current settings */
    ccheckbutton1 = gtk_check_button_new_with_label ("Cache Images" );
    gtk_box_pack_start(GTK_BOX(vbox1),ccheckbutton1, FALSE, FALSE, 0);
    if (cache_images) {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton1), TRUE);
    } else {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton1), FALSE);
    }

    ccheckbutton2 = gtk_check_button_new_with_label ("Split Windows" );
    gtk_box_pack_start(GTK_BOX(vbox1),ccheckbutton2, FALSE, FALSE, 0);
    if (split_windows) {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton2), TRUE);
    } else {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton2), FALSE);
    }

    ccheckbutton3 = gtk_check_button_new_with_label ("Sound" );
    gtk_box_pack_start(GTK_BOX(vbox1),ccheckbutton3, FALSE, FALSE, 0);
    if (nosound) {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton3), FALSE);
    } else {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton3), TRUE);
    }

    ccheckbutton4 = gtk_check_button_new_with_label ("Color invlists" );
    gtk_box_pack_start(GTK_BOX(vbox1),ccheckbutton4, FALSE, FALSE, 0);
    if (color_inv) {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton4), TRUE);
    } else {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton4), FALSE);
    }

    ccheckbutton5 = gtk_check_button_new_with_label ("Color info text" );
    gtk_box_pack_start(GTK_BOX(vbox1),ccheckbutton5, FALSE, FALSE, 0);
    if (color_text) {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton5), TRUE);
    } else {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton5), FALSE);
    }

    ccheckbutton6 = gtk_check_button_new_with_label ("Show tooltips" );
    gtk_box_pack_start(GTK_BOX(vbox1),ccheckbutton6, FALSE, FALSE, 0);
    if (tool_tips) {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton6), TRUE);
    } else {
      gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton6), FALSE);
    }

    gtk_widget_show (ccheckbutton1);
    gtk_widget_show (ccheckbutton2);
    gtk_widget_show (ccheckbutton3);
    gtk_widget_show (ccheckbutton4);
    gtk_widget_show (ccheckbutton5);
    gtk_widget_show (ccheckbutton6);

    gtk_widget_show (vbox1);
    gtk_widget_show (frame1);
    gtk_widget_show (vbox2);
 

    tablabel = gtk_label_new ("Keybindings");
    gtk_widget_show (tablabel);
    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox2, tablabel);    
    frame1 = gtk_frame_new("Keybindings");  
    gtk_frame_set_shadow_type (GTK_FRAME(frame1), GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start (GTK_BOX (vbox2), frame1, TRUE, TRUE, 0);
    vbox1 = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER(frame1), vbox1);
#ifdef GTK_HAVE_FEATURES_1_1_12
    cclists = gtk_scrolled_window_new (0,0);
#endif 
    cclist = gtk_clist_new_with_titles (5, titles);

    gtk_clist_set_column_width (GTK_CLIST(cclist), 0, 20);
    gtk_clist_set_column_width (GTK_CLIST(cclist), 1, 50);
    gtk_clist_set_column_width (GTK_CLIST(cclist), 2, 20);
    gtk_clist_set_column_width (GTK_CLIST(cclist), 3, 40);
    gtk_clist_set_column_width (GTK_CLIST(cclist), 4, 245);
    gtk_clist_set_selection_mode (GTK_CLIST(cclist) , GTK_SELECTION_SINGLE);

#ifdef GTK_HAVE_FEATURES_1_1_12
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(cclists),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (cclists), cclist);
    gtk_box_pack_start (GTK_BOX(vbox1),cclists, TRUE, TRUE, 0);
#else
    gtk_clist_set_policy (GTK_CLIST (cclist), GTK_POLICY_AUTOMATIC,
			  GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX(vbox1),cclist, TRUE, TRUE, 0);
#endif
    draw_keybindings (cclist);
    
#ifdef GTK_HAVE_FEATURES_1_1_12
    gtk_signal_connect_after (GTK_OBJECT(cclist),
                              "select_row",
                              GTK_SIGNAL_FUNC(cclist_button_event),
                              NULL);
#else
    gtk_signal_connect_after (GTK_OBJECT(cclist),
                              "button_press_event",
                              GTK_SIGNAL_FUNC(cclist_button_event),
                              NULL);
#endif

    gtk_widget_show(cclist);
#ifdef GTK_HAVE_FEATURES_1_1_12
 gtk_widget_show(cclists);
#endif  
    
    ehbox=gtk_hbox_new(FALSE, 0);


    clabel1 =  gtk_label_new ("Binding #:");
    gtk_box_pack_start (GTK_BOX (ehbox),clabel1, FALSE, TRUE, 2);
    gtk_widget_show (clabel1);

    cnumentrytext = gtk_label_new ("0");
    gtk_box_pack_start (GTK_BOX (ehbox),cnumentrytext, FALSE, TRUE, 2);
    gtk_widget_set_usize (cnumentrytext, 25, 0);
    gtk_widget_show (cnumentrytext);

    clabel2 =  gtk_label_new ("Key:");
    gtk_box_pack_start (GTK_BOX (ehbox),clabel2, FALSE, TRUE, 2);
    gtk_widget_show (clabel2);

    ckeyentrytext = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (ehbox),ckeyentrytext, TRUE, TRUE, 2);
    gtk_widget_set_usize (ckeyentrytext, 110, 0);
    gtk_signal_connect(GTK_OBJECT(ckeyentrytext), "key_press_event",
		       GTK_SIGNAL_FUNC(ckeyentry_callback),
		       ckeyentrytext);
    /*  gtk_signal_connect(GTK_OBJECT(ckeyentrytext), "activate",
		       GTK_SIGNAL_FUNC(ckeyentry_callback),
		       ckeyentrytext);*/
    gtk_widget_show (ckeyentrytext);
     gtk_entry_set_text (GTK_ENTRY(ckeyentrytext),  "Press key to bind here");

    /*    clabel3 =  gtk_label_new ("Key #:");
    gtk_box_pack_start (GTK_BOX (ehbox),clabel3, FALSE, TRUE, 2);
    gtk_widget_show (clabel3);

    cknumentrytext = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (ehbox),cknumentrytext, FALSE, TRUE, 2);
   gtk_signal_connect(GTK_OBJECT(cknumentrytext), "key_press_event",
		       GTK_SIGNAL_FUNC(cknumentry_callback),
		       cknumentrytext);
    gtk_widget_set_usize (cknumentrytext, 35, 0);
    gtk_widget_show (cknumentrytext);*/

    clabel4 =  gtk_label_new ("Mods:");
    gtk_box_pack_start (GTK_BOX (ehbox),clabel4, FALSE, TRUE, 2);
    gtk_widget_show (clabel4);

    cmodentrytext = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (ehbox),cmodentrytext, FALSE, TRUE, 2);
    gtk_widget_set_usize (cmodentrytext, 45, 0);
    gtk_widget_show (cmodentrytext);


    gtk_box_pack_start (GTK_BOX (vbox1),ehbox, FALSE, TRUE, 2);

    gtk_widget_show (ehbox);

    ehbox=gtk_hbox_new(FALSE, 0);

    clabel5 =  gtk_label_new ("Command:");
    gtk_box_pack_start (GTK_BOX (ehbox),clabel5, FALSE, TRUE, 2);
    gtk_widget_show (clabel5);

    ckentrytext = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (ehbox),ckentrytext, TRUE, TRUE, 2);
    gtk_widget_show (ckentrytext);


    gtk_box_pack_start (GTK_BOX (vbox1),ehbox, FALSE, TRUE, 2);
    
    gtk_widget_show (ehbox);

    ehbox=gtk_hbox_new(TRUE, 0);


    cb1 = gtk_button_new_with_label ("Unbind");
    gtk_box_pack_start (GTK_BOX (ehbox),cb1, FALSE, TRUE, 4);
    /*gtk_widget_set_usize (cb1, 45, 0);*/
    gtk_signal_connect_object (GTK_OBJECT (cb1), "clicked",
			       GTK_SIGNAL_FUNC(ckeyunbind),
			       NULL);
    gtk_widget_show (cb1);
    
    cb2 = gtk_button_new_with_label ("Bind");
    gtk_box_pack_start (GTK_BOX (ehbox),cb2, FALSE, TRUE, 4);
    gtk_signal_connect_object (GTK_OBJECT (cb2), "clicked",
			       GTK_SIGNAL_FUNC(bind_callback),
			       NULL);
    /*  gtk_widget_set_usize (cb2, 45, 0);*/
    gtk_widget_show (cb2);

    cb3 = gtk_button_new_with_label ("Clear");
    gtk_box_pack_start (GTK_BOX (ehbox),cb3, FALSE, TRUE, 4);
    /*    gtk_widget_set_usize (cb2, 45, 0);*/
    gtk_signal_connect_object (GTK_OBJECT (cb3), "clicked",
			       GTK_SIGNAL_FUNC(ckeyclear),
			       NULL);
    gtk_widget_show (cb3);
    gtk_box_pack_start (GTK_BOX (vbox1),ehbox, FALSE, TRUE, 2);

    gtk_widget_show (ehbox);


    gtk_widget_show (vbox1);
    gtk_widget_show (frame1);
    gtk_widget_show (vbox2);


    gtk_widget_show (notebook);
    gtk_widget_show (hbox);

    /* And give some options to actually do something with our new nifty configuration */

    hbox1 = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 6);
    savebutton = gtk_button_new_with_label("Save");
    gtk_signal_connect_object (GTK_OBJECT (savebutton), "clicked",
			       GTK_SIGNAL_FUNC(saveconfig),
			       NULL);
    gtk_box_pack_start(GTK_BOX(hbox1), savebutton, FALSE, TRUE, 4);

    applybutton = gtk_button_new_with_label("Apply");
    gtk_signal_connect_object (GTK_OBJECT (applybutton), "clicked",
			       GTK_SIGNAL_FUNC(applyconfig),
			       NULL);
    gtk_box_pack_start(GTK_BOX(hbox1), applybutton, FALSE, TRUE, 4);

    cancelbutton = gtk_button_new_with_label("Close");
    gtk_signal_connect_object (GTK_OBJECT (cancelbutton), "clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT (gtkwin_config));
 
    gtk_box_pack_start(GTK_BOX(hbox1), cancelbutton, FALSE, TRUE, 4);
    gtk_widget_show(savebutton);
    gtk_widget_show(applybutton);
    gtk_widget_show(cancelbutton);

    gtk_widget_show (hbox1);
    gtk_widget_show (vbox);
    gtk_widget_show (gtkwin_config);
  }
  else { 
    gdk_window_raise (gtkwin_config->window);
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
  GSList *pickupgroup;


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

#ifdef GTK_HAVE_FEATURES_1_1_12
  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (filemenu), menu_items);
  gtk_widget_show (menu_items);
#endif

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

  /*  menu_items = gtk_menu_item_new_with_label("Command history");
  gtk_menu_append(GTK_MENU (clientmenu), menu_items);   
  gtk_signal_connect_object(GTK_OBJECT(menu_items), "activate",
			    GTK_SIGNAL_FUNC(menu_history), NULL);
  gtk_widget_show(menu_items);
  */

#ifdef GTK_HAVE_FEATURES_1_1_12
  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (clientmenu), menu_items);
  gtk_widget_show (menu_items);
#endif

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


  root_clientmenu = gtk_menu_item_new_with_label("Client");
  
  gtk_widget_show(root_clientmenu);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_clientmenu), clientmenu);

  /* Do the actionmenu */

  actionmenu = gtk_menu_new();

#ifdef GTK_HAVE_FEATURES_1_1_12
  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (actionmenu), menu_items);
  gtk_widget_show (menu_items);
#endif

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

#ifdef GTK_HAVE_FEATURES_1_1_12
  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (pickupmenu), menu_items);
  gtk_widget_show (menu_items);
#endif

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

 
  /*Do the helpmenu */
  helpmenu = gtk_menu_new();
  
#ifdef GTK_HAVE_FEATURES_1_1_12
  menu_items = gtk_tearoff_menu_item_new ();
  gtk_menu_append (GTK_MENU (helpmenu), menu_items);
  gtk_widget_show (menu_items);
#endif

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
  GtkWidget *mhpaned;
  GtkWidget *ghpaned;
  GtkWidget *gvpaned;
  GtkWidget *vpaned;
  gint callocfailed=0;

  tooltips = gtk_tooltips_new();

  if (split_windows==FALSE) {  
    gtkwin_root = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_root, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_root, 0, 0);
    gtk_widget_set_usize (gtkwin_root,900,600);
    gtk_window_set_title (GTK_WINDOW (gtkwin_root), "Crossfire GTK Client");
    gtk_signal_connect (GTK_OBJECT (gtkwin_root), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_root);
    
    gtk_container_border_width (GTK_CONTAINER (gtkwin_root), 0);

    /* Alloc colors */
    if ( !gdk_color_parse("Black", &root_color[0])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[0])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("White", &root_color[1])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[1])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("Navy", &root_color[2])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[2])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("Red", &root_color[3])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[3])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("Orange", &root_color[4])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[4])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("DodgerBlue", &root_color[5])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[5])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("DarkOrange2", &root_color[6])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[6])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("SeaGreen", &root_color[7])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[7])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("DarkSeaGreen", &root_color[8])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[8])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("Grey50", &root_color[9])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[9])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("Sienna", &root_color[10])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[10])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("Gold", &root_color[11])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[11])) {
      printf ("calloc failed\n");
    }
    if ( !gdk_color_parse("Khaki", &root_color[12])) {
      printf ("cparse failed\n");
    }
    if ( !gdk_color_alloc (gtk_widget_get_colormap (gtkwin_root), &root_color[12])) {
     printf ("calloc failed\n");
    }

    
    /* menu / windows division */
    rootvbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add (GTK_CONTAINER (gtkwin_root), rootvbox);
    gtk_widget_show (rootvbox);
    
    get_menu_display(rootvbox);
    
    /* first horizontal division. inv+obj on left, rest on right */
    
    mhpaned = gtk_hpaned_new ();
    /*  gtk_container_add (GTK_CONTAINER (gtkwin_root), mhpaned);*/
    gtk_box_pack_start (GTK_BOX (rootvbox), mhpaned, TRUE, TRUE, 0);
    gtk_container_border_width (GTK_CONTAINER(mhpaned), 5);
    gtk_widget_show (mhpaned);
    
    /* Divisior game+stats | text */
    
    ghpaned = gtk_hpaned_new ();
    gtk_paned_add2 (GTK_PANED (mhpaned), ghpaned);
    
    /* text frame */
    
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, 300, 400);
    gtk_paned_add2 (GTK_PANED (ghpaned), frame);
    
    get_info_display (frame);
    gtk_widget_show (frame);
    /* game & statbars below, stats above */
    
    gvpaned = gtk_vpaned_new ();
    gtk_paned_add1 (GTK_PANED (ghpaned), gvpaned);
    
    /* game - statbars */
    
    vpaned = gtk_vpaned_new ();
    gtk_paned_add2 (GTK_PANED (gvpaned), vpaned);
    
    
    /* Statbars frame */
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, 270, 100);
    gtk_paned_add2 (GTK_PANED (vpaned), frame);
    
    get_message_display(frame);
    
    gtk_widget_show (frame);
    
    /* Game frame */
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, 270, 270);
    gtk_paned_add1 (GTK_PANED (vpaned), frame);
    
    get_game_display (frame);
    
    gtk_widget_show (frame);
    
    /* stats frame */
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, 270, 110);
    gtk_paned_add1 (GTK_PANED (gvpaned), frame);
    get_stats_display (frame);
    
    gtk_widget_show (frame);
    
    gtk_widget_show (vpaned);
    gtk_widget_show (gvpaned);
    
    vpaned = gtk_vpaned_new ();
    gtk_paned_add1 (GTK_PANED (mhpaned), vpaned);
    
    /* inventory frame */
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, 270, 400);
    gtk_paned_add1 (GTK_PANED (vpaned), frame);
    
    get_inv_display (frame);
    
    gtk_widget_show (frame);
    
    /* look frame */
    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_usize (frame, 270, 200);
    gtk_paned_add2 (GTK_PANED (vpaned), frame);
    
    get_look_display (frame);
    
    gtk_widget_show (frame);
    
    gtk_widget_show (vpaned);
    
    gtk_widget_show (ghpaned);
    
    gtk_widget_show (mhpaned);
    

    /* Connect signals */
    
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_root), "key_press_event",
			       GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_root));
    gtk_signal_connect_object (GTK_OBJECT (gtkwin_root), "key_release_event",
			       GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_root));
    gtk_widget_show (gtkwin_root);
    

  } else {

 
  /* game window */

    gtkwin_root = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_events (gtkwin_root, GDK_KEY_RELEASE_MASK);
    gtk_widget_set_uposition (gtkwin_root, 300, 160);
    gtk_widget_set_usize (gtkwin_root,270,270);
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
    gtk_widget_set_usize (gtkwin_stats,270,140);
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
    callocfailed = gdk_color_parse("Black", &root_color[0]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[0]);
    callocfailed = gdk_color_parse("White", &root_color[1]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[1]);
    callocfailed = gdk_color_parse("Navy", &root_color[2]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[2]);
    callocfailed = gdk_color_parse("Red", &root_color[3]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[3]);
    callocfailed = gdk_color_parse("Orange", &root_color[4]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[4]);
    callocfailed = gdk_color_parse("DodgerBlue", &root_color[5]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[5]);
    callocfailed = gdk_color_parse("DarkOrange2", &root_color[6]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[6]);
    callocfailed = gdk_color_parse("SeaGreen", &root_color[7]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[7]);
    callocfailed = gdk_color_parse("DarkSeaGreen", &root_color[8]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[8]);
    callocfailed = gdk_color_parse("Grey50", &root_color[9]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[9]);
    callocfailed = gdk_color_parse("Sienna", &root_color[10]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[10]);
    callocfailed = gdk_color_parse("Gold", &root_color[11]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[11]);
    callocfailed = gdk_color_parse("Khaki", &root_color[12]);
    callocfailed = gdk_color_alloc (gtk_widget_get_colormap (gtkwin_info), &root_color[12]);

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
    gtk_widget_set_usize (gtkwin_message,270,170);
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
    set_window_pos();
  }
  gtk_tooltips_set_delay(tooltips, 1000 );
  if (tool_tips) {
    gtk_tooltips_enable(tooltips);
  }
}

int sync_display = 0;
static int get_root_display(char *display_name,int gargc, char **gargv) {
  gtk_init (&gargc,&gargv);
  last_str=malloc(32767);

  create_splash();
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

void draw_all_info()
{
}

void resize_win_info()
{
}


int get_info_width()
{
    return infodata.info_chars;
}


/* Magic mapping code needs to be added.
 * The way I see it, server will send along data of the squares and what
 * color they should be in single character codes.  So if we are mapping a
 * 10x10 area, it would be something like: "1155123620", 1296639403",
 * etc.
 */


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

int do_timeout() {
  
  updatelock=0;
  if (draw_info_freeze) {
    guint size;
    gtk_text_thaw (GTK_TEXT (gtkwin_info_text));
    size = gtk_text_get_point (GTK_TEXT (gtkwin_info_text));
    gtk_text_set_point (GTK_TEXT (gtkwin_info_text), size);
    gtk_text_insert (GTK_TEXT (gtkwin_info_text), NULL, &gtkwin_info_text->style->black, NULL, " " , -1);
    gtk_text_backward_delete (GTK_TEXT (gtkwin_info_text), 1);
    draw_info_freeze=FALSE;
  }
  if (redraw_needed) {
    display_map_doneupdate();
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


/*void FaceCmd(unsigned char *data,  int len)
{

}
*/

void display_newbitmap(long face,long fg,long bg,char *buf)
{
}

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


    map_gc = gdk_gc_new (magicgdkpixmap);


  gdk_gc_set_foreground (map_gc, &map_color[0]);
   gdk_draw_rectangle (magicgdkpixmap, map_gc,	       
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

    gdk_gc_set_foreground (map_gc, &map_color[val&FACE_COLOR_MASK]);

	gdk_draw_rectangle (magicgdkpixmap, map_gc,
			    TRUE,
			    2+cpl.mapxres*x,
			    2+cpl.mapyres*y,
			    cpl.mapxres,
			    cpl.mapyres);
      } /* Saw into this space */
    }
    /*    gdk_gc_destroy (map_gc);*/
    gtk_widget_draw (mapvbox,NULL);
  }
  
  else { 
    /* ------------------ There is already a magic map up - replace it ---------*/

    gdk_window_raise (gtkwin_magicmap->window);
    /* --------------------------- */
 
   gdk_gc_set_foreground (map_gc, &map_color[0]);
   gdk_draw_rectangle (magicgdkpixmap, map_gc,	       
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

    gdk_gc_set_foreground (map_gc, &map_color[val&FACE_COLOR_MASK]);

	gdk_draw_rectangle (magicgdkpixmap, map_gc,
			    TRUE,
			    2+cpl.mapxres*x,
			    2+cpl.mapyres*y,
			    cpl.mapxres,
			    cpl.mapyres);
	
      } 

    }
 gtk_widget_draw (mapvbox,NULL);
    /*---------------------------*/
  }
}

/* Basically, this just flashes the player position on the magic map */

void magic_map_flash_pos()
{
  if (!cpl.showmagic) return;
  if (!gtkwin_magicmap) return;
  cpl.showmagic ^=2;
  if (cpl.showmagic & 2) {
    gdk_gc_set_foreground (map_gc, &map_color[0]);
  } else {
    gdk_gc_set_foreground (map_gc, &map_color[1]);
  }
  gdk_draw_rectangle (magicgdkpixmap, map_gc,
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

static void get_window_coord(GtkWidget *win,
                 int *x,int *y,
                 int *wx,int *wy,
                 int *w,int *h)
{
  int tmp;
  gdk_window_get_geometry (win->window, x, y, w, h, &tmp);
  gdk_window_get_origin (win->window, wx, wy);
}


void save_winpos()
{
  char savename[MAX_BUF],buf[MAX_BUF];
  FILE    *fp;
  int	    x,y,w,h,wx,wy;
  
  if (!split_windows) {
    draw_info("You can only save window positions in split windows mode", NDI_BLUE);
	return;
  }
  sprintf(savename,"%s/.crossfire/winpos", getenv("HOME"));
  if (!(fp=fopen(savename,"w"))) {
    sprintf(buf,"Unable to open %s, window positions not saved",savename);
    draw_info(buf,NDI_BLUE);
    return;
    }
  /* This is a bit simpler than what the server was doing - it has
   * some code to handle goofy window managers which I am not sure
   * is still needed.
   */
  get_window_coord(gtkwin_root, &x,&y, &wx,&wy,&w,&h);
  fprintf(fp,"win_game: %d %d %d %d\n", wx,wy, w, h);
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
  fclose(fp);
  sprintf(buf,"Window positions saved to %s",savename);
  draw_info(buf,NDI_BLUE);
}

void command_show (char *params)
{
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

    if (!split_windows) return;

    sprintf(buf,"%s/.crossfire/winpos", getenv("HOME"));
    if (!(fp=fopen(buf,"r"))) return;

    while(fgets(buf,MAX_BUF-1, fp)!=NULL) {
	buf[MAX_BUF-1]='\0';
	if (!(cp=strchr(buf,' '))) continue;
	*cp++='\0';
	if (sscanf(cp,"%d %d %d %d",&wx,&wy,&w,&h)!=4)
	    continue;
	if (!strcmp(buf,"win_game:")) {
	  gtk_widget_set_uposition (gtkwin_root, wx, wy);
	  gtk_widget_set_usize (gtkwin_root, w, h);
	}
	if (!strcmp(buf,"win_stats:")) {
	  gtk_widget_set_uposition (gtkwin_stats, wx, wy);
	  gtk_widget_set_usize (gtkwin_stats, w, h);
	}
	if (!strcmp(buf,"win_info:")) {
	  gtk_widget_set_uposition (gtkwin_info, wx, wy);
	  gtk_widget_set_usize (gtkwin_info, w, h);
	}
	if (!strcmp(buf,"win_inv:")) {
	  gtk_widget_set_uposition (gtkwin_inv, wx, wy);
	  gtk_widget_set_usize (gtkwin_inv, w, h);
	}
	if (!strcmp(buf,"win_look:")) {
	  gtk_widget_set_uposition (gtkwin_look, wx, wy);
	  gtk_widget_set_usize (gtkwin_look, w, h);
	}
	if (!strcmp(buf,"win_message:")) {
	  gtk_widget_set_uposition (gtkwin_message, wx, wy);
	  gtk_widget_set_usize (gtkwin_message, w, h);
	}
    }
}


/* -------------------------------------------------------------------------*/

#if 0
void check_x_events() {
  KeySym gkey;
 
  draw_lists();		

  while (XPending(display)!=0) {
    XNextEvent(display,&event);
    switch(event.type) {

    case ConfigureNotify:
      if(event.xconfigure.window==infodata.win_info)
	resize_win_info(&event);
      else if(event.xconfigure.window==inv_list.win)
	resize_list_info(&inv_list, event.xconfigure.width,
			 event.xconfigure.height);
      else if(event.xconfigure.window==look_list.win)
	resize_list_info(&look_list, event.xconfigure.width,
			 event.xconfigure.height);
      break;

    case Expose:

      if (event.xexpose.count!=0) continue;
      if(event.xexpose.window==win_stats) {
	XClearWindow(display,win_stats);
	draw_stats(1);
      } else if(event.xexpose.window==infodata.win_info)
	draw_all_info();
      else if(event.xexpose.window==inv_list.win)
	draw_all_list(&inv_list);
      else if(event.xexpose.window==look_list.win)
	draw_all_list(&look_list);
      else if(event.xexpose.window==win_message)
	draw_all_message();
      else if(event.xexpose.window==win_game) {
	if (cpl.showmagic) draw_magic_map();
	else display_map_doneupdate();
      } else if(split_windows==FALSE && event.xexpose.window==win_root) {
	XClearWindow(display,win_root);
      }
      break;

    case MappingNotify:
      XRefreshKeyboardMapping(&event.xmapping);
      break;


    case ButtonPress:
	if(event.xbutton.window==win_game) {
	  parse_game_button_press(event.xbutton.button,event.xbutton.x,
		event.xbutton.y);

	} else if(event.xbutton.window==inv_list.win) {
		buttonpress_in_list(&inv_list, &event.xbutton);

	} else if(event.xbutton.window==look_list.win) {
	  buttonpress_in_list(&look_list, &event.xbutton);
	}
	else if (event.xbutton.window==infodata.win_info) {
	  buttonpress_in_info(&event.xbutton);
	}
      break;

    case KeyRelease:
	parse_key_release(event.xkey.keycode, gkey);
	break;

    case KeyPress:
	do_key_press();
	break;
    }
  }
    if (cpl.showmagic) magic_map_flash_pos();
}

#endif

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
    puts("Usage of cfclient:\n\n");
    puts("-server <name>   - Connect to <name> instead of localhost.");
    puts("-port <number>   - Use port <number> instead of the standard port number");
    puts("-display <name>  - Use <name> instead if DISPLAY environment variable.\n");
    puts("-split           - Use split windows.");
    puts("-echo            - Echo the bound commands");
    puts("-pix             - Use bitmaps instead of the font.");
#ifdef Xpm_Pix
    puts("-xpm             - Use color pixmaps (XPM) for display.");
#endif
    puts("-showicon        - Print status icons in inventory window");
    puts("-scrolllines <number>    - number of lines for scrollback");
    puts("-image           - get all images from server at startup");
    puts("-sync            - Synchronize on display");
    puts("-help            - Display this message.");
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

    for (on_arg=1; on_arg<argc; on_arg++) {
	if (!strcmp(argv[on_arg],"-display")) {
	    if (++on_arg == argc) {
		fprintf(stderr,"-display requires a display name\n");
		return 1;
	    }
	    display_name = argv[on_arg];
	    continue;
	}
	if (strcmp(argv[on_arg],"-sync")==0) {
	    sync_display = 1;
	    continue;
	}
	if (!strcmp(argv[on_arg],"-port")) {
	    if (++on_arg == argc) {
		fprintf(stderr,"-port requires a port number\n");
		return 1;
	    }
	    port_num = atoi(argv[on_arg]);
	    continue;
	}
	if (!strcmp(argv[on_arg],"-server")) {
	    if (++on_arg == argc) {
		fprintf(stderr,"-server requires a host name\n");
		return 1;
	    }
	    server = argv[on_arg];
	    continue;
	}
	if (!strcmp(argv[on_arg],"-xpm")) {
#ifdef Xpm_Pix
	    display_mode = Xpm_Display;
	    continue;
#else
	    fprintf(stderr,"Client not configured with Xpm display mode enabled\n");
	    fprintf(stderr,"Ignoring -xpm flag\n");
	    continue;
#endif
	}
	else if (!strcmp(argv[on_arg],"-pix")) {
	    display_mode = Pix_Display;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-cache")) {
	    cache_images= TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-split")) {
	    split_windows=TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-showicon")) {
	    inv_list.show_icon = TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-echo")) {
	    cpl.echo_bindings=TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-scrolllines")) {
	    if (++on_arg == argc) {
		fprintf(stderr,"-scrolllines requires a number\n");
		return 1;
	    }
	    infodata.maxlines = atoi(argv[on_arg]);
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-help")) {
	    usage(argv[0]);
	    continue;
	}
	else {
	    fprintf(stderr,"Do not understand option %s\n", argv[on_arg]);
	    usage(argv[0]);
	    return 1;
	}
    }
    /* Finished parsing all the command line options.  Now start
     * working on the display.
     */
    gargc=argc;
    gargv=argv;
    for (on_arg=0; on_arg<MAXPIXMAPNUM; on_arg++)
	facecachemap[on_arg]=on_arg;

    if (get_root_display(display_name,gargc,gargv)
	/*get_game_display() ||*/
	/*get_stats_display() ||*/
	/*	get_info_display() ||*/
	/*get_inv_display() ||*/
	/*get_look_display() ||
	get_message_display()*/
	)
		return 1;

    init_keys();
    if (cache_images) init_cache_data();
    destroy_splash();
    return 0;
}


void display_map_clearcell(long x,long y)
{
  the_map.cells[x][y].count = 0;
}

void display_map_addbelow(long x,long y,long face)
{
  the_map.cells[x][y].faces[the_map.cells[x][y].count] = face&0xFFFF;
  the_map.cells[x][y].count ++;
}

/* Draw the tiled pixmap tiles in the mapcell */

void display_mapcell_pixmap(int ax,int ay)
{
  int k;

 gdk_draw_rectangle (gtkmap[ax][ay].gdkpixmap, 
		     gtkmap[ax][ay].pixmap->style->mid_gc[0],
		     TRUE,
		     0,
		     0,
		     24,
		     24);

  for(k=the_map.cells[ax][ay].count-1;k>-1;k--) {
    gen_draw_face(the_map.cells[ax][ay].faces[k], ax,ay);
  }
}


int display_usebitmaps()
{
  return display_mode == Pix_Display;
}

int display_noimages()
{
  return display_mode == Font_Display;
}


int display_willcache()
{
    return cache_images;
}

/* Do the map drawing */

void display_map_doneupdate()
{
  int ax,ay;
  GdkGC *black_gc;

  if (updatelock < 30) {
  updatelock++;

  /* draw black on all non-visible squares, and tile pixmaps on the others */
  for(ax=0;ax<11;ax++) {
    for(ay=0;ay<11;ay++) { 
      if (the_map.cells[ax][ay].count==0) {
	black_gc = gtkmap[ax][ay].pixmap->style->black_gc;
	gdk_draw_rectangle (gtkmap[ax][ay].gdkpixmap, black_gc,
			    TRUE,
			    0,
			    0,
			    24,
			    24);
	continue;
      } 
      display_mapcell_pixmap(ax,ay);
    }
  }
  gtk_widget_draw (table, NULL);
  }
  else {
    /*    printf ("WARNING - Frozen updates until updatelock is cleared!\n");*/
  }
}



void display_mapscroll(int dx,int dy)
{
  int x,y;
  struct Map newmap;
  
  for(x=0;x<11;x++) {
    for(y=0;y<11;y++) {
      newmap.cells[x][y].count = 0;
      if (x+dx < 0 || x+dx >= 11)
      continue;
      if (y+dy < 0 || y+dy >= 11)
      continue;
	memcpy((char*)&(newmap.cells[x][y]), (char*)&(the_map.cells[x+dx][y+dy]),
	       sizeof(struct MapCell));
    }
  }
  memcpy((char*)&the_map,(char*)&newmap,sizeof(struct Map));

}

/*void display_newpixmap(long face,char *buf,long buflen)
{
  FILE *tmpfile;
  char tmpfilename[200];

  GtkStyle *style;

  sprintf(tmpfilename,"/tmp/xclient.%d",(int)getpid());
  tmpfile = fopen(tmpfilename,"w");
  fprintf(tmpfile,"%s",buf);
  fclose(tmpfile);

  style = gtk_widget_get_style(gtkwin_root);

  pixmaps[face].gdkpixmap = gdk_pixmap_create_from_xpm(gtkwin_root->window,
                                              &pixmaps[face].gdkmask,
                                               &style->bg[GTK_STATE_NORMAL],
                                              (gchar *) tmpfilename );
  
}*/
void display_newpixmap(long face,char *buf,long buflen)
{
  FILE *tmpfile;

  GtkStyle *style;
  gchar **xpmbuffer;
  
  
  if (cache_images) {
    if (facetoname[face]==NULL) {
      fprintf(stderr,"Caching images, but name for %ld not set\n", face);
    }
    else if ((tmpfile = fopen(facetoname[face],"w"))==NULL) {
      fprintf(stderr,"Can not open %s for writing\n", facetoname[face]);
    }
    else {
      fprintf(tmpfile,"%s",buf);
      fclose(tmpfile);
    }
    style = gtk_widget_get_style(gtkwin_root);

   xpmbuffer=xpmbuffertodata(buf);
    pixmaps[face].gdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_root->window,
							 &pixmaps[face].gdkmask,
							 &style->bg[GTK_STATE_NORMAL],
							 (gchar **) xpmbuffer );
    

    freexpmdata (xpmbuffer);

    redraw_needed=TRUE;

  } else {

    style = gtk_widget_get_style(gtkwin_root);

    xpmbuffer=xpmbuffertodata(buf);
    pixmaps[face].gdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_root->window,
							 &pixmaps[face].gdkmask,
							 &style->bg[GTK_STATE_NORMAL],
							 (gchar **) xpmbuffer );
    

    freexpmdata (xpmbuffer);
  }

  if (facetoname[face] && cache_images) {
    free(facetoname[face]);
    facetoname[face]=NULL;
  }
}

void display_map_startupdate()
{
}

/* This function draws the magic map in the game window.  I guess if
 * we wanted to get clever, we could open up some other window or
 * something.
 *
 * A lot of this code was taken from server/xio.c  But being all
 * the map data has been figured, it tends to be much simpler.
 */









