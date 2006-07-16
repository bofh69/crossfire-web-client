/*
 * gnome-cfclient, A GNOME client for Crossfire
 * 
 * Copyright (C) 2001 Scott Barnes
 * 
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * The author can be reached via e-mail to reeve@ductape.net
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include <sys/stat.h>
#include <png.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "def-keys.h"
#include "client.h"
#include "script.h"
#include "item.h"
#include "pixmaps/crossfiretitle.xpm"
#include "pixmaps/question.xpm"
#include "pixmaps/all.xpm"
#include "pixmaps/hand.xpm"
#include "pixmaps/hand2.xpm"
#include "pixmaps/coin.xpm"
#include "pixmaps/skull.xpm"
#include "pixmaps/mag.xpm"
#include "pixmaps/nonmag.xpm"
#include "pixmaps/lock.xpm"
#include "pixmaps/unlock.xpm"
#include "pixmaps/dot.xpm"

#include "gnome-cf.h"
#include "gnomeproto.h"

#define PNGX_NOFILE	1
#define PNGX_OUTOFMEM	2
#define PNGX_DATA	3
#define TYPE_LISTS 9
#define MAXPIXMAPNUM 10000
#define GDK_XUTIL
#define MAX_HISTORY 50
#define MAX_COMMAND_LEN 256
#define SCROLLBAR_WIDTH	16
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
#define MAXFACES 5
#define MAXPIXMAPNUM 10000
#define SHOW_RESISTS 7
#define INFOCHARS 50
#define FONTWIDTH 8
#define KEYF_NORMAL	0x01
#define KEYF_FIRE	0x02
#define KEYF_RUN	0x04
#define KEYF_MODIFIERS	0x07
#define KEYF_EDIT	0x08
#define KEYF_STANDARD	0x10
#define MAX_KEYCODE 255
#define ROTATE_RIGHT(c) if ((c) & 01) (c) = ((c) >>1) + 0x80000000; else (c) >>= 1;
#define display GDK_DISPLAY()
#define MAX_BARS_MESSAGE 80


typedef struct {
	int x;
	int y;
} MapPos;

typedef struct {
	GtkWidget *bar;
	GtkStyle *style;
	int state;
} Vitals;

enum {
	locked_icon = 1, applied_icon, unpaid_icon,
	damned_icon, cursed_icon, magic_icon, close_icon,
	stipple1_icon, stipple2_icon, max_icons
};

struct FaceCache {
	char *name;
	uint16 num;
} facecache[MAXPIXMAPNUM];

struct {
	char *name;
	uint32 checksum;
	GdkPixbuf *gdkpixbuf;
	GdkPixmap *gdkpixmap;
	GdkBitmap *map_mask;
	GdkPixmap *icon_image;
	GdkBitmap *icon_mask;
} private_cache[MAXPIXMAPNUM];

typedef enum inventory_show {
	show_all = 0, show_applied = 0x1, show_unapplied = 0x2, show_unpaid = 0x4,
	show_cursed = 0x8, show_magical = 0x10, show_nonmagical = 0x20,
	show_locked = 0x40, show_unlocked = 0x80,
	show_mask = 0xff
} inventory_show;

typedef struct {
	item *env;
	char title[MAX_BUF];
	char old_title[MAX_BUF];
	Window win;
	GtkWidget *label;
	GtkWidget *weightlabel;
	GtkWidget *maxweightlabel;
	float pos[TYPE_LISTS];
	GtkWidget *gtk_list[TYPE_LISTS];
	GtkWidget *gtk_lists[TYPE_LISTS];
	GC gc_text;
	GC gc_icon;
	GC gc_status;
	uint8 multi_list:1;
	uint8 show_icon:1;
	uint8 show_weight:1;
	char format_nw[20];
	char format_nwl[20];
	char format_n[20];
	sint16 text_len;
	sint16 width;
	sint16 height;
	sint16 item_pos;
	sint16 item_used;
	sint16 size;
	sint16 *faces;
	sint8 *icon1;
	sint8 *icon2;
	sint8 *icon3;
	sint8 *icon4;
	char **names;
	sint16 bar_length;
	sint16 bar_size;
	sint16 bar_pos;
	uint32 weight_limit;
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

typedef struct Keys {
	uint8 flags;
	sint8 direction;
	KeySym keysym;
	char *command;
	struct Keys *next;
} Key_Entry;

GtkWidget *drawable;
GdkPixmap *pixmap;
GdkGC *gc;
GdkBitmap *dark1, *dark2, *dark3;
GdkPixmap *dark;
char facecachedir[MAX_BUF];
static KeyCode firekey[2], runkey[2], commandkey, *bind_keycode, prevkey, nextkey, completekey;
static KeySym firekeysym[2], runkeysym[2], commandkeysym, *bind_keysym, prevkeysym, nextkeysym, completekeysym;
static int bind_flags = 0;
static char bind_buf[MAX_BUF];
extern char *directions[9];
static Key_Entry *keys[256];
char *facetoname[MAXPIXMAPNUM];
uint8 updatekeycodes = FALSE, keepcache = FALSE;
guchar rgb[32 * 32 * 3];
int image_size = 32, map_image_size=32;
char history[MAX_HISTORY][MAX_COMMAND_LEN];
static int cur_history_position = 0, scroll_history_position = 0;
GList *anim_inv_list = NULL, *anim_look_list = NULL;
extern int maxfd;
struct timeval timeout;
gint csocket_fd = 0;
static uint8 nopopups = FALSE, splitinfo = FALSE, color_inv = TRUE, color_text = TRUE, tool_tips = TRUE;
static Vitals vitals[4];
static GtkWidget *run_label, *fire_label;
static GtkWidget *resists[SHOW_RESISTS];
static GtkWidget *ckentrytext, *ckeyentrytext, *cmodentrytext, *cnumentrytext;
static GtkWidget *diawin = NULL, *castlist;
GdkColor gdk_green = { 0, 0, 0xcfff, 0 };
GdkColor gdk_red = { 0, 0xcfff, 0, 0 };
GdkColor gdk_grey = { 0, 0xea60, 0xea60, 0xea60 };
GdkColor gdk_black = { 0, 0, 0, 0 };
GdkColor gdkdiscolor;
static GdkColor map_color[16];
static GdkColor root_color[16];
static GdkPixmap *magicgdkpixmap;
static GdkGC *map_gc;
static GtkWidget *mapvbox;
PixmapInfo pixmaps[MAXPIXMAPNUM];
static GtkWidget *ccheckbutton1;
static GtkWidget *ccheckbutton3;
static GtkWidget *ccheckbutton4;
static GtkWidget *ccheckbutton5;
static GtkWidget *ccheckbutton6;
static GtkWidget *ccheckbutton7;
static GtkWidget *ccheckbutton8;
static GtkWidget *inv_notebook;
static GtkTooltips *tooltips;
static GtkWidget *dialogtext;
static GtkWidget *dialog_window;
static GtkWidget *cclist;
static gboolean draw_info_freeze1 = FALSE, draw_info_freeze2 = FALSE;
static GtkWidget *entrytext, *counttext;
static gint redraw_needed = FALSE;
static GtkObject *text_hadj, *text_vadj;
static GtkObject *text_hadj2, *text_vadj2;
static itemlist look_list, inv_list;
static StatWindow statwindow;
static GtkWidget *gtkwin_root, *gtkwin_info_text, *gtkwin_info_text2;
static GtkWidget *gtkwin_about = NULL;
static GtkWidget *gtkwin_splash = NULL;
static GtkWidget *gtkwin_chelp = NULL;
static GtkWidget *gtkwin_shelp = NULL;
static GtkWidget *gtkwin_magicmap = NULL;
static GtkWidget *gtkwin_config = NULL;
static GtkWidget *gameframe = NULL;
static GtkWidget *invframe = NULL;
static GtkWidget *lookframe = NULL;
static GtkWidget *imagesizesb = NULL;
static char *last_str;
static int pickup_mode = 0;
int updatelock = 0;
uint16 facecachemap[MAXPIXMAPNUM], cachelastused = 0, cacheloaded = 0;
FILE *fcache;
int misses = 0, total = 0;
int last_face_num = 0;
char facecachedir[MAX_BUF];
char *facetoname[MAXPIXMAPNUM];
gboolean echobindings = FALSE;
gboolean updatemapneed = FALSE;
gboolean cast_menu_item_selected = FALSE;
int mapsizeopt = -1;
gboolean did_quit = FALSE;
static char *colorname[] = {
	"Black",
	"White",
	"Navy",
	"Red",
	"Orange",
	"DodgerBlue",
	"DarkOrange2",
	"SeaGreen",
	"DarkSeaGreen",
	"Grey50",
	"Sienna",
	"Gold",
	"Khaki"
};
struct poptOption options[] = {
	{
	 "server",
	 's',
	 POPT_ARG_STRING,
	 &server,
	 0,
	 "Server to connect to.",
	 NULL},
	{
	 "port",
	 'p',
	 POPT_ARG_INT,
	 &port_num,
	 0,
	 "Port to use to connect.",
	 NULL},
	{
	 "echo",
	 'e',
	 POPT_ARG_NONE,
	 &echobindings,
	 0,
	 N_("Echo bindings."),
	 NULL},
	{
	 "mapsize",
	 'm',
	 POPT_ARG_INT,
	 &mapsizeopt,
	 0,
	 N_("Sets map size to NxN."),
	 NULL},
	{
	 NULL,
	 '\0',
	 0,
	 NULL,
	 0,
	 NULL,
	 NULL}
};

void
disconnect(GtkWidget * widget)
{
	close(csocket.fd);
	csocket.fd = -1;
	if (csocket_fd) {
		gdk_input_remove(csocket_fd);
		csocket_fd = 0;
		gtk_main_quit();
	}
}

void
gnome_client_quit()
{
	disconnect(NULL);
	gnome_config_sync();
	exit(0);
}



void
resize_map_window(int x, int y)
{
	gtk_drawing_area_size(GTK_DRAWING_AREA(drawable), map_image_size * x, map_image_size * y);
	gtk_widget_set_usize(gameframe, (map_image_size * x) + 6, (map_image_size * y) + 6);
	gtk_widget_set_usize(drawable, (map_image_size * x), (map_image_size * y));
	gtk_widget_set_usize(invframe, 230, (((map_image_size * y) / 3) * 2));
	gtk_widget_set_usize(lookframe, 230, ((map_image_size * y) / 3));
}

static void
requestface(int pnum, char *facename, char *facepath)
{
	char buf[MAX_BUF];

	facetoname[pnum] = strdup_local(facepath);
	cs_print_string(csocket.fd, "askface %d", pnum);
	sprintf(buf, "%s/%c%c", facecachedir, facename[0], facename[1]);
	if (access(buf, R_OK))
		make_path_to_dir(buf);
}

void
finish_face_cmd(int pnum, uint32 checksum, int has_sum, char *face)
{
	char buf[MAX_BUF];
	int fd, len;
	GdkPixbuf *tmppixbuf;
	sprintf(buf, "%s/.gnome/cfgfx/%s.png", getenv("HOME"), face);

	if ((fd = open(buf, O_RDONLY)) != -1) {
		close(fd);
		pixmaps[pnum].gdkpixbuf = gdk_pixbuf_new_from_file(buf);
		has_sum = 0;
	} else {
		len = find_face_in_private_cache(face, checksum);
		if (len > 0) {
			pixmaps[pnum].gdkpixbuf = private_cache[len].gdkpixbuf;
			pixmaps[pnum].map_image = private_cache[len].gdkpixmap;
			pixmaps[pnum].map_mask = private_cache[len].map_mask;
			pixmaps[pnum].icon_image = private_cache[len].icon_image;
			pixmaps[pnum].icon_mask = private_cache[len].icon_mask;
			if (private_cache[len].checksum == checksum || !has_sum || keepcache)
				return;
		}
		sprintf(buf, "%s/%c%c/%s.png", facecachedir, face[0], face[1], face);

		if ((fd = open(buf, O_RDONLY)) == -1) {
			requestface(pnum, face, buf);
			return;
		}
		close(fd);
		pixmaps[pnum].gdkpixbuf = gdk_pixbuf_new_from_file(buf);
	}
	pixmaps[pnum].map_width = gdk_pixbuf_get_width(pixmaps[pnum].gdkpixbuf);
	pixmaps[pnum].map_height = gdk_pixbuf_get_height(pixmaps[pnum].gdkpixbuf);

/*	tmppixbuf = gdk_pixbuf_scale_simple(pixmaps[pnum].gdkpixbuf, map_image_size, map_image_size, GDK_INTERP_BILINEAR);*/
	gdk_pixbuf_render_pixmap_and_mask(pixmaps[pnum].gdkpixbuf, &pixmaps[pnum].map_image, &pixmaps[pnum].map_mask, 1);
	tmppixbuf = gdk_pixbuf_scale_simple(pixmaps[pnum].gdkpixbuf, 12, 12, GDK_INTERP_BILINEAR);
	gdk_pixbuf_render_pixmap_and_mask(tmppixbuf, &pixmaps[pnum].icon_image, &pixmaps[pnum].icon_mask, 1);
	if (!pixmaps[pnum].gdkpixbuf) {
		requestface(pnum, face, buf);
	}
}

static void
insert_key(KeySym keysym, KeyCode keycode, int flags, char *command)
{

	Key_Entry *newkey;
	int i, direction = -1;
	if (keycode > MAX_KEYCODE) {
		fprintf(stderr, "Warning insert_key:keycode that is passed is greater than 255.\n");
		keycode = 0;
	}
	if (keys[keycode] == NULL) {
		keys[keycode] = malloc(sizeof(Key_Entry));
		keys[keycode]->command = NULL;
		keys[keycode]->next = NULL;
	}
	newkey = keys[keycode];
	for (i = 0; i < 9; i++)
		if (!strcmp(command, directions[i])) {
			direction = i;
			break;
		}
	if (keys[keycode]->command != NULL) {
		while (newkey->next != NULL)
			newkey = newkey->next;
		newkey->next = malloc(sizeof(Key_Entry));
		newkey = newkey->next;
		newkey->next = NULL;
	}
	newkey->keysym = keysym;
	newkey->flags = flags;
	newkey->command = strdup_local(command);
	newkey->direction = direction;
}

static void
parse_keybind_line(char *buf, int line, int standard)
{
	char *cp, *cpnext;
	KeySym keysym;
	KeyCode keycode;
	int flags;
	if (buf[0] == '#' || buf[0] == '\n')
		return;
	if ((cpnext = strchr(buf, ' ')) == NULL) {
		fprintf(stderr, "Line %d (%s) corrupted in keybinding file.\n", line, buf);
		return;
	}
	if (buf[0] == '!') {
		char *cp1;
		while (*cpnext == ' ')
			++cpnext;
		cp = strchr(cpnext, ' ');
		if (!cp) {
			fprintf(stderr, "Line %d (%s) corrupted in keybinding file.\n", line, buf);
			return;
		}
		*cp++ = 0;
		cp1 = strchr(cp, ' ');
		if (!cp1) {
			fprintf(stderr, "Line %d (%s) corrupted in keybinding file.\n", line, buf);
			return;
		}
		*cp1++ = 0;
		keycode = atoi(cp1);
		keysym = XStringToKeysym(cp);
		if (keysym == NoSymbol) {
			fprintf(stderr, "Could not convert %s into keysym\n", cp);
			return;
		}
		if (!strcmp(cpnext, "commandkey")) {
			commandkeysym = keysym;
			commandkey = keycode;
			return;
		}
		if (!strcmp(cpnext, "firekey0")) {
			firekeysym[0] = keysym;
			firekey[0] = keycode;
			return;
		}
		if (!strcmp(cpnext, "firekey1")) {
			firekeysym[1] = keysym;
			firekey[1] = keycode;
			return;
		}
		if (!strcmp(cpnext, "runkey0")) {
			runkeysym[0] = keysym;
			runkey[0] = keycode;
			return;
		}
		if (!strcmp(cpnext, "runkey1")) {
			runkeysym[1] = keysym;
			runkey[1] = keycode;
			return;
		}
		if (!strcmp(cpnext, "completekey")) {
			completekeysym = keysym;
			completekey = keycode;
			return;
		}
		if (!strcmp(cpnext, "nextkey")) {
			nextkeysym = keysym;
			nextkey = keycode;
			return;
		}
		if (!strcmp(cpnext, "prevkey")) {
			prevkeysym = keysym;
			prevkey = keycode;
			return;
		}
	}
	if (standard)
		standard = KEYF_STANDARD;
	else
		standard = 0;
	*cpnext++ = '\0';
	keysym = XStringToKeysym(buf);
	cp = cpnext;
	if ((cpnext = strchr(cp, ' ')) == NULL) {
		fprintf(stderr, "Line %d (%s) corrupted in keybinding file.\n", line, cp);
		return;
	}
	*cpnext++ = '\0';
	keycode = atoi(cp);
	cp = cpnext;
	if ((cpnext = strchr(cp, ' ')) == NULL) {
		fprintf(stderr, "Line %d (%s) corrupted in keybinding file.\n", line, cp);
		return;
	}
	*cpnext++ = '\0';
	flags = 0;
	while (*cp != '\0') {
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
			fprintf(stderr, "Warning:  Unknown flag (%c) line %d in key binding file\n", *cp, line);
		}
		cp++;
	}
	if ((keysym != NoSymbol) && (((keycode == 1) && standard) || (flags & KEYF_STANDARD) || updatekeycodes)) {
		keycode = XKeysymToKeycode(display, keysym);
		if (keycode == 0) {
			fprintf(stderr, "Warning: could not convert keysym %s into keycode, ignoring\n", buf);
		}
	}
	cpnext[strlen(cpnext) - 1] = '\0';
    if (strlen(cpnext)>(sizeof(bind_buf)-1)){
        cpnext[sizeof(bind_buf)-1]='\0';
        LOG(LOG_WARNING,"gtk::parse_keybind_line","Had to truncate a too long command");
    }
	insert_key(keysym, keycode, flags | standard, cpnext);
}

static void
init_default_keybindings()
{
	char buf[MAX_BUF];
	int i;
	for (i = 0; i < sizeof(def_keys) / sizeof(char *); i++) {
		strcpy(buf, def_keys[i]);
		parse_keybind_line(buf, i, 1);
	}
}

static void
init_keys()
{
	int i, line = 0;
	FILE *fp;
	char buf[BIG_BUF];
	commandkeysym = XK_apostrophe;
	commandkey = XKeysymToKeycode(display, XK_apostrophe);
	if (!commandkey) {
		commandkeysym = XK_acute;
		commandkey = XKeysymToKeycode(display, XK_acute);
	}
	firekeysym[0] = XK_Shift_L;
	firekey[0] = XKeysymToKeycode(display, XK_Shift_L);
	firekeysym[1] = XK_Shift_R;
	firekey[1] = XKeysymToKeycode(display, XK_Shift_R);
	runkeysym[0] = XK_Control_L;
	runkey[0] = XKeysymToKeycode(display, XK_Control_L);
	runkeysym[1] = XK_Control_R;
	runkey[1] = XKeysymToKeycode(display, XK_Control_R);
	completekeysym = XK_Tab;
	completekey = XKeysymToKeycode(display, XK_Tab);
	nextkeysym = NoSymbol;
	nextkey = 0;
	prevkeysym = NoSymbol;
	prevkey = 0;
	for (i = 0; i <= MAX_KEYCODE; i++) {
		keys[i] = NULL;
	}
	sprintf(buf, "%s/.gnome/gnome-cfkeys", getenv("HOME"));
	if ((fp = fopen(buf, "r")) == NULL) {
		fprintf(stderr, "Could not open ~/.gnome/gnome-cfkeys, trying to load global bindings\n");
		if (client_libdir == NULL) {
			init_default_keybindings();
			return;
		}
		sprintf(buf, "%s/def_keys", client_libdir);
		if ((fp = fopen(buf, "r")) == NULL) {
			init_default_keybindings();
			return;
		}
	}
	while (fgets(buf, BIG_BUF, fp)) {
		line++;
		parse_keybind_line(buf, line, 0);
	}
	fclose(fp);
}

static void
parse_key_release(KeyCode kc, KeySym ks)
{
	if (kc == firekey[0] || ks == firekeysym[0] || kc == firekey[1] || ks == firekeysym[1]) {
		cpl.fire_on = 0;
		clear_fire();
		gtk_label_set(GTK_LABEL(fire_label), "    ");
	} else if (kc == runkey[0] || ks == runkeysym[0] || kc == runkey[1] || ks == runkeysym[1]) {
		cpl.run_on = 0;
		if (cpl.echo_bindings)
			draw_info("stop run", NDI_BLACK);
		clear_run();
		gtk_label_set(GTK_LABEL(run_label), "   ");
	} else if (cpl.fire_on)
		clear_fire();
}

