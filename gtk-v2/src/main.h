/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file gtk-v2/src/main.h
 * Contains various global definitions and XML file name and path defaults.
 */

#define NUM_COLORS 13
extern GdkColor root_color[NUM_COLORS];
extern GtkWidget *window_root, *spinbutton_count;
extern GladeXML *dialog_xml, *window_xml;

#define DEFAULT_IMAGE_SIZE      32
extern int map_image_size, map_image_half_size, image_size;

#define XML_PATH_DEFAULT CF_DATADIR "/glade-gtk2/"
#define WINDOW_XML_FILENAME "gtk-v1.glade"
#define DIALOG_XML_FILENAME "dialogs.glade"
extern char window_xml_file[];

#define MAGIC_MAP_PAGE  1 /**< Notebook page of the magic map */

extern gint csocket_fd;

extern char account_password[256];
/* gtk2proto.h depends on this - so may as well just include it here */
#include "info.h"
