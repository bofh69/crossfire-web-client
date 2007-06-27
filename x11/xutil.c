const char *rcsid_x11_xutil_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001-2003 Mark Wedel & Crossfire Development Team

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

/* This contains various 'support' functions.  These functions will probably
 * go mostly unaltered between different toolkits, as long as X11 is still
 * used.
 */

#include <client.h>
#include <item.h>
#include <config.h>
#include <p_cmd.h>

#ifdef HAVE_LIBXPM
#include <X11/xpm.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "def-keys.h"
#include "x11proto.h"
#include "x11.h"


static const char *colorname[] = {
"Black",                /* 0  */
"White",                /* 1  */
"Navy",                 /* 2  */
"Red",                  /* 3  */
"Chocolate",            /* 4  was Orange, but impossible to read on DarkSeaGreen */
"DodgerBlue",           /* 5  */
"DarkOrange2",          /* 6  */
"SeaGreen",             /* 7  */
"DarkSeaGreen",         /* 8  */
"Grey80",               /* 9  */	/* Used for window background color */
"Sienna",               /* 10 */
"Gold",                 /* 11 */
"Khaki"                 /* 12 */
};

struct {
    char    *name;
    uint32  checksum;
    Pixmap  pixmap, mask;
} private_cache[MAXPIXMAPNUM];

int use_private_cache=0, last_face_num=0;


/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

char facecachedir[MAX_BUF];

typedef struct Keys {
    uint8	flags;
    sint8	direction;
    KeySym	keysym;
    char	*command;
    struct Keys	*next;
} Key_Entry;

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


static KeyCode firekey[2], runkey[2], commandkey, *bind_keycode, prevkey, nextkey,
    completekey;
static KeySym firekeysym[2], runkeysym[2], commandkeysym,*bind_keysym,
    prevkeysym, nextkeysym, completekeysym;
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

/* Key codes can only be from 8-255 (at least according to
 * the X11 manual.  This is easier than using a hash
 * table, quicker, and doesn't use much more space.
 */

#define MAX_KEYCODE 255
static Key_Entry *keys[256];

/* Can be set when user is moving to new machine type */
uint8 updatekeycodes=FALSE;

#ifndef GDK_XUTIL
/* Initializes the data for image caching */
void init_cache_data()
{
    int i;
    Pixmap  ptmp;
#include "pixmaps/question.111"


    /* Currently, we can cache in all face modes currently supported,
     * so I removed the code that did checks on that.
     */

    pixmaps[0] = malloc(sizeof(struct PixmapInfo));
    pixmaps[0]->mask=None;
    ptmp=XCreateBitmapFromData(display,DefaultRootWindow(display),
				question_bits,question_width,question_height);

    /* In xpm mode, XCopyArea is used from this data, so we need to copy
     * the image into an pixmap of appropriate depth.
     */
    pixmaps[0]->pixmap=XCreatePixmap(display, win_root, image_size, image_size,
	DefaultDepth(display,DefaultScreen(display)));
    pixmaps[0]->width = 1;
    pixmaps[0]->height = 1;
    XCopyPlane(display, ptmp, pixmaps[0]->pixmap, gc_game,
	       0,0,image_size,image_size,0,0,1);
    XFreePixmap(display, ptmp);

    /* Initialize all the images to be of the same value. */
    for (i=1; i<MAXPIXMAPNUM; i++)  {
	pixmaps[i]=pixmaps[0];
    }

    init_common_cache_data();

}
#endif

/* Rotate right from bsd sum. */
#define ROTATE_RIGHT(c) if ((c) & 01) (c) = ((c) >>1) + 0x80000000; else (c) >>= 1;

/*#define CHECKSUM_DEBUG*/


