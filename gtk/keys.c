const char *rcsid_gtk_keys_c =
    "$Id$";
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

#include <config.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

/*#include <X11/keysym.h>*/

/* Pick up the gtk headers we need */
#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#define NoSymbol 0L /* Special KeySym */
typedef int KeyCode; /* Undefined type */
#include <io.h> /* access( ) */
#endif
#include <gdk/gdkkeysyms.h>

#include "client-types.h"
#include "gx11.h"
#include "client.h"
#include "p_cmd.h"

#include "def-keys.h"

#include "gtkproto.h"
/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

typedef struct Keys {
    uint8	flags;
    sint8	direction;
    uint32	keysym;
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


static uint32 firekeysym[2], runkeysym[2], commandkeysym,*bind_keysym,
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

/* Platform independence defines that we can't use keycodes.
 * instead, make it a hash, and set KEYHASH to a prime number for
 * this purpose.
 */
#define KEYHASH 257
static Key_Entry *keys[KEYHASH];



/* Updates the keys array with the keybinding that is passed.  All the
 * arguments are pretty self explanatory.  flags is the various state
 * that the keyboard is in.
 * This function is common to both gdk and x11 client
 */
static void insert_key(uint32 keysym, int flags, const char *command)
{

    Key_Entry *newkey;
    int i, direction=-1, slot;

    slot = keysym % KEYHASH;

    if (keys[slot]==NULL) {
	keys[slot]=malloc(sizeof(Key_Entry));
	keys[slot]->command=NULL;
	keys[slot]->next=NULL;
	newkey=keys[slot];
    } else {
	newkey=keys[slot];
	while (newkey->next!=NULL)
	    newkey = newkey->next;
	newkey->next = calloc(1, sizeof(Key_Entry));
	newkey = newkey->next;
    }

    /* Try to find out if the command is a direction command.  If so, we
     * then want to keep track of this fact, so in fire or run mode,
     * things work correctly.
     */
    for (i=0; i<9; i++)
	if (!strcmp(command, directions[i])) {
		direction=i;
		break;
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
    uint32 keysym;
    int flags;

    cp = NULL; /* There may be a rare error case when cp is used uninitialized. So let's be safe */

    if (buf[0]=='#' || buf[0]=='\n') return;
    if ((cpnext = strchr(buf,' '))==NULL) {
	LOG(LOG_WARNING,"gtk::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line,buf);
	return;
    }
    /* Special keybinding line */
    if (buf[0] == '!') {
	char *cp1;
	while (*cpnext == ' ') ++cpnext;
	cp = strchr(cpnext, ' ');
	if (!cp) {
	    LOG(LOG_WARNING,"gtk::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line,buf);
	    return;
	}
	*cp++ = 0;  /* Null terminate it */
	cp1 = strchr(cp, ' ');
	if (!cp1) {
	    LOG(LOG_WARNING,"gtk::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line,buf);
	    return;
	}
	*cp1 ++ = 0;/* Null terminate it */
	keysym = gdk_keyval_from_name(cp);
	/* As of now, all these keys must have keysyms */
	if (keysym == 0) {
	    LOG(LOG_WARNING,"gtk::parse_keybind_line","Could not convert %s into keysym", cp);
	    return;
	}
	if (!strcmp(cpnext,"commandkey")) {
	    commandkeysym = keysym;
	    return;
	}
	if (!strcmp(cpnext,"firekey0")) {
	    firekeysym[0] = keysym;
	    return;
	}
	if (!strcmp(cpnext,"firekey1")) {
	    firekeysym[1] = keysym;
	    return;
	}
	if (!strcmp(cpnext,"runkey0")) {
	    runkeysym[0] = keysym;
	    return;
	}
	if (!strcmp(cpnext,"runkey1")) {
	    runkeysym[1] = keysym;
	    return;
	}
	if (!strcmp(cpnext,"completekey")) {
	    completekeysym = keysym;
	    return;
	}
	if (!strcmp(cpnext,"nextkey")) {
	    nextkeysym = keysym;
	    return;
	}
	if (!strcmp(cpnext,"prevkey")) {
	    prevkeysym = keysym;
	    return;
	}
    } else {
	if (standard) standard=KEYF_STANDARD;
	else standard=0;

	*cpnext++ = '\0';
	keysym = gdk_keyval_from_name(buf);
	if (!keysym) {
	    LOG(LOG_WARNING,"gtk::parse_keybind_line","Unable to convert line %d (%s) into keysym", line, cp);
	    return;
	}
	cp = cpnext;
	if ((cpnext = strchr(cp,' '))==NULL) {
	    LOG(LOG_WARNING,"gtk::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line, cp);
	    return;
	}
	*cpnext++ = '\0';

	cp = cpnext;
	if ((cpnext = strchr(cp,' '))==NULL) {
	    LOG(LOG_WARNING,"gtk::parse_keybind_line","Line %d (%s) corrupted in keybinding file.", line, cp);
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
		    LOG(LOG_WARNING,"gtk::parse_keybind_line","Unknown flag (%c) line %d in key binding file",
			    *cp, line);
	    }
	    cp++;
	}

	/* Rest of the line is the actual command.  Lets kill the newline */
	cpnext[strlen(cpnext)-1]='\0';
    if (strlen(cpnext)>(sizeof(bind_buf)-1)){
        cpnext[sizeof(bind_buf)-1]='\0';
        LOG(LOG_WARNING,"gtk::parse_keybind_line","Had to truncate a too long command");
    }
	insert_key(keysym, flags | standard, cpnext);
    } /* else if not special binding line */
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
    static int was_init = 0;

    commandkeysym = GDK_apostrophe;
    firekeysym[0] =GDK_Shift_L;
    firekeysym[1] =GDK_Shift_R;
    runkeysym[0]  =GDK_Control_L;
    runkeysym[1]  =GDK_Control_R;

    completekeysym = GDK_Tab;

    /* Don't set these to anything by default.  At least on sun
     * keyboards, the keysym for up on both the keypad and arrow
     * keys is the same, so player needs to rebind this so we get proper
     * keycode.  Very unfriendly to log in and not be able to move north/south.
     */
    nextkeysym = NoSymbol;
    prevkeysym = NoSymbol;

    for (i=0; i<KEYHASH; i++) {
        if ( was_init && keys[i] )
        {
            Key_Entry* next;
            Key_Entry* cur = keys[i];
            while ( cur )
            {
                next = cur->next;
                free(cur->command);
                free(cur);
                cur = next;
            }
        }
	keys[i] = NULL;
    }
    was_init = 1;

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

#ifdef MULTKEYS
    /* For Windows, use player name if defined for key file */
    if ( strlen( cpl.name ) )
        {
        sprintf( buf, "%s/.crossfire/%s.keys", getenv( "HOME" ), cpl.name );
        if ( access( buf, 0 ) == -1 )
            {
            /* Client key file not found, reverting to default file */
            sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
            }
        }
    else
        sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
#else
    sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
#endif
    if ((fp=fopen(buf,"r"))==NULL) {
	LOG(LOG_INFO,"gtk::init_keys","Could not open ~/.crossfire/keys, trying to load global bindings");
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
    buf[BIG_BUF-1]='\0';
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
static void parse_key_release(uint32 ks) {

    /* Only send stop firing/running commands if we are in actual
     * play mode.  Something smart does need to be done when the character
     * enters a non play mode with fire or run mode already set, however.
     */

    if (ks==firekeysym[0] || ks==firekeysym[1]) {
	cpl.fire_on=0;
	clear_fire();
	gtk_label_set (GTK_LABEL(fire_label),"    ");
    }
    else if (ks==runkeysym[0] || ks==runkeysym[1]) {
	cpl.run_on=0;
	if (use_config[CONFIG_ECHO]) draw_info("stop run",NDI_BLACK);
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
static void parse_key(char key, uint32 keysym)
{
    Key_Entry *keyentry, *first_match=NULL;
    int present_flags=0;
    char buf[MAX_BUF];

    if (keysym==commandkeysym) {
	if (use_config[CONFIG_SPLITWIN]) {
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
    if (keysym==firekeysym[0] ||keysym==firekeysym[1]) {
	cpl.fire_on=1;
	gtk_label_set (GTK_LABEL(fire_label),"Fire");
	return;
    }
    if (keysym==runkeysym[0] || keysym==runkeysym[1]) {
	cpl.run_on=1;
	gtk_label_set (GTK_LABEL(run_label),"Run");
	return;
    }

    if (cpl.run_on) present_flags |= KEYF_RUN;
    if (cpl.fire_on) present_flags |= KEYF_FIRE;
    if (present_flags ==0) present_flags = KEYF_NORMAL;

    keyentry = keys[keysym % KEYHASH];
    while (keyentry!=NULL) {
	if ((keyentry->keysym!=0 && keyentry->keysym!=keysym) ||
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
	if (first_match->flags & KEYF_EDIT) {
	    strcpy(cpl.input_text, first_match->command);
	    cpl.input_state = Command_Mode;
   	    gtk_entry_set_text(GTK_ENTRY(entrytext),cpl.input_text);
	    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
#ifdef CFGTK2
        gtk_editable_select_region(GTK_EDITABLE(entrytext),strlen(cpl.input_text),-1);
#endif
	    return;
	}

	if (first_match->direction>=0) {
	    if (cpl.fire_on) {
		sprintf(buf,"fire %s", first_match->command);
		/* Some spells (dimension door) need a valid count value */
		cpl.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(counttext));
		fire_dir(first_match->direction);
	    }
	    else if (cpl.run_on) {
		run_dir(first_match->direction);
		sprintf(buf,"run %s", first_match->command);
	    }
	    else {
		extended_command(first_match->command);
	    }
	    if (use_config[CONFIG_ECHO]) draw_info(first_match->command,NDI_BLACK);
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
        gtk_spin_button_set_value (GTK_SPIN_BUTTON(counttext), (float) cpl.count );
	return;
    }
    sprintf(buf, "Key unused (%s%s%s)",
          (cpl.fire_on? "Fire&": ""),
          (cpl.run_on ? "Run&" : ""),
          keysym==NoSymbol? "unknown": gdk_keyval_name(keysym));
#ifdef WIN32
       if ( ( 65513 != keysym ) && ( 65511 != keysym ) )
#endif
    draw_info(buf,NDI_BLACK);
    cpl.count=0;
}


/* This returns a character string desribing the key. */
/* If save_mode is true, it means that the format used for saving
 * the information is used, instead of the usual format for displaying
 * the information in a friendly manner.
 */
static char * get_key_info(Key_Entry *key, int save_mode)
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
	  sprintf(buf, "(null) %i %s %s",
		0,buff, key->command);
	}
	else {
	  sprintf(buf, "%s %i %s %s",
		    gdk_keyval_name(key->keysym), 0,
		    buff, key->command);
	}
    }
    else {
	if(key->keysym == NoSymbol) {
	  sprintf(buf, "key (null) %s %s",
		buff, key->command);
	}
	else {
	  sprintf(buf, "key %s %s %s",
		    gdk_keyval_name(key->keysym),
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
    char buf[MAX_BUF + sizeof( bind_buf )];

  sprintf(buf, "Commandkey %s",
	  commandkeysym==NoSymbol?"unknown":gdk_keyval_name(commandkeysym));
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Firekeys 1: %s, 2: %s",
	  firekeysym[0]==NoSymbol?"unknown":gdk_keyval_name(firekeysym[0]),
	  firekeysym[1]==NoSymbol?"unknown":gdk_keyval_name(firekeysym[1]));
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Runkeys 1: %s, 2: %s",
	  runkeysym[0]==NoSymbol?"unknown":gdk_keyval_name(runkeysym[0]),
	  runkeysym[1]==NoSymbol?"unknown":gdk_keyval_name(runkeysym[1]));
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Command Completion Key %s",
	  completekeysym==NoSymbol?"unknown":gdk_keyval_name(completekeysym));
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Next Command in History Key %s",
	  nextkeysym==NoSymbol?"unknown":gdk_keyval_name(nextkeysym));
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Previous Command in History Key %s",
	  prevkeysym==NoSymbol?"unknown":gdk_keyval_name(prevkeysym));
  draw_info(buf,NDI_BLACK);


  /* Perhaps we should start at 8, so that we only show 'active'
   * keybindings?
   */
  for (i=0; i<KEYHASH; i++) {
	for (key=keys[i]; key!=NULL; key =key->next) {
	    if (key->flags & KEYF_STANDARD && !allbindings) continue;

	    sprintf(buf,"%3d %s",count,  get_key_info(key,0));
	    draw_info(buf,NDI_BLACK);
	    count++;
	}
  }
}



void bind_key(const char *params)
{
    /* Must have enough room for MAX_BUF and 'Push key to bind to ''.' */
    char buf[MAX_BUF + 20];

    if (!params) {
	draw_info("Usage: bind [-nfre] {<commandline>/commandkey/firekey{1/2}/runkey{1/2}/",NDI_BLACK);
	draw_info("           completekey/nextkey/prevkey}",NDI_BLACK);
	return;
    }

    /* Skip over any spaces we may have */
    while (*params==' ') params++;

    if (!strcmp(params, "commandkey")) {
	bind_keysym = &commandkeysym;
	draw_info("Push key to bind new commandkey.",NDI_BLACK);
	cpl.input_state = Configure_Keys;
	return;
    }

    if (!strcmp(params, "firekey1")) {
	bind_keysym = & firekeysym[0];
    draw_info("Push key to bind new firekey 1.",NDI_BLACK);
	cpl.input_state = Configure_Keys;
	return;
    }
    if (!strcmp(params, "firekey2")) {
	bind_keysym = & firekeysym[1];
	draw_info("Push key to bind new firekey 2.",NDI_BLACK);
	cpl.input_state = Configure_Keys;
	return;
    }
    if (!strcmp(params, "runkey1")) {
	bind_keysym = &runkeysym[0];
	draw_info("Push key to bind new runkey 1.",NDI_BLACK);
	cpl.input_state = Configure_Keys;
	return;
    }

    if (!strcmp(params, "runkey2")) {
	bind_keysym = &runkeysym[1];
	draw_info("Push key to bind new runkey 2.",NDI_BLACK);
	cpl.input_state = Configure_Keys;
	return;
    }

    if (!strcmp(params, "completekey")) {
	bind_keysym = &completekeysym;
	draw_info("Push key to bind new command completeion key",NDI_BLACK);
	cpl.input_state = Configure_Keys;
	return;
    }

    if (!strcmp(params, "prevkey")) {
	bind_keysym = &prevkeysym;
	draw_info("Push key to bind new previous command in history key.",NDI_BLACK);
	cpl.input_state = Configure_Keys;
	return;
    }

    if (!strcmp(params, "nextkey")) {
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

    /* params is read only, so we need to the truncation on
     * the buffer we will store it in, not params
     */
    strncpy(bind_buf, params, sizeof(bind_buf)-1);
    bind_buf[sizeof(bind_buf)-1] = 0;

    if (strlen(params) >= sizeof(bind_buf)) {
	draw_info("Keybinding too long! Truncated:",NDI_RED);
	draw_info(bind_buf,NDI_RED);
    }

    sprintf(buf, "Push key to bind '%s'.", bind_buf);
    draw_info(buf,NDI_BLACK);

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
    fprintf(fp, "%s\n", get_key_info(key, 1));
    save_individual_key(fp, key->next, kc);
}

static void save_keys(void)
{
    char buf[MAX_BUF], buf2[MAX_BUF];
    int i;
    FILE *fp;

#ifdef MULTKEYS
    /* Use player's name if available */
    if ( strlen( cpl.name ) )
        sprintf( buf,"%s/.crossfire/%s.keys", getenv("HOME"), cpl.name );
    else
        sprintf( buf,"%s/.crossfire/keys", getenv("HOME") );
#else
    sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
#endif

    if (make_path_to_file(buf)==-1) {
	LOG(LOG_WARNING,"gtk::save_keys","Could not create %s", buf);
	return;
    }
    if ((fp=fopen(buf,"w"))==NULL) {
	sprintf(buf2,"Could not open %s, key bindings not saved\n", buf);
	draw_info(buf2,NDI_BLACK);
	return;
    }
    if (commandkeysym != GDK_apostrophe && commandkeysym != NoSymbol) {
	fprintf(fp, "! commandkey %s %d\n",
		gdk_keyval_name(commandkeysym), 0);
    }
    if (firekeysym[0] != GDK_Shift_L && firekeysym[0] != NoSymbol) {
	fprintf(fp, "! firekey0 %s %d\n",
		gdk_keyval_name(firekeysym[0]), 0);
    }
    if (firekeysym[1] != GDK_Shift_R && firekeysym[1] != NoSymbol) {
	fprintf(fp, "! firekey1 %s %d\n",
		gdk_keyval_name(firekeysym[1]), 0);
    }
    if (runkeysym[0] != GDK_Control_L && runkeysym[0] != NoSymbol) {
	fprintf(fp, "! runkey0 %s %d\n",
		gdk_keyval_name(runkeysym[0]), 0);
    }
    if (runkeysym[1] != GDK_Control_R && runkeysym[1] != NoSymbol) {
	fprintf(fp, "! runkey1 %s %d\n",
		gdk_keyval_name(runkeysym[1]), 0);
    }
    if (completekeysym != GDK_Tab && completekeysym != NoSymbol) {
	fprintf(fp, "! completekey %s %d\n",
		gdk_keyval_name(completekeysym), 0);
    }
    /* No defaults for these, so if it is set to anything, assume its valid */
    if (nextkeysym != NoSymbol) {
	fprintf(fp, "! nextkey %s %d\n",
		gdk_keyval_name(nextkeysym), 0);
    }
    if (prevkeysym != NoSymbol) {
	fprintf(fp, "! prevkey %s %d\n",
		gdk_keyval_name(prevkeysym), 0);
    }

    for (i=0; i<KEYHASH; i++) {
    save_individual_key(fp, keys[i], 0);
    }
    fclose(fp);
    /* Should probably check return value on all writes to be sure, but... */
    draw_info("key bindings successfully saved.",NDI_BLACK);
}

static void configure_keys(uint32 keysym)
{
    char buf[MAX_BUF];

    /* I think that basically if we are not rebinding the special
     * control keys (in which case bind_kesym would be set to something)
     * we just want to handle these keypresses as normal events.
     */
    if (bind_keysym==NULL) {
	if(keysym == firekeysym[0] || keysym == firekeysym[1]) {
	    cpl.fire_on =1;
	    draw_message_window(0);
	    return;
	}
	if(keysym == runkeysym[0] || keysym == runkeysym[1]) {
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

    if (bind_keysym!=NULL) {
        *bind_keysym=keysym;
        bind_keysym=NULL;
    }
    else {
	insert_key(keysym, bind_flags, bind_buf);
    }

    sprintf(buf, "Binded to key '%s' (%i)",
	  keysym==NoSymbol?"unknown":gdk_keyval_name(keysym), keysym);
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
    /* the key macro itself can be bind_buf long, and we need some extra
     * space for the sprintf unbind message.
     */
    char buf[sizeof(bind_buf)+60];

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

    for (onkey=0; onkey<KEYHASH; onkey++) {
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
		LOG(LOG_ERROR,"gtk::unbind_key","found number entry, but could not find actual key");
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

    sprintf(buf,"Removed binding: %3d %s", count, get_key_info(key,0));

    draw_info(buf,NDI_BLACK);
    free(key->command);
    free(key);
    save_keys();
}

void keyrelfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window)
{

    updatelock=0;
    if (event->keyval>0) {
	if (GTK_WIDGET_HAS_FOCUS (entrytext) /*|| GTK_WIDGET_HAS_FOCUS(counttext)*/ ) {
	} else {
	    parse_key_release(event->keyval);
	    gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_release_event") ;
	}
    }
}


void keyfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
    char *text;
    updatelock=0;

    if (!use_config[CONFIG_POPUPS]) {
	if  (cpl.input_state == Reply_One) {
	    text=gdk_keyval_name(event->keyval);
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
    if (event->keyval<=0) return;

    if (GTK_WIDGET_HAS_FOCUS (entrytext) /*|| GTK_WIDGET_HAS_FOCUS(counttext)*/) {
	if (event->keyval == completekeysym) {
        gtk_complete_command();
        return;
    }
	if (event->keyval == prevkeysym || event->keyval == nextkeysym)
	    gtk_command_history(event->keyval==nextkeysym?0:1);
#ifdef CFGTK2
    else
        gtk_widget_event(GTK_WIDGET(entrytext), (GdkEvent*)event);
#endif
    } else {
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
			/*printf ("Run is on while ctrl is not\n");*/
			gtk_label_set (GTK_LABEL(run_label),"   ");
			cpl.run_on=0;
			stop_run();
		    }
		}
		if (cpl.fire_on) {
		    if (!(event->state & GDK_SHIFT_MASK)) {
			/* printf ("Fire is on while shift is not\n");*/
			gtk_label_set (GTK_LABEL(fire_label),"   ");
			cpl.fire_on=0;
			stop_fire();
		    }
		}

		if( (event->state & GDK_CONTROL_MASK) && (event->state & GDK_SHIFT_MASK) &&
		   (event->keyval == GDK_i || event->keyval == GDK_I) ) {
		    reset_map();
		}


		parse_key(event->string[0], event->keyval);
		gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event") ;
		break;

	    case Configure_Keys:
		configure_keys(event->keyval);
		gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event") ;
		break;

	    case Command_Mode:
		if (event->keyval == completekeysym) gtk_complete_command();
		if (event->keyval == prevkeysym || event->keyval == nextkeysym)
		gtk_command_history(event->keyval==nextkeysym?0:1);
		else {
		    gtk_widget_grab_focus (GTK_WIDGET(entrytext));
		    /* When running in split windows mode, entrytext can't get focus because
		     * it is in a different window.  So we have to pass the event to it
		     * explicitly
		     */
		    if (GTK_WIDGET_HAS_FOCUS(entrytext)==0)
			gtk_widget_event(GTK_WIDGET(entrytext), (GdkEvent*)event);
		}
		/*
		 * Don't pass signal along to default handlers - otherwise, we get
		 * get crashes in the clist area (gtk fault I believe)
		 */
		gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event") ;
		break;

	    case Metaserver_Select:
		gtk_widget_grab_focus (GTK_WIDGET(entrytext));
		break;

	    default:
		LOG(LOG_ERROR,"gtk::keyfunc","Unknown input state: %d", cpl.input_state);
	}

    }
}



void draw_keybindings (GtkWidget *keylist) {
    int i, count=1;
    Key_Entry *key;
    int allbindings=0;
    char buff[MAX_BUF];
    int bi=0;
    char buffer[5][MAX_BUF];
    char *buffers[5];
    gint tmprow;

    gtk_clist_clear (GTK_CLIST(keylist));
    for (i=0; i<KEYHASH; i++) {
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

	    if(key->keysym != NoSymbol) {
		sprintf(buffer[0], "%i",count);
		sprintf(buffer[1], "%s", gdk_keyval_name(key->keysym));
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
	    count++;
	}
    }
}

void bind_callback (GtkWidget *gtklist, GdkEventButton *event) {
    KeySym keysym;
    const gchar *entry_text;
    const gchar *cpnext;
    const gchar *mod="";
    char buf[MAX_BUF];

    bind_flags = KEYF_MODIFIERS;

    if ((bind_flags & KEYF_MODIFIERS)==KEYF_MODIFIERS) {
	bind_flags &= ~KEYF_MODIFIERS;
	mod=gtk_entry_get_text (GTK_ENTRY(cmodentrytext));
	if (!strcmp(mod, "F")) {
	    bind_flags |= KEYF_FIRE;
	}
	else if (!strcmp(mod, "R")) {
	    bind_flags |= KEYF_RUN;
	}
	else if (!strcmp(mod, "A")) {
	    bind_flags |= KEYF_MODIFIERS;
	}
    }
    cpnext = gtk_entry_get_text (GTK_ENTRY(ckentrytext));
    entry_text = gtk_entry_get_text (GTK_ENTRY(ckeyentrytext));

    keysym = gdk_keyval_from_name(entry_text);
    insert_key(keysym, bind_flags, cpnext);
    save_keys();
    draw_keybindings (cclist);
    sprintf(buf, "Binded to key '%s' (%i)", gdk_keyval_name(keysym), 0);
    draw_info(buf,NDI_BLACK);
}

void ckeyunbind (GtkWidget *gtklist, GdkEventButton *event) {
    gchar *buf;
    GList *node;
    node =  GTK_CLIST(cclist)->selection;

    if (node) {
	/* this line generates an warning about mismatched pointer sizes.  Not sure
	 * if there is any good fix for it.
	 * In addition, this appears to be using unsupported logic -
	 * proper programming logic should not be accessing the clist->selection
	 * data directly.  Better approach would probably be to catch the selection
	 * in the clist, then store away what as selected.
	 */
	gtk_clist_get_text (GTK_CLIST(cclist), (gint)node->data, 0, &buf);
	unbind_key(buf);
	draw_keybindings (cclist);
    }
}

void ckeyentry_callback (GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
  gtk_entry_set_text (GTK_ENTRY(ckeyentrytext),  gdk_keyval_name(event->keyval));

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
  /*  gdk_keyval_name(event->keyval);*/
  gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event");
}


void ckeyclear () {
  gtk_label_set (GTK_LABEL(cnumentrytext), "0");
  gtk_entry_set_text (GTK_ENTRY(ckeyentrytext), "Press key to bind here");
  /*  gtk_entry_set_text (GTK_ENTRY(cknumentrytext), ""); */
  gtk_entry_set_text (GTK_ENTRY(cmodentrytext), "");
  gtk_entry_set_text (GTK_ENTRY(ckentrytext), "");
}
