/*
 * static char *rcsid_keys_c =
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

/* This file handles most of the keyboard related functions - binding
 * and unbinding keys, and handling keypresses and looking up the
 * keys.
 */

#include "config.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/keysym.h>

/* Pick up the gtk headers we need */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include "client-types.h"
#include "gx11.h"
#include "client.h"

#include "def-keys.h"

#include "gtkproto.h"
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



/* Updates the keys array with the keybinding that is passed.  All the
 * arguments are pretty self explanatory.  flags is the various state
 * that the keyboard is in.
 * This function is common to both gdk and x11 client
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


/* This function is common to both gdk and x11 client */

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
    /* Special keybinding line */
    if (buf[0] == '!') {
	char *cp1;
	while (*cpnext == ' ') ++cpnext;
	cp = strchr(cpnext, ' ');
	if (!cp) {
	    fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line,buf);
	    return;
	}
	*cp++ = 0;  /* Null terminate it */
	cp1 = strchr(cp, ' ');
	if (!cp1) {
	    fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line,buf);
	    return;
	}
	*cp1 ++ = 0;/* Null terminate it */
	keycode = atoi(cp1);
	keysym = XStringToKeysym(cp);
	/* As of now, all these keys must have keysyms */
	if (keysym == NoSymbol) {
	    fprintf(stderr,"Could not convert %s into keysym\n", cp);
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
	fprintf(stderr,"Line %d (%s) corrupted in keybinding file.\n", line, cp);
	return;
    }
    *cpnext++ = '\0';

    /* If we can, convert the keysym into a keycode.  */
    keycode = atoi(cp);
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

    /* This gets tricky - if we are reading the keycodes from the built
     * in defaults and keycodes are specified there, we want to honor them.
     */
    if ((keysym!=NoSymbol) &&
	(( (keycode == 1) && standard) || (flags&KEYF_STANDARD) || updatekeycodes)) {

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
    /* Rest of the line is the actual command.  Lets kill the newline */
    cpnext[strlen(cpnext)-1]='\0';
    insert_key(keysym, keycode, flags | standard, cpnext);
}

/* This code is common to both x11 and gdk client */
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
/* This function is common to both x11 and gdk client */

