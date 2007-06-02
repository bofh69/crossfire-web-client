char *rcsid_gtk2_config_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005,2007 Mark Wedel & Crossfire Development Team

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


/* This file is here to cover configuration issues.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <ctype.h>

#include "client.h"

#include "interface.h"
#include "support.h"

#include "main.h"
#include "image.h"
#include "gtk2proto.h"

#include <dirent.h>

GtkWidget *config_window, *config_spinbutton_cwindow, *config_button_echo,
    *config_button_fasttcp, *config_button_grad_color, *config_button_foodbeep,
    *config_button_sound, *config_button_cache, *config_button_download,
    *config_button_fog, *config_spinbutton_iconscale, *config_spinbutton_mapscale,
    *config_spinbutton_mapwidth, *config_spinbutton_mapheight,
    *config_button_smoothing, *config_combobox_displaymode,
    *config_combobox_faceset, *config_combobox_lighting, *config_combobox_theme;

/* This is the string names that correspond to the numberic id's in client.h */

static char *display_modes[] = {"Pixmap", "SDL", "OpenGL"};
static char *theme = "Standard";

/**
 * This loads the theme stored away in the theme value above.
 * the theme string is loaded/saved into the config file, and can also
 * be changed by the user in the config menu.
 *
 * @reload
 * If true, user has changed theme after initial startup.  In this
 * mode, we need to call the routines that store away private theme
 * data.  When program is starting up, this is false, because all
 * the widgets haven't been realized yet, and the initialize routines
 * will get the theme data at that time.
 */
void load_theme(int reload)
{
    char path[MAX_BUF];
    int i;
    static char **default_files=NULL;

    /* We should only be called with reload once at startup.
     * at that time, store away the default rc files so
     * we can restore them later.
     */
    if (!reload) {
	char **tmp;

	/* GTK man page says copy of this data should be
	 * made, so lets go and do that.
	 */
	tmp = gtk_rc_get_default_files();
	i=0;
	while (tmp[i]) {
	    i++;
	}
	default_files = malloc(sizeof(char*) * (i+1));
	i=0;
	while (tmp[i]) {
	    default_files[i] = strdup(tmp[i]);
	    i++;
	}
	default_files[i] = NULL;
    }

    if (!strcmp(theme,"None")) {
	/* If this is default and initial run, nothing to do.
	 * If it not initial run, we need to reset the search path.
	 */
	if (reload) {
	    gtk_rc_set_default_files(default_files);
	}
    } else {
	snprintf(path, MAX_BUF, "%s/themes/%s", CF_DATADIR, theme);

	/* Check for existence of file.  Unfortunately, at initial run time,
	 * the window may not be realized, so we can't print this error to the user
	 * directly.
	 */
	if (access(path, R_OK) == -1) {
	    LOG(LOG_ERROR,"config.c::load_theme", "Unable to find theme file %s", path);
	    return;
	}
	/* We need to reset the search path, otherwise the effects
	 * are additive.  In practice, we could manipulate
	 * pointer so it is just one call to gtk_rc_set_default_files(),
	 * but that is probably more complicated than just doing these two
	 * calls here.
	 */
	gtk_rc_set_default_files(default_files);
	gtk_rc_add_default_file(path);
    }

    if (reload) {
	/* Reload data - so we need to force GTK to reparse and rebind all
	 * the widget data.
	 * Then, we need to call our own functions to reparse the custom
	 * widget handling we do.
	 */
	gtk_rc_reparse_all_for_settings(gtk_settings_get_for_screen(gdk_screen_get_default()), TRUE);
	gtk_rc_reset_styles(gtk_settings_get_for_screen(gdk_screen_get_default()));
	info_get_styles();
	inventory_get_styles();
	stats_get_styles();
	spell_get_styles();
	update_spell_information();

	/* set the inv_updated to force a redraw - otherwise it will not
	 * necessarily bind the lists with the new widgets.
	 */
	cpl.ob->inv_updated = 1;
	cpl.below->inv_updated = 1;
	draw_lists();

	draw_stats(TRUE);
	draw_message_window(TRUE);
    }
}