static void
parse_key(char key, KeyCode keycode, KeySym keysym)
{
	Key_Entry *keyentry, *first_match = NULL;
	int present_flags = 0;
	char buf[MAX_BUF];
	if (keycode == commandkey && keysym == commandkeysym) {
		gtk_widget_grab_focus(GTK_WIDGET(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))));
		gtk_entry_set_visibility(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))), 1);
		cpl.input_state = Command_Mode;
		cpl.no_echo = FALSE;
		return;
	}
	if (keycode == firekey[0] || keysym == firekeysym[0] || keycode == firekey[1] || keysym == firekeysym[1]) {
		cpl.fire_on = 1;
		gtk_label_set(GTK_LABEL(fire_label), "Fire");
		return;
	}
	if (keycode == runkey[0] || keysym == runkeysym[0] || keycode == runkey[1] || keysym == runkeysym[1]) {
		cpl.run_on = 1;
		gtk_label_set(GTK_LABEL(run_label), "Run");
		return;
	}
	if (cpl.run_on)
		present_flags |= KEYF_RUN;
	if (cpl.fire_on)
		present_flags |= KEYF_FIRE;
	if (present_flags == 0)
		present_flags = KEYF_NORMAL;
	keyentry = keys[keycode];
	while (keyentry != NULL) {
		if ((keyentry->keysym != NoSymbol && keyentry->keysym != keysym) || (!(keyentry->flags & present_flags))) {
			keyentry = keyentry->next;
			continue;
		}
		first_match = keyentry;
		if ((keyentry->flags & KEYF_MODIFIERS) != present_flags) {
			keyentry = keyentry->next;
			continue;
		} else
			break;
	}
	if (first_match != NULL) {
		char buf[MAX_BUF];
		if (first_match->flags & KEYF_EDIT) {
			strcpy(cpl.input_text, first_match->command);
			cpl.input_state = Command_Mode;
			sprintf(buf, "%s", cpl.input_text);
			gtk_entry_set_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))), buf);
			gtk_widget_grab_focus(GTK_WIDGET(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))));
			return;
		}
		if (first_match->direction >= 0) {
			if (cpl.fire_on) {
				sprintf(buf, "fire %s", first_match->command);
				fire_dir(first_match->direction);
			} else if (cpl.run_on) {
				run_dir(first_match->direction);
				sprintf(buf, "run %s", first_match->command);
			} else {
				strcpy(buf, first_match->command);
				extended_command(first_match->command);
			}
			if (cpl.echo_bindings)
				draw_info(buf, NDI_BLACK);
		} else {
			if (cpl.echo_bindings)
				draw_info(first_match->command, NDI_BLACK);
			extended_command(first_match->command);
		}
		return;
	}
	if (key >= '0' && key <= '9') {
		cpl.count = cpl.count * 10 + (key - '0');
		if (cpl.count > 100000)
			cpl.count %= 100000;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(counttext), (float)cpl.count);
		return;
	}
	sprintf(buf, "Key unused (%s%s%s)", (cpl.fire_on ? "Fire&" : ""), (cpl.run_on ? "Run&" : ""), keysym == NoSymbol ? "unknown" : XKeysymToString(keysym));
	draw_info(buf, NDI_BLACK);
	cpl.count = 0;
}

static char *
get_key_info(Key_Entry * key, KeyCode kc, int save_mode)
{
	/* bind buf is the maximum space allowed for a
     * binded command. We will add additional datas to
     * it so we increase by MAX_BUF*/
    static char buf[MAX_BUF+sizeof(bind_buf)];
	char buff[MAX_BUF];
	int bi = 0;
	if ((key->flags & KEYF_MODIFIERS) == KEYF_MODIFIERS)
		buff[bi++] = 'A';
	else {
		if (key->flags & KEYF_NORMAL)
			buff[bi++] = 'N';
		if (key->flags & KEYF_FIRE)
			buff[bi++] = 'F';
		if (key->flags & KEYF_RUN)
			buff[bi++] = 'R';
	}
	if (key->flags & KEYF_EDIT)
		buff[bi++] = 'E';
	if (key->flags & KEYF_STANDARD)
		buff[bi++] = 'S';
	buff[bi] = '\0';
	if (save_mode) {
		if (key->keysym == NoSymbol) {
			sprintf(buf, "(null) %i %s %s", kc, buff, key->command);
		} else {
			sprintf(buf, "%s %i %s %s", XKeysymToString(key->keysym), kc, buff, key->command);
		}
	} else {
		if (key->keysym == NoSymbol) {
			sprintf(buf, "key (null) (%i) %s %s", kc, buff, key->command);
		} else {
			sprintf(buf, "key %s (%i) %s %s", XKeysymToString(key->keysym), kc, buff, key->command);
		}
	}
	return buf;
}

static void
show_keys(int allbindings)
{
	int i, count = 1;
	Key_Entry *key;
	char buf[MAX_BUF];
	sprintf(buf, "Commandkey %s (%d)", commandkeysym == NoSymbol ? "unknown" : XKeysymToString(commandkeysym), commandkey);
	draw_info(buf, NDI_BLACK);
	sprintf(buf, "Firekeys 1: %s (%d), 2: %s (%d)", firekeysym[0] == NoSymbol ? "unknown" : XKeysymToString(firekeysym[0]), firekey[0], firekeysym[1] == NoSymbol ? "unknown" : XKeysymToString(firekeysym[1]), firekey[1]);
	draw_info(buf, NDI_BLACK);
	sprintf(buf, "Runkeys 1: %s (%d), 2: %s (%d)", runkeysym[0] == NoSymbol ? "unknown" : XKeysymToString(runkeysym[0]), runkey[0], runkeysym[1] == NoSymbol ? "unknown" : XKeysymToString(runkeysym[1]), runkey[1]);
	draw_info(buf, NDI_BLACK);
	sprintf(buf, "Command Completion Key %s (%d)", completekeysym == NoSymbol ? "unknown" : XKeysymToString(completekeysym), completekey);
	draw_info(buf, NDI_BLACK);
	sprintf(buf, "Next Command in History Key %s (%d)", nextkeysym == NoSymbol ? "unknown" : XKeysymToString(nextkeysym), nextkey);
	draw_info(buf, NDI_BLACK);
	sprintf(buf, "Previous Command in History Key %s (%d)", prevkeysym == NoSymbol ? "unknown" : XKeysymToString(prevkeysym), prevkey);
	draw_info(buf, NDI_BLACK);
	for (i = 0; i <= MAX_KEYCODE; i++) {
		for (key = keys[i]; key != NULL; key = key->next) {
			if (key->flags & KEYF_STANDARD && !allbindings)
				continue;
			sprintf(buf, "%3d %s", count, get_key_info(key, i, 0));
			draw_info(buf, NDI_BLACK);
			count++;
		}
	}
}

void
bind_key(char *params)
{
	char buf[MAX_BUF];
	if (!params) {
		draw_info("Usage: bind [-nfre] {<commandline>/commandkey/firekey{1/2}/runkey{1/2}/", NDI_BLACK);
		draw_info("           completekey/nextkey/prevkey}", NDI_BLACK);
		return;
	}
	while (*params == ' ')
		params++;
	if (!strcmp(params, "commandkey")) {
		bind_keycode = &commandkey;
		bind_keysym = &commandkeysym;
		draw_info("Push key to bind new commandkey.", NDI_BLACK);
		cpl.input_state = Configure_Keys;
		return;
	}
	if (!strcmp(params, "firekey1")) {
		bind_keycode = &firekey[0];
		bind_keysym = &firekeysym[0];
		draw_info("Push key to bind new firekey 1.", NDI_BLACK);
		cpl.input_state = Configure_Keys;
		return;
	}
	if (!strcmp(params, "firekey2")) {
		bind_keycode = &firekey[1];
		bind_keysym = &firekeysym[1];
		draw_info("Push key to bind new firekey 2.", NDI_BLACK);
		cpl.input_state = Configure_Keys;
		return;
	}
	if (!strcmp(params, "runkey1")) {
		bind_keycode = &runkey[0];
		bind_keysym = &runkeysym[0];
		draw_info("Push key to bind new runkey 1.", NDI_BLACK);
		cpl.input_state = Configure_Keys;
		return;
	}
	if (!strcmp(params, "runkey2")) {
		bind_keycode = &runkey[1];
		bind_keysym = &runkeysym[1];
		draw_info("Push key to bind new runkey 2.", NDI_BLACK);
		cpl.input_state = Configure_Keys;
		return;
	}

	if (!strcmp(params, "completekey")) {
		bind_keycode = &completekey;
		bind_keysym = &completekeysym;
		draw_info("Push key to bind new command completeion key", NDI_BLACK);
		cpl.input_state = Configure_Keys;
		return;
	}

	if (!strcmp(params, "prevkey")) {
		bind_keycode = &prevkey;
		bind_keysym = &prevkeysym;
		draw_info("Push key to bind new previous command in history key.", NDI_BLACK);
		cpl.input_state = Configure_Keys;
		return;
	}

	if (!strcmp(params, "nextkey")) {
		bind_keycode = &nextkey;
		bind_keysym = &nextkeysym;
		draw_info("Push key to bind new next command in history key.", NDI_BLACK);
		cpl.input_state = Configure_Keys;
		return;
	}
	if (params[0] != '-')
		bind_flags = KEYF_MODIFIERS;
	else {
		bind_flags = 0;
		bind_keysym = NULL;
		bind_keycode = NULL;
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
				draw_info("Try unbind to remove bindings..", NDI_BLACK);
				return;
			default:
				sprintf(buf, "Unknown flag to bind: '%c'", *params);
				draw_info(buf, NDI_BLACK);
				return;
			}
		params++;
	}
	if (!(bind_flags & KEYF_MODIFIERS))
		bind_flags |= KEYF_MODIFIERS;
	if (!params[0]) {
		draw_info("Try unbind to remove bindings..", NDI_BLACK);
		return;
	}
        if (strlen(params) >= sizeof(bind_buf)) {
	    params[sizeof(bind_buf) - 1] = '\0';
        draw_info("Keybinding too long! Truncated:",NDI_RED);
        draw_info(params,NDI_RED);
	}
	sprintf(buf, "Push key to bind '%s'.", params);
	draw_info(buf, NDI_BLACK);
	strcpy(bind_buf, params);
	bind_keycode = NULL;
	cpl.input_state = Configure_Keys;
	return;
}

static void
save_individual_key(FILE * fp, Key_Entry * key, KeyCode kc)
{
	if (key == NULL)
		return;
	fprintf(fp, "%s\n", get_key_info(key, kc, 1));
	save_individual_key(fp, key->next, kc);
}

static void
save_keys()
{
	char buf[MAX_BUF], buf2[MAX_BUF];
	int i;
	FILE *fp;
	sprintf(buf, "%s/.gnome/gnome-cfkeys", getenv("HOME"));
	if (make_path_to_file(buf) == -1) {
		fprintf(stderr, "Could not create %s\n", buf);
		return;
	}
	if ((fp = fopen(buf, "w")) == NULL) {
		sprintf(buf2, "Could not open %s, key bindings not saved\n", buf);
		draw_info(buf2, NDI_BLACK);
		return;
	}
	if (commandkeysym != XK_apostrophe && commandkeysym != NoSymbol) {
		fprintf(fp, "! commandkey %s %d\n", XKeysymToString(commandkeysym), commandkey);
	}
	if (firekeysym[0] != XK_Shift_L && firekeysym[0] != NoSymbol) {
		fprintf(fp, "! firekey0 %s %d\n", XKeysymToString(firekeysym[0]), firekey[0]);
	}
	if (firekeysym[1] != XK_Shift_R && firekeysym[1] != NoSymbol) {
		fprintf(fp, "! firekey1 %s %d\n", XKeysymToString(firekeysym[1]), firekey[1]);
	}
	if (runkeysym[0] != XK_Control_L && runkeysym[0] != NoSymbol) {
		fprintf(fp, "! runkey0 %s %d\n", XKeysymToString(runkeysym[0]), runkey[0]);
	}
	if (runkeysym[1] != XK_Control_R && runkeysym[1] != NoSymbol) {
		fprintf(fp, "! runkey1 %s %d\n", XKeysymToString(runkeysym[1]), runkey[1]);
	}
	if (completekeysym != XK_Tab && completekeysym != NoSymbol) {
		fprintf(fp, "! completekey %s %d\n", XKeysymToString(completekeysym), completekey);
	}
	if (nextkeysym != NoSymbol) {
		fprintf(fp, "! nextkey %s %d\n", XKeysymToString(nextkeysym), nextkey);
	}
	if (prevkeysym != NoSymbol) {
		fprintf(fp, "! prevkey %s %d\n", XKeysymToString(prevkeysym), prevkey);
	}
	for (i = 0; i <= MAX_KEYCODE; i++) {
		save_individual_key(fp, keys[i], i);
	}
	fclose(fp);
	draw_info("key bindings successfully saved.", NDI_BLACK);
}

static void
configure_keys(KeyCode k, KeySym keysym)
{
	char buf[MAX_BUF];
	if (bind_keycode == NULL) {
		if (k == firekey[0] || k == firekey[1]) {
			cpl.fire_on = 1;
			draw_message_window(0);
			return;
		}
		if (k == runkey[0] || k == runkey[1]) {
			cpl.run_on = 1;
			draw_message_window(0);
			return;
		}
	}
	cpl.input_state = Playing;
	if ((cpl.fire_on || cpl.run_on) && (bind_flags & KEYF_MODIFIERS) == KEYF_MODIFIERS) {
		bind_flags &= ~KEYF_MODIFIERS;
		if (cpl.fire_on)
			bind_flags |= KEYF_FIRE;
		if (cpl.run_on)
			bind_flags |= KEYF_RUN;
	}
	if (bind_keycode != NULL) {
		*bind_keycode = k;
		*bind_keysym = keysym;
	} else {
		insert_key(keysym, k, bind_flags, bind_buf);
	}
	sprintf(buf, "Binded to key '%s' (%i)", keysym == NoSymbol ? "unknown" : XKeysymToString(keysym), (int)k);
	draw_info(buf, NDI_BLACK);
	cpl.fire_on = 0;
	cpl.run_on = 0;
	draw_message_window(0);
	save_keys();
	return;
}

static void
unbind_usage()
{
	draw_info("Usage: unbind <entry_number> or", NDI_BLACK);
	draw_info("Usage: unbind [-a] [-g] to show existing bindings", NDI_BLACK);
	draw_info("    -a shows all (global) bindings", NDI_BLACK);
	draw_info("    -g unbinds a global binding", NDI_BLACK);
}

void
unbind_key(char *params)
{
	int count = 0, keyentry, onkey, global = 0;
	Key_Entry *key, *tmp;
	char buf[MAX_BUF];
	if (params == NULL || params[0] == '\0') {
		show_keys(0);
		return;
	}
	while (*params == ' ')
		params++;
	if (!strcmp(params, "-a")) {
		show_keys(1);
		return;
	}
	if (!strncmp(params, "-g", 2)) {
		global = 1;
		if (!(params = strchr(params, ' '))) {
			unbind_usage();
			return;
		}
	}
	if ((keyentry = atoi(params)) == 0) {
		unbind_usage();
		return;
	}
	for (onkey = 0; onkey <= MAX_KEYCODE; onkey++) {
		for (key = keys[onkey]; key; key = key->next) {
			if (global ||!(key->flags & KEYF_STANDARD))
				count++;
			if (keyentry == count) {
				if (key == keys[onkey]) {
					keys[onkey] = key->next;
					goto unbinded;
				}
				for (tmp = keys[onkey]; tmp->next != NULL; tmp = tmp->next) {
					if (tmp->next == key) {
						tmp->next = key->next;
						goto unbinded;
					}
				}
				fprintf(stderr, "unbind_key - found number entry, but could not find actual key\n");
			}
		}
	}
	draw_info("", NDI_BLACK);
	draw_info("No such entry. Try 'unbind' with no options to find entry.", NDI_BLACK);
	return;
  unbinded:
	sprintf(buf, "Removed binding: %3d %s", count, get_key_info(key, onkey, 0));
	draw_info(buf, NDI_BLACK);
	free(key->command);
	free(key);
	save_keys();
}

void
load_defaults()
{
	char *dispstr, *tmpstr;
	gboolean diddef = 0;
	port_num = gnome_config_get_int_with_default("gnome-cfclient/Options/Port=" DEFPORT, &diddef);
	if (diddef)
		gnome_config_set_int("gnome-cfclient/Options/Port", port_num);
	server = gnome_config_get_string_with_default("gnome-cfclient/Options/Server=" SERVER, &diddef);
	if (diddef)
		gnome_config_set_string("gnome-cfclient/Options/Server", server);
	if (DISPLAY_MODE == Png_Display)
		tmpstr = "gnome-cfclient/Options/Display=png";
	if (DISPLAY_MODE == Xpm_Display)
		tmpstr = "gnome-cfclient/Options/Display=xpm";
	dispstr = gnome_config_get_string_with_default(tmpstr, &diddef);
	if (diddef)
		gnome_config_set_string("gnome-cfclient/Options/Display", dispstr);

	image_size = gnome_config_get_int_with_default("gnome-cfclient/Options/ImageSize=24", &diddef);
	if (diddef)
		gnome_config_set_int("gnome-cfclient/Options/ImageSize", image_size);
	map_image_size = image_size;

	inv_list.show_icon = gnome_config_get_bool_with_default("gnome-cfclient/Options/ShowIcon=TRUE", &diddef);
	if (diddef)
		gnome_config_set_bool("gnome-cfclient/Options/ShowIcon", inv_list.show_icon);
	nosound = !gnome_config_get_bool_with_default("gnome-cfclient/Options/Sound=TRUE", &diddef);
	if (diddef)
		gnome_config_set_bool("gnome-cfclient/Options/Sound", !nosound);
	cpl.food_beep = gnome_config_get_bool_with_default("gnome-cfclient/Options/FoodBeep=TRUE", &diddef);
	if (diddef)
		gnome_config_set_bool("gnome-cfclient/Options/FoodBeep", cpl.food_beep);
	color_inv = gnome_config_get_bool_with_default("gnome-cfclient/Options/ColorInv=TRUE", &diddef);
	if (diddef)
		gnome_config_set_bool("gnome-cfclient/Options/ColorInv", color_inv);
	color_text = gnome_config_get_bool_with_default("gnome-cfclient/Options/ColorText=TRUE", &diddef);
	if (diddef)
		gnome_config_set_bool("gnome-cfclient/Options/ColorText", color_text);
	tool_tips = gnome_config_get_bool_with_default("gnome-cfclient/Options/Tooltips=TRUE", &diddef);
	if (diddef)
		gnome_config_set_bool("gnome-cfclient/Options/Tooltips", tool_tips);
	splitinfo = gnome_config_get_bool_with_default("gnome-cfclient/Options/SplitInfo=FALSE", &diddef);
	if (diddef)
		gnome_config_set_bool("gnome-cfclient/Options/SplitInfo", splitinfo);
	nopopups = gnome_config_get_bool_with_default("gnome-cfclient/Options/NoPopups=FALSE", &diddef);
	if (diddef)
		gnome_config_set_bool("gnome-cfclient/Options/NoPopups", nopopups);
}

void
save_defaults()
{
	char *dispstr;
	gnome_config_set_int("gnome-cfclient/Options/Port", port_num);
	gnome_config_set_string("gnome-cfclient/Options/Server", server);
	gnome_config_set_string("gnome-cfclient/Options/Display", dispstr);
	gnome_config_set_bool("gnome-cfclient/Options/ShowIcon", inv_list.show_icon);
	gnome_config_set_bool("gnome-cfclient/Options/Sound", !nosound);
	gnome_config_set_bool("gnome-cfclient/Options/FoodBeep", cpl.food_beep);
	gnome_config_set_bool("gnome-cfclient/Options/ColorInv", color_inv);
	gnome_config_set_bool("gnome-cfclient/Options/ColorText", color_text);
	gnome_config_set_bool("gnome-cfclient/Options/Tooltips", tool_tips);
	gnome_config_set_bool("gnome-cfclient/Options/SplitInfo", splitinfo);
	gnome_config_set_bool("gnome-cfclient/Options/NoPopups", nopopups);
	gnome_config_sync();
}

int
find_face_in_private_cache(char *face, int checksum)
{
	int i;
	for (i = 1; i <= last_face_num; i++)
		if (!strcmp(face, private_cache[i].name)) {
			return i;
		}
	return -1;
}

char **
xpmbuffertodata(char *buffer)
{
	char *buf = NULL;
	char **strings = NULL;
	int i = 0, q = 0, z = 0;
	for (i = 1; buffer[i] != ';'; i++) {
		if (buffer[i] == '"') {
			z = 0;
			for (i++; buffer[i] != '"'; i++) {
				buf = (char *)realloc(buf, (z + 2) * sizeof(char));
				buf[z] = buffer[i];
				z++;
			}
			buf[z] = '\0';
			strings = (char **)realloc(strings, (q + 2) * sizeof(char *));
			strings[q] = (char *)strdup(buf);
			q++;
		}
	}
	strings = (char **)realloc(strings, (q + 2) * sizeof(char *));
	strings[q] = (char *)NULL;
	free(buf);
	buf = NULL;
	return (strings);
}

void
freexpmdata(char **strings)
{
	int q = 0;
	for (q = 0; strings[q] != NULL; q++) {
		free(strings[q]);
	}
	free(strings);
}

void
do_network()
{
	fd_set tmp_read;
	int pollret;
	extern int updatelock;
	if (csocket.fd == -1) {
		if (csocket_fd) {
			gdk_input_remove(csocket_fd);
			csocket_fd = 0;
			gtk_main_quit();
		}
		return;
	}
	if (updatelock < 20) {
		FD_ZERO(&tmp_read);
		FD_SET(csocket.fd, &tmp_read);
                script_fdset(&maxfd,&tmp_read);
		pollret = select(maxfd, &tmp_read, NULL, NULL, &timeout);
		if (pollret == -1) {
			fprintf(stderr, "Got errno %d on select call.\n", errno);
		} else if (FD_ISSET(csocket.fd, &tmp_read)) {
			DoClient(&csocket);
		}
                else {
                   script_process(&tmp_read);
                }
	} else {
		printf("locked for network recieves.\n");
	}
}

