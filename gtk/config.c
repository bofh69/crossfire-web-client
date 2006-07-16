const char *rcsid_gtk_config_c =
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

/* This file handles the loading and saving of the configuration options,
 * as well as presenting a nice gui to select the
 * options
 */

#include "config.h"

#ifdef __CYGWIN__
#include <errno.h>
#endif

/* gtk */
#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#endif
#include <gdk/gdkkeysyms.h>


/* always include our local headers after the system headers are included */
#include "client.h"
/*#include "clientbmap.h"*/
#include "item.h"
#include "gx11.h"
#include "gtkproto.h"
#include <ctype.h>


/* Abstract this out a bit - rather than have a whole bunch of
 * duplicated code that generates these values, instead use
 * pointers to functions that set and get the values - this
 * makes adding new widgets a lot easier in most cases.
 *
 * button is the actual widget that is created.
 *
 * label is the label that is to be printed
 *
 * type if the type of widget.  As long as the widget only
 * needs to deal with numeric type values, this works
 * fine - this means that dials can get just as easily
 * added to this list
 *
 * config holds the corresponding CONFIG value from
 * the common/client.h file.  This allows
 * use to use a fairly common function for most
 * values.
 *
 * flags holds flags.  Currently, the
 * only flag is FLAG_UPDATE - if set,
 * then we automatically the running total
 * immediately.  Otherwise, the want_config value
 * only gets updated, and depending on the value,
 * that value may get used/copied into the use_config
 * at a later point.  Many values which can change at
 * runtime do not have this flag set simply so that it
 * is easier to notice if it has changed and do the
 * appropraite thing (eg, stop/start sound daemon, etc)
 *
 * Note on RBUTTON (radio button usage):  Since a radio
 * button is a collection of buttons of only which one can
 * be pressed, the logic the program uses is this:
 * 1) If the previous widget was a radio button, we add
 * this one to the same group.  This means if you want to
 * have multiple sets of radio buttons, you should seperate
 * them with something.
 * 2) Since the radio button is several widgets, its not as simple
 * as normal buttons to map them to a config value.  Instead,
 * use a range so that it is easy to tell what config value
 * your button belongs to, eg, 100-199 is for the lighting
 * options.
 */

#define MAX_BUTTONS	    33
#define RBUTTON	    1
#define CBUTTON	    2
#define SEPERATOR   3	    /* Seperator in the window */

#define SPIN	    0x100
#define SPIN_SCALE  0x101   /* Spin Button that is image scale */
#define SPIN_MAP    0x102   /* Spin button that is map size */
#define SPIN_CWINDOW 0x103  /* Spin command window */

#define FLAG_UPDATE	0x1
#define FLAG_MAPPANE	0x2	/* Display on the map/image pane */

typedef struct {
    GtkWidget	*widget;
    int		type;
    int		config;
    int		flags;
    const char	*label;
} CButtons;

static GtkWidget *gtkwin_config = NULL,	    /* main window */
    *faceset_combo;			    /* Combo box for faceset selection */


/* A dispatch table that can deal with the entire selection of
 * config gui elements.
 */