void load_defaults()
{
    char path[MAX_BUF],inbuf[MAX_BUF],*cp;
    FILE *fp;
    int i, val;

    /* Copy over the want values to use values now */
    for (i=0; i<CONFIG_NUMS; i++) {
	use_config[i] = want_config[i];
    }

    sprintf(path,"%s/.crossfire/gdefaults2", getenv("HOME"));
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
		LOG(LOG_WARNING,"gtk::load_defaults","Malformed mapsize option in gdefaults2.  Ignoring");
	    }
	}
	else if (!strcmp(inbuf, "server")) {
	    server = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	else if (!strcmp(inbuf, "theme")) {
	    theme = strdup_local(cp);	/* memory leak ! */
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
	/* legacy, as this is now just saved as 'lighting' */
	else if (!strcmp(inbuf, "per_tile_lighting")) {
	    if (val) want_config[CONFIG_LIGHTING] = CFG_LT_TILE;
	}
	else if (!strcmp(inbuf, "per_pixel_lighting")) {
	    if (val) want_config[CONFIG_LIGHTING] = CFG_LT_PIXEL;
	}
	else if (!strcmp(inbuf, "resists")) {
	    if (val) want_config[CONFIG_RESISTS] = val;
	}
	else if (!strcmp(inbuf, "sdl")) {
	    if (val) want_config[CONFIG_DISPLAYMODE] = CFG_DM_SDL;
	}
	else LOG(LOG_WARNING,"gtk::load_defaults","Unknown line in gdefaults2: %s %s", inbuf, cp);
    }
    fclose(fp);
    /* Make sure some of the values entered are sane - since a user can
     * edit the defaults file directly, they could put bogus values
     * in
     */
    if (want_config[CONFIG_ICONSCALE]< 25 || want_config[CONFIG_ICONSCALE]>200) {
	LOG(LOG_WARNING,"gtk::load_defaults","Ignoring iconscale value read for gdefaults2 file.\n"
            "Invalid iconscale range (%d), valid range for -iconscale is 25 through 200",
            want_config[CONFIG_ICONSCALE]);
	want_config[CONFIG_ICONSCALE] = use_config[CONFIG_ICONSCALE];
    }
    if (want_config[CONFIG_MAPSCALE]< 25 || want_config[CONFIG_MAPSCALE]>200) {
	LOG(LOG_WARNING,"gtk::load_defaults","ignoring mapscale value read for gdefaults2 file.\n"
	        "Invalid mapscale range (%d), valid range for -iconscale is 25 through 200",
            want_config[CONFIG_MAPSCALE]);
	want_config[CONFIG_MAPSCALE] = use_config[CONFIG_MAPSCALE];
    }
    if (!want_config[CONFIG_LIGHTING]) {
	LOG(LOG_WARNING,"gtk::load_defaults","No lighting mechanism selected - will not use darkness code");
	want_config[CONFIG_DARKNESS] = FALSE;
    }
    if (want_config[CONFIG_RESISTS] > 2) {
	LOG(LOG_WARNING,"gtk::load_defaults","ignoring resists display value read for gdafaults file.\n"
            "Invalid value (%d), must be one value of 0,1 or 2.",
            want_config[CONFIG_RESISTS]);
	want_config[CONFIG_RESISTS] = 0;
    }
    
    /* Make sure the map size os OK */
    if (want_config[CONFIG_MAPWIDTH] < 9 || want_config[CONFIG_MAPWIDTH] > MAP_MAX_SIZE) {
	LOG(LOG_WARNING,"gtk::load_defaults",
            "Invalid map width (%d) option in gdefaults2. Valid range is 9 to %d",
            want_config[CONFIG_MAPWIDTH], MAP_MAX_SIZE);
	want_config[CONFIG_MAPWIDTH] = use_config[CONFIG_MAPWIDTH];
    }
    if (want_config[CONFIG_MAPHEIGHT] < 9 || want_config[CONFIG_MAPHEIGHT] > MAP_MAX_SIZE) {
	LOG(LOG_WARNING,"gtk::load_defaults",
            "Invalid map height (%d) option in gdefaults2. Valid range is 9 to %d",
            want_config[CONFIG_MAPHEIGHT], MAP_MAX_SIZE);
	want_config[CONFIG_MAPHEIGHT] = use_config[CONFIG_MAPHEIGHT];
    }

    /* Now copy over the values just loaded */
    for (i=0; i<CONFIG_NUMS; i++) {
	use_config[i] = want_config[i];
    }
    
    image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_ICONSCALE] / 100;
    map_image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 100;
    map_image_half_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 200;
    /*inv_list.show_icon = use_config[CONFIG_SHOWICON];*/

}

