/*
 * char *rcsid_gtk2_info_h =
 *   "$Id$";
 */

/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2010 Mark Wedel & Crossfire Development Team

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

#ifndef INFO_H
#define INFO_H

/**
 * @file info.h
 * This file is really here to declare the Info_Pane structure.
 * the account based login code also uses info panes, so it
 * needs to be declared in a place where both files can access it.
 */

#include "shared/newclient.h"

/**
 * @{
 * @name GTK V2 Font Style Definitions.
 * Font style support definitions for the info window.
 * Font style defines are indices into the font_style_names[] array.
 * The actual fonts that they are bound to are set up in the style file.
 */
#define FONT_NORMAL     0
#define FONT_ARCANE     1
#define FONT_STRANGE    2
#define FONT_FIXED      3
#define FONT_HAND       4
#define NUM_FONTS       5


typedef struct Info_Pane
{
    GtkWidget       *textview;
    GtkWidget       *scrolled_window;
    GtkTextBuffer   *textbuffer;
    GtkTextMark     *textmark;
    GtkAdjustment   *adjustment;
    GtkTextTag      *color_tags[NUM_COLORS];
    GtkTextTag      *font_tags[NUM_FONTS];
    GtkTextTag      *bold_tag, *italic_tag, *underline_tag, *default_tag;
    GtkTextTag      **msg_type_tags[MSG_TYPE_LAST];
} Info_Pane;

#endif