void
event_loop()
{
	gint fleep;
	extern int do_timeout();
	int tag;
	if (MAX_TIME == 0) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
	}
	maxfd = csocket.fd + 1;
	if (MAX_TIME != 0) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
	}
	fleep = gtk_timeout_add(100, (GtkFunction) do_timeout, NULL);
	csocket_fd = gdk_input_add((gint) csocket.fd, GDK_INPUT_READ, (GdkInputFunction) do_network, &csocket);
	tag = csocket_fd;
	did_quit = FALSE;
	gtk_main();
	gtk_timeout_remove(tag);
	fprintf(stderr, "gtk_main exited, returning from event_loop\n");
	gnome_config_sync();
}

void
end_windows()
{
	free(last_str);
}

static animview *
newanimview()
{
	animview *op = malloc(sizeof(animview));
	if (!op)
		exit(0);
	op->row = 0;
	op->list = NULL;
	return op;
}

static animobject *
newanimobject()
{
	animobject *op = malloc(sizeof(animobject));
	if (!op)
		exit(0);
	op->view = NULL;
	return op;
}

void
freeanimview(gpointer data, gpointer user_data)
{
	if (data)
		g_free(data);
}

static void
freeanimobject(animobject * data, gpointer user_data)
{
	if (data)
		g_list_foreach(data->view, freeanimview, 0);
	g_free(data);
}

static void
animateview(animview * data, gint user_data)
{
	gtk_clist_set_pixmap(GTK_CLIST(data->list), data->row, 0, pixmaps[facecachemap[user_data]].icon_image, pixmaps[facecachemap[user_data]].icon_mask);
}

static void
animate(animobject * data, gpointer user_data)
{
	if (data) {
		data->item->last_anim++;
		if (data->item->last_anim >= data->item->anim_speed) {
			data->item->anim_state++;
			if (data->item->anim_state >= animations[data->item->animation_id].num_animations) {
				data->item->anim_state = 0;
			}
			data->item->face = animations[data->item->animation_id].faces[data->item->anim_state];
			data->item->last_anim = 0;
			g_list_foreach(data->view, (GFunc) animateview, GINT_TO_POINTER((gint) data->item->face));
		}
	}
}

void
animate_list()
{
	if (anim_inv_list) {
		g_list_foreach(anim_inv_list, (GFunc) animate, NULL);
	}
	if (anim_look_list && !look_list.env->inv_updated) {
		g_list_foreach(anim_look_list, (GFunc) animate, NULL);
	}
}

void
button_map_event(GtkWidget * widget, GdkEventButton * event)
{
	int dx, dy, i, x, y, xmidl, xmidh, ymidl, ymidh;
	x = (int)event->x;
	y = (int)event->y;
	dx = (x - 2) / map_image_size - (mapx / 2);
	dy = (y - 2) / map_image_size - (mapy / 2);
	xmidl = 5 * map_image_size - (mapx / 2);
	xmidh = 6 * map_image_size + (mapx / 2);
	ymidl = 5 * map_image_size - (mapy / 2);
	ymidh = 6 * map_image_size + (mapy / 2);
	switch (event->button) {
	case 1:
		{
			look_at(dx, dy);
		}
		break;
	case 2:
	case 3:
		if (x < xmidl)
			i = 0;
		else if (x > xmidh)
			i = 6;
		else
			i = 3;
		if (y > ymidh)
			i += 2;
		else if (y > ymidl)
			i++;
		if (event->button == 2) {
			switch (i) {
			case 0:
				fire_dir(8);
				break;
			case 1:
				fire_dir(7);
				break;
			case 2:
				fire_dir(6);
				break;
			case 3:
				fire_dir(1);
				break;
			case 5:
				fire_dir(5);
				break;
			case 6:
				fire_dir(2);
				break;
			case 7:
				fire_dir(3);
				break;
			case 8:
				fire_dir(4);
				break;
			}
			clear_fire();
		} else {
			switch (i) {
			case 0:
				move_player(8);
				break;
			case 1:
				move_player(7);
				break;
			case 2:
				move_player(6);
				break;
			case 3:
				move_player(1);
				break;
			case 5:
				move_player(5);
				break;
			case 6:
				move_player(2);
				break;
			case 7:
				move_player(3);
				break;
			case 8:
				move_player(4);
				break;
			}
		}
	}
}

static void
init_cache_data()
{
	int i;
	GdkPixbuf *tmppixbuf;
	printf("Init Cache\n");
	pixmaps[0].gdkpixbuf = gdk_pixbuf_new_from_xpm_data((const char **) question);
	tmppixbuf = gdk_pixbuf_scale_simple(pixmaps[0].gdkpixbuf, map_image_size, map_image_size, GDK_INTERP_BILINEAR);
	gdk_pixbuf_render_pixmap_and_mask(tmppixbuf, &pixmaps[0].map_image, &pixmaps[0].map_mask, 1);
	tmppixbuf = gdk_pixbuf_scale_simple(pixmaps[0].gdkpixbuf, 12, 12, GDK_INTERP_BILINEAR);
	gdk_pixbuf_render_pixmap_and_mask(tmppixbuf, &pixmaps[0].icon_image, &pixmaps[0].icon_mask, 1);
	pixmaps[0].bg = 0;
	pixmaps[0].fg = 1;
	facetoname[0] = NULL;
	for (i = 1; i < MAXPIXMAPNUM; i++) {
		pixmaps[i] = pixmaps[0];
		facetoname[i] = NULL;
	}
	sprintf(facecachedir, "%s/.gnome/gnome-cfcache", getenv("HOME"));
	if (make_path_to_dir(facecachedir) == -1) {
		fprintf(stderr, "Could not create directory %s, exiting\n", facecachedir);
		exit(1);
	}
}

void
keyrelfunc(GtkWidget * widget, GdkEventKey * event, GtkWidget * window)
{
	updatelock = 0;
	if (event->keyval > 0) {
		if (GTK_WIDGET_HAS_FOCUS(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext)))) {
		} else {
			parse_key_release(XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
			gtk_signal_emit_stop_by_name(GTK_OBJECT(window), "key_release_event");
		}
	}
}

void
keyfunc(GtkWidget * widget, GdkEventKey * event, GtkWidget * window)
{
	char *text;
	updatelock = 0;
	if (nopopups) {
		if (cpl.input_state == Reply_One) {
			text = XKeysymToString(event->keyval);
			send_reply(text);
			cpl.input_state = Playing;
			return;
		} else if (cpl.input_state == Reply_Many) {
			gtk_widget_grab_focus(GTK_WIDGET(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))));
			return;
		}
	}
	if (event->keyval > 0) {
		if (!GTK_WIDGET_HAS_FOCUS(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext)))) {
			switch (cpl.input_state) {
			case Playing:
				if (event->keyval == prevkeysym || event->keyval == nextkeysym) {
					return;
				}
				if (cpl.run_on) {
					if (!(event->state & GDK_CONTROL_MASK)) {
						gtk_label_set(GTK_LABEL(run_label), "   ");
						cpl.run_on = 0;
						stop_run();
					}
				}
				if (cpl.fire_on) {
					if (!(event->state & GDK_SHIFT_MASK)) {
						gtk_label_set(GTK_LABEL(fire_label), "   ");
						cpl.fire_on = 0;
						stop_fire();
					}
				}
				parse_key(event->string[0], XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(window), "key_press_event");
				break;
			case Configure_Keys:
				configure_keys(XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
				gtk_signal_emit_stop_by_name(GTK_OBJECT(window), "key_press_event");
				break;
			case Command_Mode:
				gtk_widget_grab_focus(GTK_WIDGET(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))));
			case Metaserver_Select:
				gtk_widget_grab_focus(GTK_WIDGET(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))));
				break;
			default:
				fprintf(stderr, "Unknown input state: %d\n", cpl.input_state);
			}
		}
	}
}

static gint
configure_event(GtkWidget *widget, GdkEventConfigure *event)
{
	if (pixmap) {
		gdk_pixmap_unref(pixmap);
		gdk_gc_unref(gc);
	}
	pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
	gdk_draw_rectangle(pixmap, widget->style->white_gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);
	gc = gdk_gc_new(widget->window);

#if 0
/*    if (!sdlimage) */
        {
	int x,y,count;
	GdkGC	*darkgc;

	/* this is used when drawing with GdkPixmaps.  Create another surface,
         * as well as some light/dark images
         */
	dark = gdk_pixmap_new(drawable->window, map_image_size, map_image_size, -1);
	gdk_draw_rectangle(dark, drawable->style->black_gc, TRUE, 0, 0, map_image_size, map_image_size);
	dark1 = gdk_pixmap_new(drawable->window, map_image_size, map_image_size, 1);
	dark2 = gdk_pixmap_new(drawable->window, map_image_size, map_image_size, 1);
	dark3 = gdk_pixmap_new(drawable->window, map_image_size, map_image_size, 1);

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
	gdk_gc_unref(darkgc);
    }
#endif
    return TRUE;
}

static gint
expose_event(GtkWidget *widget, GdkEventExpose *event)
{
	gdk_draw_pixmap(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmap, event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);
	return FALSE;
}

static int
get_game_display(GtkWidget * frame)
{
	GtkWidget *vbox, *hbox;
	vbox = gtk_vbox_new(FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	drawable = gtk_drawing_area_new();
	gtk_widget_set_usize(drawable, (map_image_size * mapx), (map_image_size * mapy));
	gtk_widget_set_events(drawable, GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect(GTK_OBJECT(drawable), "button_press_event", GTK_SIGNAL_FUNC(button_map_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawable), "configure_event", GTK_SIGNAL_FUNC(configure_event), NULL);
	gtk_signal_connect(GTK_OBJECT(drawable), "expose_event", GTK_SIGNAL_FUNC(expose_event), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), drawable, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);
	gtk_widget_show_all(hbox);
	gtk_widget_show_all(drawable);
	gtk_widget_show_all(frame);
	return 0;
}

static void
draw_list(itemlist * l)
{
	gint tmprow;
	item *tmp;
	animobject *tmpanim = NULL;
	animview *tmpanimview;
	char buf[MAX_BUF];
	char buffer[3][MAX_BUF];
	char *buffers[3];
	gint list;
	if (l->multi_list) {
		if (anim_inv_list) {
			g_list_foreach(anim_inv_list, (GFunc) freeanimobject, NULL);
			g_list_free(anim_inv_list);
			anim_inv_list = NULL;
		}
		for (list = 0; list < TYPE_LISTS; list++) {
			l->pos[list] = GTK_RANGE(GTK_SCROLLED_WINDOW(l->gtk_lists[list])->vscrollbar)->adjustment->value;
			gtk_clist_freeze(GTK_CLIST(l->gtk_list[list]));
			gtk_clist_clear(GTK_CLIST(l->gtk_list[list]));
		}
	} else {
		if (anim_look_list) {
			g_list_foreach(anim_look_list, (GFunc) freeanimobject, NULL);
			g_list_free(anim_look_list);
			anim_look_list = NULL;
		}
		l->pos[0] = GTK_RANGE(GTK_SCROLLED_WINDOW(l->gtk_lists[0])->vscrollbar)->adjustment->value;
		gtk_clist_freeze(GTK_CLIST(l->gtk_list[0]));
		gtk_clist_clear(GTK_CLIST(l->gtk_list[0]));
	}
	if (l->env->weight < 0 || l->show_weight == 0) {
		strcpy(buf, l->title);
		gtk_label_set(GTK_LABEL(l->label), buf);
		gtk_label_set(GTK_LABEL(l->weightlabel), " ");
		gtk_label_set(GTK_LABEL(l->maxweightlabel), " ");
		gtk_widget_draw(l->label, NULL);
		gtk_widget_draw(l->weightlabel, NULL);
		gtk_widget_draw(l->maxweightlabel, NULL);
	} else if (!l->weight_limit) {
		strcpy(buf, l->title);
		gtk_label_set(GTK_LABEL(l->label), buf);
		sprintf(buf, "%6.1f", l->env->weight);
		gtk_label_set(GTK_LABEL(l->weightlabel), buf);
		gtk_label_set(GTK_LABEL(l->maxweightlabel), " ");
		gtk_widget_draw(l->label, NULL);
		gtk_widget_draw(l->weightlabel, NULL);
		gtk_widget_draw(l->maxweightlabel, NULL);
	} else {
		strcpy(buf, l->title);
		gtk_label_set(GTK_LABEL(l->label), buf);
		sprintf(buf, "%6.1f", l->env->weight);
		gtk_label_set(GTK_LABEL(l->weightlabel), buf);
		sprintf(buf, "/ %4d", l->weight_limit / 1000);
		gtk_label_set(GTK_LABEL(l->maxweightlabel), buf);
		gtk_widget_draw(l->label, NULL);
		gtk_widget_draw(l->weightlabel, NULL);
		gtk_widget_draw(l->maxweightlabel, NULL);
	}
	for (tmp = l->env->inv; tmp; tmp = tmp->next) {
		strcpy(buffer[0], " ");
		strcpy(buffer[1], tmp->d_name);
		if (l->show_icon == 0)
			strcat(buffer[1], tmp->flags);
		if (tmp->weight < 0 || l->show_weight == 0) {
			strcpy(buffer[2], " ");
		} else {
			sprintf(buffer[2], "%6.1f", tmp->nrof * tmp->weight);
		}
		buffers[0] = buffer[0];
		buffers[1] = buffer[1];
		buffers[2] = buffer[2];
		if (l->multi_list) {
			tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[0]), buffers);
			gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[0]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
			gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[0]), tmprow, tmp);
			if (color_inv) {
				if (tmp->cursed || tmp->damned) {
					gtk_clist_set_background(GTK_CLIST(l->gtk_list[0]), tmprow, &root_color[NDI_RED]);
				}
				if (tmp->magical) {
					gtk_clist_set_background(GTK_CLIST(l->gtk_list[0]), tmprow, &root_color[NDI_BLUE]);
				}
				if ((tmp->cursed || tmp->damned) && tmp->magical) {
					gtk_clist_set_background(GTK_CLIST(l->gtk_list[0]), tmprow, &root_color[NDI_NAVY]);
				}
			}
			if (tmp->animation_id > 0 && tmp->anim_speed) {
				tmpanim = newanimobject();
				tmpanim->item = tmp;
				tmpanimview = newanimview();
				tmpanimview->row = tmprow;
				tmpanimview->list = l->gtk_list[0];
				tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				anim_inv_list = g_list_append(anim_inv_list, tmpanim);
			}
			if (tmp->applied) {
				tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[1]), buffers);
				gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[1]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
				gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[1]), tmprow, tmp);
				if (tmp->animation_id > 0 && tmp->anim_speed) {
					tmpanimview = newanimview();
					tmpanimview->row = tmprow;
					tmpanimview->list = l->gtk_list[1];
					tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				}
			}
			if (!tmp->applied) {
				tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[2]), buffers);
				gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[2]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
				gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[2]), tmprow, tmp);
				if (tmp->animation_id > 0 && tmp->anim_speed) {
					tmpanimview = newanimview();
					tmpanimview->row = tmprow;
					tmpanimview->list = l->gtk_list[2];
					tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				}
			}
			if (tmp->unpaid) {
				tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[3]), buffers);
				gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[3]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
				gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[3]), tmprow, tmp);
				if (tmp->animation_id > 0 && tmp->anim_speed) {
					tmpanimview = newanimview();
					tmpanimview->row = tmprow;
					tmpanimview->list = l->gtk_list[3];
					tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				}
			}
			if (tmp->cursed || tmp->damned) {
				tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[4]), buffers);
				gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[4]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
				gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[4]), tmprow, tmp);
				if (tmp->animation_id > 0 && tmp->anim_speed) {
					tmpanimview = newanimview();
					tmpanimview->row = tmprow;
					tmpanimview->list = l->gtk_list[4];
					tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				}
			}
			if (tmp->magical) {
				tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[5]), buffers);
				gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[5]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
				gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[5]), tmprow, tmp);
				if (tmp->animation_id > 0 && tmp->anim_speed) {
					tmpanimview = newanimview();
					tmpanimview->row = tmprow;
					tmpanimview->list = l->gtk_list[5];
					tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				}
			}
			if (!tmp->magical) {
				tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[6]), buffers);
				gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[6]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
				gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[6]), tmprow, tmp);
				if (tmp->animation_id > 0 && tmp->anim_speed) {
					tmpanimview = newanimview();
					tmpanimview->row = tmprow;
					tmpanimview->list = l->gtk_list[6];
					tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				}
			}
			if (tmp->locked) {
				tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[7]), buffers);
				gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[7]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
				gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[7]), tmprow, tmp);
				if (tmp->animation_id > 0 && tmp->anim_speed) {
					tmpanimview = newanimview();
					tmpanimview->row = tmprow;
					tmpanimview->list = l->gtk_list[7];
					tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				}
			}
			if (!tmp->locked) {
				tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[8]), buffers);
				gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[8]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
				gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[8]), tmprow, tmp);
				if (tmp->animation_id > 0 && tmp->anim_speed) {
					tmpanimview = newanimview();
					tmpanimview->row = tmprow;
					tmpanimview->list = l->gtk_list[8];
					tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				}
				if (color_inv) {
					if (tmp->cursed || tmp->damned) {
						gtk_clist_set_background(GTK_CLIST(l->gtk_list[8]), tmprow, &root_color[NDI_RED]);
					}
					if (tmp->magical) {
						gtk_clist_set_background(GTK_CLIST(l->gtk_list[8]), tmprow, &root_color[NDI_BLUE]);
					}
					if ((tmp->cursed || tmp->damned) && tmp->magical) {
						gtk_clist_set_background(GTK_CLIST(l->gtk_list[8]), tmprow, &root_color[NDI_NAVY]);
					}
				}
			}
		} else {
			tmprow = gtk_clist_append(GTK_CLIST(l->gtk_list[0]), buffers);
			gtk_clist_set_pixmap(GTK_CLIST(l->gtk_list[0]), tmprow, 0, pixmaps[facecachemap[tmp->face]].icon_image, pixmaps[facecachemap[tmp->face]].icon_mask);
			gtk_clist_set_row_data(GTK_CLIST(l->gtk_list[0]), tmprow, tmp);
			if (tmp->animation_id > 0 && tmp->anim_speed) {
				tmpanim = newanimobject();
				tmpanim->item = tmp;
				tmpanimview = newanimview();
				tmpanimview->row = tmprow;
				tmpanimview->list = l->gtk_list[0];
				tmpanim->view = g_list_append(tmpanim->view, tmpanimview);
				anim_look_list = g_list_append(anim_look_list, tmpanim);
			}
			if (color_inv) {
				if (tmp->cursed || tmp->damned) {
					gtk_clist_set_background(GTK_CLIST(l->gtk_list[0]), tmprow, &root_color[NDI_RED]);
				}
				if (tmp->magical) {
					gtk_clist_set_background(GTK_CLIST(l->gtk_list[0]), tmprow, &root_color[NDI_BLUE]);
				}
				if ((tmp->cursed || tmp->damned) && tmp->magical) {
					gtk_clist_set_background(GTK_CLIST(l->gtk_list[0]), tmprow, &root_color[NDI_NAVY]);
				}
			}
		}
	}
	if (l->multi_list) {
		for (list = 0; list < TYPE_LISTS; list++) {
			gtk_adjustment_set_value(GTK_ADJUSTMENT(GTK_RANGE(GTK_SCROLLED_WINDOW(l->gtk_lists[list])->vscrollbar)->adjustment), l->pos[list]);
			gtk_clist_thaw(GTK_CLIST(l->gtk_list[list]));
		}
	} else {
		gtk_adjustment_set_value(GTK_ADJUSTMENT(GTK_RANGE(GTK_SCROLLED_WINDOW(l->gtk_lists[0])->vscrollbar)->adjustment), l->pos[0]);
		gtk_clist_thaw(GTK_CLIST(l->gtk_list[0]));
	}
}

static void
enter_callback(GtkWidget * widget, GtkWidget * entry)
{
	gchar *entry_text;
	if (nopopups)
		gtk_entry_set_visibility(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))), TRUE);
	entry_text = gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))));
	if (cpl.input_state == Metaserver_Select) {
		cpl.input_state = Playing;
		strcpy(cpl.input_text, entry_text);
	} else if (cpl.input_state == Reply_One || cpl.input_state == Reply_Many) {
		cpl.input_state = Playing;
		strcpy(cpl.input_text, entry_text);
		if (cpl.input_state == Reply_One)
			cpl.input_text[1] = 0;
		send_reply(cpl.input_text);
	} else {
		cpl.input_state = Playing;
		if (entry_text[0] != 0) {
			strncpy(history[cur_history_position], entry_text, MAX_COMMAND_LEN);
			history[cur_history_position][MAX_COMMAND_LEN-1] = 0;
			cur_history_position++;
			cur_history_position %= MAX_HISTORY;
			scroll_history_position = cur_history_position;
			extended_command(entry_text);
		}
	}
	gtk_entry_set_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))), "");
	gtk_widget_grab_focus(GTK_WIDGET(gtkwin_info_text));
}