static CButtons cbuttons[MAX_BUTTONS] = {
{NULL, 	    CBUTTON,	    CONFIG_FOODBEEP,	FLAG_UPDATE,
    "Beep When Food is Low"},
{NULL, 	    CBUTTON,	    CONFIG_TIMESTAMP,	FLAG_UPDATE,
    "Timestamp Messages"},
{NULL, 	    SPIN_CWINDOW,   CONFIG_CWINDOW,	FLAG_UPDATE,
    "Command Window"},
{NULL, 	    CBUTTON,	    CONFIG_ECHO,	FLAG_UPDATE,
    "Echo Bound Commands"},
{NULL, 	    CBUTTON,	    CONFIG_FASTTCP,	0,
    "Fast TCP Send (May improve performance at expense\n of outgoing bandwidth)"},
{NULL, 	    CBUTTON,	    CONFIG_GRAD_COLOR,	FLAG_UPDATE,
    "Gradually change stat bar color based on value of the stat.\nThis option will result in some extra CPU usage."},
{NULL, 	    CBUTTON,	    CONFIG_POPUPS,	FLAG_UPDATE,
    "Popup Windows"},
{NULL, 	    CBUTTON,	    CONFIG_SIGNPOPUP,	FLAG_UPDATE,
    "Popup Sign Windows (need Popup Windows checked to be used)"},
{NULL, 	    CBUTTON,	    CONFIG_SPLASH,	FLAG_UPDATE,
    "Splash Window"},
{NULL, 	    CBUTTON,	    CONFIG_SHOWICON,	FLAG_UPDATE,
    "Show Inventory Icon"},
{NULL, 	    CBUTTON,	    CONFIG_TOOLTIPS,	0,
    "Show Tooltips"},
{NULL, 	    CBUTTON,	    CONFIG_SOUND,	0,
    "Sound"},
{NULL, 	    CBUTTON,	    CONFIG_SPLITINFO,	0,
    "Split Information Window (Takes effect next run)"},
{NULL, 	    CBUTTON,	    CONFIG_SPLITWIN,	0,
    "Split Windows"},
{NULL, 	    CBUTTON,	    CONFIG_TRIMINFO,	FLAG_UPDATE,
    "Trims text in the information window - " /**/
    "improves performance but bugs in\n gtk make the client unstable if this is used." /**/
    "This may work better with gtk 2.0"},
{NULL, 	    CBUTTON,	    CONFIG_APPLY_CONTAINER,	FLAG_UPDATE,
    "Automatically re-applies a container when you use apply to close it. \nIf off, when you use apply to close the container, it stays unapplied"},

{NULL, 	    CBUTTON,	    CONFIG_RESISTS,	0,
    "Display resistances in two columns rather than only one."},

/* The following items are shown in the map tag.
 * I grouped them together to make reading them a bit easier,
 * but in fact, they could be intermixed with the other
 * options.
 */

{NULL, 	    CBUTTON,	    CONFIG_CACHE,	FLAG_MAPPANE,
    "Cache Images"},
{NULL, 	    CBUTTON,	    CONFIG_DOWNLOAD,	FLAG_MAPPANE | FLAG_UPDATE,
    "Download All Image Information (Takes effect on next server connection)"},
{NULL, 	    CBUTTON,	    CONFIG_FOGWAR,	FLAG_MAPPANE | FLAG_UPDATE,
    "Fog of War"},
{NULL, 	    SPIN_SCALE,	    CONFIG_ICONSCALE,	FLAG_MAPPANE,
    "Icon Scale (Takes effect next run)"},
{NULL, 	    SPIN_SCALE,	    CONFIG_MAPSCALE,	FLAG_MAPPANE,
    "Map Scale (Takes effect next run)"},
{NULL, 	    CBUTTON,	    CONFIG_SMOOTH,	FLAG_MAPPANE | FLAG_UPDATE,
    "Enable smoothing - Use additionnal CPU (Take effect on next connection)."},
{NULL, 	    CBUTTON,	    CONFIG_DISPLAYMODE,	FLAG_MAPPANE,
    "SDL Image Support (Take effect next run)"},
{NULL, 	    CBUTTON,	    CONFIG_SHOWGRID,	FLAG_MAPPANE | FLAG_UPDATE,
    "Print Grid Overlay (SDL only, Slow, useful for debugging/development"},

{NULL,	    SEPERATOR,		0,		FLAG_MAPPANE,
    "Lighting options, per pixel is prettier, per tile is faster.\nIf the darkness code is off, the pixel/tile options will be ignored."},
{NULL, 	    RBUTTON,	    100 + CFG_LT_PIXEL_BEST,	FLAG_MAPPANE,
    "Best Per Pixel Lighting (slowest)"},
{NULL, 	    RBUTTON,	    100 + CFG_LT_PIXEL,	FLAG_MAPPANE,
    "Fast Per Pixel Lighting"},
{NULL, 	    RBUTTON,	    100 + CFG_LT_TILE,	FLAG_MAPPANE,
    "Per Tile Lighting"},
{NULL, 	    CBUTTON,	    CONFIG_DARKNESS,	FLAG_MAPPANE | FLAG_UPDATE,
    "Enable darkness code - if off, all spaces will not be dimmed."},

{NULL,	    SEPERATOR,	    0,			FLAG_MAPPANE,
    "Map Size: Larger map lets you see more information, but takes more CPU\npower and bandwidth.  Changing these will not take effect until the next time\nyou connect to a server"},
{NULL, 	    SPIN_MAP,	    CONFIG_MAPHEIGHT,	FLAG_MAPPANE,
    "Map Height"},
{NULL, 	    SPIN_MAP,	    CONFIG_MAPWIDTH,	FLAG_MAPPANE,
    "Map Width"},
};


