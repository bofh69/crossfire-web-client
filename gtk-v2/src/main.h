/*
 * char *rcsid_gtk2_main_h =
 *   "$Id$";
 */
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

/**
 * @file gtk-v2/src/main.h
 * Contains various global definitions and XML file name and path defaults.
 */

#define NUM_COLORS 13
extern GdkColor root_color[NUM_COLORS];
GtkWidget *window_root, *spinbutton_count;
GladeXML *dialog_xml, *window_xml;

#define DEFAULT_IMAGE_SIZE      32
extern int map_image_size, map_image_half_size, image_size;

#define XML_PATH_DEFAULT PACKAGE_DATA_DIR "/" PACKAGE "/glade-gtk2/"
#define WINDOW_XML_FILENAME "gtk-v2.glade"
#define DIALOG_XML_FILENAME "dialogs.glade"
extern char window_xml_file[];

#define MAGIC_MAP_PAGE  1 /**< Notebook page of the magic map */