static gboolean
info_text_button_press_event(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
	GtkAdjustment *vadj;
	gboolean shifted;
	gfloat v_value;
	vadj = GTK_TEXT(widget)->vadj;
	v_value = vadj->value;
	shifted = (event->state & GDK_SHIFT_MASK) != 0;
	switch (event->button) {
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
	v_value = CLAMP(v_value, vadj->lower, vadj->upper - vadj->page_size);
	gtk_adjustment_set_value(vadj, v_value);
	return TRUE;
}

static int
get_message_display(GtkWidget * frame)
{
	GtkWidget *box1;
	GtkWidget *box2;
	GtkWidget *tablet;
	GtkWidget *vscrollbar;
	FILE *infile;
	GtkWidget *vpane = NULL;
	box1 = gtk_vbox_new(FALSE, 0);
	if (splitinfo) {
		vpane = gtk_vpaned_new();
		gtk_container_add(GTK_CONTAINER(frame), vpane);
		gtk_widget_show(vpane);
		gtk_paned_add2(GTK_PANED(vpane), box1);
	} else {
		gtk_container_add(GTK_CONTAINER(frame), box1);
	}
	gtk_widget_show(box1);
	box2 = gtk_vbox_new(FALSE, 3);
	gtk_container_border_width(GTK_CONTAINER(box2), 3);
	gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
	gtk_widget_show(box2);
	tablet = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacing(GTK_TABLE(tablet), 0, 2);
	gtk_table_set_col_spacing(GTK_TABLE(tablet), 0, 2);
	gtk_box_pack_start(GTK_BOX(box2), tablet, TRUE, TRUE, 0);
	gtk_widget_show(tablet);
	text_hadj = gtk_adjustment_new(1, 0, 1, 0.01, 0.1, 40);
	text_vadj = gtk_adjustment_new(1, 0, 1, 0.01, 0.1, 40);
	gtkwin_info_text = gtk_text_new(GTK_ADJUSTMENT(text_hadj), GTK_ADJUSTMENT(text_vadj));
	gtk_text_set_editable(GTK_TEXT(gtkwin_info_text), FALSE);
	gtk_table_attach(GTK_TABLE(tablet), gtkwin_info_text, 0, 1, 0, 1, GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
	gtk_widget_show(gtkwin_info_text);
	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(gtkwin_info_text)->vadj);
	gtk_table_attach(GTK_TABLE(tablet), vscrollbar, 1, 2, 0, 1, GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
	gtk_widget_show(vscrollbar);
	gtk_signal_connect(GTK_OBJECT(gtkwin_info_text), "button_press_event", GTK_SIGNAL_FUNC(info_text_button_press_event), vscrollbar);
	gtk_text_freeze(GTK_TEXT(gtkwin_info_text));
	gtk_widget_realize(gtkwin_info_text);
	if (splitinfo) {
		box1 = gtk_vbox_new(FALSE, 0);
		gtk_widget_show(box1);
		gtk_paned_add1(GTK_PANED(vpane), box1);
		tablet = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacing(GTK_TABLE(tablet), 0, 2);
		gtk_table_set_col_spacing(GTK_TABLE(tablet), 0, 2);
		gtk_box_pack_start(GTK_BOX(box1), tablet, TRUE, TRUE, 0);
		gtk_widget_show(tablet);
		text_hadj2 = gtk_adjustment_new(1, 0, 1, 0.01, 0.1, 40);
		text_vadj2 = gtk_adjustment_new(1, 0, 1, 0.01, 0.1, 40);
		gtkwin_info_text2 = gtk_text_new(GTK_ADJUSTMENT(text_hadj2), GTK_ADJUSTMENT(text_vadj2));
		gtk_text_set_editable(GTK_TEXT(gtkwin_info_text2), FALSE);
		gtk_table_attach(GTK_TABLE(tablet), gtkwin_info_text2, 0, 1, 0, 1, GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
		gtk_widget_show(gtkwin_info_text2);
		vscrollbar = gtk_vscrollbar_new(GTK_TEXT(gtkwin_info_text2)->vadj);
		gtk_table_attach(GTK_TABLE(tablet), vscrollbar, 1, 2, 0, 1, GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
		gtk_widget_show(vscrollbar);
		gtk_signal_connect(GTK_OBJECT(gtkwin_info_text2), "button_press_event", GTK_SIGNAL_FUNC(info_text_button_press_event), vscrollbar);
		gtk_widget_realize(gtkwin_info_text2);
	}
	infile = fopen("Welcome", "r");
	if (infile) {
		char buffer[1024];
		int nchars;
		while (1) {
			nchars = fread(buffer, 1, 1024, infile);
			gtk_text_insert(GTK_TEXT(gtkwin_info_text), NULL, NULL, NULL, buffer, nchars);
			if (nchars < 1024)
				break;
		}
		fclose(infile);
	}
	gtk_text_thaw(GTK_TEXT(gtkwin_info_text));
	entrytext = gnome_entry_new("CommandHistory");
	gtk_signal_connect(GTK_OBJECT(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))), "activate", GTK_SIGNAL_FUNC(enter_callback), entrytext);
	gtk_box_pack_start(GTK_BOX(box2), entrytext, FALSE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext)), GTK_CAN_DEFAULT);
	gtk_widget_grab_default(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext)));
	gtk_widget_show(entrytext);
	return 0;
}

static void
sendstr(char *sendstr)
{
	gtk_widget_destroy(dialog_window);
	send_reply(sendstr);
	cpl.input_state = Playing;
}

static void
dialog_callback(GtkWidget * dialog)
{
	gchar *dialog_text;
	dialog_text = gtk_entry_get_text(GTK_ENTRY(dialogtext));
	gtk_widget_destroy(dialog_window);
	send_reply(dialog_text);
	cpl.input_state = Playing;
}