static void set_config_value(int cval, int value)
{
    want_config[cbuttons[cval].config] = value;
    if (cbuttons[cval].flags & FLAG_UPDATE)
	use_config[cbuttons[cval].config] = value;
}

static int splitwin_toggling = FALSE;
 
void main_window_destroyed() {
    if (!splitwin_toggling) {
        client_exit();
    }
}

static void toggle_splitwin(int newval)
{
    splitwin_toggling = TRUE;

    inventory_splitwin_toggling();
	gtk_widget_destroy(gtkwin_root);
        
    if (newval) {
        ; /* Currently don't have it, but want splitwindows */
    } else { 
        /* opposite - do have it, but don't want it */
	gtk_widget_destroy(gtkwin_info);
	gtk_widget_destroy(gtkwin_stats);
	gtk_widget_destroy(gtkwin_message);
	gtk_widget_destroy(gtkwin_inv);
	gtk_widget_destroy(gtkwin_look);
    }

	create_windows();
	display_map_doneupdate(TRUE, FALSE);
	draw_stats (1);
    update_list_labels(&inv_list); /* After exploding or unexploding client, redraw weight labels. */
    update_list_labels(&look_list);

    splitwin_toggling = FALSE;
}

/* Ok, here it sets the config and saves it. This is sorta dangerous, and I'm not sure
 * if it's actually possible to do dynamic reconfiguration of everything this way. Something may
 * blow up in our faces.
 */

#define IS_DIFFERENT(TYPE) (want_config[TYPE] != use_config[TYPE])