void allocate_colors(Display *disp, Window w, long screen_num,
        Colormap *colormap, XColor discolor[16]) {
  int i, tried = 0, depth=0;
  Status status;
  Visual *vis;
  XColor exactcolor;

  vis = DefaultVisual(disp,screen_num);
  if (vis->class >= StaticColor) {
    *colormap = DefaultColormap(disp,screen_num);
    depth = DefaultDepth(disp,screen_num);
  }
  else {
    *colormap = DefaultColormap(disp,screen_num);
    printf("You have a black and white terminal.\n");
    printf("These are no longer supported by cfclient.\n");
    return;
  }
try_private:
  if (depth > 3) {
    unsigned long pixels[13];
    for (i=0; i<13; i++){
      status = XLookupColor(disp,*colormap, colorname[i],&exactcolor,
                            &discolor[i]);
      if (!status){
        printf("Can't find colour %s.\n", colorname[i]);
        break;
      }
      status = XAllocColor(disp,*colormap,&discolor[i]);
      if (!status) {
        if (!tried) {
          printf( "Not enough colours. Trying a private colourmap.\n");
          XFreeColors(disp, *colormap, pixels, i-1, 0);
          *colormap = XCreateColormap(disp, w, vis, AllocNone);
          XSetWindowColormap(disp, w, *colormap);
          tried = 1;
          goto try_private;
        } else break;
      }
      pixels[i] = discolor[i].pixel;
    }
  }
}

/* Updates the keys array with the keybinding that is passed.  All the
 * arguments are pretty self explanatory.  flags is the various state
 * that the keyboard is in.
 * This function is common to both gdk and x11 client
 */