void
draw_prompt(const char *str)
{
	GtkWidget *dbox;
	GtkWidget *hbox;
	GtkWidget *dialoglabel;
	GtkWidget *yesbutton, *nobutton;
	GtkWidget *strbutton, *dexbutton, *conbutton, *intbutton, *wisbutton, *powbutton, *chabutton;
	gint found = FALSE;
	if (nopopups) {
		draw_info(str, NDI_BLACK);
	} else {
		dialog_window = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_set_policy(GTK_WINDOW(dialog_window), TRUE, TRUE, FALSE);
		gtk_window_set_title(GTK_WINDOW(dialog_window), "Dialog");
		gtk_window_set_transient_for(GTK_WINDOW(dialog_window), GTK_WINDOW(gtkwin_root));
		dbox = gtk_vbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(dialog_window), dbox);
		while (!found) {
			if (!strcmp(str, ":")) {
				if (!strcmp(last_str, "What is your name?")) {
					dialoglabel = gtk_label_new("What is your name?");
					gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
					gtk_widget_show(dialoglabel);
					hbox = gtk_hbox_new(FALSE, 6);
					dialogtext = gtk_entry_new();
					gtk_signal_connect(GTK_OBJECT(dialogtext), "activate", GTK_SIGNAL_FUNC(dialog_callback), dialog_window);
					gtk_box_pack_start(GTK_BOX(hbox), dialogtext, TRUE, TRUE, 6);
					gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
					gtk_widget_show(hbox);
					gtk_widget_show(dialogtext);
					gtk_widget_grab_focus(dialogtext);
					found = TRUE;
					continue;
				}
				if (!strcmp(last_str, "What is your password?")) {
					dialoglabel = gtk_label_new("What is your password?");
					gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
					gtk_widget_show(dialoglabel);
					hbox = gtk_hbox_new(FALSE, 6);
					dialogtext = gtk_entry_new();
					gtk_entry_set_visibility(GTK_ENTRY(dialogtext), FALSE);
					gtk_signal_connect(GTK_OBJECT(dialogtext), "activate", GTK_SIGNAL_FUNC(dialog_callback), dialog_window);
					gtk_box_pack_start(GTK_BOX(hbox), dialogtext, TRUE, TRUE, 6);
					gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
					gtk_widget_show(hbox);
					gtk_widget_show(dialogtext);
					gtk_widget_grab_focus(dialogtext);
					found = TRUE;
					continue;;
				}
				if (!strcmp(last_str, "Please type your password again.")) {
					dialoglabel = gtk_label_new("Please type your password again.");
					gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
					gtk_widget_show(dialoglabel);
					hbox = gtk_hbox_new(FALSE, 6);
					dialogtext = gtk_entry_new();
					gtk_entry_set_visibility(GTK_ENTRY(dialogtext), FALSE);
					gtk_signal_connect(GTK_OBJECT(dialogtext), "activate", GTK_SIGNAL_FUNC(dialog_callback), dialog_window);
					gtk_box_pack_start(GTK_BOX(hbox), dialogtext, TRUE, TRUE, 6);
					gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
					gtk_widget_show(hbox);
					gtk_widget_show(dialogtext);
					gtk_widget_grab_focus(dialogtext);
					found = TRUE;
					continue;
				}
			}
			if (!strcmp(last_str, "[1-7] [1-7] to swap stats.")
				|| !strncmp(last_str, "Str d", 5)
				|| !strncmp(last_str, "Dex d", 5)
				|| !strncmp(last_str, "Con d", 5)
				|| !strncmp(last_str, "Int d", 5)
				|| !strncmp(last_str, "Wis d", 5)
				|| !strncmp(last_str, "Pow d", 5)
				|| !strncmp(last_str, "Cha d", 5)) {
				dialoglabel = gtk_label_new("Roll again or exchange ability.");
				gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
				gtk_widget_show(dialoglabel);
				hbox = gtk_hbox_new(TRUE, 2);
				strbutton = gtk_button_new_with_label("Str");
				gtk_box_pack_start(GTK_BOX(hbox), strbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(strbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("1"));
				dexbutton = gtk_button_new_with_label("Dex");
				gtk_box_pack_start(GTK_BOX(hbox), dexbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(dexbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("2"));
				conbutton = gtk_button_new_with_label("Con");
				gtk_box_pack_start(GTK_BOX(hbox), conbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(conbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("3"));
				intbutton = gtk_button_new_with_label("Int");
				gtk_box_pack_start(GTK_BOX(hbox), intbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(intbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("4"));
				wisbutton = gtk_button_new_with_label("Wis");
				gtk_box_pack_start(GTK_BOX(hbox), wisbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(wisbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("5"));
				powbutton = gtk_button_new_with_label("Pow");
				gtk_box_pack_start(GTK_BOX(hbox), powbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(powbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("6"));
				chabutton = gtk_button_new_with_label("Cha");
				gtk_box_pack_start(GTK_BOX(hbox), chabutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(chabutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("7"));
				gtk_widget_show(strbutton);
				gtk_widget_show(dexbutton);
				gtk_widget_show(conbutton);
				gtk_widget_show(intbutton);
				gtk_widget_show(wisbutton);
				gtk_widget_show(powbutton);
				gtk_widget_show(chabutton);
				gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
				gtk_widget_show(hbox);
				hbox = gtk_hbox_new(FALSE, 6);
				yesbutton = gtk_button_new_with_label("Roll again");
				gtk_box_pack_start(GTK_BOX(hbox), yesbutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(yesbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("y"));
				nobutton = gtk_button_new_with_label("Keep this");
				gtk_box_pack_start(GTK_BOX(hbox), nobutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(nobutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("n"));
				gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
				gtk_widget_show(yesbutton);
				gtk_widget_show(nobutton);
				gtk_widget_show(hbox);
				found = TRUE;
				continue;
			}
			if (!strncmp(last_str, "Str -", 5) || !strncmp(last_str, "Dex -", 5)
				|| !strncmp(last_str, "Con -", 5)
				|| !strncmp(last_str, "Int -", 5)
				|| !strncmp(last_str, "Wis -", 5)
				|| !strncmp(last_str, "Pow -", 5)
				|| !strncmp(last_str, "Cha -", 5)) {
				dialoglabel = gtk_label_new("Exchange with which ability?");
				gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
				gtk_widget_show(dialoglabel);
				hbox = gtk_hbox_new(TRUE, 2);
				strbutton = gtk_button_new_with_label("Str");
				gtk_box_pack_start(GTK_BOX(hbox), strbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(strbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("1"));
				dexbutton = gtk_button_new_with_label("Dex");
				gtk_box_pack_start(GTK_BOX(hbox), dexbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(dexbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("2"));
				conbutton = gtk_button_new_with_label("Con");
				gtk_box_pack_start(GTK_BOX(hbox), conbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(conbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("3"));
				intbutton = gtk_button_new_with_label("Int");
				gtk_box_pack_start(GTK_BOX(hbox), intbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(intbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("4"));
				wisbutton = gtk_button_new_with_label("Wis");
				gtk_box_pack_start(GTK_BOX(hbox), wisbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(wisbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("5"));
				powbutton = gtk_button_new_with_label("Pow");
				gtk_box_pack_start(GTK_BOX(hbox), powbutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(powbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("6"));
				chabutton = gtk_button_new_with_label("Cha");
				gtk_box_pack_start(GTK_BOX(hbox), chabutton, TRUE, TRUE, 1);
				gtk_signal_connect_object(GTK_OBJECT(chabutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("7"));
				gtk_widget_show(strbutton);
				gtk_widget_show(dexbutton);
				gtk_widget_show(conbutton);
				gtk_widget_show(intbutton);
				gtk_widget_show(wisbutton);
				gtk_widget_show(powbutton);
				gtk_widget_show(chabutton);
				gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
				gtk_widget_show(hbox);
				found = TRUE;
				continue;
			}
			if (!strncmp(last_str, "Press `d'", 9)) {
				dialoglabel = gtk_label_new("Choose a character.");
				gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
				gtk_widget_show(dialoglabel);
				hbox = gtk_hbox_new(FALSE, 6);
				yesbutton = gtk_button_new_with_label("Show next");
				gtk_box_pack_start(GTK_BOX(hbox), yesbutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(yesbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER(" "));
				nobutton = gtk_button_new_with_label("Keep this");
				gtk_box_pack_start(GTK_BOX(hbox), nobutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(nobutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("d"));
				gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
				gtk_widget_show(yesbutton);
				gtk_widget_show(nobutton);
				gtk_widget_show(hbox);
				found = TRUE;
				continue;
			}
			if (!strncmp(str, "Do you want to play", 18)) {
				GtkWidget *quitbutton;
				dialoglabel = gtk_label_new("Do you want to play again?");
				gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
				gtk_widget_show(dialoglabel);
				hbox = gtk_hbox_new(FALSE, 6);
				yesbutton = gtk_button_new_with_label("Play again");
				gtk_box_pack_start(GTK_BOX(hbox), yesbutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(yesbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("a"));
				nobutton = gtk_button_new_with_label("Quit Server");
				gtk_box_pack_start(GTK_BOX(hbox), nobutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(nobutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("q"));
				quitbutton = gtk_button_new_with_label("Quit Client");
				gtk_box_pack_start(GTK_BOX(hbox), quitbutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(quitbutton), "clicked", GTK_SIGNAL_FUNC(gnome_client_quit), NULL);
				gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
				gtk_widget_show(yesbutton);
				gtk_widget_show(nobutton);
				gtk_widget_show(quitbutton);
				gtk_widget_show(hbox);
				found = TRUE;
				continue;
			}
			if (!strncmp(str, "Are you sure you want", 21)) {
				dialoglabel = gtk_label_new("Are you sure you want to quit?");
				gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
				gtk_widget_show(dialoglabel);
				hbox = gtk_hbox_new(FALSE, 6);
				yesbutton = gtk_button_new_with_label("Yes, quit");
				gtk_box_pack_start(GTK_BOX(hbox), yesbutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(yesbutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("y"));
				nobutton = gtk_button_new_with_label("Don't quit");
				gtk_box_pack_start(GTK_BOX(hbox), nobutton, TRUE, TRUE, 6);
				gtk_signal_connect_object(GTK_OBJECT(nobutton), "clicked", GTK_SIGNAL_FUNC(sendstr), GINT_TO_POINTER("n"));
				gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
				gtk_widget_show(yesbutton);
				gtk_widget_show(nobutton);
				gtk_widget_show(hbox);
				found = TRUE;
				continue;
			}
			if (!found) {
				dialoglabel = gtk_label_new(str);
				gtk_box_pack_start(GTK_BOX(dbox), dialoglabel, FALSE, TRUE, 6);
				gtk_widget_show(dialoglabel);
				hbox = gtk_hbox_new(FALSE, 6);
				dialogtext = gtk_entry_new();
				gtk_signal_connect(GTK_OBJECT(dialogtext), "activate", GTK_SIGNAL_FUNC(dialog_callback), dialog_window);
				gtk_box_pack_start(GTK_BOX(hbox), dialogtext, TRUE, TRUE, 6);
				gtk_box_pack_start(GTK_BOX(dbox), hbox, FALSE, TRUE, 6);
				gtk_widget_show(hbox);
				gtk_widget_show(dialogtext);
				gtk_widget_grab_focus(dialogtext);
				found = TRUE;
				continue;
			}
		}
		gtk_widget_show(dbox);
		gtk_widget_show(dialog_window);
	}
}

void
cast_diawin_click_cb(GtkWidget *widget, gint button_number)
{
	char *text;
	switch(button_number) {
	case 0:
		gtk_clist_get_text(GTK_CLIST(castlist), (gint)gtk_object_get_user_data(GTK_OBJECT(castlist)), 2, &text);
		extended_command(g_strdup_printf("cast %s", text));
		gnome_dialog_close(GNOME_DIALOG(diawin));
		diawin = NULL;
		break;
	case 1:
		gtk_clist_get_text(GTK_CLIST(castlist), (gint)gtk_object_get_user_data(GTK_OBJECT(castlist)), 2, &text);
		extended_command(g_strdup_printf("cast %s", text));
		break;
	case 2:
		gnome_dialog_close(GNOME_DIALOG(diawin));
		diawin = NULL;
		break;
	}
}

void
cast_diawin_select_cb(GtkWidget *widget, gint row, gint column)
{
	gnome_dialog_set_sensitive(GNOME_DIALOG(diawin), 0, TRUE);
	gnome_dialog_set_sensitive(GNOME_DIALOG(diawin), 1, TRUE);
	gtk_object_set_user_data(GTK_OBJECT(castlist), (gpointer)row);
}

void
cast_diawin_unselect_cb(GtkWidget *widget, gint row, gint column)
{
	gnome_dialog_set_sensitive(GNOME_DIALOG(diawin), 0, FALSE);
	gnome_dialog_set_sensitive(GNOME_DIALOG(diawin), 1, FALSE);
	gtk_object_set_user_data(GTK_OBJECT(castlist), (gpointer)NULL);
}

void
draw_info(const char *str, int color)
{
	int ncolor = color;
	if (cast_menu_item_selected == TRUE) {
		char *tmp = g_strdup(str), *listheaders[4] = {"Mana", "Level", "Spell Name", NULL};
		fprintf(stderr, "Cast dialog string recieved: %s\n--------\n", tmp);
		if (!strcmp(tmp, "Cast what spell?  Choose one of:") && diawin == NULL) {
			GtkWidget *castwin = gtk_scrolled_window_new(0, 0);
			diawin = gnome_dialog_new("Cast what?", GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_APPLY, GNOME_STOCK_BUTTON_CLOSE, NULL);
			gnome_dialog_set_sensitive(GNOME_DIALOG(diawin), 0, FALSE);
			gnome_dialog_set_sensitive(GNOME_DIALOG(diawin), 1, FALSE);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(castwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
			castlist = gtk_clist_new_with_titles(3, listheaders);
			gtk_widget_set_usize(castlist, 200, 300);
			gtk_object_set_user_data(GTK_OBJECT(castlist), (gpointer)NULL);
			gtk_signal_connect(GTK_OBJECT(diawin), "clicked", GTK_SIGNAL_FUNC(cast_diawin_click_cb), NULL);
			gtk_signal_connect(GTK_OBJECT(castlist), "select_row", GTK_SIGNAL_FUNC(cast_diawin_select_cb), NULL);
			gtk_signal_connect(GTK_OBJECT(castlist), "unselect_row", GTK_SIGNAL_FUNC(cast_diawin_unselect_cb), NULL);
			gtk_container_add(GTK_CONTAINER(castwin), castlist);
			gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(diawin)->vbox), castwin, TRUE, TRUE, 0);
			gtk_widget_show_all(diawin);
			return;
		}
		else if (!strcmp(tmp, "Mage spells"))
			return;
		else if (!strcmp(tmp, "Priest spells"))
			return;
		else if (!strcmp(tmp, ""))
			return;
		else if (!strcmp(tmp, "[ sp] [lev] spell name"))
			return;
		else if (tmp[0] == '[' && tmp[4] == ']' && tmp[6] == '[' && tmp[10] == ']') {
			char *tmps[3];
			tmp[5] = 0;
			tmp[11] = 0;
			tmps[0] = tmp;
			tmps[1] = tmp + 6;
			tmps[2] = tmp + 12;
			gtk_clist_append(GTK_CLIST(castlist), tmps);
			return;
		} else {
			cast_menu_item_selected = FALSE;
		}
	}
	if (ncolor == NDI_WHITE) {
		ncolor = NDI_BLACK;
	}
	strcpy(last_str, str);
	if (splitinfo && color != NDI_BLACK) {
		if (!draw_info_freeze2) {
			gtk_text_freeze(GTK_TEXT(gtkwin_info_text2));
			draw_info_freeze2 = TRUE;
		}
		gtk_text_insert(GTK_TEXT(gtkwin_info_text2), NULL, &root_color[ncolor], NULL, str, -1);
		gtk_text_insert(GTK_TEXT(gtkwin_info_text2), NULL, &root_color[ncolor], NULL, "\n", -1);
	} else {
		if (!draw_info_freeze1) {
			gtk_text_freeze(GTK_TEXT(gtkwin_info_text));
			draw_info_freeze1 = TRUE;
		}
		gtk_text_insert(GTK_TEXT(gtkwin_info_text), NULL, (ncolor == NDI_BLACK ? 0 : &root_color[ncolor]), NULL, str, -1);
		gtk_text_insert(GTK_TEXT(gtkwin_info_text), NULL, (ncolor == NDI_BLACK ? 0 : &root_color[ncolor]), NULL, "\n", -1);
	}
}

void
draw_color_info(int colr, const char *buf)
{
	if (color_text) {
		draw_info(buf, colr);
	} else {
		draw_info("==========================================", NDI_BLACK);
		draw_info(buf, NDI_BLACK);
		draw_info("==========================================", NDI_BLACK);
	}
}

static int
get_stats_display(GtkWidget * frame)
{
	GtkWidget *stats_vbox;
	GtkWidget *stats_box_1;
	GtkWidget *stats_box_2;
	GtkWidget *stats_box_4;
	GtkWidget *stats_box_5;
	GtkWidget *stats_box_6;
	GtkWidget *stats_box_7;
	GtkWidget *table;
	int x, y, i;
	char buf[2048];
	stats_vbox = gtk_vbox_new(FALSE, 0);
	stats_box_1 = gtk_hbox_new(FALSE, 0);
	statwindow.playername = gtk_label_new("Player: ");
	gtk_box_pack_start(GTK_BOX(stats_box_1), statwindow.playername, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.playername);
	gtk_box_pack_start(GTK_BOX(stats_vbox), stats_box_1, FALSE, FALSE, 0);
	gtk_widget_show(stats_box_1);
	stats_box_2 = gtk_hbox_new(FALSE, 0);
	statwindow.score = gtk_label_new("Score: 0");
	gtk_box_pack_start(GTK_BOX(stats_box_2), statwindow.score, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.score);
	statwindow.level = gtk_label_new("Level: 0");
	gtk_box_pack_start(GTK_BOX(stats_box_2), statwindow.level, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.level);
	gtk_box_pack_start(GTK_BOX(stats_vbox), stats_box_2, FALSE, FALSE, 0);
	gtk_widget_show(stats_box_2);
	stats_box_4 = gtk_hbox_new(FALSE, 0);
	statwindow.Str = gtk_label_new("S 0");
	gtk_box_pack_start(GTK_BOX(stats_box_4), statwindow.Str, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.Str);
	statwindow.Dex = gtk_label_new("D 0");
	gtk_box_pack_start(GTK_BOX(stats_box_4), statwindow.Dex, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.Dex);
	statwindow.Con = gtk_label_new("Co 0");
	gtk_box_pack_start(GTK_BOX(stats_box_4), statwindow.Con, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.Con);
	statwindow.Int = gtk_label_new("I 0");
	gtk_box_pack_start(GTK_BOX(stats_box_4), statwindow.Int, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.Int);
	statwindow.Wis = gtk_label_new("W 0");
	gtk_box_pack_start(GTK_BOX(stats_box_4), statwindow.Wis, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.Wis);
	statwindow.Pow = gtk_label_new("P 0");
	gtk_box_pack_start(GTK_BOX(stats_box_4), statwindow.Pow, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.Pow);
	statwindow.Cha = gtk_label_new("Ch 0");
	gtk_box_pack_start(GTK_BOX(stats_box_4), statwindow.Cha, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.Cha);
	gtk_box_pack_start(GTK_BOX(stats_vbox), stats_box_4, FALSE, FALSE, 0);
	gtk_widget_show(stats_box_4);
	stats_box_5 = gtk_hbox_new(FALSE, 0);
	statwindow.wc = gtk_label_new("Wc: 0");
	gtk_box_pack_start(GTK_BOX(stats_box_5), statwindow.wc, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.wc);
	statwindow.dam = gtk_label_new("Dam: 0");
	gtk_box_pack_start(GTK_BOX(stats_box_5), statwindow.dam, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.dam);
	statwindow.ac = gtk_label_new("Ac: 0");
	gtk_box_pack_start(GTK_BOX(stats_box_5), statwindow.ac, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.ac);
	statwindow.armor = gtk_label_new("Armor: 0");
	gtk_box_pack_start(GTK_BOX(stats_box_5), statwindow.armor, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.armor);
	gtk_box_pack_start(GTK_BOX(stats_vbox), stats_box_5, FALSE, FALSE, 0);
	gtk_widget_show(stats_box_5);
	stats_box_6 = gtk_hbox_new(FALSE, 0);
	statwindow.speed = gtk_label_new("Speed: 0");
	gtk_box_pack_start(GTK_BOX(stats_box_6), statwindow.speed, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.speed);
	gtk_box_pack_start(GTK_BOX(stats_vbox), stats_box_6, FALSE, FALSE, 0);
	gtk_widget_show(stats_box_6);
	stats_box_7 = gtk_hbox_new(FALSE, 0);
	statwindow.skill = gtk_label_new("Skill: 0");
	gtk_box_pack_start(GTK_BOX(stats_box_7), statwindow.skill, FALSE, FALSE, 5);
	gtk_widget_show(statwindow.skill);
	gtk_box_pack_start(GTK_BOX(stats_vbox), stats_box_7, FALSE, FALSE, 0);
	gtk_widget_show(stats_box_7);
    table = gtk_table_new (2, 3, TRUE);
    x = 0;
    y = 0;
	for (i = 0; i < MAX_SKILL; i++) {
		sprintf(buf, "%s: %d (%d)", skill_names[i], 0, 0);
		statwindow.skill_exp[i] = gtk_label_new(buf);
		gtk_table_attach(GTK_TABLE(table), statwindow.skill_exp[i], x, x + 1, y, y + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
		x++;
		if (x == 2) {
			x = 0;
			y++;
		}
		gtk_widget_show(statwindow.skill_exp[i]);
	}
	gtk_box_pack_start(GTK_BOX(stats_vbox), table, TRUE, TRUE, 0);
	gtk_widget_show(table);
	gtk_container_add(GTK_CONTAINER(frame), stats_vbox);
	gtk_widget_show(stats_vbox);
	return 0;
}

void
draw_stats(int redraw)
{
	float weap_sp;
	char buff[MAX_BUF];
	static Stats last_stats;
	static char last_name[MAX_BUF] = "", last_range[MAX_BUF] = "";
	static int init_before=0;
	int i;
	if (!init_before) {
		init_before=1;
		memset(&last_stats, 0, sizeof(Stats));
	}
	if (updatelock < 25) {
		updatelock++;
		if (strcmp(cpl.title, last_name) || redraw) {
			strcpy(last_name, cpl.title);
			strcpy(buff, cpl.title);
			gtk_label_set(GTK_LABEL(statwindow.playername), cpl.title);
			gtk_widget_draw(statwindow.playername, NULL);
		}
		if (redraw || cpl.stats.exp != last_stats.exp) {
			last_stats.exp = cpl.stats.exp;
			sprintf(buff, "Score: %5d", cpl.stats.exp);
			gtk_label_set(GTK_LABEL(statwindow.score), buff);
			gtk_widget_draw(statwindow.score, NULL);
		}
		if (redraw || cpl.stats.level != last_stats.level) {
			last_stats.level = cpl.stats.level;
			sprintf(buff, "Level: %d", cpl.stats.level);
			gtk_label_set(GTK_LABEL(statwindow.level), buff);
			gtk_widget_draw(statwindow.level, NULL);
		}
		if (redraw || cpl.stats.hp != last_stats.hp || cpl.stats.maxhp != last_stats.maxhp) {
			last_stats.hp = cpl.stats.hp;
			last_stats.maxhp = cpl.stats.maxhp;
			sprintf(buff, "Hp: %d/%d", cpl.stats.hp, cpl.stats.maxhp);
			gtk_label_set(GTK_LABEL(statwindow.hp), buff);
			gtk_widget_draw(statwindow.hp, NULL);
		}
		if (redraw || cpl.stats.sp != last_stats.sp || cpl.stats.maxsp != last_stats.maxsp) {
			last_stats.sp = cpl.stats.sp;
			last_stats.maxsp = cpl.stats.maxsp;
			sprintf(buff, "Sp: %d/%d", cpl.stats.sp, cpl.stats.maxsp);
			gtk_label_set(GTK_LABEL(statwindow.sp), buff);
			gtk_widget_draw(statwindow.sp, NULL);
		}
		if (redraw || cpl.stats.grace != last_stats.grace || cpl.stats.maxgrace != last_stats.maxgrace) {
			last_stats.grace = cpl.stats.grace;
			last_stats.maxgrace = cpl.stats.maxgrace;
			sprintf(buff, "Gr: %d/%d", cpl.stats.grace, cpl.stats.maxgrace);
			gtk_label_set(GTK_LABEL(statwindow.gr), buff);
			gtk_widget_draw(statwindow.gr, NULL);
		}
		if (redraw || cpl.stats.Str != last_stats.Str) {
			last_stats.Str = cpl.stats.Str;
			sprintf(buff, "S%2d", cpl.stats.Str);
			gtk_label_set(GTK_LABEL(statwindow.Str), buff);
			gtk_widget_draw(statwindow.Str, NULL);
		}
		if (redraw || cpl.stats.Dex != last_stats.Dex) {
			last_stats.Dex = cpl.stats.Dex;
			sprintf(buff, "D%2d", cpl.stats.Dex);
			gtk_label_set(GTK_LABEL(statwindow.Dex), buff);
			gtk_widget_draw(statwindow.Dex, NULL);
		}
		if (redraw || cpl.stats.Con != last_stats.Con) {
			last_stats.Con = cpl.stats.Con;
			sprintf(buff, "Co%2d", cpl.stats.Con);
			gtk_label_set(GTK_LABEL(statwindow.Con), buff);
			gtk_widget_draw(statwindow.Con, NULL);
		}
		if (redraw || cpl.stats.Int != last_stats.Int) {
			last_stats.Int = cpl.stats.Int;
			sprintf(buff, "I%2d", cpl.stats.Int);
			gtk_label_set(GTK_LABEL(statwindow.Int), buff);
			gtk_widget_draw(statwindow.Int, NULL);
		}
		if (redraw || cpl.stats.Wis != last_stats.Wis) {
			last_stats.Wis = cpl.stats.Wis;
			sprintf(buff, "W%2d", cpl.stats.Wis);
			gtk_label_set(GTK_LABEL(statwindow.Wis), buff);
			gtk_widget_draw(statwindow.Wis, NULL);
		}
		if (redraw || cpl.stats.Pow != last_stats.Pow) {
			last_stats.Pow = cpl.stats.Pow;
			sprintf(buff, "P%2d", cpl.stats.Pow);
			gtk_label_set(GTK_LABEL(statwindow.Pow), buff);
			gtk_widget_draw(statwindow.Pow, NULL);
		}
		if (redraw || cpl.stats.Cha != last_stats.Cha) {
			last_stats.Cha = cpl.stats.Cha;
			sprintf(buff, "Ch%2d", cpl.stats.Cha);
			gtk_label_set(GTK_LABEL(statwindow.Cha), buff);
			gtk_widget_draw(statwindow.Cha, NULL);
		}
		if (redraw || cpl.stats.wc != last_stats.wc) {
			last_stats.wc = cpl.stats.wc;
			sprintf(buff, "Wc%3d", cpl.stats.wc);
			gtk_label_set(GTK_LABEL(statwindow.wc), buff);
			gtk_widget_draw(statwindow.wc, NULL);
		}
		if (redraw || cpl.stats.dam != last_stats.dam) {
			last_stats.dam = cpl.stats.dam;
			sprintf(buff, "Dam%3d", cpl.stats.dam);
			gtk_label_set(GTK_LABEL(statwindow.dam), buff);
			gtk_widget_draw(statwindow.dam, NULL);
		}
		if (redraw || cpl.stats.ac != last_stats.ac) {
			last_stats.ac = cpl.stats.ac;
			sprintf(buff, "Ac%3d", cpl.stats.ac);
			gtk_label_set(GTK_LABEL(statwindow.ac), buff);
			gtk_widget_draw(statwindow.ac, NULL);
		}
		if (redraw || cpl.stats.resists[0] != last_stats.resists[0]) {
			last_stats.resists[0] = cpl.stats.resists[0];
			sprintf(buff, "Arm%3d", cpl.stats.resists[0]);
			gtk_label_set(GTK_LABEL(statwindow.armor), buff);
			gtk_widget_draw(statwindow.armor, NULL);
		}
		if (redraw || cpl.stats.speed != last_stats.speed || cpl.stats.weapon_sp != last_stats.weapon_sp) {
			last_stats.speed = cpl.stats.speed;
			last_stats.weapon_sp = cpl.stats.weapon_sp;
			weap_sp = (float)cpl.stats.speed / ((float)cpl.stats.weapon_sp);
			sprintf(buff, "Speed: %3.2f (%1.2f)", (float)cpl.stats.speed / FLOAT_MULTF, weap_sp);
			gtk_label_set(GTK_LABEL(statwindow.speed), buff);
			gtk_widget_draw(statwindow.speed, NULL);
		}
		if (redraw || cpl.stats.food != last_stats.food) {
			last_stats.food = cpl.stats.food;
			sprintf(buff, "Food: %3d", cpl.stats.food);
			gtk_label_set(GTK_LABEL(statwindow.food), buff);
			gtk_widget_draw(statwindow.food, NULL);
		}
		if (redraw || strcmp(cpl.range, last_range)) {
			strcpy(last_range, cpl.range);
			sprintf(buff, cpl.range);
			gtk_label_set(GTK_LABEL(statwindow.skill), buff);
			gtk_widget_draw(statwindow.skill, NULL);
		}
		for (i = 0; i < MAX_SKILL; i++) {
			if (redraw || cpl.stats.skill_level[i] != last_stats.skill_level[i] || cpl.stats.skill_exp[i] != last_stats.skill_exp[i]) {
				sprintf(buff, "%s: %d (%d)", skill_names[i], cpl.stats.skill_exp[i], cpl.stats.skill_level[i]);
				gtk_label_set(GTK_LABEL(statwindow.skill_exp[i]), buff);
				last_stats.skill_level[i] = cpl.stats.skill_level[i];
				last_stats.skill_exp[i] = cpl.stats.skill_exp[i];
			}
		}
	}
}

void
create_stat_bar(GtkWidget * mtable, gint row, gchar * label, gint bar, GtkWidget ** plabel)
{
	*plabel = gtk_label_new(label);
	gtk_table_attach(GTK_TABLE(mtable), *plabel, 0, 1, row, row + 1, GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show(*plabel);
	vitals[bar].bar = gtk_progress_bar_new();
	gtk_table_attach(GTK_TABLE(mtable), vitals[bar].bar, 0, 1, row + 1, row + 2, GTK_FILL | GTK_EXPAND, 0, 3, 0);
	gtk_widget_set_usize(vitals[bar].bar, 100, 10);
	gtk_widget_show(vitals[bar].bar);
	vitals[bar].state = 1;
	vitals[bar].style = gtk_style_new();
	vitals[bar].style->bg[GTK_STATE_PRELIGHT] = gdk_green;
	gtk_widget_set_style(vitals[bar].bar, vitals[bar].style);
}

static int
get_info_display(GtkWidget * frame)
{
	GtkWidget *plabel;
	GtkWidget *mtable;
	GtkWidget *vbox;
	int i;
	vbox = gtk_vbox_new(TRUE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	mtable = gtk_table_new(2, 9, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), mtable, TRUE, FALSE, 0);
	create_stat_bar(mtable, 1, "Hp: 0", 0, &statwindow.hp);
	create_stat_bar(mtable, 3, "Mana: 0", 1, &statwindow.sp);
	create_stat_bar(mtable, 5, "Grace: 0", 2, &statwindow.gr);
	create_stat_bar(mtable, 7, "Food: 0", 3, &statwindow.food);
	plabel = gtk_label_new("Status");
	gtk_table_attach(GTK_TABLE(mtable), plabel, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show(plabel);
	fire_label = gtk_label_new("    ");
	gtk_table_attach(GTK_TABLE(mtable), fire_label, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show(fire_label);
	run_label = gtk_label_new("   ");
	gtk_table_attach(GTK_TABLE(mtable), run_label, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	gtk_widget_show(run_label);
	for (i = 0; i < SHOW_RESISTS; i++) {
		resists[i] = gtk_label_new("          ");
		gtk_table_attach(GTK_TABLE(mtable), resists[i], 1, 2, 4 + i, 5 + i, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
		gtk_widget_show(resists[i]);
	}
	gtk_progress_bar_update(GTK_PROGRESS_BAR(vitals[0].bar), 1);
	gtk_progress_bar_update(GTK_PROGRESS_BAR(vitals[1].bar), 1);
	gtk_progress_bar_update(GTK_PROGRESS_BAR(vitals[2].bar), 1);
	gtk_progress_bar_update(GTK_PROGRESS_BAR(vitals[3].bar), 1);
	gtk_style_unref(vitals[0].style);
	gtk_style_unref(vitals[1].style);
	gtk_style_unref(vitals[2].style);
	gtk_style_unref(vitals[3].style);
	gtk_widget_show(mtable);
	gtk_widget_show(vbox);
	return 0;
}

static void
draw_stat_bar(int bar_pos, float bar, int is_alert)
{
	if (vitals[bar_pos].state != is_alert) {
		if (is_alert) {
			vitals[bar_pos].style = gtk_style_new();
			vitals[bar_pos].style->bg[GTK_STATE_PRELIGHT] = gdk_red;
			gtk_widget_set_style(vitals[bar_pos].bar, vitals[bar_pos].style);
			gtk_style_unref(vitals[bar_pos].style);
			vitals[bar_pos].state = is_alert;
		} else {
			vitals[bar_pos].style = gtk_style_new();
			vitals[bar_pos].style->bg[GTK_STATE_PRELIGHT] = gdk_green;
			gtk_widget_set_style(vitals[bar_pos].bar, vitals[bar_pos].style);
			gtk_style_unref(vitals[bar_pos].style);
			vitals[bar_pos].state = 0;
		}
	}
	gtk_progress_bar_update(GTK_PROGRESS_BAR(vitals[bar_pos].bar), bar);
	gtk_widget_draw(vitals[bar_pos].bar, NULL);
}

void
draw_message_window(int redraw)
{
	float bar;
	int is_alert, flags;
	static uint16 scrollsize_hp = 0, scrollsize_sp = 0, scrollsize_food = 0, scrollsize_grace = 0;
	static uint8 scrollhp_alert = FALSE, scrollsp_alert = FALSE, scrollfood_alert = FALSE, scrollgrace_alert = FALSE;
	if (updatelock < 25) {
		updatelock++;
		if (cpl.stats.maxhp > 0) {
			bar = (float)cpl.stats.hp / cpl.stats.maxhp;
			if (bar <= 0)
				bar = (float)0.01;
			is_alert = (cpl.stats.hp <= cpl.stats.maxhp / 4);
		} else {
			bar = (float)0.01;
			is_alert = 0;
		}
		if (redraw || scrollsize_hp != bar || scrollhp_alert != is_alert)
			draw_stat_bar(0, bar, is_alert);
		scrollsize_hp = bar;
		scrollhp_alert = is_alert;
		if (cpl.stats.sp > cpl.stats.maxsp)
			bar = (float)1;
		else
			bar = (float)cpl.stats.sp / cpl.stats.maxsp;
		if (bar <= 0)
			bar = (float)0.01;
		is_alert = (cpl.stats.sp <= cpl.stats.maxsp / 4);
		if (redraw || scrollsize_sp != bar || scrollsp_alert != is_alert)
			draw_stat_bar(1, bar, is_alert);
		scrollsize_sp = bar;
		scrollsp_alert = is_alert;
		if (cpl.stats.grace > cpl.stats.maxgrace)
			bar = MAX_BARS_MESSAGE;
		else
			bar = (float)cpl.stats.grace / cpl.stats.maxgrace;
		if (bar <= 0)
			bar = (float)0.01;
		if (bar > 1.0) {
			bar = (float)1.0;
		}
		is_alert = (cpl.stats.grace <= cpl.stats.maxgrace / 4);
		if (redraw || scrollsize_grace != bar || scrollgrace_alert != is_alert)
			draw_stat_bar(2, bar, is_alert);
		scrollsize_grace = bar;
		scrollgrace_alert = is_alert;
		bar = (float)cpl.stats.food / 999;
		if (bar <= 0)
			bar = (float)0.01;
		is_alert = (cpl.stats.food <= 999 / 4);
		if (redraw || scrollsize_food != bar || scrollfood_alert != is_alert)
			draw_stat_bar(3, bar, is_alert);
		scrollsize_food = bar;
		scrollfood_alert = is_alert;
		flags = cpl.stats.flags;
		if (redraw || cpl.stats.resist_change) {
			int i, j = 0;
			char buf[40];
			cpl.stats.resist_change = 0;
			for (i = 0; i < NUM_RESISTS; i++) {
				if (cpl.stats.resists[i]) {
					sprintf(buf, "%-10s %+4d", resists_name[i], cpl.stats.resists[i]);
					gtk_label_set(GTK_LABEL(resists[j]), buf);
					gtk_widget_draw(resists[j], NULL);
					j++;
					if (j >= SHOW_RESISTS)
						break;
				}
			}
			while (j < SHOW_RESISTS) {
				gtk_label_set(GTK_LABEL(resists[j]), "              ");
				gtk_widget_draw(resists[j], NULL);
				j++;
			}
		}
	} else {
	}
}

static void
draw_all_list(itemlist * l)
{
	int i;
	strcpy(l->old_title, "");
	for (i = 0; i < l->size; i++) {
		copy_name(l->names[i], "");
		l->faces[i] = 0;
		l->icon1[i] = 0;
		l->icon2[i] = 0;
		l->icon3[i] = 0;
		l->icon4[i] = 0;
	}
	l->bar_size = 1;
	draw_list(l);
}

void
open_container(item * op)
{
	look_list.env = op;
	sprintf(look_list.title, "%s:", op->d_name);
	draw_list(&look_list);
}

void
close_container(item * op)
{
	if (look_list.env != cpl.below) {
		client_send_apply(look_list.env->tag);
		look_list.env = cpl.below;
		strcpy(look_list.title, "You see:");
		draw_list(&look_list);
	}
}

static void
list_button_event(GtkWidget * gtklist, gint row, gint column, GdkEventButton * event, itemlist * l)
{
	item *tmp;
	if (event->button == 1) {
		tmp = gtk_clist_get_row_data(GTK_CLIST(gtklist), row);
		gtk_clist_unselect_row(GTK_CLIST(gtklist), row, 0);
		if (event->state & GDK_SHIFT_MASK)
			toggle_locked(tmp);
		else
			client_send_examine(tmp->tag);
	}
	if (event->button == 2) {
		tmp = gtk_clist_get_row_data(GTK_CLIST(gtklist), row);
		gtk_clist_unselect_row(GTK_CLIST(gtklist), row, 0);
		if (event->state & GDK_SHIFT_MASK)
			send_mark_obj(tmp);
		else
			client_send_apply(tmp->tag);
	}
	if (event->button == 3) {
		tmp = gtk_clist_get_row_data(GTK_CLIST(gtklist), row);
		gtk_clist_unselect_row(GTK_CLIST(gtklist), row, 0);
		if (tmp->locked) {
			draw_info("This item is locked.", NDI_BLACK);
		} else if (l == &inv_list) {
			cpl.count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(counttext));
			client_send_move(look_list.env->tag, tmp->tag, cpl.count);
			if (nopopups) {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(counttext), 0.0);
				cpl.count = 0;
			}
		} else {
			cpl.count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(counttext));
			client_send_move(inv_list.env->tag, tmp->tag, cpl.count);
			cpl.count = 0;
		}
	}
}

static void
resize_notebook_event(GtkWidget * widget, GtkAllocation * event)
{
	int i, newwidth;
	static int oldwidth = 0;
	newwidth = GTK_CLIST(inv_list.gtk_list[0])->clist_window_width - 87;
	if (newwidth != oldwidth) {
		oldwidth = newwidth;
		for (i = 0; i < TYPE_LISTS; i++) {
			gtk_clist_set_column_width(GTK_CLIST(inv_list.gtk_list[i]), 0, 12);
			gtk_clist_set_column_width(GTK_CLIST(inv_list.gtk_list[i]), 1, newwidth);
			gtk_clist_set_column_width(GTK_CLIST(inv_list.gtk_list[i]), 2, 50);
		}
		gtk_clist_set_column_width(GTK_CLIST(look_list.gtk_list[0]), 0, 12);
		gtk_clist_set_column_width(GTK_CLIST(look_list.gtk_list[0]), 1, newwidth);
		gtk_clist_set_column_width(GTK_CLIST(look_list.gtk_list[0]), 2, 50);
	}
}

void
count_callback(GtkWidget * widget, GtkWidget * entry)
{
	gchar *count_text;
	count_text = gtk_entry_get_text(GTK_ENTRY(counttext));
	cpl.count = atoi(count_text);
	gtk_widget_grab_focus(GTK_WIDGET(gtkwin_info_text));
}

void
create_notebook_page(GtkWidget * notebook, GtkWidget ** list, GtkWidget ** lists, gchar ** label)
{
	GtkWidget *vbox1;
	GtkStyle *liststyle, *tabstyle;
	GdkPixmap *labelgdkpixmap;
	GdkBitmap *labelgdkmask;
	GtkWidget *tablabel;
	gchar *titles[] = { "?", "Name", "Weight" };
	tabstyle = gtk_widget_get_style(gtkwin_root);
	labelgdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_root->window, &labelgdkmask, &tabstyle->bg[GTK_STATE_NORMAL], (gchar **) label);
	tablabel = gtk_pixmap_new(labelgdkpixmap, labelgdkmask);
	gtk_widget_show(tablabel);
	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox1, tablabel);
	*lists = gtk_scrolled_window_new(0, 0);
	*list = gtk_clist_new_with_titles(3, titles);
	gtk_clist_set_column_width(GTK_CLIST(*list), 0, 12);
	gtk_clist_set_column_width(GTK_CLIST(*list), 1, 150);
	gtk_clist_set_column_width(GTK_CLIST(*list), 2, 50);
	gtk_clist_set_column_resizeable(GTK_CLIST(*list), 0, FALSE);
	gtk_clist_set_column_resizeable(GTK_CLIST(*list), 1, TRUE);
	gtk_clist_set_column_resizeable(GTK_CLIST(*list), 2, TRUE);
	gtk_clist_set_selection_mode(GTK_CLIST(*list), GTK_SELECTION_SINGLE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(*lists), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	liststyle = gtk_style_new();
	liststyle->bg[GTK_STATE_SELECTED] = gdk_grey;
	liststyle->fg[GTK_STATE_SELECTED] = gdk_black;
	gtk_clist_set_button_actions(GTK_CLIST(*list), 1, GTK_BUTTON_SELECTS);
	gtk_clist_set_button_actions(GTK_CLIST(*list), 2, GTK_BUTTON_SELECTS);
	gtk_signal_connect(GTK_OBJECT(*list), "select_row", GTK_SIGNAL_FUNC(list_button_event), &inv_list);
	gtk_widget_show(*list);
	gtk_container_add(GTK_CONTAINER(*lists), *list);
	gtk_box_pack_start(GTK_BOX(vbox1), *lists, TRUE, TRUE, 0);
	gtk_widget_show(*lists);
	gtk_signal_connect(GTK_OBJECT(*list), "size-allocate", (GtkSignalFunc) resize_notebook_event, NULL);
	gtk_widget_show(vbox1);
}

static int
get_inv_display(GtkWidget * frame)
{
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *invlabel;
	GtkAdjustment *adj;
	strcpy(inv_list.title, "Inventory:");
	inv_list.env = cpl.ob;
	inv_list.show_weight = 1;
	inv_list.weight_limit = 0;
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);
	hbox1 = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox1, FALSE, FALSE, 0);
	gtk_widget_show(hbox1);
	inv_list.label = gtk_label_new("Inventory:");
	gtk_box_pack_start(GTK_BOX(hbox1), inv_list.label, TRUE, FALSE, 2);
	gtk_widget_show(inv_list.label);
	inv_list.weightlabel = gtk_label_new("0");
	gtk_box_pack_start(GTK_BOX(hbox1), inv_list.weightlabel, TRUE, FALSE, 2);
	gtk_widget_show(inv_list.weightlabel);
	inv_list.maxweightlabel = gtk_label_new("0");
	gtk_box_pack_start(GTK_BOX(hbox1), inv_list.maxweightlabel, TRUE, FALSE, 2);
	gtk_widget_show(inv_list.maxweightlabel);
	invlabel = gtk_label_new("Count:");
	gtk_box_pack_start(GTK_BOX(hbox1), invlabel, FALSE, FALSE, 5);
	gtk_widget_show(invlabel);
	adj = (GtkAdjustment *) gtk_adjustment_new(0.0, 0.0, 100000.0, 1.0, 100.0, 0.0);
	counttext = gtk_spin_button_new(adj, 1.0, 0);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(counttext), FALSE);
	gtk_widget_set_usize(counttext, 65, 0);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(counttext), GTK_UPDATE_ALWAYS);
	gtk_signal_connect(GTK_OBJECT(counttext), "activate", GTK_SIGNAL_FUNC(count_callback), counttext);
	gtk_box_pack_start(GTK_BOX(hbox1), counttext, FALSE, FALSE, 0);
	gtk_widget_show(counttext);
	gtk_tooltips_set_tip(tooltips, counttext, "This sets the number of items you wish to pickup or drop. You can also use the keys 0-9 to set it.", NULL);
	inv_notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(inv_notebook), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(vbox2), inv_notebook, TRUE, TRUE, 0);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[0], &inv_list.gtk_lists[0], all_xpm);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[1], &inv_list.gtk_lists[1], hand_xpm);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[2], &inv_list.gtk_lists[2], hand2_xpm);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[3], &inv_list.gtk_lists[3], coin_xpm);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[4], &inv_list.gtk_lists[4], skull_xpm);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[5], &inv_list.gtk_lists[5], mag_xpm);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[6], &inv_list.gtk_lists[6], nonmag_xpm);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[7], &inv_list.gtk_lists[7], lock_xpm);
	create_notebook_page(inv_notebook, &inv_list.gtk_list[8], &inv_list.gtk_lists[8], unlock_xpm);
	gtk_widget_show(vbox2);
	gtk_widget_show(inv_notebook);
	inv_list.multi_list = 1;
	draw_all_list(&inv_list);
	return 0;
}

static int
get_look_display(GtkWidget * frame)
{
	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *closebutton;
	GtkStyle *liststyle;
	gchar *titles[] = { "?", "Name", "Weight" };
	look_list.env = cpl.below;
	strcpy(look_list.title, "You see:");
	look_list.show_weight = 1;
	look_list.weight_limit = 0;
	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox1);
	hbox1 = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
	gtk_widget_show(hbox1);
	closebutton = gtk_button_new_with_label("Close");
	gtk_signal_connect_object(GTK_OBJECT(closebutton), "clicked", GTK_SIGNAL_FUNC(close_container), NULL);
	gtk_box_pack_start(GTK_BOX(hbox1), closebutton, FALSE, FALSE, 2);
	gtk_widget_show(closebutton);
	gtk_tooltips_set_tip(tooltips, closebutton, "This will close an item if you have one open.", NULL);
	look_list.label = gtk_label_new("You see:");
	gtk_box_pack_start(GTK_BOX(hbox1), look_list.label, TRUE, FALSE, 2);
	gtk_widget_show(look_list.label);
	look_list.weightlabel = gtk_label_new("0");
	gtk_box_pack_start(GTK_BOX(hbox1), look_list.weightlabel, TRUE, FALSE, 2);
	gtk_widget_show(look_list.weightlabel);
	look_list.maxweightlabel = gtk_label_new("0");
	gtk_box_pack_start(GTK_BOX(hbox1), look_list.maxweightlabel, TRUE, FALSE, 2);
	gtk_widget_show(look_list.maxweightlabel);
	look_list.gtk_lists[0] = gtk_scrolled_window_new(0, 0);
	look_list.gtk_list[0] = gtk_clist_new_with_titles(3, titles);;
	gtk_clist_set_column_width(GTK_CLIST(look_list.gtk_list[0]), 0, 12);
	gtk_clist_set_column_width(GTK_CLIST(look_list.gtk_list[0]), 1, 150);
	gtk_clist_set_column_width(GTK_CLIST(look_list.gtk_list[0]), 2, 50);
	gtk_clist_set_selection_mode(GTK_CLIST(look_list.gtk_list[0]), GTK_SELECTION_SINGLE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(look_list.gtk_lists[0]), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	liststyle = gtk_style_new();
	liststyle->bg[GTK_STATE_SELECTED] = gdk_grey;
	liststyle->fg[GTK_STATE_SELECTED] = gdk_black;
	gtk_clist_set_button_actions(GTK_CLIST(look_list.gtk_list[0]), 1, GTK_BUTTON_SELECTS);
	gtk_clist_set_button_actions(GTK_CLIST(look_list.gtk_list[0]), 2, GTK_BUTTON_SELECTS);
	gtk_signal_connect(GTK_OBJECT(look_list.gtk_list[0]), "select_row", GTK_SIGNAL_FUNC(list_button_event), &look_list);
	gtk_widget_show(look_list.gtk_list[0]);
	gtk_container_add(GTK_CONTAINER(look_list.gtk_lists[0]), look_list.gtk_list[0]);
	gtk_box_pack_start(GTK_BOX(vbox1), look_list.gtk_lists[0], TRUE, TRUE, 0);
	gtk_widget_show(look_list.gtk_lists[0]);
	gtk_widget_show(vbox1);
	look_list.multi_list = 0;
	draw_all_list(&look_list);
	return 0;
}

void
draw_lists()
{
	if (inv_list.env->inv_updated) {
		draw_list(&inv_list);
		inv_list.env->inv_updated = 0;
	} else {
		if (look_list.env->inv_updated) {
			draw_list(&look_list);
			look_list.env->inv_updated = 0;
		}
	}
}

void
set_show_icon(char *s)
{
	if (s == NULL || *s == 0 || strncmp("inventory", s, strlen(s)) == 0) {
		inv_list.show_icon = !inv_list.show_icon;
		draw_all_list(&inv_list);
	} else if (strncmp("look", s, strlen(s)) == 0) {
		look_list.show_icon = !look_list.show_icon;
		draw_all_list(&look_list);
	}
}

void
set_show_weight(char *s)
{
	if (s == NULL || *s == 0 || strncmp("inventory", s, strlen(s)) == 0) {
		inv_list.show_weight = !inv_list.show_weight;
		draw_list(&inv_list);
	} else if (strncmp("look", s, strlen(s)) == 0) {
		look_list.show_weight = !look_list.show_weight;
		draw_list(&look_list);
	}
}

void
aboutdialog(GtkWidget * widget)
{
	const gchar *authors[2] = { "Scott Barnes <reeve@ductape.net>", NULL };
	if (!gtkwin_about) {
		gtkwin_about = gnome_about_new(PACKAGE, VERSION, "(C) 2001 Scott Barnes", authors, "A GNOME client for Crossfire, a multiplayer Hack-like game.", DATADIR "/pixmaps/crossfiretitle.xpm");
		gtk_widget_show(gtkwin_about);
	} else {
		gdk_window_raise(gtkwin_about->window);
	}
}

void
applyconfig()
{
	int sound;
	image_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(imagesizesb));
	map_image_size = image_size;
	reset_image_data();
	if (GTK_TOGGLE_BUTTON(ccheckbutton3)->active) {
		if (nosound) {
			nosound = FALSE;
			sound = init_sounds();
			cs_print_string(csocket.fd, "setsound %d", sound >= 0);
		}
	} else {
		if (!nosound) {
			nosound = TRUE;
		}
	}
	if (GTK_TOGGLE_BUTTON(ccheckbutton4)->active) {
		if (!color_inv) {
			color_inv = TRUE;
			draw_all_list(&inv_list);
			draw_all_list(&look_list);
		}
	} else {
		if (color_inv) {
			color_inv = FALSE;
			draw_all_list(&inv_list);
			draw_all_list(&look_list);
		}
	}
	if (GTK_TOGGLE_BUTTON(ccheckbutton5)->active) {
		if (!color_text) {
			color_text = TRUE;
		}
	} else {
		if (color_text) {
			color_text = FALSE;
		}
	}
	if (GTK_TOGGLE_BUTTON(ccheckbutton6)->active) {
		if (!tool_tips) {
			gtk_tooltips_enable(tooltips);
			tool_tips = TRUE;
		}
	} else {
		if (tool_tips) {
			gtk_tooltips_disable(tooltips);
			tool_tips = FALSE;
		}
	}
	if (GTK_TOGGLE_BUTTON(ccheckbutton7)->active) {
		if (!splitinfo) {
			gtk_tooltips_enable(tooltips);
			splitinfo = TRUE;
		}
	} else {
		if (splitinfo) {
			gtk_tooltips_disable(tooltips);
			splitinfo = FALSE;
		}
	}
	if (GTK_TOGGLE_BUTTON(ccheckbutton8)->active) {
		if (!nopopups) {
			gtk_tooltips_enable(tooltips);
			nopopups = TRUE;
		}
	} else {
		if (nopopups) {
			gtk_tooltips_disable(tooltips);
			nopopups = FALSE;
		}
	}
}

void
saveconfig()
{
	save_defaults();
}

static void
ckeyentry_callback(GtkWidget * widget, GdkEventKey * event, GtkWidget * window)
{
	gtk_entry_set_text(GTK_ENTRY(ckeyentrytext), XKeysymToString(event->keyval));
	switch (event->state) {
	case GDK_CONTROL_MASK:
		gtk_entry_set_text(GTK_ENTRY(cmodentrytext), "R");
		break;
	case GDK_SHIFT_MASK:
		gtk_entry_set_text(GTK_ENTRY(cmodentrytext), "F");
		break;
	default:
		gtk_entry_set_text(GTK_ENTRY(cmodentrytext), "A");
	}
	gtk_signal_emit_stop_by_name(GTK_OBJECT(window), "key_press_event");
}

void
ckeyclear()
{
	gtk_label_set(GTK_LABEL(cnumentrytext), "0");
	gtk_entry_set_text(GTK_ENTRY(ckeyentrytext), "Press key to bind here");
	gtk_entry_set_text(GTK_ENTRY(cmodentrytext), "");
	gtk_entry_set_text(GTK_ENTRY(ckentrytext), "");
}

void
cclist_button_event(GtkWidget * gtklist, gint row, gint column, GdkEventButton * event)
{
	gchar *buf;
	if (event->button == 1) {
		gtk_clist_get_text(GTK_CLIST(cclist), row, 0, &buf);
		gtk_label_set(GTK_LABEL(cnumentrytext), buf);
		gtk_clist_get_text(GTK_CLIST(cclist), row, 1, &buf);
		gtk_entry_set_text(GTK_ENTRY(ckeyentrytext), buf);
		gtk_clist_get_text(GTK_CLIST(cclist), row, 3, &buf);
		gtk_entry_set_text(GTK_ENTRY(cmodentrytext), buf);
		gtk_clist_get_text(GTK_CLIST(cclist), row, 4, &buf);
		gtk_entry_set_text(GTK_ENTRY(ckentrytext), buf);
	}
}

void
draw_keybindings(GtkWidget * keylist)
{
	int i, count = 1;
	Key_Entry *key;
	int allbindings = 0;
	char buff[MAX_BUF];
	int bi = 0;
	char buffer[5][MAX_BUF];
	char *buffers[5];
	gint tmprow;
	gtk_clist_clear(GTK_CLIST(keylist));
	for (i = 0; i <= MAX_KEYCODE; i++) {
		for (key = keys[i]; key != NULL; key = key->next) {
			if (key->flags & KEYF_STANDARD && !allbindings)
				continue;
			bi = 0;
			if ((key->flags & KEYF_MODIFIERS) == KEYF_MODIFIERS)
				buff[bi++] = 'A';
			else {
				if (key->flags & KEYF_NORMAL)
					buff[bi++] = 'N';
				if (key->flags & KEYF_FIRE)
					buff[bi++] = 'F';
				if (key->flags & KEYF_RUN)
					buff[bi++] = 'R';
			}
			if (key->flags & KEYF_EDIT)
				buff[bi++] = 'E';
			if (key->flags & KEYF_STANDARD)
				buff[bi++] = 'S';
			buff[bi] = '\0';
			if (key->keysym == NoSymbol) {
			} else {
				sprintf(buffer[0], "%i", count);
				sprintf(buffer[1], "%s", XKeysymToString(key->keysym));
				sprintf(buffer[2], "%i", i);
				sprintf(buffer[3], "%s", buff);
				sprintf(buffer[4], "%s", key->command);
				buffers[0] = buffer[0];
				buffers[1] = buffer[1];
				buffers[2] = buffer[2];
				buffers[3] = buffer[3];
				buffers[4] = buffer[4];
				tmprow = gtk_clist_append(GTK_CLIST(keylist), buffers);
			}
			count++;
		}
	}
}

void
bind_callback(GtkWidget * gtklist, GdkEventButton * event)
{
	KeySym keysym;
	gchar *entry_text;
	gchar *cpnext;
	KeyCode k;
	gchar *mod = "";
	char buf[MAX_BUF];
	bind_flags = KEYF_MODIFIERS;
	if ((bind_flags & KEYF_MODIFIERS) == KEYF_MODIFIERS) {
		bind_flags &= ~KEYF_MODIFIERS;
		mod = gtk_entry_get_text(GTK_ENTRY(cmodentrytext));
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
	cpnext = gtk_entry_get_text(GTK_ENTRY(ckentrytext));
	entry_text = gtk_entry_get_text(GTK_ENTRY(ckeyentrytext));
	keysym = XStringToKeysym(entry_text);
	k = XKeysymToKeycode(GDK_DISPLAY(), keysym);
	insert_key(keysym, k, bind_flags, cpnext);
	save_keys();
	draw_keybindings(cclist);
	sprintf(buf, "Binded to key '%s' (%i)", XKeysymToString(keysym), (int)k);
	draw_info(buf, NDI_BLACK);
	gnome_property_box_changed(GNOME_PROPERTY_BOX(gtkwin_config));
}

void
ckeyunbind(GtkWidget * gtklist, GdkEventButton * event)
{
	gchar *buf;
	GList *node;
	node = GTK_CLIST(cclist)->selection;
	if (node) {
		gtk_clist_get_text(GTK_CLIST(cclist), (gint) node->data, 0, &buf);
		unbind_key(buf);
		draw_keybindings(cclist);
	}
	gnome_property_box_changed(GNOME_PROPERTY_BOX(gtkwin_config));
}

void
tbccb(GtkWidget *widget)
{
	gnome_property_box_changed(GNOME_PROPERTY_BOX(gtkwin_config));
}

void
configdialog(GtkWidget * widget)
{
	GtkWidget *tablabel;
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *frame1;
	GtkWidget *ehbox;
	GtkWidget *clabel1, *clabel2, *clabel4, *clabel5, *cb1, *cb2, *cb3;
	GtkWidget *cclists;
	GtkWidget *slabel, *shbox;
	GtkAdjustment *adjust;
	gchar *titles[] = { "#", "Key", "(#)", "Mods", "Command" };
	if (!gtkwin_config) {
		gtkwin_config = gnome_property_box_new();
		gtk_signal_connect(GTK_OBJECT(gtkwin_config), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_config);
		gtk_signal_connect(GTK_OBJECT(gtkwin_config), "apply", GTK_SIGNAL_FUNC(applyconfig), NULL);
		tablabel = gtk_label_new("General");
		gtk_widget_show(tablabel);
		vbox2 = gtk_vbox_new(FALSE, 0);
		gnome_property_box_append_page(GNOME_PROPERTY_BOX(gtkwin_config), vbox2, tablabel);
		frame1 = gtk_frame_new("General options");
		gtk_frame_set_shadow_type(GTK_FRAME(frame1), GTK_SHADOW_ETCHED_IN);
		gtk_box_pack_start(GTK_BOX(vbox2), frame1, TRUE, TRUE, 0);
		vbox1 = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(frame1), vbox1);
		shbox = gtk_hbox_new(FALSE, 0);
		slabel = gtk_label_new("Image Size [NxN]");
		gtk_box_pack_start(GTK_BOX(shbox), slabel, FALSE, FALSE, 0);
		adjust = GTK_ADJUSTMENT(gtk_adjustment_new(image_size, 12, 128, 1, 10, 10));
		imagesizesb = gtk_spin_button_new(adjust, 1, 3);
		gtk_box_pack_start(GTK_BOX(shbox), imagesizesb, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(imagesizesb), "changed", GTK_SIGNAL_FUNC(tbccb), NULL);
		gtk_box_pack_start(GTK_BOX(vbox1), shbox, FALSE, FALSE, 0);
		gtk_widget_show_all(shbox);
		ccheckbutton3 = gtk_check_button_new_with_label("Sound");
		gtk_box_pack_start(GTK_BOX(vbox1), ccheckbutton3, FALSE, FALSE, 0);
		if (nosound) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton3), FALSE);
		} else {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton3), TRUE);
		}
		gtk_signal_connect(GTK_OBJECT(ccheckbutton3), "clicked", GTK_SIGNAL_FUNC(tbccb), NULL);
		ccheckbutton4 = gtk_check_button_new_with_label("Color invlists");
		gtk_box_pack_start(GTK_BOX(vbox1), ccheckbutton4, FALSE, FALSE, 0);
		if (color_inv) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton4), TRUE);
		} else {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton4), FALSE);
		}
		gtk_signal_connect(GTK_OBJECT(ccheckbutton4), "clicked", GTK_SIGNAL_FUNC(tbccb), NULL);
		ccheckbutton5 = gtk_check_button_new_with_label("Color info text");
		gtk_box_pack_start(GTK_BOX(vbox1), ccheckbutton5, FALSE, FALSE, 0);
		if (color_text) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton5), TRUE);
		} else {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton5), FALSE);
		}
		gtk_signal_connect(GTK_OBJECT(ccheckbutton5), "clicked", GTK_SIGNAL_FUNC(tbccb), NULL);
		ccheckbutton6 = gtk_check_button_new_with_label("Show tooltips");
		gtk_box_pack_start(GTK_BOX(vbox1), ccheckbutton6, FALSE, FALSE, 0);
		if (tool_tips) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton6), TRUE);
		} else {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton6), FALSE);
		}
		gtk_signal_connect(GTK_OBJECT(ccheckbutton6), "clicked", GTK_SIGNAL_FUNC(tbccb), NULL);
		ccheckbutton7 = gtk_check_button_new_with_label("Split Information Window\n(Takes effect next run)");
		gtk_box_pack_start(GTK_BOX(vbox1), ccheckbutton7, FALSE, FALSE, 0);
		if (splitinfo) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton7), TRUE);
		} else {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton7), FALSE);
		}
		gtk_signal_connect(GTK_OBJECT(ccheckbutton7), "clicked", GTK_SIGNAL_FUNC(tbccb), NULL);
		ccheckbutton8 = gtk_check_button_new_with_label("No popup windows");
		gtk_box_pack_start(GTK_BOX(vbox1), ccheckbutton8, FALSE, FALSE, 0);
		if (nopopups) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton8), TRUE);
		} else {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(ccheckbutton8), FALSE);
		}
		gtk_signal_connect(GTK_OBJECT(ccheckbutton8), "clicked", GTK_SIGNAL_FUNC(tbccb), NULL);
		gtk_widget_show(ccheckbutton1);
		gtk_widget_show(ccheckbutton3);
		gtk_widget_show(ccheckbutton4);
		gtk_widget_show(ccheckbutton5);
		gtk_widget_show(ccheckbutton6);
		gtk_widget_show(ccheckbutton7);
		gtk_widget_show(ccheckbutton8);
		gtk_widget_show(vbox1);
		gtk_widget_show(frame1);
		gtk_widget_show(vbox2);
		tablabel = gtk_label_new("Keybindings");
		gtk_widget_show(tablabel);
		vbox2 = gtk_vbox_new(FALSE, 0);
		gnome_property_box_append_page(GNOME_PROPERTY_BOX(gtkwin_config), vbox2, tablabel);
		frame1 = gtk_frame_new("Keybindings");
		gtk_frame_set_shadow_type(GTK_FRAME(frame1), GTK_SHADOW_ETCHED_IN);
		gtk_box_pack_start(GTK_BOX(vbox2), frame1, TRUE, TRUE, 0);
		vbox1 = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(frame1), vbox1);
		cclists = gtk_scrolled_window_new(0, 0);
		cclist = gtk_clist_new_with_titles(5, titles);
		gtk_clist_set_column_width(GTK_CLIST(cclist), 0, 20);
		gtk_clist_set_column_width(GTK_CLIST(cclist), 1, 50);
		gtk_clist_set_column_width(GTK_CLIST(cclist), 2, 20);
		gtk_clist_set_column_width(GTK_CLIST(cclist), 3, 40);
		gtk_clist_set_column_width(GTK_CLIST(cclist), 4, 245);
		gtk_clist_set_selection_mode(GTK_CLIST(cclist), GTK_SELECTION_SINGLE);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cclists), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(cclists), cclist);
		gtk_box_pack_start(GTK_BOX(vbox1), cclists, TRUE, TRUE, 0);
		draw_keybindings(cclist);
		gtk_signal_connect_after(GTK_OBJECT(cclist), "select_row", GTK_SIGNAL_FUNC(cclist_button_event), NULL);
		gtk_widget_show(cclist);
		gtk_widget_show(cclists);
		ehbox = gtk_hbox_new(FALSE, 0);
		clabel1 = gtk_label_new("Binding #:");
		gtk_box_pack_start(GTK_BOX(ehbox), clabel1, FALSE, TRUE, 2);
		gtk_widget_show(clabel1);
		cnumentrytext = gtk_label_new("0");
		gtk_box_pack_start(GTK_BOX(ehbox), cnumentrytext, FALSE, TRUE, 2);
		gtk_widget_set_usize(cnumentrytext, 25, 0);
		gtk_widget_show(cnumentrytext);
		clabel2 = gtk_label_new("Key:");
		gtk_box_pack_start(GTK_BOX(ehbox), clabel2, FALSE, TRUE, 2);
		gtk_widget_show(clabel2);
		ckeyentrytext = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(ehbox), ckeyentrytext, TRUE, TRUE, 2);
		gtk_widget_set_usize(ckeyentrytext, 110, 0);
		gtk_signal_connect(GTK_OBJECT(ckeyentrytext), "key_press_event", GTK_SIGNAL_FUNC(ckeyentry_callback), ckeyentrytext);
		gtk_widget_show(ckeyentrytext);
		gtk_entry_set_text(GTK_ENTRY(ckeyentrytext), "Press key to bind here");
		clabel4 = gtk_label_new("Mods:");
		gtk_box_pack_start(GTK_BOX(ehbox), clabel4, FALSE, TRUE, 2);
		gtk_widget_show(clabel4);
		cmodentrytext = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(ehbox), cmodentrytext, FALSE, TRUE, 2);
		gtk_widget_set_usize(cmodentrytext, 45, 0);
		gtk_widget_show(cmodentrytext);
		gtk_box_pack_start(GTK_BOX(vbox1), ehbox, FALSE, TRUE, 2);
		gtk_widget_show(ehbox);
		ehbox = gtk_hbox_new(FALSE, 0);
		clabel5 = gtk_label_new("Command:");
		gtk_box_pack_start(GTK_BOX(ehbox), clabel5, FALSE, TRUE, 2);
		gtk_widget_show(clabel5);
		ckentrytext = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(ehbox), ckentrytext, TRUE, TRUE, 2);
		gtk_widget_show(ckentrytext);
		gtk_box_pack_start(GTK_BOX(vbox1), ehbox, FALSE, TRUE, 2);
		gtk_widget_show(ehbox);
		ehbox = gtk_hbox_new(TRUE, 0);
		cb1 = gtk_button_new_with_label("Unbind");
		gtk_box_pack_start(GTK_BOX(ehbox), cb1, FALSE, TRUE, 4);
		gtk_signal_connect_object(GTK_OBJECT(cb1), "clicked", GTK_SIGNAL_FUNC(ckeyunbind), NULL);
		gtk_widget_show(cb1);
		cb2 = gtk_button_new_with_label("Bind");
		gtk_box_pack_start(GTK_BOX(ehbox), cb2, FALSE, TRUE, 4);
		gtk_signal_connect_object(GTK_OBJECT(cb2), "clicked", GTK_SIGNAL_FUNC(bind_callback), NULL);
		gtk_widget_show(cb2);
		cb3 = gtk_button_new_with_label("Clear");
		gtk_box_pack_start(GTK_BOX(ehbox), cb3, FALSE, TRUE, 4);
		gtk_signal_connect_object(GTK_OBJECT(cb3), "clicked", GTK_SIGNAL_FUNC(ckeyclear), NULL);
		gtk_widget_show(cb3);
		gtk_box_pack_start(GTK_BOX(vbox1), ehbox, FALSE, TRUE, 2);
		gtk_widget_show(ehbox);
		gtk_widget_show(vbox1);
		gtk_widget_show(frame1);
		gtk_widget_show(vbox2);
		gtk_widget_show(gtkwin_config);
	} else {
		gdk_window_raise(gtkwin_config->window);
	}
}