static void applyconfig(void) {

    int onbutton;
    int lighting = 0;

    free(face_info.want_faceset);
    face_info.want_faceset = strdup_local(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(faceset_combo)->entry)));
    for (onbutton =0; onbutton < MAX_BUTTONS; onbutton++) {
	if (cbuttons[onbutton].type == CBUTTON) {
	    set_config_value(onbutton, GTK_TOGGLE_BUTTON (cbuttons[onbutton].widget)->active);
	} else if (cbuttons[onbutton].type & SPIN) {
	    set_config_value(onbutton, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cbuttons[onbutton].widget)));
	    /*
	     * Nothing special for command window, icon_scale, map_scale,
	     * map width and height.  It should be possible to dynamically
	     * change the width and height values, but that is for another day.
	     */

	} else if (cbuttons[onbutton].type == RBUTTON) {
	    /* We know that the only radio buttons currently in use are those for
	     * lighting.  IF other radio buttons are added later, this should
	     * be changed.
	     */
	    if ( GTK_TOGGLE_BUTTON (cbuttons[onbutton].widget)->active) {
		if ( cbuttons[onbutton].config >= 100 &&  cbuttons[onbutton].config < 200)
		    lighting = cbuttons[onbutton].config - 100;
	    }
	}
    } /* for onbutton ... loop */


    /* User has toggled splitwindows - adjust accordingly */
    if (IS_DIFFERENT(CONFIG_SPLITWIN)) {
	use_config[CONFIG_SPLITWIN] = want_config[CONFIG_SPLITWIN];
	toggle_splitwin(want_config[CONFIG_SPLITWIN]);
    }
    if (IS_DIFFERENT(CONFIG_SOUND)) {
	int tmp;
	if (want_config[CONFIG_SOUND]) {
	    tmp = init_sounds();
	    if (csocket.fd)
		cs_print_string(csocket.fd, "setup sound %d", tmp >= 0);
	} else {
	    if (csocket.fd)
		cs_print_string(csocket.fd, "setup sound 0");
	}
	use_config[CONFIG_SOUND] = want_config[CONFIG_SOUND];
    }
    if (IS_DIFFERENT(CONFIG_TOOLTIPS)) {
	if (want_config[CONFIG_TOOLTIPS]) gtk_tooltips_enable(tooltips);
	else gtk_tooltips_disable(tooltips);
	use_config[CONFIG_TOOLTIPS] = want_config[CONFIG_TOOLTIPS];
    }
    else if (IS_DIFFERENT(CONFIG_FASTTCP)) {
#ifdef TCP_NODELAY
#ifndef WIN32
	int q = want_config[CONFIG_FASTTCP];

	if (csocket.fd && setsockopt(csocket.fd, SOL_TCP, TCP_NODELAY, &q, sizeof(q)) == -1)
	    perror("TCP_NODELAY");
#else
	int q = want_config[CONFIG_FASTTCP];

	if (csocket.fd && setsockopt(csocket.fd, SOL_TCP, TCP_NODELAY, ( const char* )&q, sizeof(q)) == -1)
	    perror("TCP_NODELAY");
#endif
#endif
	use_config[CONFIG_FASTTCP] = want_config[CONFIG_FASTTCP];
    }
    if (IS_DIFFERENT(CONFIG_SHOWICON)) {
	itemlist_set_show_icon(&inv_list, want_config[CONFIG_SHOWICON]);
	/* TODO What about the look list? And should showicon propogate back here? */
	use_config[CONFIG_SHOWICON] = want_config[CONFIG_SHOWICON];
    }
    if (IS_DIFFERENT(CONFIG_RESISTS)) {    
	use_config[CONFIG_RESISTS] = want_config[CONFIG_RESISTS];
	resize_resistance_table(use_config[CONFIG_RESISTS]);
    }
    if (!use_config[CONFIG_GRAD_COLOR]) {
	reset_stat_bars();
    }

    if (lighting) {
	if (want_config[CONFIG_LIGHTING] != lighting) {
	    want_config[CONFIG_LIGHTING] = lighting;
	    use_config[CONFIG_LIGHTING] = lighting;
	}
#ifdef HAVE_SDL
	if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL)
	    /* This is done to make the 'lightmap' in the proper format */
	    init_SDL( NULL, 1);
#endif
	if( csocket.fd)
	    cs_print_string(csocket.fd, "mapredraw");
    }
    if (want_config[CONFIG_RESISTS] != use_config[CONFIG_RESISTS]) {
	resize_resistance_table(want_config[CONFIG_RESISTS]);
	use_config[CONFIG_RESISTS] = want_config[CONFIG_RESISTS];
	draw_message_window(1);
    }
}


/* Ok, here it sets the config and saves it. This is sorta dangerous, and I'm not sure
 * if it's actually possible to do dynamic reconfiguration of everything this way.
 */

static void saveconfig(void) {

    /* No idea why applyconfig was basically replicated - just call the
     * function instead!
     */
    applyconfig();
    save_defaults();
}

/*
 *  GUI Config dialog. 
 *
 *
 */

