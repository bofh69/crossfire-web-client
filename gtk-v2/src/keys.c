char *rcsid_gtk2_keys_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005 Mark Wedel & Crossfire Development Team

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

    The author can be reached via e-mail to crossfire@metalforge.org
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



/* Pick up the gtk headers we need */
#include <gtk/gtk.h>
#include <glade/glade.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#define NoSymbol 0L /* Special KeySym */
typedef int KeyCode; /* Undefined type */
#endif
#include <gdk/gdkkeysyms.h>

#include "client-types.h"
#include "main.h"
#include "client.h"
#include "proto.h"
#include "def-keys.h"

#include "image.h"
#include "gtk2proto.h"
#include "p_cmd.h"

static GtkWidget *fire_label, *run_label, *keybinding_window, *keybinding_checkbutton_control,
    *keybinding_checkbutton_shift, *keybinding_checkbutton_alt, *keybinding_checkbutton_meta,
    *keybinding_checkbutton_edit, *keybinding_entry_key, *keybinding_entry_command,
    *keybinding_treeview, *keybinding_button_remove, *keybinding_button_update,
    *keybinding_button_bind;
static GtkListStore    *keybinding_store;
static GtkTreeSelection  *keybinding_selection;

/* Changed to KLIST_* to avoid conflicts in Win2000 and up */
enum {
    KLIST_ENTRY, KLIST_KEY, KLIST_MODS, KLIST_EDIT, KLIST_COMMAND, KLIST_KEY_ENTRY
};

GtkWidget *spinbutton_count;
GtkWidget *entry_commands;

#define MAX_HISTORY 50
#define MAX_COMMAND_LEN 256
char history[MAX_HISTORY][MAX_COMMAND_LEN];
static int cur_history_position=0, scroll_history_position=0;

/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

