/*
 * char *rcsid_gtk2_image_h =
 *   "$Id$";
 */

/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005-2008,2010 Mark Wedel & Crossfire Development Team

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
 * @file image.h
 * Pixmap data.  This is abstracted in the sense that the code here does not
 * care what the data points to (hence the void).  The module using this data
 * should know whether it is these point to png data or image data of whatever
 * form.  The module is not required to use all these fields - as png data
 * includes transparancy, it will generally not use the mask fields and instead
 * just put its data into the appropriate image fields.
 *
 * As images can now be of variable size (and potentially re-sized), the size
 * information is stored here.
 */
#define DEFAULT_IMAGE_SIZE 32
#define MAXPIXMAPNUM 10000

typedef struct PixmapInfo {
    /* Icons, used in the inventory tree view */
    /* Scaled according to inventory scale config */
    GdkPixbuf *icon_image;
    guint16 icon_width, icon_height;
    /* "Full icons", used in the spell list and inventory table view */
    /* Same as above, but not scaled */
    GdkPixbuf *full_icon_image;
    guint16 full_icon_width, full_icon_height;
    /* Map images */
    void *map_image; // same as full_icon_image, but as a cairo_surface_t
    guint16 smooth_face; /**< A face used for smoothing with this face. */
} PixmapInfo;

extern PixmapInfo *pixmaps[MAXPIXMAPNUM];
extern int have_new_image;
