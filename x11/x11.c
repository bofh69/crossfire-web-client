const char *rcsid_x11_x11_c =
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
 * This file handles mose of the windowing stuff.  The idea is
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

#define X_PROG_NAME "cfclient"

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


#include <client.h>
#include "clientbmap.h"
#include <item.h>
#include <config.h>
#include <script.h>
#include <p_cmd.h>

#ifdef HAVE_LIBXPM
#include <X11/xpm.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "mapdata.h"
#include "x11proto.h"
#include "x11.h"

#include <errno.h>

#if defined(__pyrsoft)
#define _Xconst 
#endif

/* All the following are static because these variables should
 * be local only to this file.  Since the idea is to have only
 * this file be replaced for different windowing systems, use of
 * any of these variables anyplace else would not be portable.
 */

typedef enum inventory_show {
  show_all = 0, show_applied = 0x1, show_unapplied = 0x2, show_unpaid = 0x4,
  show_cursed = 0x8, show_magical = 0x10, show_nonmagical = 0x20,
  show_locked = 0x40, show_unlocked = 0x80,
  show_mask=0xff
} inventory_show;

/*
 *  This is similar obwin, but totally redone for client
 */
typedef struct {
    item *env;		  /* Environment shown in window */
    char title[MAX_BUF];  /* title of item list */
    char old_title[MAX_BUF];  /* previos title (avoid redrawns) */

    Window win;		  /* for X-windows */
    GC gc_text;
    GC gc_icon;
    GC gc_status;

    uint8 show_icon:1;	  /* show status icons */
    uint8 show_weight:1;  /* show item's weight */

    char format_nw[20];	  /* sprintf-format for text (name and weight) */
    char format_nwl[20];  /* sprintf-format for text (name, weight, limit) */
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
    inventory_show show_what;	/* What to show in inventory */
    uint32  weight_limit;   /* Weight limit for this list - used for title */
} itemlist;

int noautorepeat = FALSE;	/* turn off autorepeat detection */

static char *font_name="8x13",	**gargv;

#define SCROLLBAR_WIDTH	16	/* +2+2 for border on each side */
#define INFOCHARS 50
#define INFOLINES 36
/* Perhaps decent defaults, but not quite right */
static int  FONTWIDTH= 8;
static int FONTHEIGHT= 13;
#define MAX_INFO_WIDTH 80
#define MAXNAMELENGTH 50

/* What follows is various constants (or calculations) for various
 * window sizes.
 */

/* Width (and height) of the game window */
#define GAME_WIDTH  (image_size * use_config[CONFIG_MAPWIDTH] + 5)

#define STAT_HEIGHT 140

/* Width of the inventory and look window */
#define INV_WIDTH   300
/* spacing between windows */
#define WINDOW_SPACING	3
/* Height of the master (root) window */
#define ROOT_HEIGHT	522

static int gargc, old_mapx=11, old_mapy=11;

Display *display;
static Window def_root;	/* default root window */
static long def_screen;	/* default screen number */
static unsigned long foreground,background;
Window win_stats,win_message;
Window win_root,win_game;
Colormap colormap;
static XColor discolor[16];
static XFontStruct *font;	/* Font loaded to display in the windows */
static XEvent event;
static XSizeHints messagehint, roothint;
static Atom wm_delete_window;

/* This struct contains the information to draw 1 line of data. */
typedef struct {
    char	*info;		/* Actual character data for a line */
    uint8	color;		/* Color to draw that line */
} InfoLine;