typedef struct Keys {
    uint8       flags;
    sint8       direction;
    uint32      keysym;
    char        *command;
    struct Keys *next;
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


static uint32 firekeysym[2], runkeysym[2], commandkeysym, *bind_keysym,
    prevkeysym, nextkeysym, completekeysym, altkeysym[2], metakeysym[2];
static int bind_flags=0;
static char bind_buf[MAX_BUF];

#define KEYF_NORMAL     0x01    /* Used in normal mode */
#define KEYF_FIRE       0x02    /* Used in fire mode */
#define KEYF_RUN        0x04    /* Used in run mode */
#define KEYF_EDIT       0x08    /* Line editor */
#define KEYF_STANDARD   0x10    /* For standard (built in) key definitions */
#define KEYF_ALT        0x20    /* For ALT key modifier */
#define KEYF_META       0x40    /* For Meta key modifier */

#define KEYF_MODIFIERS  0x67    /* Mask for actual keyboard modifiers, */
                                /* not action modifiers */

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
        if (!strcmp(cpnext,"altkey0")) {
            altkeysym[0] = keysym;
            return;
        }
        if (!strcmp(cpnext,"altkey1")) {
            altkeysym[1] = keysym;
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
        if (!strcmp(cpnext,"metakey0")) {
            metakeysym[0] = keysym;
            return;
        }
        if (!strcmp(cpnext,"metakey1")) {
            metakeysym[1] = keysym;
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
                case 'E':
                    flags |= KEYF_EDIT;
                    break;
                case 'F':
                    flags |= KEYF_FIRE;
                    break;
                case 'L':   /* A is used, so using L for alt */
                    flags |= KEYF_ALT;
                    break;
                case 'M':
                    flags |= KEYF_META;
                    break;
                case 'N':
                    flags |= KEYF_NORMAL;
                    break;
                case 'R':
                    flags |= KEYF_RUN;
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

void keys_init(GtkWidget *window_root)
{
    int i, line=0;
    FILE *fp;
    char buf[BIG_BUF];
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GladeXML *xml_tree;
    GtkWidget *widget;

    for (i = 0; i<MAX_HISTORY; i++)
        history[i][0]=0;

    commandkeysym = GDK_apostrophe;
    firekeysym[0] =GDK_Shift_L;
    firekeysym[1] =GDK_Shift_R;
    runkeysym[0]  =GDK_Control_L;
    runkeysym[1]  =GDK_Control_R;
    metakeysym[0] =GDK_Meta_L;
    metakeysym[1] =GDK_Meta_R;
    altkeysym[0] =GDK_Alt_L;
    altkeysym[1] =GDK_Alt_R;

    completekeysym = GDK_Tab;

    /* Don't set these to anything by default.  At least on sun
     * keyboards, the keysym for up on both the keypad and arrow
     * keys is the same, so player needs to rebind this so we get proper
     * keycode.  Very unfriendly to log in and not be able to move north/south.
     */
    nextkeysym = NoSymbol;
    prevkeysym = NoSymbol;

    for (i=0; i<KEYHASH; i++) {
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

#ifdef WIN32
    /* For Windows, use player name if defined for key file */
    if ( strlen( cpl.name ) )
        sprintf( buf, "%s/.crossfire/%s.keys", getenv( "HOME" ), cpl.name );
    else
        sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
#else
    sprintf(buf,"%s/.crossfire/keys", getenv("HOME"));
#endif

    xml_tree = glade_get_widget_tree(GTK_WIDGET(window_root));

    fire_label = glade_xml_get_widget(xml_tree, "fire_label");
    run_label = glade_xml_get_widget(xml_tree, "run_label");
    entry_commands = glade_xml_get_widget(xml_tree, "entry_commands");
    spinbutton_count = glade_xml_get_widget(xml_tree, "spinbutton_count");

    g_signal_connect ((gpointer) entry_commands, "activate",
        G_CALLBACK (on_entry_commands_activate), NULL);

    keybinding_window = glade_xml_get_widget(dialog_xml, "keybinding_window");
    xml_tree = glade_get_widget_tree(GTK_WIDGET(keybinding_window));

    keybinding_checkbutton_control =
        glade_xml_get_widget(xml_tree, "keybinding_checkbutton_control");
    keybinding_checkbutton_shift =
        glade_xml_get_widget(xml_tree, "keybinding_checkbutton_shift");
    keybinding_checkbutton_alt =
        glade_xml_get_widget(xml_tree, "keybinding_checkbutton_alt");
    keybinding_checkbutton_meta =
        glade_xml_get_widget(xml_tree, "keybinding_checkbutton_meta");
    keybinding_checkbutton_edit =
        glade_xml_get_widget(xml_tree, "keybinding_checkbutton_stayinedit");
    keybinding_entry_key =
        glade_xml_get_widget(xml_tree, "keybinding_entry_key");
    keybinding_entry_command =
        glade_xml_get_widget(xml_tree, "keybinding_entry_command");
    keybinding_treeview =
        glade_xml_get_widget(xml_tree, "keybinding_treeview");
    keybinding_button_remove =
        glade_xml_get_widget(xml_tree, "keybinding_button_remove");
    keybinding_button_update =
        glade_xml_get_widget(xml_tree, "keybinding_button_update");
    keybinding_button_bind =
        glade_xml_get_widget(xml_tree, "keybinding_button_bind");

    g_signal_connect ((gpointer) keybinding_entry_key, "key_press_event",
        G_CALLBACK (on_keybinding_entry_key_key_press_event), NULL);
    g_signal_connect ((gpointer) keybinding_button_remove, "clicked",
        G_CALLBACK (on_keybinding_button_remove_clicked), NULL);
    g_signal_connect ((gpointer) keybinding_button_update, "clicked",
        G_CALLBACK (on_keybinding_button_update_clicked), NULL);
    g_signal_connect ((gpointer) keybinding_button_bind, "clicked",
        G_CALLBACK (on_keybinding_button_bind_clicked), NULL);

    widget = glade_xml_get_widget(xml_tree, "keybinding_button_clear");
    g_signal_connect ((gpointer) widget, "clicked",
        G_CALLBACK (on_keybinding_button_clear_clicked), NULL);

    widget = glade_xml_get_widget(xml_tree, "keybinding_button_close");
    g_signal_connect ((gpointer) widget, "clicked",
        G_CALLBACK (on_keybinding_button_close_clicked), NULL);

    gtk_widget_set_sensitive(keybinding_button_remove, FALSE);
    gtk_widget_set_sensitive(keybinding_button_update, FALSE);
    keybinding_store = gtk_list_store_new(6,
                                          G_TYPE_INT,
                                          G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                          G_TYPE_POINTER
                                          );
    gtk_tree_view_set_model(GTK_TREE_VIEW(keybinding_treeview), GTK_TREE_MODEL(keybinding_store));

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Key", renderer,
                                                      "text", KLIST_KEY,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_KEY);
    gtk_tree_view_append_column (GTK_TREE_VIEW (keybinding_treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Modifiers", renderer,
                                                      "text", KLIST_MODS,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_MODS);
    gtk_tree_view_append_column (GTK_TREE_VIEW (keybinding_treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Edit Mode", renderer,
                                                      "text", KLIST_EDIT,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_EDIT);
    gtk_tree_view_append_column (GTK_TREE_VIEW (keybinding_treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Command", renderer,
                                                      "text", KLIST_COMMAND,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, KLIST_COMMAND);
    gtk_tree_view_append_column (GTK_TREE_VIEW (keybinding_treeview), column);


    keybinding_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(keybinding_treeview));
    gtk_tree_selection_set_mode (keybinding_selection, GTK_SELECTION_BROWSE);
    gtk_tree_selection_set_select_function(keybinding_selection, keybinding_selection_func, NULL, NULL);

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(keybinding_store),
                                             KLIST_KEY,
                                             GTK_SORT_ASCENDING);


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
    else if (ks==altkeysym[0] || ks==altkeysym[1]) {
        cpl.alt_on=0;
    }
    else if (ks==metakeysym[0] || ks==metakeysym[1]) {
        cpl.meta_on=0;
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
    char buf[MAX_BUF], tmpbuf[MAX_BUF];

    if (keysym==commandkeysym) {
        gtk_widget_grab_focus (GTK_WIDGET(entry_commands));
        gtk_entry_set_visibility(GTK_ENTRY(entry_commands), 1);
        cpl.input_state = Command_Mode;
        cpl.no_echo=FALSE;
        return;
    }
    if (keysym==altkeysym[0] ||keysym==altkeysym[1]) {
        cpl.alt_on=1;
        return;
    }
    if (keysym==metakeysym[0] ||keysym==metakeysym[1]) {
        cpl.meta_on=1;
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
    if (cpl.alt_on) present_flags |= KEYF_ALT;
    if (cpl.meta_on) present_flags |= KEYF_META;
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
            gtk_entry_set_text(GTK_ENTRY(entry_commands),cpl.input_text);
            gtk_widget_grab_focus (GTK_WIDGET(entry_commands));
            gtk_editable_select_region(GTK_EDITABLE(entry_commands), 0, 0);
            gtk_editable_set_position(GTK_EDITABLE(entry_commands), -1);
            return;
        }

        if (first_match->direction>=0) {
            if (cpl.fire_on) {
                sprintf(buf,"fire %s", first_match->command);
                /* Some spells (dimension door) need a valid count value */
                cpl.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(spinbutton_count));
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
        gtk_spin_button_set_value (GTK_SPIN_BUTTON(spinbutton_count), (float) cpl.count );
        return;
    }
    tmpbuf[0]=0;
    if (cpl.fire_on) strcat(tmpbuf,"fire+");
    if (cpl.run_on) strcat(tmpbuf,"run+");
    if (cpl.alt_on) strcat(tmpbuf,"alt+");
    if (cpl.meta_on) strcat(tmpbuf,"meta+");

    sprintf(buf, "Key %s%s is not bound to any command.  Use bind to associate this keypress with a command",
            tmpbuf, keysym==NoSymbol? "unknown": gdk_keyval_name(keysym));
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
        if (key->flags & KEYF_ALT)
          buff[bi++] ='L';
        if (key->flags & KEYF_META)
          buff[bi++] ='M';
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
    char buf[MAX_BUF];

  sprintf(buf, "Commandkey %s",
          commandkeysym==NoSymbol?"unknown":gdk_keyval_name(commandkeysym));
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Firekeys 1: %s, 2: %s",
          firekeysym[0]==NoSymbol?"unknown":gdk_keyval_name(firekeysym[0]),
          firekeysym[1]==NoSymbol?"unknown":gdk_keyval_name(firekeysym[1]));
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Altkeys 1: %s, 2: %s",
          altkeysym[0]==NoSymbol?"unknown":gdk_keyval_name(altkeysym[0]),
          altkeysym[1]==NoSymbol?"unknown":gdk_keyval_name(altkeysym[1]));
  draw_info(buf,NDI_BLACK);

  sprintf(buf, "Metakeys 1: %s, 2: %s",
          metakeysym[0]==NoSymbol?"unknown":gdk_keyval_name(metakeysym[0]),
          metakeysym[1]==NoSymbol?"unknown":gdk_keyval_name(metakeysym[1]));
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



void bind_key(char *params)
{
    char buf[MAX_BUF + 16];

    if (!params) {
        draw_info("Usage: bind [-aefmnr] {<commandline>/commandkey/firekey{1/2}/runkey{1/2}/altkey{1/2}/metakey{1/2}",NDI_BLACK);
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
    if (!strcmp(params, "metakey1")) {
        bind_keysym = & metakeysym[0];
        draw_info("Push key to bind new metakey 1.",NDI_BLACK);
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "metakey2")) {
        bind_keysym = & metakeysym[1];
        draw_info("Push key to bind new metakey 2.",NDI_BLACK);
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "altkey1")) {
        bind_keysym = & altkeysym[0];
        draw_info("Push key to bind new altkey 1.",NDI_BLACK);
        cpl.input_state = Configure_Keys;
        return;
    }
    if (!strcmp(params, "altkey2")) {
        bind_keysym = & altkeysym[1];
        draw_info("Push key to bind new altkey 2.",NDI_BLACK);
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
            case 'a':
                bind_flags |= KEYF_ALT;
                break;
            case 'e':
                bind_flags |= KEYF_EDIT;
                break;
            case 'f':
                bind_flags |= KEYF_FIRE;
                break;
            case 'm':
                bind_flags |= KEYF_META;
                break;
            case 'n':
                bind_flags |= KEYF_NORMAL;
                break;
            case 'r':
                bind_flags |= KEYF_RUN;
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

    if (strlen(params) >= sizeof(bind_buf)) {
        params[sizeof(bind_buf) - 1] = '\0';
        draw_info("Keybinding too long! Truncated:",NDI_RED);
        draw_info(params,NDI_RED);
    }
    sprintf(buf, "Push key to bind '%s'.", params);
    draw_info(buf,NDI_BLACK);

    strcpy(bind_buf, params);
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

#ifdef WIN32
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
    if (metakeysym[0] != GDK_Shift_L && metakeysym[0] != NoSymbol) {
        fprintf(fp, "! metakey0 %s %d\n",
                gdk_keyval_name(metakeysym[0]), 0);
    }
    if (metakeysym[1] != GDK_Shift_R && metakeysym[1] != NoSymbol) {
        fprintf(fp, "! metakey1 %s %d\n",
                gdk_keyval_name(metakeysym[1]), 0);
    }
    if (altkeysym[0] != GDK_Shift_L && altkeysym[0] != NoSymbol) {
        fprintf(fp, "! altkey0 %s %d\n",
                gdk_keyval_name(altkeysym[0]), 0);
    }
    if (altkeysym[1] != GDK_Shift_R && altkeysym[1] != NoSymbol) {
        fprintf(fp, "! altkey1 %s %d\n",
                gdk_keyval_name(altkeysym[1]), 0);
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
    Key_Entry *keyentry, *first_match=NULL;

    /* I think that basically if we are not rebinding the special
     * control keys (in which case bind_kesym would be set to something)
     * we just want to handle these keypresses as normal events.
     */
    if (bind_keysym==NULL) {
        if(keysym == altkeysym[0] || keysym == altkeysym[1]) {
            cpl.alt_on =1;
            return;
        }
        if(keysym == metakeysym[0] || keysym == metakeysym[1]) {
            cpl.meta_on =1;
            return;
        }
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

    if ((cpl.fire_on || cpl.run_on || cpl.meta_on || cpl.alt_on) &&
      (bind_flags & KEYF_MODIFIERS)==KEYF_MODIFIERS) {
        bind_flags &= ~KEYF_MODIFIERS;
        if (cpl.fire_on) bind_flags |= KEYF_FIRE;
        if (cpl.run_on) bind_flags |= KEYF_RUN;
        if (cpl.meta_on) bind_flags |= KEYF_META;
        if (cpl.alt_on) bind_flags |= KEYF_ALT;
    }

    if (bind_keysym!=NULL) {
        *bind_keysym=keysym;
        bind_keysym=NULL;
    }
    else {
        keyentry = keys[keysym % KEYHASH];
        while (keyentry!=NULL) {
            if ((keyentry->keysym!=0 && keyentry->keysym!=keysym) ||
                (!(keyentry->flags & bind_flags))) {
                    keyentry=keyentry->next;
                    continue;
            }
            first_match = keyentry;

            /* Try to find a prefect match */
            if ((keyentry->flags & KEYF_MODIFIERS)!= bind_flags) {
                keyentry=keyentry->next;
                continue;
            }
            else break;
        }
        if (first_match) {
            sprintf(buf, "Warning: Keybind %s may conflict with new binding.", first_match->command);
            draw_info(buf,NDI_RED);
        }

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

    if (event->keyval>0 && !GTK_WIDGET_HAS_FOCUS (entry_commands)) {
            parse_key_release(event->keyval);
            gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_release_event") ;
    }
}


void keyfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window) {
    char *text;

    if (!use_config[CONFIG_POPUPS]) {
        if  (cpl.input_state == Reply_One) {
            text=gdk_keyval_name(event->keyval);
            send_reply(text);
            cpl.input_state = Playing;
            return;
        }
        else if (cpl.input_state == Reply_Many) {
            if (GTK_WIDGET_HAS_FOCUS (entry_commands))
                gtk_widget_event(GTK_WIDGET(entry_commands), (GdkEvent*)event);
            else
                gtk_widget_grab_focus (GTK_WIDGET(entry_commands));
            return;
        }
    }

    /* Better check for really weirdo keys, X doesnt like keyval 0*/
    if (event->keyval<=0) return;

    if (GTK_WIDGET_HAS_FOCUS (entry_commands)) {
        if (event->keyval == completekeysym) gtk_complete_command();
        if (event->keyval == prevkeysym || event->keyval == nextkeysym)
            gtk_command_history(event->keyval==nextkeysym?0:1);
    else
        gtk_widget_event(GTK_WIDGET(entry_commands), (GdkEvent*)event);
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
                    gtk_widget_grab_focus (GTK_WIDGET(entry_commands));
                    /* When running in split windows mode, entry_commands can't get focus because
                     * it is in a different window.  So we have to pass the event to it
                     * explicitly
                     */
                    if (GTK_WIDGET_HAS_FOCUS(entry_commands)==0)
                        gtk_widget_event(GTK_WIDGET(entry_commands), (GdkEvent*)event);
                }
                /*
                 * Don't pass signal along to default handlers - otherwise, we get
                 * get crashes in the clist area (gtk fault I believe)
                 */
                gtk_signal_emit_stop_by_name (GTK_OBJECT(window), "key_press_event") ;
                break;

            case Metaserver_Select:
                gtk_widget_grab_focus (GTK_WIDGET(entry_commands));
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
                if (key->flags & KEYF_ALT)
                    buff[bi++] ='L';
                if (key->flags & KEYF_META)
                    buff[bi++] ='M';
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

void x_set_echo() {
    gtk_entry_set_visibility(GTK_ENTRY(entry_commands), !cpl.no_echo);
}

/* Draws a prompt.  Don't deal with popups for the time being. */
void draw_prompt(const char *str)
{
    draw_info(str, NDI_WHITE);
    gtk_widget_grab_focus (GTK_WIDGET(entry_commands));
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
            gtk_entry_set_text(GTK_ENTRY(entry_commands), "");
            gtk_entry_set_position(GTK_ENTRY(entry_commands), 0);
            scroll_history_position=cur_history_position;
            return;
        }
    }

    if (history[i][0] == 0) return;

    scroll_history_position=i;
/*    fprintf(stderr,"resetting postion to %d, data = %s\n", i, history[i]);*/
    gtk_entry_set_text(GTK_ENTRY(entry_commands), history[i]);
    gtk_widget_grab_focus (GTK_WIDGET(entry_commands));
    gtk_editable_select_region(GTK_EDITABLE(entry_commands), 0, 0);
    gtk_editable_set_position(GTK_EDITABLE(entry_commands), -1);

    cpl.input_state = Command_Mode;
}

void gtk_complete_command()
{
    const gchar *entry_text, *newcommand;

    entry_text = gtk_entry_get_text(GTK_ENTRY(entry_commands));
    newcommand = complete_command(entry_text);
    /* value differ, so update window */
    if (newcommand != NULL) {
        gtk_entry_set_text(GTK_ENTRY(entry_commands), newcommand);
        gtk_widget_grab_focus (GTK_WIDGET(entry_commands));
        gtk_editable_select_region(GTK_EDITABLE(entry_commands), 0, 0);
        gtk_editable_set_position(GTK_EDITABLE(entry_commands), -1);
    }
}

void
on_entry_commands_activate             (GtkEntry        *entry,
                                        gpointer         user_data)
{
    const gchar *entry_text;
    extern GtkWidget *treeview_look;

    /* Next reply will reset this as necessary */
    if (!use_config[CONFIG_POPUPS])
        gtk_entry_set_visibility(GTK_ENTRY(entry), TRUE);

    entry_text = gtk_entry_get_text(GTK_ENTRY(entry));

    if (cpl.input_state==Metaserver_Select) {
        strcpy(cpl.input_text, entry_text);
    } else if (cpl.input_state == Reply_One || cpl.input_state == Reply_Many) {
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
    gtk_entry_set_text(GTK_ENTRY(entry),"");

    /* This grab fous is really just so the entry box doesn't have
     * focus - this way, keypresses are used to play the game, and
     * not as stuff that goes into the entry box.
     * it doesn't make much difference what widget this is set
     * to, as long as it is something that can get focus.
     */
    gtk_widget_grab_focus (GTK_WIDGET(treeview_look));

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

/****************************************************************************
 * Code below here handles the keybinding window.
 ****************************************************************************/

void update_keybinding_list()
{
    int i, allbindings=0;
    Key_Entry *key;
    char    modifier_buf[256];
    GtkTreeIter iter;


    gtk_list_store_clear(keybinding_store);

    for (i=0; i<KEYHASH; i++) {
        for (key=keys[i]; key!=NULL; key =key->next) {
            if (key->flags & KEYF_STANDARD && !allbindings) continue;

            modifier_buf[0] = 0;

            if ((key->flags & KEYF_MODIFIERS) != KEYF_MODIFIERS) {
                if (key->flags & KEYF_ALT)  strcat(modifier_buf,"Alt ");
                if (key->flags & KEYF_FIRE)  strcat(modifier_buf,"Fire ");
                if (key->flags & KEYF_RUN)  strcat(modifier_buf,"Run ");
                if (key->flags & KEYF_META)  strcat(modifier_buf,"Meta ");
            }
            if (key->flags & KEYF_STANDARD)  strcat(modifier_buf,"(Standard) ");
            gtk_list_store_append(keybinding_store, &iter);
            gtk_list_store_set(keybinding_store, &iter,
                               KLIST_ENTRY, i,
                               KLIST_KEY,  gdk_keyval_name(key->keysym),
                               KLIST_MODS, modifier_buf,
                               KLIST_EDIT, (key->flags & KEYF_EDIT) ? "Yes":"No",
                               KLIST_COMMAND, key->command,
                               KLIST_KEY_ENTRY, key,
                               -1);
        }
    }
    reset_keybinding_status();
}


/* Menubar item to activate keybindings window */
void
on_keybindings_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

    gtk_widget_show(keybinding_window);
    update_keybinding_list();

}


gboolean
on_keybinding_entry_key_key_press_event
                                        (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{

    gtk_entry_set_text (GTK_ENTRY(keybinding_entry_key),  gdk_keyval_name(event->keyval));

    /* This code is basically taken from the GTKv1 client.  However, at some
     * level it is broken, since the control/shift/etc masks are hardcoded,
     * yet we do let the users redefine them.
     */

    /* The clearing of the modifiers is disabled.  In my basic testing, I checked
     * the modifiers and then pressed the key - have those modifiers go away I think
     * is less intuitive.
     */
    if (event->state & GDK_CONTROL_MASK)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), TRUE);
#if 0
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), FALSE);
#endif

    if (event->state & GDK_SHIFT_MASK)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), TRUE);
#if 0
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), FALSE);

    /* The GDK_MOD_MASK* will likely correspond to alt and meta, yet there is
     * no way to be sure what goes to what, so easiest to just not allow them.
     */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta), FALSE);