void configdialog(GtkWidget *widget) {
    GtkWidget *vbox;
    GtkWidget *tablabel;
    GtkWidget *notebook;
    GtkWidget *vbox1;
    GtkWidget *vbox2;
    GtkWidget *hbox1;
    GtkWidget *applybutton;
    GtkWidget *cancelbutton;
    GtkWidget *savebutton;
    GtkWidget *frame1;
    GtkWidget *frame_map, *vbox_map;	/* frame and vbox for map notebook */
    GtkWidget *addwidget;		/* Used in buildin the tab to point to the widget to add to */
    GtkWidget *ehbox;
    GtkWidget *clabel1, *clabel2, *clabel4, *clabel5, *cb1, *cb2, *cb3;
    GtkWidget *cclists;
    GtkWidget *extras[250];
    GList	*flist;
    int i, num_extras=0;

    gchar *titles[] ={"#","Key","(#)","Mods","Command"};	   

    /* If the window isnt already up (in which case it's just raised) */
    if(!gtkwin_config) {
	int x, y, wx, wy, w, h;


	gtkwin_config = gtk_window_new (GTK_WINDOW_DIALOG);
	/* Pet peeve - center new window on top of parent, and not on the
	 * the center of the screen - the later is really annoying in
	 * xinerama mode.  Thankfully, GTK 2.0 adds an option to
	 * center on parent - for now, just fake it by getting the parents
	 * geometry.
	 */
	/*gtk_window_position (GTK_WINDOW (gtkwin_config), GTK_WIN_POS_CENTER);*/
        get_window_coord(gtkwin_root, &x,&y, &wx,&wy,&w,&h);
        gtk_widget_set_uposition(gtkwin_config, (wx + w - 450)/2, (wy + h-500) / 2);
	gtk_widget_set_usize (gtkwin_config,450,600);
	gtk_window_set_title (GTK_WINDOW (gtkwin_config), "Crossfire Configure");
	gtk_window_set_policy (GTK_WINDOW (gtkwin_config), TRUE, TRUE, FALSE);

	gtk_signal_connect (GTK_OBJECT (gtkwin_config), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_config);
    
	gtk_container_border_width (GTK_CONTAINER (gtkwin_config), 0);

	/* vbox splits the window - top portion is the notebook, bottom
	 * portion is for the tabs for apply/save/config.
	 */
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_container_add (GTK_CONTAINER(gtkwin_config),vbox);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP );
	gtk_box_pack_start (GTK_BOX(vbox),notebook, TRUE, TRUE, 0);

	tablabel = gtk_label_new ("General");
	gtk_widget_show (tablabel);

	frame1 = gtk_frame_new("General options");  
	gtk_frame_set_shadow_type (GTK_FRAME(frame1), GTK_SHADOW_ETCHED_IN);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame1, tablabel);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame1), vbox1);

	tablabel = gtk_label_new ("Map & Image");
	gtk_widget_show (tablabel);

	frame_map = gtk_frame_new("Map and Image options");  
	gtk_frame_set_shadow_type (GTK_FRAME(frame_map), GTK_SHADOW_ETCHED_IN);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame_map, tablabel);

	vbox_map = gtk_vbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame_map), vbox_map);

	for (i=0; i < MAX_BUTTONS; i++) {
	    if (cbuttons[i].flags & FLAG_MAPPANE)
		addwidget = vbox_map;
	    else
		addwidget = vbox1;

	    if (cbuttons[i].type == CBUTTON) {
		cbuttons[i].widget = gtk_check_button_new_with_label(cbuttons[i].label);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(cbuttons[i].widget), want_config[cbuttons[i].config]);
	    }
	    else if (cbuttons[i].type == RBUTTON) {
		if ((i>0) && (cbuttons[i-1].type == RBUTTON)) {
		    cbuttons[i].widget = gtk_radio_button_new_with_label_from_widget(
				GTK_RADIO_BUTTON(cbuttons[i-1].widget), cbuttons[i].label);
		} else {
		    cbuttons[i].widget = gtk_radio_button_new_with_label(NULL, cbuttons[i].label);
		}
		if ((want_config[CONFIG_LIGHTING]+100) == cbuttons[i].config)
		    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(cbuttons[i].widget), 1);
	    }
	    else if (cbuttons[i].type & SPIN) {
		GtkAdjustment *adj=NULL;

		if (cbuttons[i].type == SPIN_SCALE) 
		    adj = (GtkAdjustment *) gtk_adjustment_new(want_config[cbuttons[i].config], 25, 200, 1, 5, 5);
		else if (cbuttons[i].type == SPIN_MAP)
		    adj = (GtkAdjustment *) gtk_adjustment_new(want_config[cbuttons[i].config], 9, MAP_MAX_SIZE, 1, 5, 5);
		else if (cbuttons[i].type == SPIN_CWINDOW)
		    adj = (GtkAdjustment *) gtk_adjustment_new(want_config[cbuttons[i].config], 1, 127, 1, 5, 5);
		cbuttons[i].widget = gtk_spin_button_new(adj, 1, 0);
		extras[num_extras] = gtk_hbox_new(FALSE, 2);
		gtk_box_pack_start(GTK_BOX(extras[num_extras]), cbuttons[i].widget, FALSE, FALSE, 0);
		extras[++num_extras] = gtk_label_new(cbuttons[i].label);
		gtk_box_pack_start(GTK_BOX(extras[num_extras-1]), extras[num_extras], FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(addwidget), extras[num_extras-1], FALSE, FALSE, 0);
		num_extras++;
		extras[num_extras++] = cbuttons[i].widget;
		continue;   /* What to skip the box_pack_start below */
	    }
	    else if (cbuttons[i].type == SEPERATOR) {
		extras[num_extras] = (GtkWidget*)gtk_hseparator_new ();
		gtk_box_pack_start (GTK_BOX (addwidget), extras[num_extras], FALSE, FALSE, 0);
		cbuttons[i].widget = gtk_label_new(cbuttons[i].label);
		gtk_label_set_justify(GTK_LABEL(cbuttons[i].widget), GTK_JUSTIFY_LEFT);
		num_extras++;
	    }
	    else {
            LOG(LOG_WARNING,"gtk::configdialog","Unknown cbutton type %d", cbuttons[i].type);
	    }
	    if (cbuttons[i].widget) {
		extras[num_extras++] = cbuttons[i].widget;
		gtk_box_pack_start(GTK_BOX(addwidget), cbuttons[i].widget, FALSE, FALSE, 0);
	    }
	}

	for (i=0; i < num_extras; i++) {
	    gtk_widget_show(extras[i]);
	}

	/* faceset is special because it is string data. */
	faceset_combo = gtk_combo_new();
	flist = NULL;
	if (face_info.want_faceset) {
	    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(faceset_combo)->entry), face_info.want_faceset);
	    flist = g_list_append(flist, face_info.want_faceset);
	}

	/* If we have real faceset info from the server, use it */
	if (face_info.have_faceset_info) {
	    for (i=0; i<MAX_FACE_SETS; i++)
		if (face_info.facesets[i].fullname)
		    flist = g_list_append(flist, face_info.facesets[i].fullname);
	} else {
	    flist = g_list_append(flist, "standard");
	    flist = g_list_append(flist, "classic");
	}
	if (flist) gtk_combo_set_popdown_strings(GTK_COMBO(faceset_combo), flist);
	addwidget = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(addwidget), faceset_combo, FALSE, FALSE, 0);
	tablabel = gtk_label_new("Faceset to use.  Only takes effect for new\n face information from server.  Not supported on\n all servers.");
	gtk_label_set_justify(GTK_LABEL(tablabel), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(addwidget), tablabel, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox_map), addwidget, FALSE, FALSE, 0);
	gtk_widget_show(tablabel);
	gtk_widget_show(faceset_combo);
	gtk_widget_show(addwidget);

	gtk_widget_show (vbox1);
	gtk_widget_show (frame1);
	gtk_widget_show(vbox_map);
	gtk_widget_show(frame_map);
 

	/*
	 * This block deals with drawing the keybindings
	 * block.
	 */

	tablabel = gtk_label_new ("Keybindings");
	gtk_widget_show (tablabel);
	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox2, tablabel);    
	frame1 = gtk_frame_new("Keybindings");  
	gtk_frame_set_shadow_type (GTK_FRAME(frame1), GTK_SHADOW_ETCHED_IN);
	gtk_box_pack_start (GTK_BOX (vbox2), frame1, TRUE, TRUE, 0);
	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER(frame1), vbox1);
	cclists = gtk_scrolled_window_new (0,0);
	cclist = gtk_clist_new_with_titles (5, titles);

	gtk_clist_set_column_width (GTK_CLIST(cclist), 0, 20);
	gtk_clist_set_column_width (GTK_CLIST(cclist), 1, 50);
	gtk_clist_set_column_width (GTK_CLIST(cclist), 2, 20);
	gtk_clist_set_column_width (GTK_CLIST(cclist), 3, 40);
	gtk_clist_set_column_width (GTK_CLIST(cclist), 4, 245);
	gtk_clist_set_selection_mode (GTK_CLIST(cclist) , GTK_SELECTION_SINGLE);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(cclists),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (cclists), cclist);
	gtk_box_pack_start (GTK_BOX(vbox1),cclists, TRUE, TRUE, 0);
	draw_keybindings (cclist);
    
	gtk_signal_connect_after (GTK_OBJECT(cclist),
                              "select_row",
                              GTK_SIGNAL_FUNC(cclist_button_event),
                              NULL);

	gtk_widget_show(cclist);
	gtk_widget_show(cclists);
    
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
	gtk_widget_show (ckeyentrytext);
	gtk_entry_set_text (GTK_ENTRY(ckeyentrytext),  "Press key to bind here");

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