static void insert_key(KeySym keysym, KeyCode keycode, int flags, char *command)
{

    Key_Entry *newkey;
    int i, direction=-1;

#if 0
    /* This is at least a meaningless check on my system that results in
     * a compile warning (always false result), so may was well comment it
     * out - MSW 2005-02-09
     */
    if (keycode>MAX_KEYCODE) {
	LOG(LOG_WARNING,"x11::insert_key", "keycode that is passed is greater than 255.");
	keycode=0;	/* hopefully the rest of the data is OK */
    }
#endif
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


#ifdef GDK_XUTIL
#define display GDK_DISPLAY()
#endif

/* This function is common to both gdk and x11 client */

void parse_keybind_line(char *buf, int line, int standard)
{
    char *cp, *cpnext;
    KeySym keysym;
    KeyCode keycode;
    int flags;

    if (buf[0]=='#' || buf[0]=='\n') return;
    if ((cpnext = strchr(buf,' '))==NULL) {
	LOG(LOG_WARNING,"x11::parse_keybind_line","Line %d (%s) corrupted.", line,buf);
	return;
    }
    /* Special keybinding line */
    if (buf[0] == '!') {
	char *cp1;
	while (*cpnext == ' ') ++cpnext;
	cp = strchr(cpnext, ' ');
	if (!cp) {
	    LOG(LOG_WARNING,"x11::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line,buf);
	    return;
	}
	*cp++ = 0;  /* Null terminate it */
	cp1 = strchr(cp, ' ');
	if (!cp1) {
	    LOG(LOG_WARNING,"x11::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line,buf);
	    return;
	}
	*cp1 ++ = 0;/* Null terminate it */
	keycode = atoi(cp1);
	keysym = XStringToKeysym(cp);
	/* As of now, all these keys must have keysyms */
	if (keysym == NoSymbol) {
	    LOG(LOG_WARNING,"x11::parse_keybind_line","Could not convert %s into keysym", cp);
	    return;
	}
	if (!strcmp(cpnext,"commandkey")) {
	    commandkeysym = keysym;
	    commandkey = keycode;
	    return;
	}
	if (!strcmp(cpnext,"firekey0")) {
	    firekeysym[0] = keysym;
	    firekey[0] = keycode;
	    return;
	}
	if (!strcmp(cpnext,"firekey1")) {
	    firekeysym[1] = keysym;
	    firekey[1] = keycode;
	    return;
	}
	if (!strcmp(cpnext,"runkey0")) {
	    runkeysym[0] = keysym;
	    runkey[0] = keycode;
	    return;
	}
	if (!strcmp(cpnext,"runkey1")) {
	    runkeysym[1] = keysym;
	    runkey[1] = keycode;
	    return;
	}
	if (!strcmp(cpnext,"completekey")) {
	    completekeysym = keysym;
	    completekey = keycode;
	    return;
	}
	if (!strcmp(cpnext,"nextkey")) {
	    nextkeysym = keysym;
	    nextkey = keycode;
	    return;
	}
	if (!strcmp(cpnext,"prevkey")) {
	    prevkeysym = keysym;
	    prevkey = keycode;
	    return;
	}
    }
    if (standard) standard=KEYF_STANDARD;
    else standard=0;

    *cpnext++ = '\0';
    keysym = XStringToKeysym(buf);
    cp = cpnext;
    if ((cpnext = strchr(cp,' '))==NULL) {
	LOG(LOG_WARNING,"x11::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line, cp);
	return;
    }
    *cpnext++ = '\0';

    /* If we can, convert the keysym into a keycode.  */
    keycode = atoi(cp);
    cp = cpnext;
    if ((cpnext = strchr(cp,' '))==NULL) {
	LOG(LOG_WARNING,"x11::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line, cp);
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
	    LOG(LOG_WARNING,"x11::parse_keybind_line","Unknown flag (%c) line %d in key binding file",
		*cp, line);
        }
        cp++;
    }

    /* This gets tricky - if we are reading the keycodes from the built
     * in defaults and keycodes are specified there, we want to honor them.
     */
    if ((keysym!=NoSymbol) &&
	(( (keycode == 1) && standard) || (flags&KEYF_STANDARD) || updatekeycodes)) {

        keycode = XKeysymToKeycode(display, keysym);

        /* It is possible that we get a keysym that we can not convert
         * into a keycode (such a case might be binding the key on
         * one system, and later trying to run on another system that
         * doesn't have that key.
         * While the client will not be able to use it this invocation,
         * it may be able to use it in the future.  As such, don't throw
         * it away, but at least print a warning message.
         */
        if (keycode==0) {
	    LOG(LOG_WARNING,"x11::parse_keybind_line","could not convert keysym %s into keycode, ignoring",
		buf);
	}
    }
    /* Rest of the line is the actual command.  Lets kill the newline */
    cpnext[strlen(cpnext)-1]='\0';
    if (strlen(cpnext)>(sizeof(bind_buf)-1)){
        cpnext[sizeof(bind_buf)-1]='\0';
        LOG(LOG_WARNING,"gtk::parse_keybind_line","Had to truncate a too long command");
    }
    insert_key(keysym, keycode, flags | standard, cpnext);
}

/* This code is common to both x11 and gdk client */
static void init_default_keybindings(void)
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
/* This function is common to both x11 and gdk client */

void init_keys(void)
{
    int i, line=0;
    FILE *fp;
    char buf[BIG_BUF];

    commandkeysym = XK_apostrophe;
    commandkey =XKeysymToKeycode(display,XK_apostrophe);
    if (!commandkey) {
      commandkeysym =XK_acute;
      commandkey =XKeysymToKeycode(display, XK_acute);
    }
    firekeysym[0] =XK_Shift_L;
    firekey[0] =XKeysymToKeycode(display, XK_Shift_L);
    firekeysym[1] =XK_Shift_R;
    firekey[1] =XKeysymToKeycode(display, XK_Shift_R);
    runkeysym[0]  =XK_Control_L;
    runkey[0]  =XKeysymToKeycode(display, XK_Control_L);
    runkeysym[1]  =XK_Control_R;
    runkey[1]  =XKeysymToKeycode(display, XK_Control_R);

    completekeysym = XK_Tab;
    completekey = XKeysymToKeycode(display, XK_Tab);
    /* Don't set these to anything by default.  At least on sun
     * keyboards, the keysym for up on both the keypad and arrow
     * keys is the same, so player needs to rebind this so we get proper
     * keycode.  Very unfriendly to log in and not be able to move north/south.
     */
    nextkeysym = NoSymbol;
    nextkey = 0;
    prevkeysym = NoSymbol;
    prevkey = 0;

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
	LOG(LOG_INFO,"x11::init_keys","Could not open ~/.crossfire/keys, trying to load global bindings");
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
    while (fgets(buf, BIG_BUF, fp)) {
	line++;
	parse_keybind_line(buf,line,0);
    }
    fclose(fp);
}

/* The only things we actually care about is the run and fire keys.
 * Other key releases are not important.
 * If it is the release of a run or fire key, we tell the client
 * to stop firing or running.  In some cases, it is possible that we
 * actually are not running or firing, and in such cases, the server
 * will just ignore the command.
 */

/* This code is used by gdk and x11 client, but has
 *  a fair number of #ifdefs to get the right
 * behavioiur
 */
void parse_key_release(KeyCode kc, KeySym ks) {

    /* Only send stop firing/running commands if we are in actual
     * play mode.  Something smart does need to be done when the character
     * enters a non play mode with fire or run mode already set, however.
     */

    if (kc==firekey[0] || ks==firekeysym[0] ||
	kc==firekey[1] || ks==firekeysym[1]) {
		cpl.fire_on=0;
#ifdef GDK_XUTIL
		clear_fire();
		gtk_label_set (GTK_LABEL(fire_label),"    ");
#else
		stop_fire();
		draw_message_window(0);
#endif
	}
    else if (kc==runkey[0] || ks==runkeysym[0] ||
	kc==runkey[1] || ks==runkeysym[1]) {
		cpl.run_on=0;
		if (use_config[CONFIG_ECHO]) draw_info("stop run",NDI_BLACK);
#ifdef GDK_XUTIL
		clear_run();
		gtk_label_set (GTK_LABEL(run_label),"   ");
#else
		stop_run();
		draw_message_window(0);
#endif
	}
    /* Firing is handled on server side.  However, to keep more like the
     * old version, if you release the direction key, you want the firing
     * to stop.  This should do that.
     */
    else if (cpl.fire_on)
	stop_fire();
}

/* This parses a keypress.  It should only be called when in Playing
 * mode.
 */
void parse_key(char key, KeyCode keycode, KeySym keysym, int repeated)
{
    Key_Entry *keyentry, *first_match=NULL;
    int present_flags=0;
    char buf[MAX_BUF];

    if (keycode == commandkey && keysym==commandkeysym) {
	draw_prompt(">");
	cpl.input_state = Command_Mode;
	cpl.no_echo=FALSE;
	return;
    }
    if (keycode == firekey[0] || keysym==firekeysym[0] ||
	keycode == firekey[1] || keysym==firekeysym[1]) {
		cpl.fire_on=1;
#ifdef GDK_XUTIL
		gtk_label_set (GTK_LABEL(fire_label),"Fire");
#else
		draw_message_window(0);
#endif
		return;
	}
    if (keycode == runkey[0] || keysym==runkeysym[0] ||
	keycode==runkey[1] || keysym==runkeysym[1]) {
		cpl.run_on=1;
#ifdef GDK_XUTIL
		gtk_label_set (GTK_LABEL(run_label),"Run");
#else
		draw_message_window(0);
#endif
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
	/* Try to find a prefect match */
	if ((keyentry->flags & KEYF_MODIFIERS)!= present_flags) {
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
#ifdef GDK_XUTIL
	    snprintf(buf, sizeof(buf), "%s", cpl.input_text);
   	    gtk_entry_set_text(GTK_ENTRY(entrytext),buf);
	    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
#else
	    snprintf(buf, sizeof(buf), ">%s", cpl.input_text);
	    draw_prompt(buf);
#endif
	    return;
	}

	if (first_match->direction>=0) {
	    if (cpl.fire_on) {
		snprintf(buf, sizeof(buf), "fire %s", first_match->command);
		fire_dir(first_match->direction);
	    }
	    else if (cpl.run_on) {
		run_dir(first_match->direction);
		snprintf(buf, sizeof(buf), "run %s", first_match->command);
	    }
	    else if (!repeated) {
		strcpy(buf,first_match->command);
		extended_command(first_match->command);
	    }
	    else
		snprintf(buf, sizeof(buf), "move %s (ignored)", first_match->command);
	    if (use_config[CONFIG_ECHO]) draw_info(buf,NDI_BLACK);
	}
        else {
	    if (use_config[CONFIG_ECHO]) draw_info(first_match->command,NDI_BLACK);
	    extended_command(first_match->command);
	}
	return;
    }
    if (key>='0' && key<='9') {
	cpl.count = cpl.count*10 + (key-'0');
	if (cpl.count>100000) cpl.count%=100000;
#ifdef GDK_XUTIL
        gtk_spin_button_set_value (GTK_SPIN_BUTTON(counttext), (float) cpl.count );
#endif
	return;
    }
    snprintf(buf, sizeof(buf), "Key unused (%s%s%s)",
          (cpl.fire_on? "Fire&": ""),
          (cpl.run_on ? "Run&" : ""),
          keysym==NoSymbol? "unknown": XKeysymToString(keysym));
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
    /* bind buf is the maximum space allowed for a
     * binded command. We will add additional datas to
     * it so we increase by MAX_BUF*/
    static char buf[MAX_BUF+sizeof(bind_buf)];
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
	  snprintf(buf, sizeof(buf), "(null) %i %s %s",
		kc,buff, key->command);
	}
	else {
	  snprintf(buf, sizeof(buf), "%s %i %s %s",
		    XKeysymToString(key->keysym), kc,
		    buff, key->command);
	}
    }
    else {
	if(key->keysym == NoSymbol) {
	  snprintf(buf, sizeof(buf), "key (null) (%i) %s %s",
		kc,buff, key->command);
	}
	else {
	  snprintf(buf, sizeof(buf), "key %s (%i) %s %s",
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

  snprintf(buf, sizeof(buf), "Commandkey %s (%d)",
	  commandkeysym==NoSymbol?"unknown":XKeysymToString(commandkeysym),
	  commandkey);
  draw_info(buf,NDI_BLACK);
  snprintf(buf, sizeof(buf), "Firekeys 1: %s (%d), 2: %s (%d)",
	  firekeysym[0]==NoSymbol?"unknown":XKeysymToString(firekeysym[0]), firekey[0],
	  firekeysym[1]==NoSymbol?"unknown":XKeysymToString(firekeysym[1]), firekey[1]);
  draw_info(buf,NDI_BLACK);
  snprintf(buf, sizeof(buf), "Runkeys 1: %s (%d), 2: %s (%d)",
	  runkeysym[0]==NoSymbol?"unknown":XKeysymToString(runkeysym[0]), runkey[0],
	  runkeysym[1]==NoSymbol?"unknown":XKeysymToString(runkeysym[1]), runkey[1]);
  draw_info(buf,NDI_BLACK);

  snprintf(buf, sizeof(buf), "Command Completion Key %s (%d)",
	  completekeysym==NoSymbol?"unknown":XKeysymToString(completekeysym),
	  completekey);
  draw_info(buf,NDI_BLACK);

  snprintf(buf, sizeof(buf), "Next Command in History Key %s (%d)",
	  nextkeysym==NoSymbol?"unknown":XKeysymToString(nextkeysym),
	  nextkey);
  draw_info(buf,NDI_BLACK);

  snprintf(buf, sizeof(buf), "Previous Command in History Key %s (%d)",
	  prevkeysym==NoSymbol?"unknown":XKeysymToString(prevkeysym),
	  prevkey);
  draw_info(buf,NDI_BLACK);


  /* Perhaps we should start at 8, so that we only show 'active'
   * keybindings?
   */
  for (i=0; i<=MAX_KEYCODE; i++) {
    for (key=keys[i]; key!=NULL; key =key->next) {
	if (key->flags & KEYF_STANDARD && !allbindings) continue;

	snprintf(buf, sizeof(buf), "%3d %s", count, get_key_info(key, i, 0));
	draw_info(buf,NDI_BLACK);
	count++;
    }
  }
}




void bind_key(const char *params)
{
  char buf[MAX_BUF];

  if (!params) {
    draw_info("Usage: bind [-nfre] {<commandline>/commandkey/firekey{1/2}/runkey{1/2}/",NDI_BLACK);
    draw_info("           completekey/nextkey/prevkey}",NDI_BLACK);
    return;
  }

  /* Skip over any spaces we may have */
  while (*params==' ') params++;

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

  if (!strcmp(params, "completekey")) {
    bind_keycode = &completekey;
    bind_keysym = &completekeysym;
    draw_info("Push key to bind new command completeion key",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }

  if (!strcmp(params, "prevkey")) {
    bind_keycode = &prevkey;
    bind_keysym = &prevkeysym;
    draw_info("Push key to bind new previous command in history key.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }

  if (!strcmp(params, "nextkey")) {
    bind_keycode = &nextkey;
    bind_keysym = &nextkeysym;
    draw_info("Push key to bind new next command in history key.",NDI_BLACK);
    cpl.input_state = Configure_Keys;
    return;
  }


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
	snprintf(buf, sizeof(buf), "Unknown flag to bind: '%c'", *params);
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

  strncpy(bind_buf, params, sizeof(bind_buf)-1);
  bind_buf[sizeof(bind_buf)-1]=0;
  if (strlen(params) >= sizeof(bind_buf)) {
    draw_info("Keybinding too long! Truncated:",NDI_RED);
    draw_info(bind_buf,NDI_RED);
  }

  snprintf(buf, sizeof(buf), "Push key to bind '%s'.", bind_buf);
  draw_info(buf,NDI_BLACK);
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

static void save_keys(void)
{
    char buf[MAX_BUF], buf2[MAX_BUF];
    int i;
    FILE *fp;

    sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
    if (make_path_to_file(buf)==-1) {
	LOG(LOG_WARNING,"x11::save_keys","Could not create %s", buf);
	return;
    }
    if ((fp=fopen(buf,"w"))==NULL) {
	snprintf(buf2, sizeof(buf2), "Could not open %s, key bindings not saved\n", buf);
	draw_info(buf2,NDI_BLACK);
	return;
    }
    if (commandkeysym != XK_apostrophe && commandkeysym != NoSymbol) {
	fprintf(fp, "! commandkey %s %d\n",
		XKeysymToString(commandkeysym), commandkey);
    }
    if (firekeysym[0] != XK_Shift_L && firekeysym[0] != NoSymbol) {
	fprintf(fp, "! firekey0 %s %d\n",
		XKeysymToString(firekeysym[0]), firekey[0]);
    }
    if (firekeysym[1] != XK_Shift_R && firekeysym[1] != NoSymbol) {
	fprintf(fp, "! firekey1 %s %d\n",
		XKeysymToString(firekeysym[1]), firekey[1]);
    }
    if (runkeysym[0] != XK_Control_L && runkeysym[0] != NoSymbol) {
	fprintf(fp, "! runkey0 %s %d\n",
		XKeysymToString(runkeysym[0]), runkey[0]);
    }
    if (runkeysym[1] != XK_Control_R && runkeysym[1] != NoSymbol) {
	fprintf(fp, "! runkey1 %s %d\n",
		XKeysymToString(runkeysym[1]), runkey[1]);
    }
    if (completekeysym != XK_Tab && completekeysym != NoSymbol) {
	fprintf(fp, "! completekey %s %d\n",
		XKeysymToString(completekeysym), completekey);
    }
    /* No defaults for these, so if it is set to anything, assume its valid */
    if (nextkeysym != NoSymbol) {
	fprintf(fp, "! nextkey %s %d\n",
		XKeysymToString(nextkeysym), nextkey);
    }
    if (prevkeysym != NoSymbol) {
	fprintf(fp, "! prevkey %s %d\n",
		XKeysymToString(prevkeysym), prevkey);
    }

    for (i=0; i<=MAX_KEYCODE; i++) {
	save_individual_key(fp, keys[i], i);
    }
    fclose(fp);
    /* Should probably check return value on all writes to be sure, but... */
    draw_info("key bindings successfully saved.",NDI_BLACK);
}

void configure_keys(KeyCode k, KeySym keysym)
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

  snprintf(buf, sizeof(buf), "Binded to key '%s' (%i)",
	  keysym==NoSymbol?"unknown":XKeysymToString(keysym), (int)k);
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

static void unbind_usage(void)
{
    draw_info("Usage: unbind <entry_number> or",NDI_BLACK);
    draw_info("Usage: unbind [-a] [-g] to show existing bindings", NDI_BLACK);
    draw_info("    -a shows all (global) bindings", NDI_BLACK);
    draw_info("    -g unbinds a global binding", NDI_BLACK);
}

void unbind_key(const char *params)
{
    int count=0, keyentry, onkey,global=0;
    Key_Entry *key, *tmp;
    char buf[MAX_BUF];

    if (params==NULL || params[0]=='\0') {
	show_keys(0);
	return;
    }

    /* Skip over any spaces we may have */
    while (*params==' ') params++;

    if (!strcmp(params,"-a")) {
	show_keys(1);
	return;
    }
    if (!strncmp(params,"-g",2)) {
	global=1;
	if (!(params=strchr(params,' ')))  {
	    unbind_usage();
	    return;
	}
    }
    if ((keyentry=atoi(params))==0) {
	unbind_usage();
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
		LOG(LOG_ERROR,"x11::unbind_key","found number entry, but could not find actual key");
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

    snprintf(buf, sizeof(buf), "Removed binding: %3d %s", count, get_key_info(key, onkey, 0));


    draw_info(buf,NDI_BLACK);
    free(key->command);
    free(key);
    save_keys();
}


/* try to find a face in our private cache.  We return the face
 * number if we find one, -1 for no face match
 */
int  find_face_in_private_cache(char *face, int checksum)
{
    int i;

    for (i=1; i<=last_face_num; i++)
	if (!strcmp(face, private_cache[i].name)) {
	    return i;
	}
    return -1;
}


void image_update_download_status(int start, int end, int total)
{
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), "Downloaded %d of %d images", start, total);

    draw_info(buf,NDI_BLUE);
}

/* Start of map handling code.
 * For the most part, this actually is not window system specific,
 * but certainly how the client wants to store this may vary.
 *
 * At least in the gtk and x11 clients, 95+% of this code
 * is the same - the only think I think is different is
 * that the gtk will call sdl_scroll.  This should
 * probably be moved into the common area.
 */

#define MAXFACES 5
#define MAXPIXMAPNUM 10000


/*
 * Added for fog of war. Current size of the map structure in memory.
 * We assume a rectangular map so this is the length of one side.
 * command.c needs to know about this so not static
 * FIX ME: Don't assume rectangle
 */

PlayerPosition pl_pos;


/**
 * Request a map update from the server. This is to circumvent a bug in some
 * server versions.
 * @todo remove
 */
void reset_map()
{
}

/* x11 client doesn't do smoothing, so don't need to do anything with this */
void addsmooth(uint16 face, uint16 smooth_face)
{
}

/* X11 client doesn't care about this */
void client_tick(uint32 tick)
{
}

/**
 * We get pickup information from server, update our status.
 */
void client_pickup(uint32 pickup)
{
}