#endif

    /* Returning TRUE prevents widget from getting this event */
    return TRUE;
}


void
on_keybinding_button_remove_clicked    (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkTreeModel    *model;
    GtkTreeIter iter;
    Key_Entry   *entry, *key, *tmp;
    int onkey;

    if (!gtk_tree_selection_get_selected (keybinding_selection, &model, &iter)) {
        LOG(LOG_ERROR,"keys.c:on_keybinding_button_remove_clicked", "Function called with nothing selected\n");
        return;
    }
    gtk_tree_model_get(model, &iter, KLIST_KEY_ENTRY, &entry, -1);
    for (onkey=0; onkey<KEYHASH; onkey++) {
        for (key=keys[onkey]; key; key =key->next) {
            if (key == entry) {

                /* This code is directly from unbind_key() above */

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
            }
        }
    }
    LOG(LOG_ERROR,"keys.c:on_keybinding_button_remove_clicked", "Unable to find matching key entry\n");

unbinded:
    free(key->command);
    free(key);
    save_keys();
    update_keybinding_list();

}

/* This function gets the state information from what checkboxes and
 * other data in the window and puts it in the variables passed
 * passed.  This is used by both the update and add functions.
 */
static void keybinding_get_data(uint32 *keysym, uint8 *flags, const char **command)
{
    static char bind_buf[MAX_BUF];
    const char *ed;
    *flags = 0;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_control)))
        *flags |= KEYF_RUN;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift)))
        *flags |= KEYF_FIRE;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt)))
        *flags |= KEYF_ALT;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta)))
        *flags |= KEYF_META;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_edit)))
        *flags |= KEYF_EDIT;

    /* If no modifiers set, the presume all should be used */
    if (!(*flags & KEYF_MODIFIERS))
        *flags |= KEYF_MODIFIERS;

    ed = gtk_entry_get_text(GTK_ENTRY(keybinding_entry_command));
    if (strlen(ed)) {
        sprintf(bind_buf,"Keybinding command to long - truncating!");
        draw_info(bind_buf,NDI_BLACK);
        strncpy(bind_buf, ed, MAX_BUF-1);
        bind_buf[MAX_BUF-1] = 0;
        *command = bind_buf;
    } else {
        *command = ed;
    }

    /* This isn't ideal - when the key is pressed, we convert it to a string,
     * and now we are converting it back.  It'd be nice to tuck the keysym
     * itself away someplace.
     */
    *keysym = gdk_keyval_from_name(gtk_entry_get_text(GTK_ENTRY(keybinding_entry_key)));
    if (*keysym == GDK_VoidSymbol) {
        LOG(LOG_ERROR,"keys.ckeybinding_get_data", "Can not get valid keysym from selection");
    }
}