void save_defaults()
{
    char path[MAX_BUF],buf[MAX_BUF];
    FILE *fp;
    int i;

    sprintf(path,"%s/.crossfire/gdefaults2", getenv("HOME"));
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
    fprintf(fp,"theme: %s\n", theme);
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

void config_init(GtkWidget *window_root)
{
    static int has_init=0;
    int count, i;

    has_init=1;

    config_window = create_config_window();

    config_spinbutton_cwindow = lookup_widget(config_window, "config_spinbutton_cwindow");
    config_button_echo = lookup_widget(config_window, "config_button_echo");
    config_button_fasttcp = lookup_widget(config_window, "config_button_fasttcp");
    config_button_grad_color  = lookup_widget(config_window, "config_button_grad_color");
    config_button_foodbeep = lookup_widget(config_window, "config_button_foodbeep");
    config_button_sound = lookup_widget(config_window, "config_button_sound");
    config_button_cache = lookup_widget(config_window, "config_button_cache");
    config_button_download = lookup_widget(config_window, "config_button_download");
    config_button_fog = lookup_widget(config_window, "config_button_fog");
    config_button_smoothing = lookup_widget(config_window, "config_button_smoothing");
    config_spinbutton_iconscale = lookup_widget(config_window, "config_spinbutton_iconscale");
    config_spinbutton_mapscale = lookup_widget(config_window, "config_spinbutton_mapscale");
    config_spinbutton_mapwidth = lookup_widget(config_window, "config_spinbutton_mapwidth");
    config_spinbutton_mapheight = lookup_widget(config_window, "config_spinbutton_mapheight");
    config_combobox_displaymode = lookup_widget(config_window, "config_combobox_displaymode");
    config_combobox_faceset = lookup_widget(config_window, "config_combobox_faceset");
    config_combobox_lighting = lookup_widget(config_window, "config_combobox_lighting");
    config_combobox_theme = lookup_widget(config_window, "config_combobox_theme");

    /* 
     * Display mode combo box setup.
     * First, remove all entries, then populate with what options are
     * available based on what was compiled in.
     */
    count =  gtk_tree_model_iter_n_children(
		    gtk_combo_box_get_model(GTK_COMBO_BOX(config_combobox_displaymode)), NULL);
    for (i=0; i < count; i++)
	gtk_combo_box_remove_text(GTK_COMBO_BOX(config_combobox_displaymode), 0);

#ifdef HAVE_OPENGL
	gtk_combo_box_append_text(GTK_COMBO_BOX(config_combobox_displaymode),  "OpenGL");
#endif

#ifdef HAVE_SDL
	gtk_combo_box_append_text(GTK_COMBO_BOX(config_combobox_displaymode),  "SDL");
#endif
	gtk_combo_box_append_text(GTK_COMBO_BOX(config_combobox_displaymode),  "Pixmap");


}

/**
 * This function is used by scandir below to get only the
 * entries we want.  Basically, we just
 * skip any dot files.
 *
 * @d
 * the dirent entry from scandir.
 *
 * function returns 1 if we want the entry, 0 if we don't.
 */
static int scandir_filter(const struct dirent *d)
{
    if (d->d_name[0] == '.') return 0;
    return 1;
}

/*
 * setup_config_window sets the buttons, combos, etc, to the
 * state that matches the want_config[] values.
 */
static void setup_config_window()
{
    int count, i;
    GtkTreeModel    *model;
    gchar   *buf;
    GtkTreeIter iter;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_echo),
				 want_config[CONFIG_ECHO]);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_fasttcp),
				 want_config[CONFIG_FASTTCP]);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_grad_color),
				 want_config[CONFIG_GRAD_COLOR]);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_foodbeep),
				 want_config[CONFIG_FOODBEEP]);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_sound),
				 want_config[CONFIG_SOUND]);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_cache),
				 want_config[CONFIG_CACHE]);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_download),
				 want_config[CONFIG_DOWNLOAD]);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_fog),
				 want_config[CONFIG_FOGWAR]);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_button_smoothing),
				 want_config[CONFIG_SMOOTH]);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(config_spinbutton_cwindow),
			      (float)want_config[CONFIG_CWINDOW]);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(config_spinbutton_iconscale),
			      (float)want_config[CONFIG_ICONSCALE]);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(config_spinbutton_mapscale),
			      (float)want_config[CONFIG_MAPSCALE]);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(config_spinbutton_mapwidth),
			      (float)want_config[CONFIG_MAPWIDTH]);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(config_spinbutton_mapheight),
			      (float)want_config[CONFIG_MAPHEIGHT]);

    /* 
     * Face set combo box setup.
     * Remove all the entries currently in the combo box 
     */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(config_combobox_faceset));
    count =  gtk_tree_model_iter_n_children(model, NULL);

    for (i=0; i < count; i++)
	gtk_combo_box_remove_text(GTK_COMBO_BOX(config_combobox_faceset), 0);

    /* If we have real faceset info from the server, use it */
    if (face_info.have_faceset_info) {
	for (i=0; i<MAX_FACE_SETS; i++)
	    if (face_info.facesets[i].fullname)
		gtk_combo_box_append_text(GTK_COMBO_BOX(config_combobox_faceset), 
			  face_info.facesets[i].fullname);
    } else {
	gtk_combo_box_append_text(GTK_COMBO_BOX(config_combobox_faceset),  "Standard");
	gtk_combo_box_append_text(GTK_COMBO_BOX(config_combobox_faceset),  "Classic");
    }
    count =  gtk_tree_model_iter_n_children(model, NULL);

    for (i=0; i < count; i++) {
	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, i)) {
	    LOG(LOG_ERROR,"config.c:setup_config_window", "Unable to get faceset iter\n");
	    break;
	}
	gtk_tree_model_get(model, &iter, 0, &buf, -1);

	if (face_info.want_faceset && !strcasecmp(face_info.want_faceset, buf)) {
	    gtk_combo_box_set_active(GTK_COMBO_BOX(config_combobox_faceset), i);
	    g_free(buf);
	    break;
	}
	g_free(buf);
    }

    if (sizeof(display_modes) < want_config[CONFIG_DISPLAYMODE]) {
	LOG(LOG_ERROR, "config.c:setup_config_window", "Player display mode not in display_modes range\n");
    } else {
	/* We want to set up the boxes to match current settings for things
	 * like displaymode.
	 */
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(config_combobox_displaymode));
	count =  gtk_tree_model_iter_n_children(model, NULL);
	for (i=0; i < count; i++) {
	    if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, i)) {
		LOG(LOG_ERROR,"config.c:setup_config_window", "Unable to get faceset iter\n");
		break;
	    }
	    gtk_tree_model_get(model, &iter, 0, &buf, -1);
	    if (!strcasecmp(display_modes[want_config[CONFIG_DISPLAYMODE]], buf)) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(config_combobox_displaymode), i);
		g_free(buf);
		break;
	    }
	    g_free(buf);
	}
    }

    /* We want to set up the boxes to match current settings for things
     * like lighting.  A bit of a hack to hardcode the strings below.
     */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(config_combobox_lighting));
    count =  gtk_tree_model_iter_n_children(model, NULL);
    for (i=0; i < count; i++) {
	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, i)) {
	    LOG(LOG_ERROR,"config.c:setup_config_window", "Unable to get lighting iter\n");
	    break;
	}
	gtk_tree_model_get(model, &iter, 0, &buf, -1);
	if ((want_config[CONFIG_LIGHTING] == CFG_LT_TILE && !strcasecmp(buf, "Per Tile")) ||
	    (want_config[CONFIG_LIGHTING] == CFG_LT_PIXEL && !strcasecmp(buf, "Fast Per Pixel")) ||
	    (want_config[CONFIG_LIGHTING] == CFG_LT_PIXEL_BEST && !strcasecmp(buf, "Best Per Pixel")) ||
	    (want_config[CONFIG_LIGHTING] == CFG_LT_NONE && !strcasecmp(buf, "None"))) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(config_combobox_lighting), i);
		g_free(buf);
		break;
	}
	g_free(buf);
    }


    /* This sees what themes are in the theme directory and puts them in the
     * pulldown menu for the them selection box.
     * the presumption is made that these files won't change during
     * actual runtime, so we only need to do this once.
     */
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(config_combobox_theme));
    count =  gtk_tree_model_iter_n_children(model, NULL);

    /* If count is 0, this hasn't been initialized yet, so do it now */
    if (count == 0) {
	char path[MAX_BUF];
	struct dirent **files;
	int done_none=0;

	snprintf(path, MAX_BUF, "%s/themes", CF_DATADIR);

	count = scandir(path, &files, scandir_filter, alphasort);

	for (i=0; i<count; i++) {
	    /* We want a 'None' entry, so if we get an entry
	     * that would fall after None and we haven't done None
	     * yet, insert it now.
	     */
	    if (!done_none && strcmp(files[i]->d_name, "None") > 0) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(config_combobox_theme),
					  "None");
		done_none=1;
	    }

	    gtk_combo_box_append_text(GTK_COMBO_BOX(config_combobox_theme),
		files[i]->d_name);
	}
	/* Just to set this for below */
	count =  gtk_tree_model_iter_n_children(model, NULL);
    }
    /* Block belows causes the matching theme to be selected.
     * we want toset it regardless of this is the first time
     * we are running through this or not.
     */
    for (i=0; i < count; i++) {
	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, i)) {
	    LOG(LOG_ERROR,"config.c:setup_config_window", "Unable to get theme iter\n");
	    break;
	}
	gtk_tree_model_get(model, &iter, 0, &buf, -1);
	if (!strcasecmp(theme, buf)) {
	    gtk_combo_box_set_active(GTK_COMBO_BOX(config_combobox_theme), i);
	    g_free(buf);
	    break;
	}
	g_free(buf);
    }
}