void init_keys()
{
    int i, line=0;
    FILE *fp;
    char buf[BIG_BUF];

    commandkeysym = XK_apostrophe;
    commandkey =XKeysymToKeycode(GDK_DISPLAY(),XK_apostrophe);
    if (!commandkey) {
      commandkeysym =XK_acute;
      commandkey =XKeysymToKeycode(GDK_DISPLAY(), XK_acute);
    }
    firekeysym[0] =XK_Shift_L;
    firekey[0] =XKeysymToKeycode(GDK_DISPLAY(), XK_Shift_L);
    firekeysym[1] =XK_Shift_R;
    firekey[1] =XKeysymToKeycode(GDK_DISPLAY(), XK_Shift_R);
    runkeysym[0]  =XK_Control_L;
    runkey[0]  =XKeysymToKeycode(GDK_DISPLAY(), XK_Control_L);
    runkeysym[1]  =XK_Control_R;
    runkey[1]  =XKeysymToKeycode(GDK_DISPLAY(), XK_Control_R);

    completekeysym = XK_Tab;
    completekey = XKeysymToKeycode(GDK_DISPLAY(), XK_Tab);
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
static void parse_key_release(KeyCode kc, KeySym ks) {

    /* Only send stop firing/running commands if we are in actual
     * play mode.  Something smart does need to be done when the character
     * enters a non play mode with fire or run mode already set, however.
     */

    if (kc==firekey[0] || ks==firekeysym[0] || 
	kc==firekey[1] || ks==firekeysym[1]) {
		cpl.fire_on=0;
		clear_fire();
		gtk_label_set (GTK_LABEL(fire_label),"    ");
	}
    else if (kc==runkey[0] || ks==runkeysym[0] ||
	kc==runkey[1] || ks==runkeysym[1]) {
		cpl.run_on=0;
		if (cpl.echo_bindings) draw_info("stop run",NDI_BLACK);
		clear_run();
		gtk_label_set (GTK_LABEL(run_label),"   ");
	}
    /* Firing is handled on server side.  However, to keep more like the
     * old version, if you release the direction key, you want the firing
     * to stop.  This should do that.
     */
    else if (cpl.fire_on) 
	clear_fire();
}

/* This parses a keypress.  It should only be called when in Playing
 * mode.
 */
static void parse_key(char key, KeyCode keycode, KeySym keysym)
{
    Key_Entry *keyentry, *first_match=NULL;
    int present_flags=0;
    char buf[MAX_BUF];

    if (keycode == commandkey && keysym==commandkeysym) {
	if (split_windows) {
	    gtk_widget_grab_focus (GTK_WIDGET(gtkwin_info));
	    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
	} else {
	    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
	}

	gtk_entry_set_visibility(GTK_ENTRY(entrytext), 1);
	cpl.input_state = Command_Mode;
	cpl.no_echo=FALSE;
	return;
    }
    if (keycode == firekey[0] || keysym==firekeysym[0] ||
	keycode == firekey[1] || keysym==firekeysym[1]) {
		cpl.fire_on=1;
		gtk_label_set (GTK_LABEL(fire_label),"Fire");
		return;
	}
    if (keycode == runkey[0] || keysym==runkeysym[0] ||
	keycode==runkey[1] || keysym==runkeysym[1]) {
		cpl.run_on=1;
		gtk_label_set (GTK_LABEL(run_label),"Run");
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
		extended_command(first_match->command);
	    }
	    if (cpl.echo_bindings) draw_info(buf,NDI_BLACK);
	}
        else {
	    if (cpl.echo_bindings) draw_info(first_match->command,NDI_BLACK);
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

  sprintf(buf, "Commandkey %s (%d)", 
	  commandkeysym==NoSymbol?"unknown":XKeysymToString(commandkeysym),
	  commandkey);
  draw_info(buf,NDI_BLACK);
  sprintf(buf, "Firekeys 1: %s (%d), 2: %s (%d)",
	  firekeysym[0]==NoSymbol?"unknown":XKeysymToString(firekeysym[0]), firekey[0],
	  firekeysym[1]==NoSymbol?"unknown":XKeysymToString(firekeysym[1]), firekey[1]);
  draw_info(buf,NDI_BLACK);
  sprintf(buf, "Runkeys 1: %s (%d), 2: %s (%d)",
	  runkeysym[0]==NoSymbol?"unknown":XKeysymToString(runkeysym[0]), runkey[0],
	  runkeysym[1]==NoSymbol?"unknown":XKeysymToString(runkeysym[1]), runkey[1]);
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Command Completion Key %s (%d)", 
	  completekeysym==NoSymbol?"unknown":XKeysymToString(completekeysym),
	  completekey);
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Next Command in History Key %s (%d)", 
	  nextkeysym==NoSymbol?"unknown":XKeysymToString(nextkeysym),
	  nextkey);
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Previous Command in History Key %s (%d)", 
	  prevkeysym==NoSymbol?"unknown":XKeysymToString(prevkeysym),
	  prevkey);
  draw_info(buf,NDI_BLACK);


  /* Perhaps we should start at 8, so that we only show 'active'
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
    if (make_path_to_file(buf)==-1) {
	fprintf(stderr,"Could not create %s\n", buf);
	return;
    }
    if ((fp=fopen(buf,"w"))==NULL) {
	sprintf(buf2,"Could not open %s, key bindings not saved\n", buf);
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

  sprintf(buf, "Binded to key '%s' (%i)", 
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

static void unbind_usage()
{
    draw_info("Usage: unbind <entry_number> or",NDI_BLACK);
    draw_info("Usage: unbind [-a] [-g] to show existing bindings", NDI_BLACK);
    draw_info("    -a shows all (global) bindings", NDI_BLACK);
    draw_info("    -g unbinds a global binding", NDI_BLACK);
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

void keyrelfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
  
  updatelock=0;
  if (event->keyval>0) {
    if (GTK_WIDGET_HAS_FOCUS (entrytext) /*|| GTK_WIDGET_HAS_FOCUS(counttext)*/ ) {
    } else {
      parse_key_release(XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
      gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_release_event") ;
    }
  }
}


void keyfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
  char *text;
  updatelock=0;

  if (nopopups) {
    if  (cpl.input_state == Reply_One) {
	text=XKeysymToString(event->keyval);
	send_reply(text);
	cpl.input_state = Playing;
	return;
    }
    else if (cpl.input_state == Reply_Many) {
	gtk_widget_grab_focus (GTK_WIDGET(entrytext));
	return;
    }
  }

  /* Better check for really weirdo keys, X doesnt like keyval 0*/
  if (event->keyval>0) {
    if (GTK_WIDGET_HAS_FOCUS (entrytext) /*|| GTK_WIDGET_HAS_FOCUS(counttext)*/) {
	if (event->keyval == completekeysym) gtk_complete_command();
	if (event->keyval == prevkeysym || event->keyval == nextkeysym) 
	    gtk_command_history(event->keyval==nextkeysym?0:1);
    }  else {
      
      switch(cpl.input_state) {
      case Playing:
	/* Specials - do command history - many times, the player
	 * will want to go the previous command when nothing is entered
	 * in the command window.
	 */
	if (event->keyval == prevkeysym || event->keyval == nextkeysym) {
	    gtk_command_history(event->keyval==nextkeysym?0:1);
	    return;
	}

	if (cpl.run_on) {
	  if (!(event->state & GDK_CONTROL_MASK)) {
/*	    printf ("Run is on while ctrl is not\n");*/
	    gtk_label_set (GTK_LABEL(run_label),"   ");
	    cpl.run_on=0;
	    stop_run();
	  }
	}
	if (cpl.fire_on) {
	  if (!(event->state & GDK_SHIFT_MASK)) {
/*	    printf ("Fire is on while shift is not\n");*/
	    gtk_label_set (GTK_LABEL(fire_label),"   ");
	    cpl.fire_on=0;
	    stop_fire();
	  }
	}

	if( (event->state & GDK_CONTROL_MASK) && (event->state & GDK_SHIFT_MASK) && 
	    (event->keyval == GDK_i || event->keyval == GDK_I) ) {
	    reset_map();
	}
	
	
	parse_key(event->string[0], XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
	gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event") ;
	break;
      case Configure_Keys:
	configure_keys(XKeysymToKeycode(GDK_DISPLAY(), event->keyval), event->keyval);
	gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event") ;
	break;
      case Command_Mode:
	if (event->keyval == completekeysym) gtk_complete_command();
	if (event->keyval == prevkeysym || event->keyval == nextkeysym) 
	    gtk_command_history(event->keyval==nextkeysym?0:1);
	else gtk_widget_grab_focus (GTK_WIDGET(entrytext));

      case Metaserver_Select:
	gtk_widget_grab_focus (GTK_WIDGET(entrytext));
      break;
      default:
	fprintf(stderr,"Unknown input state: %d\n", cpl.input_state);
      }
      
    }
    
  }
}



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

void ckeyentry_callback (GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
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


void ckeyclear () {
  gtk_label_set (GTK_LABEL(cnumentrytext), "0"); 
  gtk_entry_set_text (GTK_ENTRY(ckeyentrytext), "Press key to bind here"); 
  /*  gtk_entry_set_text (GTK_ENTRY(cknumentrytext), ""); */
  gtk_entry_set_text (GTK_ENTRY(cmodentrytext), ""); 
  gtk_entry_set_text (GTK_ENTRY(ckentrytext), ""); 
}