void
on_keybinding_button_bind_clicked      (GtkButton       *button,
                                        gpointer         user_data)
{
    uint32  keysym;
    uint8   flags;
    const char *command;

    keybinding_get_data(&keysym, &flags, &command);

    /* insert_key will do a strdup of command for us */
    insert_key(keysym, flags, command);

    /* I think it is more appropriate to clear the fields once the user adds
     * it.  I suppose the ideal case would be to select the newly inserted
     * keybinding.
     */
    reset_keybinding_status();
    update_keybinding_list();
    save_keys();
}



void
on_keybinding_button_update_clicked    (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkTreeIter iter;
    Key_Entry   *entry;
    GtkTreeModel    *model;
    const char *buf;

    if (gtk_tree_selection_get_selected (keybinding_selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, KLIST_KEY_ENTRY, &entry, -1);

        if (!entry) {
            LOG(LOG_ERROR,"keys.c:on_keybinding_button_update_clicked", "Unable to get key_entry structure\n");
            return;
        }
        free(entry->command);
        keybinding_get_data(&entry->keysym, &entry->flags, &buf);
        entry->command = strdup_local(buf);
        update_keybinding_list();
        save_keys();
    } else {
        LOG(LOG_ERROR,"keys.c:on_keybinding_button_update_clicked", "Nothing selected to update\n");
    }
}