/*
 * This is basically the opposite of setup_config_window() above -
 * instead of setting the display state appropriately, we read the
 * display state and update the want_config values.
 */
#define IS_DIFFERENT(TYPE) (want_config[TYPE] != use_config[TYPE])

static void read_config_window()
{
    gchar   *buf;

    want_config[CONFIG_ECHO] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_echo));
    want_config[CONFIG_FASTTCP] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_fasttcp));
    want_config[CONFIG_GRAD_COLOR] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_grad_color));
    want_config[CONFIG_FOODBEEP] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_foodbeep));
    want_config[CONFIG_SOUND] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_sound));
    want_config[CONFIG_CACHE] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_cache));
    want_config[CONFIG_DOWNLOAD] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_download));
    want_config[CONFIG_FOGWAR] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_fog));
    want_config[CONFIG_SMOOTH] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(config_button_smoothing));
    want_config[CONFIG_CWINDOW] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(config_spinbutton_cwindow));
    want_config[CONFIG_ICONSCALE] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(config_spinbutton_iconscale));
    want_config[CONFIG_MAPSCALE] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(config_spinbutton_mapscale));
    want_config[CONFIG_MAPWIDTH] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(config_spinbutton_mapwidth));
    want_config[CONFIG_MAPHEIGHT] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(config_spinbutton_mapheight));

    buf = gtk_combo_box_get_active_text(GTK_COMBO_BOX(config_combobox_faceset));

    /* We may be able to eliminate the extra strdup/free, but I'm not
     * 100% sure that we are guaranteed that glib won't implement
     * them through its own/different malloc library.
     */
    if (buf) {
	free(face_info.want_faceset);
	face_info.want_faceset = strdup_local(buf);
	g_free(buf);
    }

    buf = gtk_combo_box_get_active_text(GTK_COMBO_BOX(config_combobox_displaymode));
    if (buf) {
	if (!strcasecmp(buf,"OpenGL")) want_config[CONFIG_DISPLAYMODE] = CFG_DM_OPENGL;
	else if (!strcasecmp(buf,"SDL")) want_config[CONFIG_DISPLAYMODE] = CFG_DM_SDL;
	else if (!strcasecmp(buf,"Pixmap")) want_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
	g_free(buf);
    }

    buf = gtk_combo_box_get_active_text(GTK_COMBO_BOX(config_combobox_lighting));
    if (buf) {
	if (!strcasecmp(buf, "Per Tile")) want_config[CONFIG_LIGHTING] = CFG_LT_TILE;
	else if (!strcasecmp(buf, "Fast Per Pixel")) want_config[CONFIG_LIGHTING] = CFG_LT_PIXEL;
	else if (!strcasecmp(buf, "Best Per Pixel")) want_config[CONFIG_LIGHTING] = CFG_LT_PIXEL_BEST;
	else if (!strcasecmp(buf, "None")) want_config[CONFIG_LIGHTING] = CFG_LT_NONE;
	if  (want_config[CONFIG_LIGHTING] != CFG_LT_NONE) {
	    want_config[CONFIG_DARKNESS] = 1;
	    use_config[CONFIG_DARKNESS] = 1;
	}

	g_free(buf);
    }

    /* Load up the theme.  A problem is we don't really know
     * if theme has been allocated or is just pointing as a
     * a static string.  So we loose a few bytes and just don't free
     * the data.
     */
    buf = gtk_combo_box_get_active_text(GTK_COMBO_BOX(config_combobox_theme));
    if (buf) {
	if (strcmp(buf, theme)) {
	    theme = buf;
	    load_theme(TRUE);
	} else {
	    g_free(buf);
	}
    } else {
	theme = "None";
    }

    /* Some values can take effect right now, others not.  Code below
     * handles these cases - largely grabbed from gtk/config.c
     */
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
    if (IS_DIFFERENT(CONFIG_FASTTCP)) {
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

    if (IS_DIFFERENT(CONFIG_LIGHTING)) {
#ifdef HAVE_SDL
	if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL)
	    /* This is done to make the 'lightmap' in the proper format */
	    init_SDL( NULL, 1);
#endif
    }

    /* Nothing to do, but we can switch immediately without problems.
     * do force a redraw
     */
    if (IS_DIFFERENT(CONFIG_GRAD_COLOR)) {
	use_config[CONFIG_GRAD_COLOR] = want_config[CONFIG_GRAD_COLOR];
	draw_stats(TRUE);
    }
}