void
chelpdialog(GtkWidget * widget)
{
#include "help/chelp.h"
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *chelptext;
	GtkWidget *helpbutton;
	GtkWidget *vscrollbar;
	if (!gtkwin_chelp) {
		gtkwin_chelp = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_position(GTK_WINDOW(gtkwin_chelp), GTK_WIN_POS_CENTER);
		gtk_widget_set_usize(gtkwin_chelp, 400, 300);
		gtk_window_set_title(GTK_WINDOW(gtkwin_chelp), "Crossfire Client Help");
		gtk_window_set_policy(GTK_WINDOW(gtkwin_chelp), TRUE, TRUE, FALSE);
		gtk_signal_connect(GTK_OBJECT(gtkwin_chelp), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_chelp);
		gtk_container_border_width(GTK_CONTAINER(gtkwin_chelp), 0);
		vbox = gtk_vbox_new(FALSE, 2);
		gtk_container_add(GTK_CONTAINER(gtkwin_chelp), vbox);
		hbox = gtk_hbox_new(FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
		chelptext = gtk_text_new(NULL, NULL);
		gtk_text_set_editable(GTK_TEXT(chelptext), FALSE);
		gtk_box_pack_start(GTK_BOX(hbox), chelptext, TRUE, TRUE, 0);
		gtk_widget_show(chelptext);
		vscrollbar = gtk_vscrollbar_new(GTK_TEXT(chelptext)->vadj);
		gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0);
		gtk_widget_show(vscrollbar);
		gtk_widget_show(hbox);
		hbox = gtk_hbox_new(FALSE, 2);
		helpbutton = gtk_button_new_with_label("Close");
		gtk_signal_connect_object(GTK_OBJECT(helpbutton), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(gtkwin_chelp));
		gtk_box_pack_start(GTK_BOX(hbox), helpbutton, TRUE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(helpbutton);
		gtk_widget_show(hbox);
		gtk_widget_show(vbox);
		gtk_widget_show(gtkwin_chelp);
		gtk_text_insert(GTK_TEXT(chelptext), NULL, &chelptext->style->black, NULL, text, -1);
	} else {
		gdk_window_raise(gtkwin_chelp->window);
	}
}

void
shelpdialog(GtkWidget * widget)
{
#include "help/shelp.h"
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *shelptext;
	GtkWidget *helpbutton;
	GtkWidget *vscrollbar;
	if (!gtkwin_shelp) {
		gtkwin_shelp = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_position(GTK_WINDOW(gtkwin_shelp), GTK_WIN_POS_CENTER);
		gtk_widget_set_usize(gtkwin_shelp, 400, 300);
		gtk_window_set_title(GTK_WINDOW(gtkwin_shelp), "Crossfire Server Help");
		gtk_window_set_policy(GTK_WINDOW(gtkwin_shelp), TRUE, TRUE, FALSE);
		gtk_signal_connect(GTK_OBJECT(gtkwin_shelp), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_shelp);
		gtk_container_border_width(GTK_CONTAINER(gtkwin_shelp), 0);
		vbox = gtk_vbox_new(FALSE, 2);
		gtk_container_add(GTK_CONTAINER(gtkwin_shelp), vbox);
		hbox = gtk_hbox_new(FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
		shelptext = gtk_text_new(NULL, NULL);
		gtk_text_set_editable(GTK_TEXT(shelptext), FALSE);
		gtk_box_pack_start(GTK_BOX(hbox), shelptext, TRUE, TRUE, 0);
		gtk_widget_show(shelptext);
		vscrollbar = gtk_vscrollbar_new(GTK_TEXT(shelptext)->vadj);
		gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0);
		gtk_widget_show(vscrollbar);
		gtk_widget_show(hbox);
		hbox = gtk_hbox_new(FALSE, 2);
		helpbutton = gtk_button_new_with_label("Close");
		gtk_signal_connect_object(GTK_OBJECT(helpbutton), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(gtkwin_shelp));
		gtk_box_pack_start(GTK_BOX(hbox), helpbutton, TRUE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(helpbutton);
		gtk_widget_show(hbox);
		gtk_widget_show(vbox);
		gtk_widget_show(gtkwin_shelp);
		gtk_text_insert(GTK_TEXT(shelptext), NULL, &shelptext->style->black, NULL, text, -1);
	} else {
		gdk_window_raise(gtkwin_shelp->window);
	}
}

void
menu_pickup0()
{
	pickup_mode = 0;
	send_command("pickup 0", -1, 0);
}

void
menu_pickup1()
{
	pickup_mode = 1;
	send_command("pickup 1", -1, 0);
}

void
menu_pickup2()
{
	pickup_mode = 2;
	send_command("pickup 2", -1, 0);
}

void
menu_pickup3()
{
	pickup_mode = 3;
	send_command("pickup 3", -1, 0);
}

void
menu_pickup4()
{
	pickup_mode = 4;
	send_command("pickup 4", -1, 0);
}

void
menu_pickup5()
{
	pickup_mode = 5;
	send_command("pickup 5", -1, 0);
}

void
menu_pickup6()
{
	pickup_mode = 6;
	send_command("pickup 6", -1, 0);
}

void
menu_pickup7()
{
	pickup_mode = 7;
	send_command("pickup 7", -1, 0);
}

void
menu_pickup10()
{
	pickup_mode = 10;
	send_command("pickup 10", -1, 0);
}

void
menu_who()
{
	extended_command("who");
}

void
menu_apply()
{
	extended_command("apply");
}

void
menu_cast()
{
	cast_menu_item_selected = TRUE;
	extended_command("cast");
}

void
menu_search()
{
	extended_command("search");
}

void
menu_disarm()
{
	extended_command("disarm");
}

void
menu_spells()
{
	char buf[MAX_BUF];
	int i;
	for (i = 0; i < 25; i++) {
		sprintf(buf, "Range: spell (%s)", cpl.spells[cpl.ready_spell]);
		printf("Spell: %s\n", cpl.spells[cpl.ready_spell]);
	}
}

void
menu_clear()
{
	guint size;
	size = gtk_text_get_length(GTK_TEXT(gtkwin_info_text));
	gtk_text_freeze(GTK_TEXT(gtkwin_info_text));
	gtk_text_set_point(GTK_TEXT(gtkwin_info_text), 0);
	gtk_text_forward_delete(GTK_TEXT(gtkwin_info_text), size);
	gtk_text_thaw(GTK_TEXT(gtkwin_info_text));
	size = gtk_text_get_length(GTK_TEXT(gtkwin_info_text2));
	gtk_text_freeze(GTK_TEXT(gtkwin_info_text2));
	gtk_text_set_point(GTK_TEXT(gtkwin_info_text2), 0);
	gtk_text_forward_delete(GTK_TEXT(gtkwin_info_text2), size);
	gtk_text_thaw(GTK_TEXT(gtkwin_info_text2));
}

void
sexit()
{
	extended_command("quit");
}

void
create_splash()
{
	GtkWidget *vbox;
	GtkWidget *aboutgtkpixmap;
	GdkPixmap *aboutgdkpixmap;
	GdkBitmap *aboutgdkmask;
	GtkStyle *style;
	gtkwin_splash = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_position(GTK_WINDOW(gtkwin_splash), GTK_WIN_POS_CENTER);
	gtk_widget_set_usize(gtkwin_splash, 346, 87);
	gtk_window_set_title(GTK_WINDOW(gtkwin_splash), "Welcome to Crossfire");
	gtk_signal_connect(GTK_OBJECT(gtkwin_splash), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_splash);
	gtk_container_border_width(GTK_CONTAINER(gtkwin_splash), 0);
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gtkwin_splash), vbox);
	style = gtk_widget_get_style(gtkwin_splash);
	gtk_widget_realize(gtkwin_splash);
	aboutgdkpixmap = gdk_pixmap_create_from_xpm_d(gtkwin_splash->window, &aboutgdkmask, &style->bg[GTK_STATE_NORMAL], (gchar **) crossfiretitle);
	aboutgtkpixmap = gtk_pixmap_new(aboutgdkpixmap, aboutgdkmask);
	gtk_box_pack_start(GTK_BOX(vbox), aboutgtkpixmap, FALSE, TRUE, 0);
	gtk_widget_show(aboutgtkpixmap);
	gtk_widget_show(vbox);
	gtk_widget_show(gtkwin_splash);
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
	sleep(1);
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
}