void
on_keybinding_button_close_clicked     (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide(keybinding_window);
}



/* This function is called when the user clicks on one of the
 * entries in the list of keybindings.  When that happens, we
 * want to update the fields below the window (actual binding
 * information) as well as enable the remove and update windows.
 */
gboolean keybinding_selection_func (
                      GtkTreeSelection *selection,
                      GtkTreeModel     *model,
                      GtkTreePath      *path,
                      gboolean          path_currently_selected,
                      gpointer          userdata)
{
    GtkTreeIter iter;
    Key_Entry   *entry;

    gtk_widget_set_sensitive(keybinding_button_remove, TRUE);
    gtk_widget_set_sensitive(keybinding_button_update, TRUE);

    if (gtk_tree_model_get_iter(model, &iter, path)) {

        gtk_tree_model_get(model, &iter, KLIST_KEY_ENTRY, &entry, -1);

        if (!entry) {
            LOG(LOG_ERROR,"keys.c:keybinding_selection_func", "Unable to get key_entry structure\n");
            return FALSE;
        }
        if (entry->flags & KEYF_RUN)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), FALSE);

        if (entry->flags & KEYF_FIRE)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), FALSE);

        if (entry->flags & KEYF_ALT)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt), TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt), FALSE);

        if (entry->flags & KEYF_META)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta), TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta), FALSE);

        if (entry->flags & KEYF_EDIT)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_edit), TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_edit), FALSE);

        gtk_entry_set_text (GTK_ENTRY(keybinding_entry_key), gdk_keyval_name(entry->keysym));
        gtk_entry_set_text (GTK_ENTRY(keybinding_entry_command), entry->command );

    }
    return TRUE;
}

void reset_keybinding_status()
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_control), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_shift), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_alt), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_meta), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keybinding_checkbutton_edit), FALSE);
    gtk_entry_set_text (GTK_ENTRY(keybinding_entry_key), "");
    gtk_entry_set_text (GTK_ENTRY(keybinding_entry_command), "");
    gtk_widget_set_sensitive(keybinding_button_remove, FALSE);
    gtk_widget_set_sensitive(keybinding_button_update, FALSE);

}

/* If the user clicks the clear button, want to clear the
 * selection as well as clear all the fields associated
 * with the selection.
 */
void
on_keybinding_button_clear_clicked     (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkTreeModel    *model;
    GtkTreeIter iter;

    /* Need to unselect this first - otherwise, it seems we get another selection
     * event triggering the stuff active again.
     */
    if (gtk_tree_selection_get_selected (keybinding_selection, &model, &iter)) {
        gtk_tree_selection_unselect_iter (keybinding_selection, &iter);
    }
    reset_keybinding_status();


}