void
on_config_button_save_clicked          (GtkButton       *button,
                                        gpointer         user_data)
{
    read_config_window();
    save_defaults();

}


void
on_config_button_apply_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
    read_config_window();

}


void
on_config_button_close_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
    gtk_widget_hide(config_window);

}


void
on_configure_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    gtk_widget_show(config_window);
    setup_config_window();

}



void save_winpos()
{
    char savename[MAX_BUF],buf[MAX_BUF];
    FILE    *save;
    int     x,y,w,h,wx,wy;
    extern GtkWidget *window_root;

    sprintf(savename,"%s/.crossfire/gwinpos2", getenv("HOME"));
    if (!(save=fopen(savename,"w"))) {
        sprintf(buf,"Unable to open %s - window positions not saved!",savename);
        draw_info(buf,NDI_RED);
        return;
    }
    get_window_coord(window_root, &x,&y, &wx,&wy,&w,&h);
    fprintf(save,"window_root: +%d+%dx%dx%d\n", wx, wy, w, h);

    fprintf(save,"vpaned_map_stats: %d\n", 
	    gtk_paned_get_position(GTK_PANED(lookup_widget(window_root,"vpaned_map_stats"))));

    fprintf(save,"vpaned_info_inventory: %d\n", 
	    gtk_paned_get_position(GTK_PANED(lookup_widget(window_root,"vpaned_info_inventory"))));

    fprintf(save,"vpaned_inv_look: %d\n", 
	    gtk_paned_get_position(GTK_PANED(lookup_widget(window_root,"vpaned_inv_look"))));

    fprintf(save,"hpaned_map_other: %d\n", 
	    gtk_paned_get_position(GTK_PANED(lookup_widget(window_root,"hpaned_map_other"))));

    fprintf(save,"hpaned_statbar_stats: %d\n", 
	    gtk_paned_get_position(GTK_PANED(lookup_widget(window_root,"hpaned_statbar_stats"))));

    fclose(save);
    sprintf(buf,"Window positions saved to %s",savename);
    draw_info(buf,NDI_BLUE);

}