void
destroy_splash()
{
	gtk_widget_destroy(gtkwin_splash);
}

void
create_windows()
{
	GtkWidget *frame;
	GtkWidget *appbar;
	GtkWidget *hbox;
	GnomeUIInfo filem[] = { {GNOME_APP_UI_ITEM, "Save Config", "Save your current configuration", &saveconfig, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_SAVE, 0, (GdkModifierType) 0, NULL},
	GNOMEUIINFO_SEPARATOR,
	{GNOME_APP_UI_ITEM, "Quit Character", "Stop playing and delete the current character", &sexit, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_CLOSE, 0, (GdkModifierType) 0, NULL},
	{GNOME_APP_UI_ITEM, "Quit Client", "Exit the program and keep character", &gnome_client_quit, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_QUIT, 0, (GdkModifierType) 0, NULL},
	GNOMEUIINFO_END
	};
	GnomeUIInfo clientm[] = { {GNOME_APP_UI_ITEM, "Clear Info", "Clear the information text", &menu_clear, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_CLEAR, 0, (GdkModifierType) 0, NULL},
	{GNOME_APP_UI_ITEM, "Spells", "Show currently known spells", &menu_spells, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_BOOK_OPEN, 0, (GdkModifierType) 0, NULL},
	{GNOME_APP_UI_ITEM, "Configure", "Change your configuration", &configdialog, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_PREFERENCES, 0, (GdkModifierType) 0, NULL},
	{GNOME_APP_UI_ITEM, "Disconnect", "Disconnect from server", &disconnect, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_CLOSE, 0, (GdkModifierType) 0, NULL},
	GNOMEUIINFO_END
	};
	GnomeUIInfo pickupl[] = { GNOMEUIINFO_RADIOITEM("Don't pick up", "Don't automatically pick up anything", menu_pickup0, dot_xpm),
		GNOMEUIINFO_RADIOITEM("Pick up one", "Pick up the top item", menu_pickup1, dot_xpm),
		GNOMEUIINFO_RADIOITEM("Pick up one + stop", "Pick up the top item and stop moving", menu_pickup2, dot_xpm),
		GNOMEUIINFO_RADIOITEM("Stop before picking up", "", menu_pickup3, dot_xpm),
		GNOMEUIINFO_RADIOITEM("Pick up everything", "Pick up every item", menu_pickup4, dot_xpm),
		GNOMEUIINFO_RADIOITEM("Pick up everything + stop", "Pick up every item and stop moving", menu_pickup5, dot_xpm),
		GNOMEUIINFO_RADIOITEM("Pick up magic items", "Pick up magical items", menu_pickup6, dot_xpm),
		GNOMEUIINFO_RADIOITEM("Pick up coins and gems", "Pick up only money", menu_pickup7, dot_xpm),
		GNOMEUIINFO_RADIOITEM("Pick up valuables", "Pick up only valuable things", menu_pickup10, dot_xpm),
		GNOMEUIINFO_END
	};
	GnomeUIInfo pickupm[] = { GNOMEUIINFO_RADIOLIST(pickupl),
		GNOMEUIINFO_END
	};
	GnomeUIInfo actionm[] = { {GNOME_APP_UI_ITEM, "Who", "Who?", &menu_who, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_HOME, 0, (GdkModifierType) 0, NULL},
	{GNOME_APP_UI_ITEM, "Cast...", "Cast a spell", &menu_cast, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_BOOK_OPEN, 0, (GdkModifierType) 0, NULL},
	{GNOME_APP_UI_ITEM, "Apply", "Apply an item", &menu_apply, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_EXEC, 0, (GdkModifierType) 0, NULL},
	GNOMEUIINFO_SUBTREE_STOCK("Pickup", pickupm, GNOME_STOCK_PIXMAP_REDO),
	{GNOME_APP_UI_ITEM, "Search", "Search for traps", &menu_search, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_SEARCH, 0, (GdkModifierType) 0, NULL},
	{GNOME_APP_UI_ITEM, "Disarm", "Disarm traps", &menu_disarm, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_CUT, 0, (GdkModifierType) 0, NULL},
	GNOMEUIINFO_END
	};
	GnomeUIInfo helpm[] = { {GNOME_APP_UI_ITEM, "Client Help", "Help on using the client", &chelpdialog, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_HELP, 0, (GdkModifierType) 0, NULL},
	{GNOME_APP_UI_ITEM, "Server Help", "Help with server issues", &shelpdialog, NULL, NULL, GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_PIXMAP_HELP, 0, (GdkModifierType) 0, NULL},
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_ABOUT_ITEM(&aboutdialog, NULL),
	GNOMEUIINFO_END
	};
	GnomeUIInfo mainm[] = { GNOMEUIINFO_MENU_FILE_TREE(filem),
		GNOMEUIINFO_SUBTREE("Client", clientm),
		GNOMEUIINFO_SUBTREE("Action", actionm),
		GNOMEUIINFO_MENU_HELP_TREE(helpm),
		GNOMEUIINFO_END
	};
	int i;
	tooltips = gtk_tooltips_new();
	gtkwin_root = gnome_app_new(PACKAGE, "GNOME Crossfire Client");
	gnome_app_create_menus(GNOME_APP(gtkwin_root), mainm);
	appbar = gnome_appbar_new(TRUE, TRUE, GNOME_PREFERENCES_NEVER);
	gnome_app_set_statusbar(GNOME_APP(gtkwin_root), appbar);
	gnome_app_install_menu_hints(GNOME_APP(gtkwin_root), mainm);
	gnome_app_enable_layout_config(GNOME_APP(gtkwin_root), TRUE);
	gtk_widget_set_events(gtkwin_root, GDK_KEY_RELEASE_MASK);
	gtk_signal_connect(GTK_OBJECT(gtkwin_root), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_root);
	for (i = 0; i <= 12; i++) {
		if (!gdk_color_parse(colorname[i], &root_color[i])) {
			printf("cparse failed (%s)\n", colorname[i]);
		}
		if (!gdk_color_alloc(gtk_widget_get_colormap(gtkwin_root), &root_color[i])) {
			printf("calloc failed\n");
		}
	}
	gtk_widget_show_all(gtkwin_root);
	frame = gtk_frame_new(NULL);
/*	gtk_widget_set_usize(frame, 350, 175);*/
	get_stats_display(frame);
	gnome_app_add_docked(GNOME_APP(gtkwin_root), frame, "StatsBox", GNOME_DOCK_ITEM_BEH_NORMAL, GNOME_DOCK_BOTTOM, 0, 0, 0);
	gtk_widget_show_all(frame);
	frame = gtk_frame_new(NULL);
/*	gtk_widget_set_usize(frame, 350, 175);*/
	get_info_display(frame);
	gnome_app_add_docked(GNOME_APP(gtkwin_root), frame, "InfoBox", GNOME_DOCK_ITEM_BEH_NORMAL, GNOME_DOCK_BOTTOM, 0, 1, 0);
	gtk_widget_show_all(frame);
	invframe = gtk_frame_new(NULL);
	gtk_widget_set_usize(invframe, 230, (((mapy * map_image_size) / 3) * 2));
	get_inv_display(invframe);
	gnome_app_add_docked(GNOME_APP(gtkwin_root), invframe, "InvBox", GNOME_DOCK_ITEM_BEH_NORMAL, GNOME_DOCK_LEFT, 0, 0, 0);
	gtk_widget_show_all(invframe);
	lookframe = gtk_frame_new(NULL);
	gtk_widget_set_usize(lookframe, 230, ((mapy * map_image_size) / 3));
	get_look_display(lookframe);
	gnome_app_add_docked(GNOME_APP(gtkwin_root), lookframe, "LookBox", GNOME_DOCK_ITEM_BEH_NORMAL, GNOME_DOCK_LEFT, 0, 1, 0);
	gtk_widget_show_all(lookframe);
	hbox = gtk_hbox_new(FALSE, 0);
	frame = gtk_frame_new(NULL);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_widget_show_all(hbox);
	gtk_widget_show_all(frame);
	gnome_app_set_contents(GNOME_APP(gtkwin_root), frame);
	gameframe = gtk_frame_new(NULL);
	gtk_widget_set_usize(gameframe, (map_image_size * mapx) + 6, (map_image_size * mapy) + 6);
	get_game_display(gameframe);
	gtk_box_pack_start(GTK_BOX(hbox), gameframe, FALSE, FALSE, 0);
	gtk_widget_show_all(gameframe);
	frame = gtk_frame_new(NULL);
/*	gtk_widget_set_usize(frame, 300, (map_image_size * mapy) + 6);*/
	gtk_box_pack_end(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
	gtk_widget_show_all(frame);
	get_message_display(frame);
	gtk_signal_connect_object(GTK_OBJECT(gtkwin_root), "key_press_event", GTK_SIGNAL_FUNC(keyfunc), GTK_OBJECT(gtkwin_root));
	gtk_signal_connect_object(GTK_OBJECT(gtkwin_root), "key_release_event", GTK_SIGNAL_FUNC(keyrelfunc), GTK_OBJECT(gtkwin_root));
	gtk_widget_show_all(gtkwin_root);
	if (tool_tips) {
		gtk_tooltips_enable(tooltips);
	}
}

void
set_weight_limit(uint32 wlim)
{
	inv_list.weight_limit = wlim;
}

void
set_scroll(char *s)
{
}

void
set_autorepeat(char *s)
{
}

void
draw_all_info()
{
}

void
resize_win_info()
{
}

int
get_info_width()
{
	return 40;
}

void
do_clearlock()
{
}

void
x_set_echo()
{
	if (nopopups) {
		gtk_entry_set_visibility(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entrytext))), !cpl.no_echo);
	}
}