/* This contains all other information for the info window */
typedef struct {
    uint16	info_chars;	/* width in chars of info window */
    uint16	max_info_chars;	/* Max value of info_chars */
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

InfoData infodata = {0, 0, 0, 0, 1, 0, INFOLINES, INFOLINES, NDI_BLACK,
	NULL, 0, 0,0,0,0,0,0,0,0};

uint8	image_size=24;



static char stats_buff[7][600];
struct PixmapInfo *pixmaps[MAXPIXMAPNUM];
/* Off the 'free' space in the window, this floating number is the
 * portion that the info takes up.
 */
static float info_ratio=0;

#define XPMGCS 100

enum {
    no_icon = 0, locked_icon, applied_icon, unpaid_icon,
    damned_icon, cursed_icon, magic_icon, close_icon, 
    stipple1_icon, stipple2_icon, max_icons
};
static Pixmap icons[max_icons];

static Pixmap icon,xpm_pixmap,xpm_masks[XPMGCS], dark1, dark2, dark3;
static GC gc_root,gc_stats,gc_message,
	gc_floor,gc_xpm_object,gc_clear_xpm,gc_xpm[XPMGCS],
	gc_blank; 
GC gc_game;
static GC gc_copy;		/* used for copying when scrolling map view */

/*
 * These are used for inventory and look window
 */
static itemlist look_list, inv_list;

/* Used to know what stats has changed */
static Stats last_stats = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* info win */
#define INFOCHARS 50


/* This is the loop that the client goes through once all the
 * initialization is done.  Basically, it checks for input and
 * processes X events (calls function to do that.)
 * The time for command_loop is fairly arbitrary - it can be most
 * any value.  If it is very low, however, as it will be doing a lot
 * of checks to see if there is data instead of blocking on input.
 *
 * check_x_events takes all the events that are waiting.
 */

extern int maxfd;

/* Handle errors.  I really needed this when debugging
 * the crashes with the big image stuff - I need to know
 * what function is causing the crash.
 */
int error_handler(Display *dp, XErrorEvent *xe)
{
    char buf[MAX_BUF];

    XGetErrorText(dp, xe->error_code, buf, MAX_BUF-1);
    fprintf(stderr,buf);
    /* If you want to try to live through errors, comment out
     * the abort below.
     */
    abort();

    return 0;	/* just to prevent warnings */
}

void event_loop()
{
    fd_set tmp_read;
    int pollret;
    struct timeval timeout;


    if (MAX_TIME==0) {
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
    }
    maxfd = csocket.fd + 1;
    while (1) {
	if (csocket.fd==-1) return;

	/* Do a quick check here for better performance */
	check_x_events();

	FD_ZERO(&tmp_read);
	FD_SET(csocket.fd, &tmp_read);
	script_fdset(&maxfd,&tmp_read);
	if (MAX_TIME!=0) {
	    timeout.tv_sec = MAX_TIME / 1000000;
	    timeout.tv_usec = MAX_TIME % 1000000;
	}
	pollret = select(maxfd, &tmp_read, NULL, NULL, &timeout);
	if (pollret==-1) {
	    fprintf(stderr, "Got errno %d on select call.\n", errno);
	}
	else if (FD_ISSET(csocket.fd, &tmp_read)) {
	    DoClient(&csocket);
	}
	else {
	    script_process(&tmp_read);
	}
	animate_objects();  /* Do this before the x events, since they
			     * can redraw this for us.
			     */
	check_x_events();
    }
}

int misses=0,total=0,newimages=0;

/* Draws 'face' onto 'where' at x,y.
 * sx and sy is the the offset to draw from.
 */

static void gen_draw_face(Drawable where,int face,int x,int y, int sx, int sy)
{
    if (face < 0 || face >= MAXPIXMAPNUM) {
	fprintf(stderr,"Invalid face number: %d @ %d, %d\n", face, x, y);
	return;
    }
    if (pixmaps[face]->mask == None) {
	XCopyArea(display, pixmaps[face]->pixmap,
	    where,gc_floor,
	    sx,sy,image_size,image_size,x,y);

    /* Xpm and png do exactly the same thing */
    } else {
	/* Basically, what it looks like all this does is to try and preserve
	 * gc's with various clipmasks set. */

	int gcnum,i;
	Pixmap mask;
	GC gc;
	total++;
	mask = pixmaps[face]->mask;
	/* Lets see if we can find a stored mask with matching gc */
	for(gcnum=0;gcnum<XPMGCS;gcnum++) {
	    if (xpm_masks[gcnum] == mask)
		break;
	}
	/* Nope - we didn't. set one up, but only temporarily */
	if (gcnum == XPMGCS) {
	    misses++;
	    gcnum--;
	    gc = gc_xpm[gcnum];
	    XSetClipMask(display,gc,mask);
	}
	gc = gc_xpm[gcnum];
	/* Now, we move all the ones up one, and then place the one just used
	 * at position 0.  Thus, the one in the last position was the least
	 * used entry
	 */
	for(i=gcnum-1;i>=0;i--) {
	    xpm_masks[i+1] = xpm_masks[i];
	    gc_xpm[i+1] = gc_xpm[i];
	}
	xpm_masks[0] = mask;
	gc_xpm[0] = gc;
	/* Hopefully, this isn't too costly - needed for the inventory and look
	 * window drawing code.
	 */
	XSetClipOrigin(display, gc_xpm[0], x - sx , y - sy);
	XCopyArea(display, pixmaps[face]->pixmap,
	    where,gc_xpm[0],
	    sx,sy,image_size,image_size,x,y);
    }
}

void end_windows()
{
    XFreeGC(display, gc_root);
    XFreeGC(display, gc_game);
    XFreeGC(display, gc_copy);
    XFreeGC(display, gc_stats);
    XFreeGC(display, infodata.gc_info);
    XFreeGC(display, inv_list.gc_text);
    XFreeGC(display, inv_list.gc_icon);
    XFreeGC(display, inv_list.gc_status);
    XFreeGC(display, look_list.gc_text);
    XFreeGC(display, look_list.gc_icon);
    XFreeGC(display, look_list.gc_status);
    XFreeGC(display, gc_message);
    XFreeGC(display, gc_xpm_object);
    XDestroyWindow(display,win_game);
    XCloseDisplay(display);
}





/***********************************************************************
 *
 * Stats window functions follow
 *
 ***********************************************************************/

static int get_game_display(void) {
    XSizeHints gamehint;
    int i;
   
    gamehint.x=INV_WIDTH + WINDOW_SPACING;
    gamehint.y=STAT_HEIGHT + WINDOW_SPACING;

    gamehint.width=GAME_WIDTH;
    gamehint.height=gamehint.width;

    gamehint.max_width=gamehint.min_width=gamehint.width;
    gamehint.max_height=gamehint.min_height=gamehint.height;
    gamehint.flags=PPosition | PSize;
    win_game=XCreateSimpleWindow(display,win_root,
	gamehint.x,gamehint.y,gamehint.width,gamehint.height,2,
	background,foreground);
    icon=XCreateBitmapFromData(display,win_game,
	(_Xconst char *) crossfire_bits,
	(unsigned int) crossfire_width, (unsigned int)crossfire_height);
    if (want_config[CONFIG_SPLITWIN]) {
	allocate_colors(display, win_root, def_screen,
	    &colormap, discolor);
	foreground=discolor[0].pixel;
	background=discolor[9].pixel;
    } else
	XSetWindowColormap(display, win_game, colormap);

    XSetStandardProperties(display,win_game,X_PROG_NAME, X_PROG_NAME,
	icon,gargv,gargc, &(gamehint));

    gc_game=XCreateGC(display,win_game,0,0);
    XSetForeground(display,gc_game,discolor[0].pixel);
    XSetBackground(display,gc_game,discolor[9].pixel);

    XSetGraphicsExposures(display, gc_game, False);
    gc_copy=XCreateGC(display,win_game,0,0);
    XSetGraphicsExposures(display, gc_game, True);
    gc_floor = XCreateGC(display,win_game,0,0);
    XSetGraphicsExposures(display, gc_floor, False);
    gc_blank = XCreateGC(display,win_game,0,0);
    XSetForeground(display,gc_blank,discolor[0].pixel);	/*set to black*/
    XSetGraphicsExposures(display,gc_blank,False);

    for (i=0; i<XPMGCS; i++) {
	    gc_xpm[i] = XCreateGC(display, win_game, 0,0);
	    XSetClipOrigin(display, gc_xpm[i], 0, 0);
	    XSetGraphicsExposures(display, gc_xpm[i], False);
    }
    gc_xpm_object = XCreateGC(display,win_game,0,0);
    XSetGraphicsExposures(display, gc_xpm_object, False);
    XSetClipOrigin(display, gc_xpm_object,0, 0);
    xpm_pixmap = XCreatePixmap(display, def_root, image_size, image_size,
		DefaultDepth(display, DefaultScreen(display)));
    gc_clear_xpm = XCreateGC(display,xpm_pixmap,0,0);
    XSetGraphicsExposures(display,gc_clear_xpm,False);
    XSetForeground(display,gc_clear_xpm,discolor[12].pixel); /* khaki */

    XSelectInput(display,win_game,
	ButtonPressMask|KeyPressMask|KeyReleaseMask|ExposureMask);
    XSetWMProtocols(display, win_game, &wm_delete_window, 1);
    XMapRaised(display,win_game);
    return 0;
}


/******************************************************************************
 *
 * The functions dealing with the info window follow
 *
 *****************************************************************************/

static int get_info_display(void) {
    XSizeHints infohint;
    int i;

    /* The following could happen if bad values are given. */
    if (infodata.maxlines<INFOLINES) infodata.maxlines=INFOLINES;
    infohint.x=INV_WIDTH + GAME_WIDTH + WINDOW_SPACING*2;
    infohint.y=0;
    infohint.width=infodata.width=6+INFOCHARS*FONTWIDTH;
    infohint.height=infodata.height=roothint.height;
    infodata.maxdisp = roothint.height/FONTHEIGHT;

    infohint.min_width=100;
    infohint.min_height=30;
    infohint.flags=PPosition | PSize;
    infodata.win_info=XCreateSimpleWindow(display, win_root,
	infohint.x,infohint.y,infohint.width,infohint.height,2,
	foreground,background);
    XSetWindowColormap(display, infodata.win_info, colormap);
    icon=XCreateBitmapFromData(display,infodata.win_info,
	(_Xconst char *) crossfire_bits,
	(unsigned int) crossfire_width, (unsigned int)crossfire_height);
    XSetStandardProperties(display,infodata.win_info,"Crossfire - text",
	"Crosstext",icon,gargv,gargc,&(infohint));
    infodata.gc_info=XCreateGC(display,infodata.win_info,0,0);
    XSetForeground(display,infodata.gc_info,foreground);
    XSetBackground(display,infodata.gc_info,background);

    XSetFont(display,infodata.gc_info,font->fid);

    XSelectInput(display,infodata.win_info,
	ButtonPressMask|KeyPressMask|KeyReleaseMask|ExposureMask|
	StructureNotifyMask);
    XSetWMProtocols(display, infodata.win_info, &wm_delete_window, 1);
    XMapRaised(display,infodata.win_info);
    if (infodata.maxlines>infodata.maxdisp) infodata.has_scrollbar=1;
    infodata.info_chars = (infohint.width/FONTWIDTH)-1;
    if (infodata.has_scrollbar) infodata.info_chars -=3;
    infodata.max_info_chars=infodata.info_chars;
    infodata.data=(InfoLine *) malloc(sizeof(InfoLine) * infodata.maxlines);
    infodata.bar_length=infodata.height - 8;
    for (i=0; i<infodata.maxlines; i++) {
	infodata.data[i].info = malloc(sizeof(char)* (infodata.info_chars+1));
	infodata.data[i].info[0]='\0';
	infodata.data[i].color=0;
    }

    return 0;
}

static void delete_ch(void) {
    if(strlen(cpl.input_text)==0)
	return;
    cpl.input_text[strlen(cpl.input_text)-1] = '\0';

    /* If not on the first line but backspacing to the front, we need to
     * do some special handling.
     */
    if ((strlen(cpl.input_text)>3) &&
      strlen(infodata.data[infodata.infopos].info)<3) {
        int line=infodata.infopos-1;
        if (line<0) line=infodata.numlines;
        strcpy(infodata.data[infodata.infopos].info, 
	       infodata.data[line].info);
        infodata.data[infodata.infopos].info[
	    strlen(infodata.data[infodata.infopos].info)-1]=0;
	XDrawImageString(display,infodata.win_info,infodata.gc_info,
		FONTWIDTH, (infodata.infoline+1)*FONTHEIGHT,
		infodata.data[infodata.infopos].info,
		strlen(infodata.data[infodata.infopos].info));
    } else {
        infodata.data[infodata.infopos].info[
	    strlen(infodata.data[infodata.infopos].info)-1]=0;
	XDrawImageString(display,infodata.win_info,infodata.gc_info,
		(strlen(infodata.data[infodata.infopos].info)+1)*FONTWIDTH,
		(infodata.infoline+1)*FONTHEIGHT," ",1);
    }
}

/* Writes one character to the screen.  Used when player is typing
 * stuff we that we want to appear, or used to give prompts.
 */

void write_ch(char key)
{
    char c2[2];

    /* Sort of a gross hack, but this gets it so that we actually put
     * the command into the buffer.
     */
    if (key==13) {
        /* We turn off command mode for the draw_info call, because
         * it has special handling for normal output during command
         * mode; but we do this manually now. 
	 */
        Input_State old_state = cpl.input_state;
        cpl.input_state = Playing;
	draw_info(infodata.data[infodata.infopos].info,NDI_BLACK);
        cpl.input_state = old_state;
	return;
    }

    if (infodata.lastcolor!=NDI_BLACK) {
	XSetForeground(display,infodata.gc_info,discolor[NDI_BLACK].pixel);
	infodata.lastcolor=NDI_BLACK;
    }

    if (key == 9) { /* Tab */
	/* check for command mode */
	if (infodata.data[infodata.infopos].info[0] == '>') {
	    const char *str = complete_command(infodata.data[infodata.infopos].info+1);

	    if (str != NULL) {
		/* +1 so that we keep our > at start of line.  Don't
		 * recopy the data on top of ourself.
		 */
		strcpy(infodata.data[infodata.infopos].info+1, str);
		strcpy(cpl.input_text, str);
	    }
	}
    } else {

	if ((key < 32 || (unsigned char) key > 127) && key != 8)
	    return;
	c2[0] = key;
	c2[1] ='\0';


	if(key==8||key==127) {
	    /* By backspacking enough, let them get out of command mode */
	    if (cpl.input_text[0]=='\0' && cpl.input_state==Command_Mode) {
		cpl.input_state=Playing;
		/* Erase the prompt */
		XDrawImageString(display,infodata.win_info,infodata.gc_info,
				 FONTWIDTH,(infodata.infoline+1)*FONTHEIGHT," ",1);
	    }
	    delete_ch();
	    return;
	}
	/* Give some leeway here */
	if(strlen(cpl.input_text)>=(MAX_BUF-15))
	    return;

	strcat(cpl.input_text,c2);
    }
    
    if(strlen(infodata.data[infodata.infopos].info)>=(infodata.info_chars-2)) {
        /* Draw the currently line and scroll down one */

        /* We turn off command mode for the draw_info call, because
         * it has special handling for normal output during command
         * mode; but we do this manually now. 
	 */
        cpl.input_state = Playing;
	draw_info(infodata.data[infodata.infopos].info,NDI_BLACK);
        cpl.input_state = Command_Mode;
	infodata.data[infodata.infopos].info[0]=(((strlen(cpl.input_text)/ 
	    infodata.info_chars))%10)+49;
        infodata.data[infodata.infopos].info[1]='>';
	infodata.data[infodata.infopos].info[2]=0;
	XDrawImageString(display,infodata.win_info,infodata.gc_info,
		FONTWIDTH,(infodata.infoline+1)*FONTHEIGHT,
		infodata.data[infodata.infopos].info,
		strlen(infodata.data[infodata.infopos].info));
    }

    if (key != 9 ) strcat(infodata.data[infodata.infopos].info,(cpl.no_echo? "?": c2));

    XDrawImageString(display,infodata.win_info,infodata.gc_info,
	FONTWIDTH,(infodata.infoline+1)*FONTHEIGHT,
	infodata.data[infodata.infopos].info,
	strlen(infodata.data[infodata.infopos].info));
}



/* This is similar to draw_info below, but doesn't advance to a new
 * line.  Generally, queries use this function to draw the prompt for
 * the name, password, etc.
 * This also starts from character position 0.  Thus, only 1 call of this
 * per a given line is useful
 */

void draw_prompt(const char *str)
{
    if (infodata.lastcolor!=NDI_BLACK) {
	XSetForeground(display,infodata.gc_info,discolor[NDI_BLACK].pixel);
	infodata.lastcolor=NDI_BLACK;
    }

    strncpy(infodata.data[infodata.infopos].info,str,infodata.info_chars);
    infodata.data[infodata.infopos].info[infodata.info_chars] = '\0';
    infodata.data[infodata.infopos].color=NDI_BLACK;
    XDrawImageString(display,infodata.win_info,
	infodata.gc_info,FONTWIDTH,(infodata.infoline+1)*FONTHEIGHT,
	infodata.data[infodata.infopos].info,
	strlen(infodata.data[infodata.infopos].info));
}

/* If redarew is true, draw the scrollbar no matter what */
static void draw_info_scrollbar(int redraw)
{
    static int last_length=0, last_y=0;

    if (!infodata.has_scrollbar) return;

    if (infodata.numlines <infodata.maxdisp) {
	infodata.bar_size=infodata.bar_length;
	infodata.bar_y=0;
    }
    else {
	infodata.bar_size=infodata.bar_length*infodata.maxdisp/infodata.numlines;
	infodata.bar_y=infodata.bar_length*(infodata.bar_pos-infodata.maxdisp)/infodata.numlines;
	if (infodata.bar_y<0) infodata.bar_y=0;
    }
/*
    if ((infodata.bar_size+infodata.bar_y)>infodata.bar_length) {
	    infodata.bar_y=infodata.bar_length-infodata.bar_size;
    }
*/
    if (!redraw && last_length==infodata.bar_size && last_y==infodata.bar_y) return;

    last_y=infodata.bar_y;
    last_length=infodata.bar_size;

    /* Note - with the way this is set up, it wouldn't be too hard to make
     * the scrollbar color customizable
     */
    if (infodata.lastcolor!=NDI_BLACK) {
      XSetForeground(display,infodata.gc_info,discolor[NDI_BLACK].pixel);
      infodata.lastcolor=NDI_BLACK;
    }
	
    XDrawRectangle(display, infodata.win_info,
	infodata.gc_info, infodata.width-SCROLLBAR_WIDTH-6,
		   3, 20,
		   infodata.height -6);
    XClearArea(display, infodata.win_info, 
		   infodata.width-SCROLLBAR_WIDTH-4, 4, 16, 
		   infodata.bar_length, False);

    XFillRectangle(display, infodata.win_info, infodata.gc_info,
	infodata.width - SCROLLBAR_WIDTH-4,4+infodata.bar_y,
	16, infodata.bar_size);
}

/* draw_info adds a line to the info window. */

void draw_info(const char *str, int color) {
  char *cp;
  uint16 new_infopos = (infodata.infopos+1)% infodata.maxlines ;
  size_t len;

  if(str == (char *) NULL) {
    draw_info("[NULL]",color);
    return;
  }

  if((cp=strchr(str,'\n'))!=NULL) {
    /* 4096 is probably way overkill, but 1024 could very well be too small.
     * And I don't see the need to malloc and then free this either -
     * this is a single user program.
     */
    char obuf[4096],*buf = obuf;

    strncpy(buf,str, 4095);
    do {
      if ((cp = strchr(buf, '\n'))) {
	*cp='\0';
	draw_info(buf,color);
	buf = cp +1;
      } else
	draw_info(buf,color);
    } while (cp!=NULL);
    return;
  }

  /* Lets do the word wrap for messages - MSW */
  if ((int)strlen(str) >= infodata.info_chars) {
	int i=infodata.info_chars-1;

	/* i=last space (or ')' for armor.  Wrap armor, because
	otherwise, the two sets of ()() can be about half the line */
	while ((str[--i]!=' ') && (str[i]!=')') && (i!=0)) ;
	/* if i==0, string has no space.  Just let it be truncated */
	if (i!=0) {
	    char *buf = (char *)malloc(sizeof(char)*(i+2));
	    int j;

	    i++;	/* want to keep the ')'.  This also keeps
			the space, but that really doesn't matter */
	    strncpy(buf, str, i);
	    buf[i]='\0';
	    draw_info(buf,color);
	    free(buf);

	    for (j=i; j < (int)strlen(str); j++) /* if the wrap portion is */
		if (str[j]!=' ') break;		/* only space, don't wrap it*/
	    if ((((strlen(str)-i)!=1) || (str[i]!='.')) && (j!=strlen(str)))
		draw_info((str+i),color);
	    return;
	}
  }
  
  /* This is the real code here - stuff above is just formating and making
   * it look nice.  This stuff here is actually drawing the code
   */

  /* clear the new last line in window */
  memset(infodata.data[new_infopos].info, 32, infodata.info_chars-1);
  if(cpl.input_state == Command_Mode)
  {
      /* we copy the last command line to the new last line in window */
      strcpy(infodata.data[new_infopos].info, infodata.data[infodata.infopos].info);
  }
  infodata.data[new_infopos].info[infodata.info_chars] = '\0';
  
  len = MIN(strlen(str), infodata.info_chars);
  memmove(infodata.data[infodata.infopos].info, str, len);
  infodata.data[infodata.infopos].info[len] = '\0';
  infodata.data[infodata.infopos].color=color;

  /* This area is for scrollbar handling.  The first check is to see if
   * the scrollbar is at the very end, if it is, then we don't care about this.
   * IF not at the end, then see if it is at the end of the window.  If
   * so, increase the bar position so that that view area keeps up with what
   * is being drawn.  If we are not at the end of the buffer, then decrease
   * the bar position - in this way, we keep the same viewable area visible
   * for redraws.
   *
   * A couple notes:  If jump to end was desired on output, then this
   * code just needs to be replaced with a line like infodata.bar_pos=
   * infodata.numlines.
   * If it is desired for the window to scroll up as new output is printed
   * out, then the second case would need to be removed, and a draw_all_info
   * call added instead.
   */
  if (infodata.bar_pos<infodata.maxlines) {
	if (infodata.bar_pos==infodata.numlines) {
		infodata.bar_pos++;
	}
	else if (infodata.numlines==infodata.maxlines) {
	    infodata.bar_pos--;
	    if (infodata.bar_pos<infodata.maxdisp)
		infodata.bar_pos=infodata.maxdisp;
	}
  }
  if (infodata.numlines<infodata.maxlines) infodata.numlines++;

  /* Basically, if we don't have a scrollbar, or we are at the end of it,
   * then do the drawing stuff, otherwise don't.
   */
  if (!infodata.has_scrollbar || infodata.bar_pos>=infodata.numlines) {
    /*
     * The XDrawImageString draws the line.
     */
    if (infodata.lastcolor!=color) {
      XSetForeground(display,infodata.gc_info,discolor[color].pixel);
      infodata.lastcolor=color;
    }
  
    XDrawImageString(display,infodata.win_info,
      infodata.gc_info,FONTWIDTH,(infodata.infoline+1)*FONTHEIGHT,
      infodata.data[infodata.infopos].info,
      strlen(infodata.data[infodata.infopos].info));

    /* Now it gets potentially more complicated - now we have to handle
     * wrapping and stuff like that.
     */

    if(++(infodata.infoline)>=infodata.maxdisp){
      if (infodata.scroll_info_window) {
  	XCopyArea(display,infodata.win_info,infodata.win_info,
  	    infodata.gc_info,0,FONTHEIGHT,infodata.info_chars*FONTWIDTH,
  	    infodata.maxdisp*FONTHEIGHT,0,0);
  	infodata.infoline--;
      }
      else
   	infodata.infoline=0;
    }
  }

  infodata.infopos = new_infopos;

  if (!infodata.has_scrollbar || infodata.bar_pos>=infodata.numlines) {
        if(cpl.input_state == Command_Mode)
        {
                uint8 endpos = strlen(infodata.data[infodata.infopos].info);

                infodata.data[infodata.infopos].info[endpos] = ' ';
                XDrawImageString(display,infodata.win_info,
	        infodata.gc_info,FONTWIDTH,(infodata.infoline+1)*FONTHEIGHT,
	        infodata.data[infodata.infopos].info, infodata.info_chars-1);
                infodata.data[infodata.infopos].info[endpos] = '\0';
        }
        else
        {
                XDrawImageString(display,infodata.win_info,
	        infodata.gc_info,FONTWIDTH,(infodata.infoline+1)*FONTHEIGHT,
	        infodata.data[infodata.infopos].info, infodata.info_chars-1);
        }
  }

  /* If in a reply state, grab the input buffer and store it.
   */
  if (cpl.input_state==Reply_Many) {
    strncpy(infodata.data[infodata.infopos].info, cpl.input_text, 
	    infodata.info_chars);
    infodata.data[infodata.infopos].info[infodata.info_chars] = '\0';
     
    XDrawImageString(display,infodata.win_info,infodata.gc_info,
	FONTWIDTH,(infodata.infoline+1)*FONTHEIGHT,
	infodata.data[infodata.infopos].info, 
	strlen(infodata.data[infodata.infopos].info));
  }
  /* We should have some intelligent checks so it is not drawn unnecessarily */
  draw_info_scrollbar(FALSE);
}

/* This is pretty much print_message, but the name is changed, and some
 * unnecessary code has been removed.
 */

void draw_color_info(int colr, const char *buf){
    draw_info(buf,colr);
}


/*
 * draw_all_info is only needed for redraws, which includes scrollbar
 * movement
 */


static void draw_all_info(void) {
  int i;

  XClearWindow(display,infodata.win_info);
  if (infodata.numlines>=infodata.maxdisp) {
     int startline=infodata.infopos-infodata.maxdisp-
	(infodata.numlines-infodata.bar_pos)+1,displine=0;

     if (startline<0) startline+=infodata.maxlines;

     /* If we are scrolling back (bar_pos<numlines), then we want to
      * start drawing from the top, and want to keep displine 0.
      */
     if (!infodata.scroll_info_window) {
	if (infodata.bar_pos>=infodata.numlines) {
		displine=(infodata.infoline+1) % infodata.maxdisp;
		/* So that when we now that we have scrolled back */
		
	}
	else infodata.infoline=infodata.maxdisp-1;
     }

     for (i=0; i<infodata.maxdisp; i++) {
	if (infodata.lastcolor!=infodata.data[startline].color) {
		XSetForeground(display,infodata.gc_info,
			       discolor[infodata.data[startline].color].pixel);
		infodata.lastcolor=infodata.data[startline].color;
	}
	XDrawImageString(display,infodata.win_info,
	    infodata.gc_info,FONTWIDTH,(displine+1)*FONTHEIGHT,
	    infodata.data[startline].info,
	    strlen(infodata.data[startline].info));
	startline++;
	startline %= infodata.maxlines;
	displine++;
	displine %= infodata.maxdisp;
     }
  }
  else  {
      for(i=0;i<=infodata.numlines;i++) {
	if (infodata.lastcolor!=infodata.data[i].color)  {
		XSetForeground(display,infodata.gc_info,
			       discolor[infodata.data[i].color].pixel);
		infodata.lastcolor=infodata.data[i].color;
	}
	XDrawImageString(display,infodata.win_info,
	    infodata.gc_info,FONTWIDTH,(i+1)*FONTHEIGHT,
	    infodata.data[i].info,strlen(infodata.data[i].info));
      }
  }
  draw_info_scrollbar(TRUE);
}

static void resize_win_message(int width, int height) {
    messagehint.width = width;
    messagehint.height = height;
}

static void resize_win_info(int width, int height) {
    int chars=(width/FONTWIDTH)-1;
    int lines=(height/FONTHEIGHT)-1;
    int i;
    InfoLine	*newlines;

    if (infodata.width==width &&
	infodata.height==height) return;

    if(chars<3 || lines<3)
	return;

    infodata.width=width;
    infodata.height=height;
    infodata.bar_length=infodata.height - 8;
    if (infodata.has_scrollbar) chars-=3;


    /* We have a scrollback buffer.  All we need to change then is maxdisp */
    if (infodata.maxdisp != infodata.maxlines && lines<infodata.maxlines) {
	/* Move insert line to bottom of the screen if we are already there
	 * or it would otherwise be off the screen.
	 */
	if (((infodata.infoline+1) == infodata.maxdisp) ||
	    (infodata.infoline >= lines)) infodata.infoline = lines - 1;
	infodata.maxdisp=lines;
    }
    /* The window has changed size, but the amount of data we can display
     * has not.  so just redraw the window and return.
     */
    if (chars == infodata.info_chars && lines == infodata.maxdisp) {
	draw_all_info();
	return;
    }	

    /* Either we have a scrollbar (as above), or the window has not
     * changed in height, so we just need to change the size of the
     * buffers.
     */
    if (lines == infodata.maxdisp) {
	for (i=0; i<infodata.maxlines; i++) {
	    if (chars>infodata.max_info_chars) {
		infodata.data[i].info= realloc(infodata.data[i].info, sizeof(char) * (chars+1));
	    }
	    /* Terminate buffer in both cases */
	    infodata.data[i].info[chars]='\0';
	}
	infodata.info_chars=chars;
	draw_all_info();
	return;
    }
    /* IF we get here, the window has grown or shrunk, and we don't have
     * a scrollbar.  This code is a lot simpler than what was here before,
     * but probably is not as efficient (But with the number of resize
     * events likely, this should not be a big deal).
     */

    /* First, allocate new storage */
    newlines = malloc(sizeof(InfoLine) * lines);
    for (i=0; i<lines; i++) {
	newlines[i].info = malloc(sizeof(char) * (chars +1));
	newlines[i].info[0]='\0';
	newlines[i].color=0;
    }
    /* First case - we can keep all the old data.  Note that the old
     * buffer could have been filled up, so we still need to do some
     * checking to find the start
     */
    if (infodata.numlines <= lines) {
	int start=0,k;

	/* Buffer was full, so the start could be someplace else */
	if (infodata.numlines == infodata.maxlines) {
	    start = infodata.infopos+1;
	}
	for (i=0; i<infodata.numlines; i++) {
	    k= (start+i) % infodata.maxlines;
	    strncpy(newlines[i].info, infodata.data[k].info, chars);
	    newlines[i].info[chars]=0;
	    newlines[i].color = infodata.data[k].color;
	}
    }
    else {
	/* We have to lose data, so keep the most recent. */

	int start=infodata.infopos-lines,k;

	if (start<0) start += infodata.maxlines;
	for (i=0; i<lines; i++) {
	    k= (start+i) % infodata.maxlines;
	    strncpy(newlines[i].info, infodata.data[k].info, chars);
	    newlines[i].info[chars]=0;
	    newlines[i].color = infodata.data[k].color;
	}
	infodata.infopos = 0;
	newlines[0].info[0] = '\0';
	infodata.infoline = lines-1;
	infodata.numlines = lines;
	infodata.bar_pos = lines;
    }
    infodata.maxdisp = lines;
    for (i=0; i<infodata.maxlines; i++) {
	free(infodata.data[i].info);
    }
    free(infodata.data);
    infodata.data = newlines;
    infodata.maxlines = lines;
    infodata.info_chars=chars;
    draw_all_info();
}




/***********************************************************************
 *
 * Stats window functions follow
 *
 ***********************************************************************/

static int get_stats_display(void) {
    XSizeHints stathint;

    stathint.x=INV_WIDTH + WINDOW_SPACING;
    stathint.y=0;
    stathint.width=GAME_WIDTH;
    stathint.height=STAT_HEIGHT;
    stathint.min_width=stathint.max_width=stathint.width;
    stathint.min_height=stathint.max_height=stathint.height;
    stathint.flags=PPosition | PSize;
    win_stats=XCreateSimpleWindow(display,win_root,
	stathint.x,stathint.y,stathint.width,stathint.height,2,
	foreground,background);
    XSetWindowColormap(display, win_stats, colormap);
    icon=XCreateBitmapFromData(display,win_stats,
	(_Xconst char *) crossfire_bits,
	(unsigned int) crossfire_width, (unsigned int)crossfire_height);
    XSetStandardProperties(display,win_stats,"Crossfire - status",
	"crosstatus",icon,gargv,gargc, &(stathint));

    gc_stats=XCreateGC(display,win_stats,0,0);
    XSetForeground(display,gc_stats,foreground);
    XSetBackground(display,gc_stats,background);
    XSetFont(display,gc_stats,font->fid);
    XSelectInput(display,win_stats,KeyPressMask|KeyReleaseMask|ExposureMask);
    XMapRaised(display,win_stats);
    XSetWMProtocols(display, win_stats, &wm_delete_window, 1);
   return 0;
}

/* This draws the stats window.  If redraw is true, it means
 * we need to redraw the entire thing, and not just do an
 * updated.
 */

void draw_stats(int redraw) {
  char buff[MAX_BUF];
  static char last_name[MAX_BUF]="", last_range[MAX_BUF]="";
  int i;
  char *s;

    if (strcmp(cpl.title, last_name) || redraw) {
	strcpy(last_name,cpl.title);
	strcpy(buff,cpl.title);
 	strcat(buff,"                     ");
	XDrawImageString(display,win_stats,
	    gc_stats,10,10, buff,strlen(buff));
    }

    if(redraw || cpl.stats.exp!=last_stats.exp ||
      cpl.stats.level!=last_stats.level) {
	sprintf(buff,"Score: %5" FMT64 "  Level: %d",cpl.stats.exp,
	    cpl.stats.level);
 	strcat(buff,"                     ");
	XDrawImageString(display,win_stats,
	    gc_stats,10,24, buff,strlen(buff));

	last_stats.exp = cpl.stats.exp;
	last_stats.level = cpl.stats.level;
      }

    if(redraw || 
       cpl.stats.hp!=last_stats.hp || cpl.stats.maxhp!=last_stats.maxhp ||
       cpl.stats.sp!=last_stats.sp || cpl.stats.maxsp!=last_stats.maxsp ||
       cpl.stats.grace!=last_stats.grace || 
       cpl.stats.maxgrace!=last_stats.maxgrace) {

	sprintf(buff,"Hp %d/%d  Sp %d/%d Gr %d/%d",
	    cpl.stats.hp, cpl.stats.maxhp,
	    cpl.stats.sp, cpl.stats.maxsp,
	    cpl.stats.grace, cpl.stats.maxgrace);

 	strcat(buff,"                     ");
	XDrawImageString(display,win_stats,
	    gc_stats,10,38, buff,strlen(buff));
	last_stats.hp=cpl.stats.hp;
	last_stats.maxhp=cpl.stats.maxhp;
	last_stats.sp=cpl.stats.sp;
	last_stats.maxsp=cpl.stats.maxsp;
	last_stats.grace=cpl.stats.grace;
	last_stats.maxgrace=cpl.stats.maxgrace;
    }

    if(redraw || cpl.stats.Dex!=last_stats.Dex ||
      cpl.stats.Con!=last_stats.Con || cpl.stats.Str!=last_stats.Str ||
      cpl.stats.Int!=last_stats.Int || cpl.stats.Wis!=last_stats.Wis ||
      cpl.stats.Cha!=last_stats.Cha || cpl.stats.Pow!=last_stats.Pow) {

	sprintf(buff,"S%2d D%2d Co%2d I%2d W%2d P%2d Ch%2d",
	    cpl.stats.Str,cpl.stats.Dex,cpl.stats.Con,
	    cpl.stats.Int,cpl.stats.Wis,cpl.stats.Pow,
	    cpl.stats.Cha);

 	strcat(buff,"                     ");
	XDrawImageString(display,win_stats,
	    gc_stats,10,52, buff,strlen(buff));

	last_stats.Str=cpl.stats.Str;
	last_stats.Con=cpl.stats.Con;
	last_stats.Dex=cpl.stats.Dex;
	last_stats.Int=cpl.stats.Int;
	last_stats.Wis=cpl.stats.Wis;
	last_stats.Cha=cpl.stats.Cha;
	last_stats.Pow=cpl.stats.Pow;
      }

    if(redraw || cpl.stats.wc!=last_stats.wc ||
      cpl.stats.ac!=last_stats.ac ||
      cpl.stats.resists[0]!=last_stats.resists[0] ||
      cpl.stats.dam!=last_stats.dam) {

	sprintf(buff,"Wc:%3d Dam:%3d Ac:%3d Arm:%3d",
	    cpl.stats.wc,cpl.stats.dam,cpl.stats.ac,
	    cpl.stats.resists[0]);
 	strcat(buff,"                     ");
	XDrawImageString(display,win_stats,
	    gc_stats,10,66, buff,strlen(buff));

	last_stats.wc=cpl.stats.wc;
	last_stats.ac=cpl.stats.ac;
	last_stats.dam=cpl.stats.dam;
	last_stats.resists[0] = cpl.stats.resists[0];
      }

  if(redraw || last_stats.speed!=cpl.stats.speed ||
     cpl.stats.food!=last_stats.food ||
     cpl.stats.weapon_sp != last_stats.weapon_sp) {
	/* since both speed and weapon speed have been multiplied by
	 * the same value, to get proper weapon, we only need to divide
	 * by speed - the multiplication/division factor on both factors
	 * out.
	 */
	double weap_sp;

	/* Seems that weapon_sp can be 0 in some cases which caused SIGFPE's */
	if (cpl.stats.weapon_sp ==0) weap_sp = 0;
	else weap_sp = (float) cpl.stats.speed/	((float)cpl.stats.weapon_sp);

	/* The following is generating an FPE on alpha systems - changed
	 * everything to be doubles to see if that might make some
	 * difference.
	 */
	if(cpl.stats.food<100 && (cpl.stats.food&4)) {
	    sprintf(buff,"Speed: %3.2f (%1.2f) Food: *%d* HUNGRY!",
		(double)cpl.stats.speed/FLOAT_MULTF,
		weap_sp,cpl.stats.food);
	    if (use_config[CONFIG_FOODBEEP] && (cpl.stats.food%4==3)) XBell(display, 0);
	} else {
	    sprintf(buff,"Speed: %3.2f (%1.2f)  Food: %3d",
		(float)cpl.stats.speed/FLOAT_MULTF,
		weap_sp, cpl.stats.food);
	    if (use_config[CONFIG_FOODBEEP] && cpl.stats.food<1) XBell(display,100);
	}

 	strcat(buff,"                     ");
	XDrawImageString(display,win_stats,
	    gc_stats,10,80, buff,strlen(buff));

	last_stats.food=cpl.stats.food;
	last_stats.speed = cpl.stats.speed;
	last_stats.weapon_sp = cpl.stats.weapon_sp;
     }
    if (redraw || strcmp(cpl.range, last_range)) {
	strcpy(last_range, cpl.range);
	strcpy(buff,cpl.range);
 	strcat(buff,"                     ");
	XDrawImageString(display,win_stats,
	    gc_stats,10,94, buff,strlen(buff));
    }

    if (redraw) {
      i = 0;
    } else {
      for (i=0; i<MAX_SKILL; i++) {
	if ((cpl.stats.skill_level[i] != last_stats.skill_level[i] ||
	    cpl.stats.skill_exp[i] != last_stats.skill_exp[i]) &&
	    skill_names[i] && cpl.stats.skill_exp[i])
	  break;
      }
    }

    if (i < MAX_SKILL) {
	int on_skill=0;

	*buff = '\0';
	s = buff;
	for (i=0; i<MAX_SKILL; i++) {
	    if (!skill_names[i] || !cpl.stats.skill_exp[i]) continue;

	    last_stats.skill_level[i] = cpl.stats.skill_level[i];
	    last_stats.skill_exp[i] = cpl.stats.skill_exp[i];
	    s += sprintf(s,"%.3s: %7" FMT64 " (%d) ", skill_names[i], cpl.stats.skill_exp[i],
		cpl.stats.skill_level[i]);
	    if ((on_skill % 2) == 1) {
		XDrawImageString(display,win_stats,gc_stats,10,
				108 + (14 * (on_skill / 2)), buff,strlen(buff));
		*buff = '\0';
		s = buff;
	    }
	    on_skill++;
	}
	if (*buff) 
	    XDrawImageString(display,win_stats,gc_stats,10,
			108 + (14 * (on_skill / 2)), buff,strlen(buff));
    }
}

/***********************************************************************
*
* Handles the message window
*
***********************************************************************/

static int get_message_display(void) {

    messagehint.x=INV_WIDTH + WINDOW_SPACING;
    /* Game window is square so we can use the width */
    messagehint.y=GAME_WIDTH + STAT_HEIGHT+WINDOW_SPACING*2;
    messagehint.width=GAME_WIDTH;
    messagehint.height=roothint.height - messagehint.y;
    messagehint.max_width=messagehint.min_width=messagehint.width;
    messagehint.max_height=STAT_HEIGHT;
    messagehint.min_height=messagehint.height;
    messagehint.flags=PPosition | PSize;
    win_message=XCreateSimpleWindow(display,win_root,
	messagehint.x,messagehint.y,messagehint.width,
	messagehint.height,2,foreground,background);
    XSetWindowColormap(display, win_message, colormap);
    icon=XCreateBitmapFromData(display,win_message,
	(_Xconst char *) crossfire_bits,
	(unsigned int) crossfire_width, (unsigned int)crossfire_height);
    XSetStandardProperties(display,win_message,"Crossfire - vitals",
	"crossvitals",icon, gargv,gargc,&(messagehint));
    gc_message=XCreateGC(display,win_message,0,0);
    XSetForeground(display,gc_message,foreground);
    XSetBackground(display,gc_message,background);
    XSetFont(display,gc_message,font->fid);
    XSelectInput(display,win_message,
	       ButtonPressMask|KeyPressMask|KeyReleaseMask|ExposureMask);
    XSetWMProtocols(display, win_message, &wm_delete_window, 1);
    XMapRaised(display,win_message);
   return 0;
}

static void xwritedown(const char *txt, int x) {
  int y=13;
  for(;*txt!='\0';txt++,y+=13)
    XDrawImageString(display,win_message,
		     look_list.gc_text,x,y,txt,1);
}

#define MAX_BARS_MESSAGE 80

static void draw_stat_bar(int bar_pos, int height, int is_alert)
{
  if(height!=MAX_BARS_MESSAGE)	/* clear the top of the bar */
    XClearArea(display,win_message, bar_pos, 4,
	10, MAX_BARS_MESSAGE-height, 0);

  if(height==0)			/* empty bar */
    return;

  if(is_alert) /* this should have its own gc */
	XSetForeground(display,look_list.gc_text,
			discolor[3].pixel);

  XFillRectangle(display,win_message,
	 look_list.gc_text, bar_pos, 4+MAX_BARS_MESSAGE-height, 10, height);

  if(is_alert)
	 XSetForeground(display,look_list.gc_text,
			foreground);
}


/* This updates the status bars.  If redraw, then redraw them
 * even if they have not changed
 */

void draw_message_window(int redraw) {
    int bar,is_alert,flags;
    static uint16 oldflags=0;
    static uint16 scrollsize_hp=0, scrollsize_sp=0, scrollsize_food=0,
	scrollsize_grace=0;
    static uint8 scrollhp_alert=FALSE, scrollsp_alert=FALSE,
	scrollfood_alert=FALSE, scrollgrace_alert=FALSE;

    /* draw hp bar */
    if(cpl.stats.maxhp>0)
    {
	bar=(cpl.stats.hp*MAX_BARS_MESSAGE)/cpl.stats.maxhp;
	if(bar<0)
	    bar=0;
	is_alert=(cpl.stats.hp <= cpl.stats.maxhp/4);
    }
    else
    {
	bar=0;
	is_alert=0;
    }
    if (redraw || scrollsize_hp!=bar || scrollhp_alert!=is_alert)
	draw_stat_bar(20, bar, is_alert);
    scrollsize_hp=bar;
    scrollhp_alert=is_alert;

    /* draw sp bar.  spellpoints can go above max
     * spellpoints via supercharging with the transferrance spell,
     * or taking off items that raise max spellpoints.
     */
    if (cpl.stats.sp>cpl.stats.maxsp)
	bar = MAX_BARS_MESSAGE;
    else
	bar=(cpl.stats.sp*MAX_BARS_MESSAGE)/cpl.stats.maxsp;
    if(bar<0) 
	bar=0;

    is_alert=(cpl.stats.sp <= cpl.stats.maxsp/4);

    if (redraw || scrollsize_sp!=bar || scrollsp_alert!=is_alert)
	draw_stat_bar(60, bar, is_alert);

    scrollsize_sp=bar;
    scrollsp_alert=is_alert;

    /* draw sp bar. grace can go above max or below min */
    if (cpl.stats.grace>cpl.stats.maxgrace)
	bar = MAX_BARS_MESSAGE;
    else
	bar=(cpl.stats.grace*MAX_BARS_MESSAGE)/cpl.stats.maxgrace;
    if(bar<0) 
	bar=0;

    is_alert=(cpl.stats.grace <= cpl.stats.maxgrace/4);

    if (redraw || scrollsize_grace!=bar || scrollgrace_alert!=is_alert)
	draw_stat_bar(100, bar, is_alert);

    scrollsize_grace=bar;
    scrollgrace_alert=is_alert;
  
    /* draw food bar */
    bar=(cpl.stats.food*MAX_BARS_MESSAGE)/999;
    if(bar<0) 
	bar=0;
    is_alert=(cpl.stats.food <= 999/4);

    if (redraw || scrollsize_food!=bar || scrollfood_alert!=is_alert)
	draw_stat_bar(140, bar, is_alert);

    scrollsize_food=bar;
    scrollfood_alert=is_alert;

    flags = cpl.stats.flags;

    if (cpl.fire_on) flags |= SF_FIREON;
    if (cpl.run_on) flags |= SF_RUNON;

    if ((flags & SF_FIREON ) != (oldflags & SF_FIREON)) {
	if (flags & SF_FIREON)
	    XDrawImageString(display, win_message,
			     look_list.gc_text, 180, 15, "Fire On", 7);
	else
	    XClearArea(display, win_message,
                   180, 0, 60, 15, False);
    }
    if ((flags & SF_RUNON ) != (oldflags & SF_RUNON)) {
	if (flags & SF_RUNON)
	    XDrawImageString(display, win_message,
			     look_list.gc_text, 180, 30, "Run On", 6);
	else
	    XClearArea(display, win_message,
                   180, 15, 60, 15, False);
    }
    oldflags = flags;
    if (redraw || cpl.stats.resist_change) {
	int x=180, y=45,i;
	char buf[40];

	cpl.stats.resist_change=0;
	XClearArea(display, win_message, 180, 30, messagehint.width-180, messagehint.height-30, False);
	for (i=0; i<NUM_RESISTS; i++) {
	    if (cpl.stats.resists[i]) {
		XDrawImageString(display, win_message,
			 look_list.gc_text, x, y, resists_name[i],
			 strlen(resists_name[i]));
		sprintf(buf,"%+4d", cpl.stats.resists[i]);
		XDrawImageString(display, win_message,
			 look_list.gc_text, x+40, y, buf, strlen(buf));
		y+=15;
		/* Move to the next draw position.  If we run
		 * out of space, just break out of the function.
		 */
		if (y>messagehint.height) break;
	    } /* If we have a resistance with value */
	} /* For loop of resistances */
    } /* If we need to draw the resistances */
}
    

static void draw_all_message(void) {

  XClearWindow(display,win_message);
  xwritedown("HP",06);
  XDrawRectangle(display,win_message,
		 look_list.gc_text,18,2,14,MAX_BARS_MESSAGE+4);
  xwritedown("Mana",46);
  XDrawRectangle(display,win_message,
		 look_list.gc_text,58,2,14,MAX_BARS_MESSAGE+4);
  xwritedown("Grace",86);
  XDrawRectangle(display,win_message,
		 look_list.gc_text,98,2,14,MAX_BARS_MESSAGE+4);
  xwritedown("Food",126);
  XDrawRectangle(display,win_message,
		 look_list.gc_text,138,2,14,MAX_BARS_MESSAGE+4);
  draw_message_window(1);
}

/****************************************************************************
 *
 * Inventory window functions follow
 *
 ****************************************************************************/


static void draw_status_icon(itemlist *l, int x, int y, int face)
{
    XCopyPlane(display, icons[face], l->win, l->gc_status, 0,0, 24,6, x,y, 1);
}


/* compares the 'flags' against the item.  return 1 if we should draw
 * that object, 0 if it should not be drawn.
 */

static int show_object(item *ip, inventory_show flags)
{
    if (flags==show_all) return 1;
    if ((flags & show_applied) && (ip->applied)) return 1;
    if ((flags & show_unapplied) && (!ip->applied)) return 1;
    if ((flags & show_unpaid) && (ip->unpaid)) return 1;
    if ((flags & show_cursed) && (ip->cursed || ip->damned)) return 1;
    if ((flags & show_magical) && (ip->magical)) return 1;
    if ((flags & show_nonmagical) && (!ip->magical)) return 1;
    if ((flags & show_locked) && (ip->locked)) return 1;
    if ((flags & show_unlocked) && (!ip->locked)) return 1;

    /* Doesn't match - probalby don't want it then */
    return 0;
}

/* This will need to be changed to do the 'right thing' for different display
 * modes (use bitmap data if we are using bitmaps for the game.
 */

static void create_status_icons(void)
{
#include "pixmaps/clear.xbm"
#include "pixmaps/locked.xbm"
#include "pixmaps/applied.xbm"
#include "pixmaps/unpaid.xbm"
#include "pixmaps/damned.xbm"
#include "pixmaps/cursed.xbm"
#include "pixmaps/magic.xbm"
#include "pixmaps/close.xbm"
#include "pixmaps/stipple.111"
#include "pixmaps/stipple.112"

    static int hasinit=0;
    int x, y;
    GC tmpgc;

    if (hasinit) return;
    hasinit=1;

#define CREATEPM(name,data) \
    (icons[name] = XCreateBitmapFromData(display, def_root,\
			    data##_bits, data##_width, data##_height))

    if (0
	|| CREATEPM(no_icon, clear) == None
	|| CREATEPM(locked_icon, locked) == None
	|| CREATEPM(applied_icon, applied) == None
	|| CREATEPM(unpaid_icon, unpaid) == None
	|| CREATEPM(damned_icon, damned) == None
	|| CREATEPM(cursed_icon, cursed) == None
	|| CREATEPM(magic_icon, magic) == None
	|| CREATEPM(close_icon, close) == None
	|| CREATEPM(stipple1_icon, stipple) == None
	|| CREATEPM(stipple2_icon, stipple1) == None
    )
    {
	fprintf(stderr, "Unable to create pixmaps.\n");
	exit (0);
    }
    dark1 = XCreatePixmap(display, def_root, image_size, image_size, 1);
    dark2 = XCreatePixmap(display, def_root, image_size, image_size, 1);
    dark3 = XCreatePixmap(display, def_root, image_size, image_size, 1);
    tmpgc = XCreateGC(display, dark1, 0, 0);
    XSetFillStyle(display, tmpgc, FillSolid);
    XSetForeground(display, tmpgc, 1);
    XFillRectangle(display, dark1, tmpgc, 0, 0, image_size, image_size);
    XFillRectangle(display, dark2, tmpgc, 0, 0, image_size, image_size);
    XFillRectangle(display, dark3, tmpgc, 0, 0, image_size, image_size);
    XSetForeground(display, tmpgc, 0);
    for (x=0; x<image_size; x++) {
	for (y=0; y<image_size; y++) {
	    /* we just fill in points every X pixels - dark1 is the darkest, dark3 is the li!
	     * dark1 has 50% of the pixels filled in, dark2 has 33%, dark3 has 25%
	     * The formula's here are not perfect - dark2 will not match perfectly with an
	     * adjacent dark2 image.  dark3 results in diagonal stripes.  OTOH, these will
	     * change depending on the image size.
	     */
	    if ((x+y) % 2) {
		XDrawPoint(display, dark1, tmpgc, x, y);
	    }
	    if ((x+y) %3) {
		XDrawPoint(display, dark2, tmpgc, x, y);
	    }
	    if ((x+y) % 4) {
		XDrawPoint(display, dark3, tmpgc, x, y);
	    }
	}
    }
    XFreeGC(display, tmpgc);
}

/*
 *  draw_list redraws changed item to the window and updates a scrollbar
 */
static void draw_list (itemlist *l)
{
    int i, items, size, pos;
    item *tmp;
    char buf[MAX_BUF], buf2[MAX_BUF];

    /* draw title */
    strcpy(buf2, l->title);
    if (l->show_what & show_mask) {
	strcat(buf2," (");
	if (l->show_what & show_applied) strcat(buf2,"applied, ");
	if (l->show_what & show_unapplied) strcat(buf2,"unapplied, ");
	if (l->show_what & show_unpaid) strcat(buf2,"unpaid, ");
	if (l->show_what & show_cursed) strcat(buf2,"cursed, ");
	if (l->show_what & show_magical) strcat(buf2,"magical, ");
	if (l->show_what & show_nonmagical) strcat(buf2,"nonmagical, ");
	if (l->show_what & show_locked) strcat(buf2,"locked, ");
	if (l->show_what & show_unlocked) strcat(buf2,"unlocked, ");
	/* want to kill the comma we put in above.  Replace it with the paren */
	buf2[strlen(buf2)-2]=')';
	buf2[strlen(buf2)-1]='\0';
    }
    if(l->env->weight < 0 || l->show_weight == 0)
	sprintf (buf, l->format_n, buf2);
    else if (!l->weight_limit)
	sprintf (buf, l->format_nw, buf2, l->env->weight);
    else
	sprintf(buf, l->format_nwl, buf2, l->env->weight,
		l->weight_limit/1000);

    if (strcmp (buf, l->old_title)) {
	XCopyPlane(display, icons[l->env->open ? close_icon : no_icon], 
			    l->win, l->gc_status, 0,0, image_size,13, 2,2, 1);
	strcpy (l->old_title, buf);
	XDrawImageString(display, l->win, l->gc_text, 
			 (l->show_icon ? image_size+24+4 : image_size+4),
			 13, buf, strlen(buf));
    }

    /* Find how many objects we should draw. */

    for(tmp = l->env->inv, items=0; tmp ;tmp=tmp->next)
	if (show_object(tmp, l->show_what)) items++;

    if(l->item_pos > items - l->size)
	l->item_pos = items - l->size;
    if(l->item_pos < 0)
	l->item_pos = 0;

    /* Fast forward to the appropriate item location */

    for(tmp = l->env->inv, i=l->item_pos; tmp && i; tmp=tmp->next)
	if (show_object(tmp, l->show_what)) i--;

    for(i=0; tmp && i < l->size; tmp=tmp->next) {
	if (!show_object(tmp, l->show_what)) continue;
	/* draw face */
	if(l->faces[i] != tmp->face) {
	    l->faces[i] = tmp->face;
	    XClearArea(display, l->win, 4, 16 + image_size * i, 
		       image_size, image_size, False);
	    gen_draw_face (l->win, tmp->face,4, 16 + image_size * i, 0, 0);
	}
	/* draw status icon */
	if (l->show_icon) {
	    sint8 tmp_icon;
	    tmp_icon = tmp->locked ? locked_icon : no_icon;
	    if (l->icon1[i] != tmp_icon) {
		l->icon1[i] = tmp_icon;
		draw_status_icon (l, image_size+4, 16 + image_size * i, tmp_icon);
	    }
	    tmp_icon = tmp->applied ? applied_icon :
		       tmp->unpaid  ? unpaid_icon : no_icon;
	    if (l->icon2[i] != tmp_icon) {
		l->icon2[i] = tmp_icon;
		draw_status_icon (l, image_size+4, 22 + image_size * i, tmp_icon);
	    }
	    tmp_icon = tmp->magical ? magic_icon : no_icon;
	    if (l->icon3[i] != tmp_icon) {
		l->icon3[i] = tmp_icon;
		draw_status_icon (l, image_size+4, 28 + image_size * i, tmp_icon);
	    }
	    tmp_icon = tmp->damned ? damned_icon : 
		       tmp->cursed ? cursed_icon : no_icon;
	    if (l->icon4[i] != tmp_icon) {
		l->icon4[i] = tmp_icon;
		draw_status_icon (l, image_size+4, 34 + image_size * i, tmp_icon);
	    }
	}
	/* draw name */
	strcpy (buf2, tmp->d_name);
	if (l->show_icon == 0)
	    strcat (buf2, tmp->flags);

	if(tmp->weight < 0 || l->show_weight == 0)
	    sprintf(buf, l->format_n, buf2);
	else 
	    sprintf(buf, l->format_nw, buf2, tmp->nrof * tmp->weight);

	if(!l->names[i] || strcmp(buf, l->names[i])) {
	    copy_name (l->names[i], buf);
	    XDrawImageString(display, l->win, l->gc_text, 
			     (l->show_icon?image_size+24+4:image_size+4),
			     34 + image_size * i, buf, strlen(buf));
	}
	i++;
    }

    /* If there are not enough items to fill in the display area,
     * then set the unused ares to nothing.
     */
    if(items < l->item_used) {
	XClearArea(display, l->win, 0, 16 + image_size * i, l->width - 23,
		   image_size * (l->size - i) , False);
	while (i < l->item_used) {
	    copy_name (l->names[i], "");
	    l->faces[i] = 0;
	    l->icon1[i] = l->icon2[i] = l->icon3[i] = l->icon4[i] = 0;
	    i++;
	}
    }
    l->item_used = items > l->size ? l->size : items;

    /* draw the scrollbar now */
    if(items < l->size)
	items = l->size;

    size = (long) l->bar_length * l->size / items;
    pos = (long) l->bar_length * l->item_pos / items;

    if(l->bar_size != size || pos != l->bar_pos) {

	XClearArea(display, l->win, l->width-20, 17 + l->bar_pos, 16, 
		   l->bar_size, False);
	l->bar_size = size;
	l->bar_pos  = pos;

	XFillRectangle(display, l->win, l->gc_text, l->width - 20,
		       17 + l->bar_pos, 16, l->bar_size);
    }
}

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

    XClearWindow(display, l->win);
    XDrawRectangle(display, l->win, l->gc_text, l->width - 22, 15, 20,
		   l->bar_length + 4);

#if 0
    /* Don't reset these - causes window position to reset too often */
    l->item_pos = 0;
    l->item_used = 0;
#endif
    l->bar_size = 1;    /* so scroll bar is drawn */
    draw_list (l);
}


/* we have received new images.  update these only */
static void update_icons_list(itemlist *l)
{
    int i;
    for (i = 0; i < l->size; ++i)
	l->faces[i] = 0;
    draw_list(l);
}


void open_container (item *op) 
{
    look_list.env = op;
    sprintf (look_list.title, "%s:", op->d_name);
    draw_list (&look_list);
}

void close_container (item *op) 
{
    look_list.env = cpl.below;
    strcpy (look_list.title, "You see:");
    draw_list (&look_list);
}

static void resize_list_info(itemlist *l, int w, int h)
{
    int i;

    free(l->faces);
    free(l->icon1);
    free(l->icon2);
    free(l->icon3);
    free(l->icon4);
    if (l->names) {
	for (i=0; i < l->size; i++)
	    if (l->names[i])
		free(l->names[i]);
	free(l->names);
    }
    l->width  = w;
    l->height = h;
    l->size = (l->height - FONTHEIGHT - 8) / image_size;
    l->text_len = (l->width - (l->show_icon ? 84 : 60)) / FONTWIDTH;
    l->bar_length = l->size * image_size;
    sprintf (l->format_nw, "%%-%d.%ds%%6.1f", l->text_len-6, l->text_len-6);
    sprintf (l->format_nwl, "%%-%d.%ds%%6.1f/%%4d", l->text_len-11, l->text_len-11);
    sprintf (l->format_n, "%%-%d.%ds", l->text_len, l->text_len);

    if ((l->faces = malloc (sizeof (*(l->faces)) * l->size )) == NULL) {
	printf ("Can't allocate memory.\n");
	exit (0);
    }
    if ((l->icon1 = malloc (sizeof (*(l->icon1)) * l->size )) == NULL) {
	printf ("Can't allocate memory.\n");
	exit (0);
    }
    if ((l->icon2 = malloc (sizeof (*(l->icon2)) * l->size )) == NULL) {
	printf ("Can't allocate memory.\n");
	exit (0);
    }
    if ((l->icon3 = malloc (sizeof (*(l->icon3)) * l->size )) == NULL) {
	printf ("Can't allocate memory.\n");
	exit (0);
    }
    if ((l->icon4 = malloc (sizeof (*(l->icon4)) * l->size )) == NULL) {
	printf ("Can't allocate memory.\n");
	exit (0);
    }
    if ((l->names = malloc (sizeof (char *) * l->size )) == NULL) {
	printf ("Can't allocate memory.\n");
	exit (0);
    }
    for (i=0; i < l->size; i++) {
	if ((l->names[i] = malloc (NAME_LEN)) == NULL) {
	    printf ("Can't allocate memory.\n");
	    exit (0);
	}
    }
    draw_all_list(l);	/* this also initializes above allocated tables */
}

static void get_list_display(itemlist *l, int x, int y, int w, int h,
		   const char *t, const char *s)
{
    XSizeHints hint;

    l->faces=NULL;
    l->names=NULL;
    hint.x = x;
    hint.y = y;
    hint.width  = w;
    hint.height = h;
    hint.min_width  = 60 + 10 * FONTWIDTH;
    hint.min_height = FONTHEIGHT + 8 + image_size * 2;
    hint.flags = PPosition | PSize;
    l->win = XCreateSimpleWindow(display, win_root, hint.x, hint.y, hint.width,
			       hint.height, 2, foreground, background);
    XSetWindowColormap(display, l->win, colormap);
    icon = XCreateBitmapFromData(display, l->win, 
			       (_Xconst char *) crossfire_bits,
			       (unsigned int) crossfire_width, 
			       (unsigned int)crossfire_height);
    XSetStandardProperties(display, l->win, t, s, icon, gargv, gargc, &(hint));
    l->gc_text = XCreateGC (display, l->win, 0, 0);
    l->gc_icon = XCreateGC (display, l->win, 0, 0);
    l->gc_status = XCreateGC (display, l->win, 0, 0);
    XSetForeground (display, l->gc_text, foreground);
    XSetBackground (display, l->gc_text, background);
    XSetForeground (display, l->gc_icon, foreground);
    XSetBackground (display, l->gc_icon, background);
    XSetForeground (display, l->gc_status, foreground);
    XSetBackground (display, l->gc_status, background);
    XSetGraphicsExposures (display, l->gc_icon, False);
    XSetGraphicsExposures (display, l->gc_status, False);
    XSetFont (display, l->gc_text, font->fid);
    XSelectInput (display, l->win, ButtonPressMask|KeyPressMask|KeyReleaseMask|
		ExposureMask|StructureNotifyMask);
    XSetWMProtocols(display, l->win, &wm_delete_window, 1);
    XMapRaised(display,l->win);
    create_status_icons();
    resize_list_info(l, w, h);
}

static int get_inv_display(void)
{
    inv_list.env = cpl.ob;
    strcpy (inv_list.title, ""/*ET: too long: "Inventory:"*/);
    inv_list.show_weight = 1;
    inv_list.show_what = show_all;
    inv_list.weight_limit=0;
    get_list_display ( &inv_list, 0, 0, INV_WIDTH, 
		      2*(roothint.height - WINDOW_SPACING) / 3,
		      "Crossfire - inventory",
		      "crossinventory");
    return 0;
}

static int get_look_display(void)
{
    look_list.env = cpl.below;
    strcpy (look_list.title, "You see:");
    look_list.show_weight = 1;
    look_list.show_what = show_all;
    inv_list.weight_limit = 0;
    get_list_display ( &look_list, 0, 
	      (2*(roothint.height - WINDOW_SPACING) / 3) + WINDOW_SPACING,
		      INV_WIDTH, 
		      (roothint.height - WINDOW_SPACING) / 3,
		      "Crossfire - look",
		    "crosslook");
    return 0;
}

/*
 *  draw_lists() redraws inventory and look windows when necessary
 */
void draw_lists(void)
{
    if (inv_list.env->inv_updated) {
	draw_list (&inv_list);
	inv_list.env->inv_updated = 0;
    }
    if (look_list.env->inv_updated) {
	draw_list (&look_list);
	look_list.env->inv_updated = 0;
    }
}


void set_show_icon (const char *s)
{
    if (s == NULL || *s == 0 || strncmp ("inventory", s, strlen(s)) == 0) {
	inv_list.show_icon = ! inv_list.show_icon; /* toggle */
	resize_list_info(&inv_list, inv_list.width, inv_list.height);
    } else if (strncmp ("look", s, strlen(s)) == 0) {
	look_list.show_icon = ! look_list.show_icon; /* toggle */
	resize_list_info(&look_list, look_list.width, look_list.height);
    }
}

void set_show_weight (const char *s)
{
    if (s == NULL || *s == 0 || strncmp ("inventory", s, strlen(s)) == 0) {
	inv_list.show_weight = ! inv_list.show_weight; /* toggle */
	draw_list (&inv_list);
    } else if (strncmp ("look", s, strlen(s)) == 0) {
	look_list.show_weight = ! look_list.show_weight; /* toggle */
	draw_list (&look_list);
    }
}

void set_weight_limit (uint32 wlim)
{
    inv_list.weight_limit = wlim;
}

void set_scroll(const char *s)
{
    if (!infodata.scroll_info_window) {
	infodata.scroll_info_window=1;
	if (infodata.numlines>=infodata.maxdisp) {
	    infodata.infoline=infodata.maxdisp-1;
	}
	draw_all_info();
	draw_info("Scroll is enabled", NDI_BLACK);
    }
    else {
	draw_info("Scroll is disabled", NDI_BLACK);
	infodata.scroll_info_window=0;
    }
}

void set_autorepeat(const char *s)
{
    noautorepeat = noautorepeat ? FALSE : TRUE;
    draw_info(noautorepeat ? "Autorepeat is disabled":"Autorepeat is enabled",
	      NDI_BLACK);
}

int get_info_width()
{
    return infodata.info_chars;
}

void menu_clear(void)
{
    draw_info("clearinfo command not implemented for this client", NDI_BLACK);
    return;
}

/* TODO Figure out if/how to *use* these events here. */
void item_event_item_deleting(item * it) {}
void item_event_container_clearing(item * container) {}
void item_event_item_changed(item * it) {}

/******************************************************************************
 * Root Window code
 ****************************************************************************/

/* get_root_display:
 * this sets up the root window (or none, if in split
 * windows mode, and also scans for any Xdefaults.  Right now, only
 * splitwindow and image are used.  image is the display
 * mechanism to use.  I thought having one type that is set
 * to font, xpm, or pixmap was better than using xpm and pixmap
 * resources with on/off values (which gets pretty weird
 * if one of this is set to off.
 */

/* Error handlers removed.  Right now, there is nothing for
 * the client to do if it gets a fatal error - it doesn't have
 * any information to save.  And we might as well let the standard
 * X11 error handler handle non fatal errors.
 */
int sync_display = 0;

static int get_root_display(char *display_name) {
    char *cp;

    display=XOpenDisplay(display_name);
    if (!display) {
	fprintf(stderr, "Can't open display %s.\n", display_name);
	return 1;
    }

    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", 0);
    /* This generates warnings, but looking at the documenation,
     * it seems like it _should_ be ok.
     */
    XSetErrorHandler(error_handler);

    if ((getenv("ERIC_SYNC")!= NULL) || sync_display)
	XSynchronize(display,True);

    def_root = DefaultRootWindow(display);
    def_screen = DefaultScreen(display);

    /* For both split_windows and display mode, check to make sure that
     * the command line has not set the value.  Command line settings
     * should always have precedence over settings in the Xdefaults file.
     *
     * Also, if you add new defaults, make sure that the same rules for
     * the command line defaults are used.  The client assumes that certain
     * values can only be set if they have meaning (save_data will not
     * be set unless the data can actually be saved, for example.)  If
     * this is not followed for then the client might very well crash.
     */
    if (!want_config[CONFIG_SPLITWIN] && 
      (cp=XGetDefault(display,X_PROG_NAME,"splitwindow")) != NULL) {
	if (!strcmp("on",cp) || !strcmp("yes",cp))
	    want_config[CONFIG_SPLITWIN] = TRUE;
	else if (!strcmp("off",cp) || !strcmp("no",cp))
	    want_config[CONFIG_SPLITWIN] = FALSE;
    }
    if (!use_config[CONFIG_ECHO] &&
      (cp=XGetDefault(display,X_PROG_NAME,"echo")) != NULL) {
	if (!strcmp("on",cp) || !strcmp("yes",cp))
	    use_config[CONFIG_ECHO] = TRUE;
	else if (!strcmp("off",cp) || !strcmp("no",cp))
	    use_config[CONFIG_ECHO] = FALSE;
    }
    if ((cp=XGetDefault(display,X_PROG_NAME, "scrollLines"))!=NULL) {
	infodata.maxlines=atoi(cp);
    }
    if ((cp=XGetDefault(display,X_PROG_NAME,"font")) != NULL) {
	font_name = strdup_local(cp);
    }
    /* Failure will result in an uncaught X11 error */
    font=XLoadQueryFont(display,font_name);
    if (!font) {
	fprintf(stderr,"Could not load font %s\n", font_name);
	exit(1);
    }
    FONTWIDTH=font->max_bounds.width;
    FONTHEIGHT=font->max_bounds.ascent + font->max_bounds.descent;

    background=WhitePixel(display,def_screen);
    foreground=BlackPixel(display,def_screen);
    roothint.x=0;
    roothint.y=0;
    roothint.width=582+6+INFOCHARS*FONTWIDTH;
    roothint.height=ROOT_HEIGHT;
    /* Make up for the extra size of the game window.  88 is
     * 11 tiles * 8 pixels/tile bigger size.
     */
    roothint.width += 88;
    roothint.height+= 88;
    init_pngx_loader(display);

    roothint.max_width=roothint.min_width=roothint.width;
    roothint.max_height=roothint.min_height=roothint.height;
    roothint.flags=PSize; /*ET: no PPosition. let window manager handle that. */

    if(!want_config[CONFIG_SPLITWIN]) {
	win_root=XCreateSimpleWindow(display,def_root,
	    roothint.x,roothint.y,roothint.width,roothint.height,2,
	    background,foreground);

	allocate_colors(display, win_root, def_screen, &colormap, discolor);
	foreground=discolor[0].pixel;
	background=discolor[9].pixel;
	icon=XCreateBitmapFromData(display,win_root,
	    (_Xconst char *) crossfire_bits,
	    (unsigned int) crossfire_width, (unsigned int)crossfire_height);
	XSetStandardProperties(display,win_root,X_PROG_NAME,X_PROG_NAME,
	    icon,gargv,gargc,&(roothint));
	gc_root=XCreateGC(display,win_root,0,0);
	XSetForeground(display,gc_root,foreground);
	XSetBackground(display,gc_root,background);

	XSelectInput(display,win_root,KeyPressMask|
	     KeyReleaseMask|ExposureMask|StructureNotifyMask);
	XMapRaised(display,win_root);
	XNextEvent(display,&event);	/*ET: this is bogus */
    XSetWMProtocols(display, win_root, &wm_delete_window, 1);
    }
    else
	win_root = def_root;
    return 0;
}

static void resize_win_root(XEvent *event) {
    int width, inv_width, info_width;

    if (want_config[CONFIG_SPLITWIN]) {
	fprintf(stderr,"Got a resize root window in split windows mode\n");
	return;
    }

    /* The middle 3 windows don't really benefit from the resize, so keep
     * them the same size, and use the leftover equally between the left
     * and right windows.
     */
    width = (event->xconfigure.width - GAME_WIDTH -WINDOW_SPACING*2);
    info_width = width * info_ratio;
    inv_width = width - info_width;

    /* With png (and 32x32 images), the message window can get scrunched,
     * so lets make it taller if we can - there is no reason not to, as otherwise
     * that space is lost anyways.
     */
    XMoveResizeWindow(display, win_message, inv_width + WINDOW_SPACING, 
		GAME_WIDTH + STAT_HEIGHT + WINDOW_SPACING*2, GAME_WIDTH,
		event->xconfigure.height - GAME_WIDTH + STAT_HEIGHT + WINDOW_SPACING*2);
    messagehint.width=GAME_WIDTH;
    messagehint.height=event->xconfigure.height - GAME_WIDTH + STAT_HEIGHT + WINDOW_SPACING*2;

    /* These windows just need to be relocated.  The y constants are
     * hardcoded - those windows don't really benefit from being resized
     * (actually, no code in place to currently do it), so no reason
     * to get trick with those just now.
     */

    XMoveWindow(display, win_game, inv_width + WINDOW_SPACING, STAT_HEIGHT + WINDOW_SPACING);
    XMoveWindow(display, win_stats, inv_width + WINDOW_SPACING, 0);

    /* Resize the info window */
    XMoveResizeWindow(display, infodata.win_info,  
	inv_width + GAME_WIDTH + WINDOW_SPACING * 2, 0,
	info_width, event->xconfigure.height);


    /* Resize the inventory, message window */
    XResizeWindow(display, inv_list.win, inv_width, 
		  2 * (event->xconfigure.height - WINDOW_SPACING) / 3);


    XMoveResizeWindow(display, look_list.win, 0, 
	      (2*(event->xconfigure.height - WINDOW_SPACING) / 3) + WINDOW_SPACING,
		      inv_width, 
		      (event->xconfigure.height - WINDOW_SPACING) / 3);

    /* The Resize requests will generate events.  As such, we don't call
     * the resize functions here - the event handler would get those anyways,
     * so there isn't a big gain in doing it here.
     */
}


/***********************************************************************
 *
 * Here is the start of event handling functions
 *
 ***********************************************************************/

static void parse_game_button_press(int button, int x, int y)
{
    int dx, dy, i, xmidl, xmidh, ymidl, ymidh;

    dx = (x-2)/image_size-use_config[CONFIG_MAPWIDTH]/2;
    dy= (y-2)/image_size-use_config[CONFIG_MAPHEIGHT]/2;
    xmidl=(use_config[CONFIG_MAPWIDTH]/2) * image_size;
    xmidh=(use_config[CONFIG_MAPWIDTH]/2 + 1) * image_size;
    ymidl=(use_config[CONFIG_MAPHEIGHT]/2) * image_size;
    ymidh=(use_config[CONFIG_MAPHEIGHT]/2 + 1) * image_size;

    switch (button) {
	case 1:
	{
	    /* Its unlikely this will happen, but if the window is
	     * resized, its possible to be out of bounds.
	     */
	    if(dx<(-use_config[CONFIG_MAPWIDTH]/2)||dx>(use_config[CONFIG_MAPWIDTH]/2)||dy<(-use_config[CONFIG_MAPHEIGHT]/2)||dy>(use_config[CONFIG_MAPHEIGHT]/2)) return;
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

	    if (button==2) {
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


/* Handles key presses.  Note that the client really doesn't know
 * much about states.  But there are at least a couple that the
 * client needs to know about - game play, where it parses keystrokes,
 * applies any bindings, and sends the command to the server,
 * and reply states.  The only time reply's are used right now is for rolling
 * up a character - sending 'SAY' commands is not appropriate.
 *
 * After input is completed in either of the reply states, the program
 * sets the game back to Play state.  May not be appropriate, but let
 * the server ignore the commands if they are not proper - a hack
 * client could always send such commands anyways.
 * If the server expect additional reply's, it should send an query.
 * Note that these can not be stacked up, (ie, server can not send
 * 5 queries and expect 5 replies).   But in no place does the server
 * do this anyways, so it is not a problem.
 */

static void do_key_press(int repeated)
{
    KeySym gkey;
    char text[10];

    /* Turn off the magic map.  Perhaps this should be more selective? */
    if (cpl.showmagic) {
	cpl.showmagic=0;
	display_map_doneupdate(TRUE, FALSE);
    }
    if(!XLookupString(&event.xkey,text,10, &gkey,NULL)) {
/*
 *	This happens quite a bit - most modifier keys (shift, control, etc)
 *	can not be mapped to a value.
 */
 /*
	fprintf(stderr,"XLookupString failed, xkey.code=%d, gkey=%ld\n",
		event.xkey.keycode, gkey);
*/
	text[0]='\0';
    }
    switch(cpl.input_state) {
	case Playing:
	    parse_key(text[0],event.xkey.keycode,gkey,repeated);
	    break;

	case Reply_One:
	/* Don't send modifier keys as reply to query. Tries to prevent
	 * from getting into the "...state is not ST_PLAYING" state. */
	    if (text[0]) {
		text[1]='\0';
		send_reply(text);
		cpl.input_state = Playing;
	    }
	    break;

	case Configure_Keys:
	    configure_keys(event.xkey.keycode, gkey);
	    break;

	case Reply_Many:
	case Command_Mode:
	case Metaserver_Select:
	    if (text[0]==13) {
		enum Input_State old_state=cpl.input_state;
		if (cpl.input_state==Metaserver_Select) {
		    cpl.input_state=Playing;
		    return;
		}
		if (cpl.input_state==Reply_Many)
		    send_reply(cpl.input_text);
		else {
		    write_ch(13);
		    extended_command(cpl.input_text);
		}
		/* Only set state to playing if the state has otherwise
		 * not changed - this check is needed because 'bind
		 * changes the state, and we don't want to change to playing
		 * again.
		 */
		if (old_state==cpl.input_state)
		    cpl.input_state = Playing;
		cpl.input_text[0]='\0';
		cpl.no_echo=0;	/* By default, start echoing thing again */
	    }
	    else {
		write_ch(text[0]);
	    }
	    break;

	default:
	    fprintf(stderr,"Unknown input state: %d\n", cpl.input_state);
    }
}


static void buttonpress_in_info(XButtonEvent *xbutton)
{
    int y = xbutton->y-16, x=xbutton->x, button = xbutton->button,dy,pos=0;

    if (!infodata.has_scrollbar)
	return;

    if (button < 4 && x <= infodata.width-SCROLLBAR_WIDTH-4)
	return;

    dy = y / FONTHEIGHT > 0 ? y / FONTHEIGHT : 1;
      
    switch(button) {
	  case 1:
	    pos = infodata.bar_pos - dy;
	    break;

	  case 2:
	    pos = y * infodata.numlines / infodata.bar_length;
	    break;

	  case 3:
	    pos = infodata.bar_pos + dy;
	    break;

	  case 4:
	    pos = infodata.bar_pos - 1;
	    break;

	  case 5:
	    pos = infodata.bar_pos + 1;
	    break;

    }
    if (pos<infodata.maxdisp) {
	if (infodata.numlines<infodata.maxdisp)
	    pos=infodata.numlines;
	else
	    pos=infodata.maxdisp;
    }
    if (pos>infodata.numlines) pos=infodata.numlines;
    if (pos != infodata.bar_pos) {
	infodata.bar_pos = pos;
	draw_all_info();
    }
}


/*
 *  buttonpress_in_list handles mouse button event in list window.
 *  It updates scrollbar or calls right function for item
 *  (apply/examine/move). It's probably better move calling the 
 *  functions to the upper level.
 */

static int buttonpress_in_list (itemlist *l, XButtonEvent *xbutton)
{
    item *tmp;
    int y = xbutton->y - 16, x=xbutton->x, button = xbutton->button;
    int items, pos=0, dy;

    if (y < 0 && l->env->open) /* close the sack */
	client_send_apply (l->env->tag);
 
    if (y < 0 || y > image_size * l->size)
	return 1;

    if (button == 4 || button == 5)
    {
	if (button == 4)
	    l->item_pos--;
	else
	    l->item_pos++;
	draw_list(l);
	return 1;
    }

    if (x > l->width-23) {    /* scrollbar */

	dy = y / image_size > 0 ? y / image_size : 1;
      
	switch(button) {
	  case 1:
	    pos = l->item_pos - dy;
	    break;

	  case 2:
	    for(tmp=l->env->inv, items=0; tmp; tmp=tmp->next)
		items++;
	    pos = y * items / l->bar_length;
	    break;

	  case 3:
	    pos = l->item_pos + dy;
	    break;

	}
	if (pos != l->item_pos) {
	    l->item_pos = pos;
	    draw_list (l);
	}
	return 1;
    }

    pos = l->item_pos + y / image_size;
    for(tmp=l->env->inv, items=0; tmp; tmp=tmp->next) {
	if (show_object(tmp, l->show_what)) items++;
	if (items>pos) break;
    }
    if (tmp) {
	switch(button) {
	  case 1:
	    if (xbutton->state & ShiftMask)
		toggle_locked(tmp);
	    else
		client_send_examine (tmp->tag);
	    break;
	  case 2:
	    if (xbutton->state & ShiftMask)
		send_mark_obj(tmp);
	    else
		client_send_apply (tmp->tag);
	    break;
	  case 3:
	    if (tmp->locked) {
		draw_info ("This item is locked. To drop it, first unlock by shift+leftclicking on it.",
		    NDI_BLACK);
	    } else if (l == &inv_list)
		client_send_move (look_list.env->tag, tmp->tag, cpl.count);
	    else
		client_send_move (inv_list.env->tag, tmp->tag, cpl.count);
	    cpl.count=0;
	    break;
	}
    }
    return 1;
}


/* get_metaserver returns a string for what the user has selected as
 * their metaserver.  It basically does a subset of check_x_events.
 * and keeps looping until the user finishes selecting the metaserver
 * (detected by change of state.
 */
char *get_metaserver()
{
    static char ret_buf[MAX_BUF];

    cpl.input_state = Metaserver_Select;
    draw_prompt(":");
    while (cpl.input_state == Metaserver_Select) {
	check_x_events();
	usleep(50000);	/* 1/20 sec */
    } /* while input state is metaserver select. */

    /* We need to clear out cpl.input_text - otherwise the next
     * long input (like player name) won't work right.
     * so copy it to a private buffer and return.
     */
    strncpy(ret_buf, cpl.input_text, MAX_BUF-1);
    ret_buf[MAX_BUF-1]=0;
    cpl.input_text[0]=0;
    return ret_buf;
}


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

void check_x_events() {
    KeySym gkey=0;
    static int lastupdate=0;
    static XEvent prev_event;	/* to detect autorepeated keys */

    /* If not connected, the below area does not apply, so don't deal with it */
    if (cpl.input_state != Metaserver_Select) {

	/* We need to periodically redraw stuff if we are caching images - otherwise
	 * the default images might be display indefinately.  This is tuned so
	 * that we should only be redrawing stuff if we got new images from
	 * the last draw and enough time has passed.  This should be a bit
	 * more efficient than redrawing everything anytime we get a new image -
	 * especially since we might get a bunch of images at the same time.
	 */
	if (want_config[CONFIG_CACHE] && lastupdate>5 && newimages) {
	    update_icons_list(&inv_list);
	    update_icons_list(&look_list);
	    if (!cpl.showmagic) display_map_doneupdate(TRUE, FALSE);
	    newimages=0;
	    lastupdate=0;
	}
	else {
	    if (newimages)
		lastupdate++;
	    draw_lists();		/* testing if this can work this way */
	}
    }
  

    while (XPending(display)!=0) {
	prev_event = event;
	XNextEvent(display,&event);
	switch(event.type) {

	    case ConfigureNotify:
		if(event.xconfigure.window==infodata.win_info)
		    resize_win_info(event.xconfigure.width, event.xconfigure.height);
		else if(event.xconfigure.window==inv_list.win)
		    resize_list_info(&inv_list, event.xconfigure.width,
			 event.xconfigure.height);
		else if(event.xconfigure.window==look_list.win)
		    resize_list_info(&look_list, event.xconfigure.width,
			 event.xconfigure.height);
		else if(event.xconfigure.window==win_root)
		    resize_win_root(&event);
		else if(event.xconfigure.window==win_message)
		    resize_win_message(event.xconfigure.width, event.xconfigure.height);
		break;

	    case Expose:
	    /* No point redrawing windows if there are more Expose's to
	     * come.
	     */
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
		else display_map_doneupdate(TRUE, FALSE);
	    } else if(want_config[CONFIG_SPLITWIN]==FALSE && event.xexpose.window==win_root) {
		XClearWindow(display,win_root);
	    }
	    break;

	    case GraphicsExpose:
		/* No point redrawing windows if there are more GraphicExpose's
		 * to come.
		 */
		if (event.xgraphicsexpose.count!=0) continue;
		if(event.xgraphicsexpose.drawable==win_game) {
		    if (cpl.showmagic) draw_magic_map();
		    else display_map_doneupdate(TRUE, FALSE);
		}
		break;

	    case MappingNotify:
		XRefreshKeyboardMapping(&event.xmapping);
		break;


	    case ButtonPress:
		/* Most of these will try to send requests to the server - since we
		 * are not connected, this will probably fail in some bad way.
		 */
		if (cpl.input_state != Metaserver_Select) {

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
		}

	    case KeyRelease:
		parse_key_release(event.xkey.keycode, gkey);
		break;

	    case KeyPress:
		if (noautorepeat
		    && prev_event.type == KeyRelease
		    && prev_event.xkey.keycode == event.xkey.keycode
		    && prev_event.xkey.state == event.xkey.state
		    && prev_event.xkey.time == event.xkey.time)
		    do_key_press(1);	/* auto-repeated key */
		else
		    do_key_press(0);	/* regular key */
		break;
        case ClientMessage:
            if(event.xclient.data.l[0] == wm_delete_window){
                LOG(LOG_INFO,"x11::check_x_events","Window closed. Exiting.");
                exit(0);
            }
        break;
	}
    }
    /* Below does not apply if we're not connected */
    if (cpl.input_state != Metaserver_Select) {
	/* Since we cycle through all the Xevents, there is no need to do
	 * keyboard flushing.  All commands get sent to the server.  There really
	 * does need to be some decent way to flush the commands we have sent,
	 * but there is no real way to do that.  This is at least somewhat
	 * true because the client and server will never be completely 
	 * synchronized.
	 */

	/* Need to do this to show the players position */
	if (cpl.showmagic) magic_map_flash_pos();
	clear_fire_run();
    }
    /* a good place to check for childs */
    monitorChilds();
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
    puts("Usage of cfclient:\n\n");
    puts("-server <name>   - Connect to <name> instead of localhost.");
    puts("-port <number>   - Use port <number> instead of the standard port number");
    puts("-display <name>  - Use <name> instead if DISPLAY environment variable.\n");
    puts("-split           - Use split windows.");
    puts("-nosplit         - Disable split windows (default action).");
    puts("-echo            - Echo the bound commands");
    puts("-pix             - Use bitmaps for display.");
#ifdef HAVE_LIBXPM
    puts("-xpm             - Use color pixmaps (XPM) for display.");
#endif
#ifdef HAVE_LIBPNG
    puts("-png             - Use png images for display.");
#endif
    puts("-showicon        - Print status icons in inventory window");
    puts("-scrolllines <number>    - number of lines for scrollback");
    puts("-sync            - Synchronize on display");
    puts("-help            - Display this message.");
    puts("-cache           - Cache images for future use.");
    puts("-nocache         - Do not cache images (default action).");
    puts("-mapscroll       - Enable mapscrolling by bitmap operations");
    puts("-nomapscroll     - Disable mapscrolling by bitmap operations");
    puts("-darkness        - Enables darkness code (default)");
    puts("-nodarkness      - Disables darkness code");
    puts("-nosound         - Disable sound output.");
    puts("-updatekeycodes  - Update the saved bindings for this keyboard.");
    puts("-keepcache       - Keep already cached images even if server has different ones.");
    puts("-font <name>     - Use <name> as font to display data.");
    puts("-pngfile <name>  - Use <name> for source of images");
    puts("-mapsize xXy     - Set the mapsize to be X by Y spaces.");
    puts("-noautorepeat    - Auto repeat on directional keys is ignored.");
    puts("-faceset <num>   - Select a specific facset to use.");
    exit(0);
}

/* init_windows:  This initiliazes all the windows - it is an
 * interface routine.  The command line arguments are passed to
 * this function to interpert.  Note that it is not in fact
 * required to parse anything, but doing at least -server and
 * -port would be a good idea.  This is a little messy, as non window
 * related options as well as those relating to windowing are parsed here,
 * but this file is meant for only the X11 stuff.
 *
 * This function returns 0 on success, nonzero on failure.
 */

int init_windows(int argc, char **argv)
{
    int on_arg=1;
    char *display_name="";

    strcpy(VERSION_INFO,"X11 Unix Client " FULL_VERSION);

    load_defaults();	/* Load these first, so they can get overwritten by
			 * command line options.
			 */
    want_skill_exp=1;
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
	    use_config[CONFIG_PORT] = atoi(argv[on_arg]);
	    continue;
	}
	if (!strcmp(argv[on_arg],"-mapsize")) {
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
	    if (x<=9 || y<=9) {
		fprintf(stderr,"map size must be positive values of at least 9\n");
	    } if (x>MAP_MAX_SIZE || y>MAP_MAX_SIZE) {
		fprintf(stderr,"Map size can not be larger than %d x %d \n", MAP_MAX_SIZE, MAP_MAX_SIZE);
	    } else {
		want_config[CONFIG_MAPWIDTH]=x;
		want_config[CONFIG_MAPHEIGHT]=y;
	    }
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
	else if (!strcmp(argv[on_arg],"-nofasttcpsend")) {
	    use_config[CONFIG_FASTTCP]=0;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-cache")) {
	    want_config[CONFIG_CACHE]= TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nocache")) {
	    want_config[CONFIG_CACHE]= FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-mapscroll")) {
	    want_config[CONFIG_MAPSCROLL] = TRUE;
	}
	else if (!strcmp(argv[on_arg],"-nomapscroll")) {
	    want_config[CONFIG_MAPSCROLL] = FALSE;
	}
	else if (!strcmp(argv[on_arg],"-darkness")) {
	    use_config[CONFIG_DARKNESS]= TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nodarkness")) {
	    use_config[CONFIG_DARKNESS]= FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-fogofwar")) {
	    use_config[CONFIG_FOGWAR]= TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nofogofwar")) {
	    use_config[CONFIG_FOGWAR]= FALSE;
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
	else if (!strcmp(argv[on_arg],"-showicon")) {
	    inv_list.show_icon = TRUE;
	    continue;
	}
        else if (!strcmp(argv[on_arg],"-download_all_faces")) {
            use_config[CONFIG_DOWNLOAD]=TRUE;
            continue;
	}
	else if (!strcmp(argv[on_arg],"-echo")) {
	    use_config[CONFIG_ECHO]=TRUE;
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
	else if (!strcmp(argv[on_arg],"-scrolllines")) {
	    if (++on_arg == argc) {
		fprintf(stderr,"-scrolllines requires a number\n");
		return 1;
	    }
	    infodata.maxlines = atoi(argv[on_arg]);
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-font")) {
	    if (++on_arg == argc) {
		fprintf(stderr,"-font requires a font name\n");
		return 1;
	    }
	    font_name = argv[on_arg];
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-help")) {
	    usage(argv[0]);
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-nosound")) {
	    want_config[CONFIG_SOUND]=FALSE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-updatekeycodes")) {
	    updatekeycodes=TRUE;
	    continue;
	}
	else if (!strcmp(argv[on_arg],"-autorepeat")) {
	    noautorepeat=TRUE;
	    continue;
	}
	else {
	    fprintf(stderr,"Do not understand option %s\n", argv[on_arg]);
	    usage(argv[0]);
	    return 1;
	}
    }
    /* Moving processing and setting of display attributes down here.
     * This is because a default display mode may also require special
     * handling.
     * This is not ideal, as if we don't have one of the optional modes,
     * we just fall back to pixmap mode.  But I don't really want to get into
     * a big nest of #ifdefs checking/setting modes.
     */
#ifndef HAVE_LIBPNG
    fprintf(stderr,"Client not configured with Png display mode enabled\n");
    fprintf(stderr,"Install the png library and try recompiling.\n");
    exit(1);
#else
    image_size=32;
#endif

    mapdata_init();

    /* Finished parsing all the command line options.  Now start
     * working on the display.
     */
    gargc=argc;
    gargv=argv;

    if (get_root_display(display_name) ||
	get_game_display() ||
	get_stats_display() ||
	get_info_display() ||
	get_inv_display() ||
	get_look_display() ||
	get_message_display())
		return 1;

    init_keys();
    init_cache_data();
    set_window_pos();
    info_ratio=(float) infodata.width/ (float) (infodata.width + INV_WIDTH);
    return 0;
}


void display_map_newmap()
{
    reset_map();
}

/* This can also get called for png.  Really, anything that gets rendered
 * into a pixmap that does not need colors set or other specials done.
 */
static void display_mapcell(int ax, int ay)
{
    int layer, mx, my, got_one = 0;

    mx = pl_pos.x+ax;
    my = pl_pos.y+ay;

    if (!use_config[CONFIG_FOGWAR] && the_map.cells[mx][my].cleared) {
	XFillRectangle(display,win_game,gc_blank,image_size*ax,image_size*ay,image_size,image_size);
	return;
    }

    XFillRectangle(display,xpm_pixmap,gc_clear_xpm,0,0,image_size,image_size);
    for (layer=0; layer<MAXLAYERS; layer++) {
	int sx, sy;

	/* draw single-tile faces first */
	int face = mapdata_face(ax, ay, layer);
	if (face > 0) {
	    int w = pixmaps[face]->width;
	    int h = pixmaps[face]->height;
	    gen_draw_face(xpm_pixmap, face, 0, 0, (w-1)*image_size, (h-1)*image_size);
	    got_one = 1;
	}

	/* draw big faces last (should overlap other objects) */
	face = mapdata_bigface(ax, ay, layer, &sx, &sy);
	if (face > 0) {
	    gen_draw_face(xpm_pixmap, face, 0, 0, sx*image_size, sy*image_size);
	    got_one = 1;
	}
    }
    if (got_one) {
	if (the_map.cells[mx][my].cleared) {
	    XSetClipOrigin(display, gc_xpm[XPMGCS-1], 0, 0);
	    XSetForeground(display, gc_xpm[XPMGCS-1], discolor[0].pixel);
	    XSetClipMask(display, gc_xpm[XPMGCS-1], dark2);
	    XCopyPlane(display, dark2, xpm_pixmap, gc_xpm[XPMGCS-1], 0, 0, image_size, image_size, 0, 0, 1);
	    xpm_masks[XPMGCS-1] = dark2;
	}
	else if (use_config[CONFIG_DARKNESS] && the_map.cells[mx][my].have_darkness) {
	    int darkness;
	    XSetClipOrigin(display, gc_xpm[XPMGCS-1], 0, 0);
	    XSetForeground(display, gc_xpm[XPMGCS-1], discolor[0].pixel);
	    darkness = the_map.cells[mx][my].darkness;
	    if (darkness > 192) {
		XSetClipMask(display, gc_xpm[XPMGCS-1], dark1);
		XCopyPlane(display, dark1, xpm_pixmap, gc_xpm[XPMGCS-1], 0, 0, image_size, image_size, 0, 0, 1);
		xpm_masks[XPMGCS-1] = dark1;
	    }
	    else if (darkness > 128) {
		XSetClipMask(display, gc_xpm[XPMGCS-1], dark2);
		XCopyPlane(display, dark2, xpm_pixmap, gc_xpm[XPMGCS-1], 0, 0, image_size, image_size, 0, 0, 1);
		xpm_masks[XPMGCS-1] = dark2;
	    }
	    else if (darkness > 64) {
		XSetClipMask(display, gc_xpm[XPMGCS-1], dark3);
		XCopyPlane(display, dark3, xpm_pixmap, gc_xpm[XPMGCS-1], 0, 0, image_size, image_size, 0, 0, 1);
		xpm_masks[XPMGCS-1] = dark3;
	    }
	}
	XCopyArea(display, xpm_pixmap, win_game, gc_game, 0, 0, image_size, image_size, image_size*ax, image_size*ay);
    } else {
	XFillRectangle(display, win_game, gc_blank, image_size*ax, image_size*ay, image_size, image_size);
    }
}

void resize_map_window(int x, int y)
{
    if (!want_config[CONFIG_SPLITWIN]) {
	XWindowAttributes attrib;
	int width=0, height=0;

	/* width and height are how much larger we need to make 
	 * the window to keep it displayable.  This isn't perfect,
	 * but does a reasonable job.  Don't do shrinks
	 */

	if (use_config[CONFIG_MAPWIDTH] > old_mapx) width = (use_config[CONFIG_MAPWIDTH] - old_mapx)* image_size;

	if (use_config[CONFIG_MAPHEIGHT] > old_mapy) height = (use_config[CONFIG_MAPHEIGHT] - old_mapy)* image_size;

	/* if somethign to do */
	if (width>0 || height > 0) {
	    XGetWindowAttributes(display, win_root, &attrib);
	    width += attrib.width;  
	    height += attrib.height;
	    XResizeWindow(display, win_game, x*image_size, y*image_size);
	    XResizeWindow(display, win_root, width, height);
	}
	old_mapx=use_config[CONFIG_MAPWIDTH];
	old_mapy=use_config[CONFIG_MAPHEIGHT];
    }
    else {
	XResizeWindow(display, win_game, x*image_size, y*image_size);
    }
}

/* we don't need to do anything, as we figure this out as needed */
void x_set_echo() { }


/**
 * If redraw is set, force redraw of all tiles.
 *
 * If notice is set, another call will follow soon.
 */
void display_map_doneupdate(int redraw, int notice)
{
    int ax,ay, mx, my;

    if(notice) {
	return;
    }

    if (cpl.showmagic) {
	magic_map_flash_pos();
	return;
    }

    XSetClipMask(display,gc_floor,None);
    for(ax=0;ax<use_config[CONFIG_MAPWIDTH];ax++) {
	for(ay=0;ay<use_config[CONFIG_MAPHEIGHT];ay++) { 
	    mx = pl_pos.x+ax;
	    my = pl_pos.y+ay;
	    if (redraw || the_map.cells[mx][my].need_update)  {
		display_mapcell(ax, ay);
		the_map.cells[mx][my].need_update = 0;
	    }
	}
    }
    XFlush(display);
}

int display_mapscroll(int dx, int dy)
{
    int srcx, srcy;
    int dstx, dsty;
    int w, h;

    srcx = dx > 0 ? image_size*dx : 0;
    srcy = dy > 0 ? image_size*dy : 0;
    dstx = dx < 0 ? image_size*-dx : 0;
    dsty = dy < 0 ? image_size*-dy : 0;

    w = use_config[CONFIG_MAPWIDTH];
    w -= abs(dx);
    h = use_config[CONFIG_MAPHEIGHT];
    h -= abs(dy);
    
    XCopyArea(display, win_game, win_game, gc_copy,
        srcx, srcy,
        w*image_size, h*image_size,
        dstx, dsty);

    return 1;
}

/* This functions associates the image_data in the cache entry
 * with the specific pixmap number.  Returns 0 on success, -1
 * on failure.  Currently, there is no failure condition, but
 * there is the potential that in the future, we want to more
 * closely look at the data and if it isn't valid, return
 * the failure code.
 */
int associate_cache_entry(Cache_Entry *ce, int pixnum)
{
    pixmaps[pixnum] = ce->image_data;
    return 0;
}

void redisplay_stats()
{
  int i;
  for(i=0;i<7;i++) {
    XDrawImageString(display,win_stats,
		   gc_stats,10,i*14+10, stats_buff[i],strlen(stats_buff[i]));
}
  XFlush(display);
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
void draw_magic_map()
{
    XWindowAttributes win_info;
    int x, y;

    if (!cpl.magicmap) {
	draw_info ("You have yet to cast magic map.",NDI_BLACK);
	return;
    }

    /* Do this in case the window has been resized. */
    XGetWindowAttributes(display, win_game, &win_info);

    /* server used to set a grey background because grey was not used much.
     * however, with the magic map cleanup, that is not necessarily true,
     * so just set it to the default.
     */
    XClearWindow(display,win_game);

    cpl.mapxres = (win_info.width-4)/cpl.mmapx;
    cpl.mapyres = (win_info.height-4)/cpl.mmapy;
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
	    XSetForeground(display,gc_game,
		discolor[val&FACE_COLOR_MASK].pixel);
	    XFillRectangle(display,win_game,
                gc_game, cpl.mapxres*x, cpl.mapyres*y, cpl.mapxres, cpl.mapyres);
	} /* Saw into this space */
    }
}

/* Basically, this just flashes the player position on the magic map */
void magic_map_flash_pos()
{
    if (!cpl.showmagic) return;
    cpl.showmagic ^=2;
    if (cpl.showmagic & 2) {
	XSetForeground(display, gc_game, foreground);
	XSetBackground(display, gc_game, background);
    } else {
	XSetForeground(display, gc_game, background);
	XSetBackground(display, gc_game, foreground);
    }
    XFillRectangle(display, win_game, gc_game, cpl.mapxres*cpl.pmapx,
		   cpl.mapyres*cpl.pmapy, cpl.mapxres, cpl.mapyres);
}

/* We can now connect to different servers, so we need to clear out
 * any old images.  We try to free the data also to prevent memory
 * leaks.
 * This could be more clever, ie, if we're caching images and go to
 * a new server and get a name, we should try to re-arrange our cache
 * or the like.
 */
 
void reset_image_data()
{
    int i;

    reset_image_cache_data();

    for (i=1; i<MAXPIXMAPNUM; i++) {
	if (!want_config[CONFIG_CACHE] && pixmaps[i] != pixmaps[0]) {
	    XFreePixmap(display, pixmaps[i]->pixmap);
	    if (pixmaps[i]->mask) {
		XFreePixmap(display, pixmaps[i]->mask);
	    }
	    free(pixmaps[i]);
	    pixmaps[i] = pixmaps[0];
	}
    }
    look_list.env=cpl.below;
}


/* Gets a specified windows coordinates.  This function is pretty much
 * an exact copy out of the server.
 */
 
static void get_window_coord(Window win,
                 int *x,int *y,
                 int *wx,int *wy,
                 unsigned int *w,unsigned int *h)
{
  Window root,child;
  unsigned int tmp;

  XGetGeometry(display,win,&root,x,y,w,h,&tmp,&tmp);
  XTranslateCoordinates(display,win,root,0,0,wx,wy, &child);
}



void save_winpos()
{
    char savename[MAX_BUF],buf[MAX_BUF];
    FILE    *fp;
    int	    x,y,wx,wy;
    unsigned int w,h;

    if (!want_config[CONFIG_SPLITWIN]) {
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
    get_window_coord(win_game, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_game: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(win_stats, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_stats: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(infodata.win_info, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_info: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(inv_list.win, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_inv: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(look_list.win, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_look: %d %d %d %d\n", wx,wy, w, h);
    get_window_coord(win_message, &x,&y, &wx,&wy,&w,&h);
    fprintf(fp,"win_message: %d %d %d %d\n", wx,wy, w, h);
    fclose(fp);
    sprintf(buf,"Window positions saved to %s",savename);
    draw_info(buf,NDI_BLUE);
}

/* Reads in the winpos file created by the above function and sets the
 * the window positions appropriately.
 */
void set_window_pos()
{
    unsigned int xwc_mask = CWX|CWY|CWWidth|CWHeight;
    XWindowChanges xwc;
    char buf[MAX_BUF],*cp;
    FILE *fp;

    if (!want_config[CONFIG_SPLITWIN]) return;

    sprintf(buf,"%s/.crossfire/winpos", getenv("HOME"));
    if (!(fp=fopen(buf,"r"))) return;

    while(fgets(buf,MAX_BUF-1, fp)!=NULL) {
	buf[MAX_BUF-1]='\0';
	if (!(cp=strchr(buf,' '))) continue;
	*cp++='\0';
	if (sscanf(cp,"%d %d %d %d",&xwc.x,&xwc.y,&xwc.width,&xwc.height)!=4)
	    continue;
	if (!strcmp(buf,"win_game:")) 
	    XConfigureWindow(display,win_game,xwc_mask, &xwc);
	if (!strcmp(buf,"win_stats:")) 
	    XConfigureWindow(display,win_stats,xwc_mask, &xwc);
	if (!strcmp(buf,"win_info:")) 
	    XConfigureWindow(display,infodata.win_info,xwc_mask, &xwc);
	if (!strcmp(buf,"win_inv:")) 
	    XConfigureWindow(display,inv_list.win,xwc_mask, &xwc);
	if (!strcmp(buf,"win_look:")) 
	    XConfigureWindow(display,look_list.win,xwc_mask, &xwc);
	if (!strcmp(buf,"win_message:")) 
	    XConfigureWindow(display,win_message,xwc_mask, &xwc);

    }
    fclose(fp);
}


void load_defaults()
{
    char path[MAX_BUF],inbuf[MAX_BUF],*cp;
    FILE *fp;

    sprintf(path,"%s/.crossfire/defaults", getenv("HOME"));
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
	    use_config[CONFIG_PORT] = atoi(cp);
	    continue;
	}
	if (!strcmp(inbuf, "server")) {
	    server = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	if (!strcmp(inbuf,"cacheimages")) {
	    if (!strcmp(cp,"True")) want_config[CONFIG_CACHE]=TRUE;
	    else want_config[CONFIG_CACHE]=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"split")) {
	    if (!strcmp(cp,"True")) want_config[CONFIG_SPLITWIN]=TRUE;
	    else want_config[CONFIG_SPLITWIN]=FALSE;
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
	    if (!strcmp(cp,"True")) want_config[CONFIG_SOUND]=TRUE;
	    else want_config[CONFIG_SOUND]=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"command_window")) {
	    use_config[CONFIG_CWINDOW] = atoi(cp);
	    if (use_config[CONFIG_CWINDOW]<1 || use_config[CONFIG_CWINDOW]>127)
		use_config[CONFIG_CWINDOW]=COMMAND_WINDOW;
	    continue;
	}
	if (!strcmp(inbuf,"foodbeep")) {
	    if (!strcmp(cp,"True")) use_config[CONFIG_FOODBEEP]=TRUE;
	    else use_config[CONFIG_FOODBEEP]=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"noautorepeat")) {
	    if (!strcmp(cp,"True")) noautorepeat=TRUE;
	    else noautorepeat=FALSE;
	    continue;
	}
	if (!strcmp(inbuf,"font")) {
	    font_name = strdup_local(cp);
	    continue;
	}
	fprintf(stderr,"Got line we did not understand: %s: %s\n", inbuf, cp);
    }
    fclose(fp);
}

void save_defaults()
{
    char path[MAX_BUF],buf[MAX_BUF];
    FILE *fp;

    sprintf(path,"%s/.crossfire/defaults", getenv("HOME"));
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
    fprintf(fp,"# 'True' and 'False' are the proper cases for those two values\n");

    fprintf(fp,"port: %d\n", use_config[CONFIG_PORT]);
    fprintf(fp,"server: %s\n", server);
    fprintf(fp,"font: %s\n", font_name);
    fprintf(fp,"cacheimages: %s\n", want_config[CONFIG_CACHE]?"True":"False");
    fprintf(fp,"split: %s\n", want_config[CONFIG_SPLITWIN]?"True":"False");
    fprintf(fp,"showicon: %s\n", inv_list.show_icon?"True":"False");
    fprintf(fp,"scrolllines: %d\n", infodata.maxlines);
    fprintf(fp,"scrollinfo: %s\n", infodata.scroll_info_window?"True":"False");
    fprintf(fp,"sound: %s\n", want_config[CONFIG_SOUND]?"True":"False");
    fprintf(fp,"command_window: %d\n", use_config[CONFIG_CWINDOW]);
    fprintf(fp,"foodbeep: %s\n", use_config[CONFIG_FOODBEEP]?"True":"False");
    fprintf(fp,"noautorepeat: %s\n", noautorepeat?"True":"False");

    fclose(fp);
    sprintf(buf,"Defaults saved to %s",path);
    draw_info(buf,NDI_BLUE);
}

/* determine what we show in the inventory window.  This is a slightly
 * more complicated version than the server side, since we use a bitmask
 * which means we could show things like magical and cursed, or unpaid
 * and magical, etc.  Current time, we don't really support setting it
 * all that well.
 *
 */

void command_show (const char *params)
{
    if(!params) {
	if (inv_list.show_what==show_all) inv_list.show_what = show_applied;
	else { /* rotate the bit.  If no valid bits are set, start over */
	    inv_list.show_what = inv_list.show_what << 1;
	    if (!(inv_list.show_what & show_mask))
		inv_list.show_what = show_all;
	}
	inv_list.env->inv_updated =1;
	return;
    }

    if (!strncmp(params, "all", strlen(params)))
        inv_list.show_what = show_all;
    else if (!strncmp(params, "applied", strlen(params)))
        inv_list.show_what = show_applied;
    else if (!strncmp(params, "unapplied", strlen(params)))
        inv_list.show_what = show_unapplied;
    else if (!strncmp(params, "unpaid", strlen(params)))
        inv_list.show_what = show_unpaid;
    else if (!strncmp(params, "cursed", strlen(params)))
        inv_list.show_what = show_cursed;
    else if (!strncmp(params, "magical", strlen(params)))
        inv_list.show_what = show_magical;
    else if (!strncmp(params, "nonmagical", strlen(params)))
        inv_list.show_what = show_nonmagical;
    else if (!strncmp(params, "locked", strlen(params)))
        inv_list.show_what = show_locked;
    else if (!strncmp(params, "unlocked", strlen(params)))
        inv_list.show_what = show_unlocked;

    inv_list.env->inv_updated =1;
}

/* This doesn't need to do anything for x11 client */
void cleanup_connection() { }


int main(int argc, char *argv[])
{
    int sound,got_one=0;
    int i;

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

    sound = init_sounds();

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

	display_map_doneupdate(FALSE, FALSE);
	if (!server || got_one) {
	    char *ms;
	    metaserver_get_info(meta_server, meta_port);
	    metaserver_show(TRUE);
	    do {
		ms=get_metaserver();
	    } while (metaserver_select(ms));
	    negotiate_connection(sound);
	} else {
	    csocket.fd=init_connection(server, use_config[CONFIG_PORT]);
	    if (csocket.fd == -1) { /* specified server no longer valid */
		server = NULL;
		continue;
	    }
	    negotiate_connection(sound);
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
	/* We know the following is the private map structure in
	 * item.c.  But we don't have direct access to it, so
	 * we still use locate.
	 */
	remove_item_inventory(locate_item(0));
	look_list.env=cpl.below;
    }
    exit(0);	/* never reached */
}