void
on_save_window_position_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    save_winpos();
}


void load_window_positions(GtkWidget *window_root)
{
    char loadname[MAX_BUF],buf[MAX_BUF], *cp;
    FILE    *load;

    sprintf(loadname,"%s/.crossfire/gwinpos2", getenv("HOME"));
    if (!(load=fopen(loadname,"r"))) {
        return;
    }
    while(fgets(buf, MAX_BUF-1, load)!=NULL) {
	if ((cp=strchr(buf,':'))!=NULL) {
	    *cp=0;
	    cp++;
	    while(isspace(*cp)) cp++;

	    if (!strcmp(buf,"window_root")) {
		int x,y,w,h;

		if (sscanf(cp,"+%d+%dx%dx%d", &x, &y, &w, &h) == 4) {
		    gtk_window_set_default_size (GTK_WINDOW(window_root), w, h);
		    gtk_window_move(GTK_WINDOW(window_root), x, y);
		    
		} else {
		    LOG(LOG_ERROR,"gtk-v2:load_window_positions", "Window size %s corrupt\n",
			cp);
		}
	    } else if (strstr(buf,"pane")) {
		/* The save names match the widget names, so this works fine to load
		 * up the values.
		 */
		gtk_paned_set_position(GTK_PANED(lookup_widget(window_root,buf)), atoi(cp));
	    } else {
		LOG(LOG_ERROR,"gtk-v2:load_window_positions", "Found unknown line %s in %s\n",
		    buf, loadname);
	    }
	}
    }
    fclose(load);
    
}