int
do_timeout()
{
	updatelock = 0;
	if (draw_info_freeze1) {
		gtk_text_thaw(GTK_TEXT(gtkwin_info_text));
		gtk_adjustment_set_value(GTK_ADJUSTMENT(text_vadj), GTK_ADJUSTMENT(text_vadj)->upper - GTK_ADJUSTMENT(text_vadj)->page_size);
		gtk_text_set_adjustments(GTK_TEXT(gtkwin_info_text), GTK_ADJUSTMENT(text_hadj), GTK_ADJUSTMENT(text_vadj));
		draw_info_freeze1 = FALSE;
	}
	if (draw_info_freeze2) {
		gtk_text_thaw(GTK_TEXT(gtkwin_info_text2));
		gtk_adjustment_set_value(GTK_ADJUSTMENT(text_vadj2), GTK_ADJUSTMENT(text_vadj2)->upper - GTK_ADJUSTMENT(text_vadj2)->page_size);
		gtk_text_set_adjustments(GTK_TEXT(gtkwin_info_text2), GTK_ADJUSTMENT(text_hadj2), GTK_ADJUSTMENT(text_vadj2));
		draw_info_freeze2 = FALSE;
	}
	if (redraw_needed) {
		display_map_doneupdate(0);
		draw_all_list(&inv_list);
		draw_all_list(&look_list);
		redraw_needed = FALSE;
	}
	if (!inv_list.env->inv_updated) {
		animate_list();
	}
	if (cpl.showmagic)
		magic_map_flash_pos();
	draw_lists();
	return TRUE;
}

void
display_newbitmap(long face, long fg, long bg, char *buf)
{
}

void
draw_magic_map()
{
	int x = 0;
	int y = 0;
	GtkWidget *hbox;
	GtkWidget *closebutton;
	GtkStyle *style;
	static GtkWidget *magicgtkpixmap;
	static GdkBitmap *magicgdkmask;
	if (!cpl.magicmap) {
		draw_info("You have yet to cast magic map.", NDI_BLACK);
		return;
	}
	if (!gtkwin_magicmap) {
		gtkwin_magicmap = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_position(GTK_WINDOW(gtkwin_magicmap), GTK_WIN_POS_CENTER);
		gtk_widget_set_usize(gtkwin_magicmap, 264, 300);
		gtk_window_set_title(GTK_WINDOW(gtkwin_magicmap), "Magic map");
		gtk_window_set_policy(GTK_WINDOW(gtkwin_magicmap), FALSE, FALSE, FALSE);
		gtk_signal_connect(GTK_OBJECT(gtkwin_magicmap), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_magicmap);
		mapvbox = gtk_vbox_new(FALSE, 0);
		gtk_widget_set_usize(mapvbox, 264, 300);
		gtk_container_add(GTK_CONTAINER(gtkwin_magicmap), mapvbox);
		style = gtk_widget_get_style(gtkwin_magicmap);
		gtk_widget_realize(mapvbox);
		magicgdkpixmap = gdk_pixmap_new(gtkwin_magicmap->window, 264, 264, -1);
		magicgtkpixmap = gtk_pixmap_new(magicgdkpixmap, magicgdkmask);
		gtk_box_pack_start(GTK_BOX(mapvbox), magicgtkpixmap, FALSE, FALSE, 0);
		gtk_widget_show(magicgtkpixmap);
		hbox = gtk_hbox_new(FALSE, 2);
		closebutton = gtk_button_new_with_label("Close");
		gtk_signal_connect_object(GTK_OBJECT(closebutton), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(gtkwin_magicmap));
		gtk_box_pack_start(GTK_BOX(hbox), closebutton, TRUE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(mapvbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(closebutton);
		gtk_widget_show(hbox);
		gtk_widget_show(mapvbox);
		gtk_widget_show(gtkwin_magicmap);
		gdk_color_parse("Black", &map_color[0]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[0]);
		gdk_color_parse("White", &map_color[1]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[1]);
		gdk_color_parse("Navy", &map_color[2]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[2]);
		gdk_color_parse("Red", &map_color[3]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[3]);
		gdk_color_parse("Orange", &map_color[4]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[4]);
		gdk_color_parse("DodgerBlue", &map_color[5]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[5]);
		gdk_color_parse("DarkOrange2", &map_color[6]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[6]);
		gdk_color_parse("SeaGreen", &map_color[7]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[7]);
		gdk_color_parse("DarkSeaGreen", &map_color[8]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[8]);
		gdk_color_parse("Grey50", &map_color[9]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[9]);
		gdk_color_parse("Sienna", &map_color[10]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[10]);
		gdk_color_parse("Gold", &map_color[11]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[11]);
		gdk_color_parse("Khaki", &map_color[12]);
		gdk_color_alloc(gtk_widget_get_colormap(magicgtkpixmap), &map_color[12]);
		map_gc = gdk_gc_new(magicgdkpixmap);
		gdk_gc_set_foreground(map_gc, &map_color[0]);
		gdk_draw_rectangle(magicgdkpixmap, map_gc, TRUE, 0, 0, 264, 264);
		cpl.mapxres = (262) / cpl.mmapx;
		cpl.mapyres = (262) / cpl.mmapy;
		if (cpl.mapxres < 1 || cpl.mapyres < 1) {
			fprintf(stderr, "magic map resolution less than 1, map is %dx%d\n", cpl.mmapx, cpl.mmapy);
			return;
		}
		if (cpl.mapxres > cpl.mapyres)
			cpl.mapxres = cpl.mapyres;
		else
			cpl.mapyres = cpl.mapxres;
		if (cpl.mapxres > 24) {
			cpl.mapxres = 24;
			cpl.mapyres = 24;
		}
		for (y = 0; y < cpl.mmapy; y++) {
			for (x = 0; x < cpl.mmapx; x++) {
				uint8 val = cpl.magicmap[y * cpl.mmapx + x];
				gdk_gc_set_foreground(map_gc, &map_color[val & FACE_COLOR_MASK]);
				gdk_draw_rectangle(magicgdkpixmap, map_gc, TRUE, 2 + cpl.mapxres * x, 2 + cpl.mapyres * y, cpl.mapxres, cpl.mapyres);
			}
		}
		gtk_widget_draw(mapvbox, NULL);
	} else {
		gdk_window_raise(gtkwin_magicmap->window);
		gdk_gc_set_foreground(map_gc, &map_color[0]);
		gdk_draw_rectangle(magicgdkpixmap, map_gc, TRUE, 0, 0, 264, 264);
		cpl.mapxres = (262) / cpl.mmapx;
		cpl.mapyres = (262) / cpl.mmapy;
		if (cpl.mapxres < 1 || cpl.mapyres < 1) {
			fprintf(stderr, "magic map resolution less than 1, map is %dx%d\n", cpl.mmapx, cpl.mmapy);
			return;
		}
		if (cpl.mapxres > cpl.mapyres)
			cpl.mapxres = cpl.mapyres;
		else
			cpl.mapyres = cpl.mapxres;
		if (cpl.mapxres > 24) {
			cpl.mapxres = 24;
			cpl.mapyres = 24;
		}
		for (y = 0; y < cpl.mmapy; y++) {
			for (x = 0; x < cpl.mmapx; x++) {
				uint8 val = cpl.magicmap[y * cpl.mmapx + x];
				gdk_gc_set_foreground(map_gc, &map_color[val & FACE_COLOR_MASK]);
				gdk_draw_rectangle(magicgdkpixmap, map_gc, TRUE, 2 + cpl.mapxres * x, 2 + cpl.mapyres * y, cpl.mapxres, cpl.mapyres);
			}
		}
		gtk_widget_draw(mapvbox, NULL);
	}
}

void
magic_map_flash_pos()
{
	if (!cpl.showmagic)
		return;
	if (!gtkwin_magicmap)
		return;
	cpl.showmagic ^= 2;
	if (cpl.showmagic & 2) {
		gdk_gc_set_foreground(map_gc, &map_color[0]);
	} else {
		gdk_gc_set_foreground(map_gc, &map_color[1]);
	}
	gdk_draw_rectangle(magicgdkpixmap, map_gc, TRUE, 2 + cpl.mapxres * cpl.pmapx, 2 + cpl.mapyres * cpl.pmapy, cpl.mapxres, cpl.mapyres);
	gtk_widget_draw(mapvbox, NULL);
}

void
command_show(char *params)
{
	if (!params) {
		if (gtk_notebook_get_current_page(GTK_NOTEBOOK(inv_notebook)) == 8)
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



static void
gen_draw_face(int face, int x, int y)
{
	gdk_gc_set_clip_mask(gc, pixmaps[facecachemap[face]].map_mask);
	gdk_gc_set_clip_origin(gc, x * map_image_size, y * map_image_size);
	gdk_window_copy_area(pixmap, gc, x * map_image_size, y * map_image_size, pixmaps[facecachemap[face]].map_image, 0, 0, map_image_size, map_image_size);
}

void
display_mapcell_pixmap(int ax, int ay)
{
	int k;
	if (the_map.cells[ax][ay].need_update == TRUE) {
		gdk_draw_rectangle(pixmap, drawable->style->mid_gc[0], TRUE, ax * map_image_size, ay * map_image_size, map_image_size, map_image_size);
		if (mapx > 11 && mapy > 11) {
			for (k = 0; k < the_map.cells[ax][ay].count; k++) {
				if (the_map.cells[ax][ay].faces[k] > 0)
					gen_draw_face(the_map.cells[ax][ay].faces[k], ax, ay);
			}
		} else {
			for (k = the_map.cells[ax][ay].count - 1; k > -1; k--) {
				if (the_map.cells[ax][ay].faces[k] > 0)
					gen_draw_face(the_map.cells[ax][ay].faces[k], ax, ay);
			}
		}
	}
}

int
display_willcache()
{
	return TRUE;
}

void
display_map_newmap()
{
}

void
display_map_doneupdate(int redraw)
{
    if (updatelock > 30)
	    return;
    gtk_draw_map();
}


void
display_newpix(long face, char *buf, long buflen)
{
	char *filename;
	FILE *tmpfile;
	GdkPixbuf *tmppixbuf;
	if (facetoname[face] == NULL) {
		fprintf(stderr, "Caching images, but name for %ld not set\n", face);
	}
	filename = facetoname[face];
	if ((tmpfile = fopen(filename, "w")) == NULL) {
		fprintf(stderr, "Can not open %s for writing\n", filename);
	} else {
		fwrite(buf, buflen, 1, tmpfile);
		fclose(tmpfile);
	}
	pixmaps[face].gdkpixbuf = gdk_pixbuf_new_from_file(filename);
	tmppixbuf = gdk_pixbuf_scale_simple(pixmaps[face].gdkpixbuf, map_image_size, map_image_size, GDK_INTERP_BILINEAR);
	gdk_pixbuf_render_pixmap_and_mask(tmppixbuf, &pixmaps[face].map_image, &pixmaps[face].map_mask, 1);
	tmppixbuf = gdk_pixbuf_scale_simple(pixmaps[face].gdkpixbuf, 12, 12, GDK_INTERP_BILINEAR);
	gdk_pixbuf_render_pixmap_and_mask(tmppixbuf, &pixmaps[face].icon_image, &pixmaps[face].icon_mask, 1);
	if (!pixmaps[face].gdkpixbuf) {
		fprintf(stderr, "Got error on image load\n");
	}
	free(facetoname[face]);
	facetoname[face] = NULL;
}

void
display_newpng(long face, char *buf, long buflen)
{
	display_newpix(face, buf, buflen);
}

void
display_newpixmap(long face, char *buf, long buflen)
{
	display_newpix(face, buf, buflen);
}

void
display_map_startupdate()
{
}

char *
get_metaserver()
{
	cpl.input_state = Metaserver_Select;
	while (cpl.input_state == Metaserver_Select) {
		if (gtk_events_pending())
			gtk_main_iteration();
		usleep(10 * 1000);
	}
	return cpl.input_text;
}

void
reset_image_data()
{
	int i;
	for (i = 1; i < MAXPIXMAPNUM; i++) {
		if (pixmaps[i].gdkpixbuf && (pixmaps[i].gdkpixbuf != pixmaps[0].gdkpixbuf)) {
			gdk_pixbuf_unref(pixmaps[i].gdkpixbuf);
			pixmaps[i].gdkpixbuf = NULL;
		}
		free(facetoname[i]);
		facetoname[i] = NULL;
	}
	memset(&the_map, 0, sizeof(struct Map));
	look_list.env = cpl.below;
}

void
save_winpos()
{
}

int
main(int argc, char **argv)
{
    poptContext pctx;
    int on_arg = 1, got_one, sound;
    gchar **args;

    init_client_vars();

    strcpy(VERSION_INFO,"Gnome Unix Client " VERSION);
    gnome_init_with_popt_table(PACKAGE, VERSION, argc, argv, options, 0, &pctx);
    load_defaults();
    args = (gchar **) poptGetArgs(pctx);
    if (echobindings == TRUE)
	cpl.echo_bindings = TRUE;
    if (mapsizeopt != -1 && (mapsizeopt < 11 || mapsizeopt > MAP_MAX_SIZE)) {
	    printf("Mapsize must be between 11 and %d!\n", MAP_MAX_SIZE);
	    exit(0);
    } else if (mapsizeopt != -1) {
	    want_mapx = mapsizeopt;
	    want_mapy = mapsizeopt;
    }
    poptFreeContext(pctx);
    want_skill_exp = 1;
    last_str = malloc(32767);
    create_splash();
    create_windows();
    gdk_rgb_init();
    for (on_arg = 0; on_arg < MAXPIXMAPNUM; on_arg++)
	facecachemap[on_arg] = on_arg;
    init_keys();
    init_cache_data();
    destroy_splash();
    allocate_map(&the_map, want_mapx, want_mapy);
        csocket.inbuf.buf=malloc(MAXSOCKBUF);

#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX);
#else
    maxfd = getdtablesize();
#endif

    sound=init_sounds();

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
	    negotiate_connection(sound);
	} else {
	    csocket.fd=init_connection(server, port_num);
	    if (csocket.fd == -1) { /* specified server no longer valid */
		server = SERVER;
		continue;
	    }
	    negotiate_connection(sound);
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