void load_defaults(void)
{
    char path[MAX_BUF],inbuf[MAX_BUF],*cp;
    FILE *fp;
    int i, val;

    /* Copy over the want values to use values now */
    for (i=0; i<CONFIG_NUMS; i++) {
	use_config[i] = want_config[i];
    }

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

	val = -1;
	if (isdigit(*cp)) val=atoi(cp);
	else if (!strcmp(cp,"True")) val = TRUE;
	else if (!strcmp(cp,"False")) val = FALSE;

	for (i=1; i<CONFIG_NUMS; i++) {
	    if (!strcmp(config_names[i], inbuf)) {
		if (val == -1) {
		    LOG(LOG_WARNING,"gtk::load_defaults","Invalid value/line: %s: %s", inbuf, cp);
		} else {
		    want_config[i] = val;
		}
		break;	/* Found a match - won't find another */
	    }
	}
	/* We found a match in the loop above, so no need to do anything more */
	if (i < CONFIG_NUMS) continue;

	/* Legacy - now use the map_width and map_height values
	 * Don't do sanity checking - that will be done below
	 */
	if (!strcmp(inbuf,"mapsize")) {
	    if (sscanf(cp,"%hdx%hd", &want_config[CONFIG_MAPWIDTH], &want_config[CONFIG_MAPHEIGHT])!=2) {
		LOG(LOG_WARNING,"gtk::load_defaults","Malformed mapsize option in gdefaults.  Ignoring");
	    }
	}
	else if (!strcmp(inbuf, "server")) {
	    server = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	else if (!strcmp(inbuf, "sound_server")) {
	    sound_server = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	else if (!strcmp(inbuf, "nopopups")) {
	    /* Changed name from nopopups to popups, so inverse value */
	    want_config[CONFIG_POPUPS] = !val;
	    continue;
	}
	else if (!strcmp(inbuf, "nosplash")) {
	    want_config[CONFIG_SPLASH] = !val;
	    continue;
	}
	else if (!strcmp(inbuf, "splash")) {
	    want_config[CONFIG_SPLASH] = val;
	    continue;
	}
	else if (!strcmp(inbuf, "faceset")) {
	    face_info.want_faceset = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	/* legacy support for the old resistances values, we need to adjust the values to the new form */
	else if (!strcmp(inbuf, "resists")) {
	    if (val) want_config[CONFIG_RESISTS] = val-1;
	}
        else if (!strcmp(inbuf, "sdl")) {
            if (val) want_config[CONFIG_DISPLAYMODE] = CFG_DM_SDL;
	}

	
	else LOG(LOG_WARNING,"gtk::load_defaults","Unknown line in gdefaults: %s %s", inbuf, cp);
    }
    fclose(fp);
    /* Make sure some of the values entered are sane - since a user can
     * edit the defaults file directly, they could put bogus values
     * in
     */
    if (want_config[CONFIG_ICONSCALE]< 25 || want_config[CONFIG_ICONSCALE]>200) {
	LOG(LOG_WARNING,"gtk::load_defaults","Ignoring iconscale value read for gdefaults file.\n"
            "Invalid iconscale range (%d), valid range for -iconscale is 25 through 200",
            want_config[CONFIG_ICONSCALE]);
	want_config[CONFIG_ICONSCALE] = use_config[CONFIG_ICONSCALE];
    }
    if (want_config[CONFIG_MAPSCALE]< 25 || want_config[CONFIG_MAPSCALE]>200) {
	LOG(LOG_WARNING,"gtk::load_defaults","ignoring mapscale value read for gdefaults file.\n"
	        "Invalid mapscale range (%d), valid range for -iconscale is 25 through 200",
            want_config[CONFIG_MAPSCALE]);
	want_config[CONFIG_MAPSCALE] = use_config[CONFIG_MAPSCALE];
    }
    if (!want_config[CONFIG_LIGHTING]) {
	LOG(LOG_WARNING,"gtk::load_defaults","No lighting mechanism selected - will not use darkness code");
	want_config[CONFIG_DARKNESS] = FALSE;
    }
    
    /* Make sure the map size os OK */
    if (want_config[CONFIG_MAPWIDTH] < 9 || want_config[CONFIG_MAPWIDTH] > MAP_MAX_SIZE) {
	LOG(LOG_WARNING,"gtk::load_defaults",
            "Invalid map width (%d) option in gdefaults. Valid range is 9 to %d",
            want_config[CONFIG_MAPWIDTH], MAP_MAX_SIZE);
	want_config[CONFIG_MAPWIDTH] = use_config[CONFIG_MAPWIDTH];
    }
    if (want_config[CONFIG_MAPHEIGHT] < 9 || want_config[CONFIG_MAPHEIGHT] > MAP_MAX_SIZE) {
	LOG(LOG_WARNING,"gtk::load_defaults",
            "Invalid map height (%d) option in gdefaults. Valid range is 9 to %d",
            want_config[CONFIG_MAPHEIGHT], MAP_MAX_SIZE);
	want_config[CONFIG_MAPHEIGHT] = use_config[CONFIG_MAPHEIGHT];
    }

#ifndef HAVE_SDL
	/* If SDL is not built in, having SDL mode turned on causes many issues. */
    want_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
#endif

    /* Now copy over the values just loaded */
    for (i=0; i<CONFIG_NUMS; i++) {
	use_config[i] = want_config[i];
    }
    
    image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_ICONSCALE] / 100;
    map_image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 100;
    map_image_half_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 200;
    itemlist_set_show_icon(&inv_list, use_config[CONFIG_SHOWICON]); 

}

void save_defaults(void)
{
    char path[MAX_BUF],buf[MAX_BUF];
    FILE *fp;
    int i;

    sprintf(path,"%s/.crossfire/gdefaults", getenv("HOME"));
    if (make_path_to_file(path)==-1) {
	LOG(LOG_ERROR,"gtk::save_defaults","Could not create %s", path);
	return;
    }
    if ((fp=fopen(path,"w"))==NULL) {
	LOG(LOG_ERROR,"gtk::save_defaults","Could not open %s", path);
	return;
    }
    fprintf(fp,"# This file is generated automatically by gcfclient.\n");
    fprintf(fp,"# Manually editing is allowed, however gcfclient may be a bit finicky about\n");
    fprintf(fp,"# some of the matching it does.  all comparisons are case sensitive.\n");
    fprintf(fp,"# 'True' and 'False' are the proper cases for those two values\n");
    fprintf(fp,"# 'True' and 'False' have been replaced with 1 and 0 respectively\n");
    fprintf(fp,"server: %s\n", server);
    fprintf(fp,"sound_server: %s\n", sound_server);
    fprintf(fp,"faceset: %s\n", face_info.want_faceset);

    /* This isn't quite as good as before, as instead of saving things as 'True'
     * or 'False', it is just 1 or 0.  However, for the most part, the user isn't
     * going to be editing the file directly. 
     */
    for (i=1; i < CONFIG_NUMS; i++) {
	fprintf(fp,"%s: %d\n", config_names[i], want_config[i]);
    }

    fclose(fp);
    sprintf(buf,"Defaults saved to %s",path);
    draw_info(buf,NDI_BLUE);
}
