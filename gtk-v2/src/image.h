/*
 * char *rcsid_gtk2_image_h =
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


/* Pixmap data.  This is abstracted in the sense that the
 * code here does not care what the data points to (hence the
 * void).  The module using this data should know whether it
 * is these point to png data or image data of whatever form.
 * The module is not required to use all these fileds -
 * as png data includes transperancy, it will generally not
 * use the mask fields and instead just put its data into the
 * appropiate image fields.
 *
 * As images can now be of variable size (and potentially re-sized),
 * the size information is stored here.
 */
#define DEFAULT_IMAGE_SIZE	32
#define MAXPIXMAPNUM 10000

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#endif

typedef struct {
    void	*icon_mask, *icon_image;
    uint16	icon_width, icon_height;
    void	*map_mask, *map_image;
    uint16	map_width, map_height;
    void	*fog_image;
    /* smooth_face is a pointer that points to the face we use
     * for smoothing this particular face.
     */
    uint16	smooth_face;
#ifdef HAVE_OPENGL
    GLuint	map_texture, fog_texture;
#endif
} PixmapInfo;

extern PixmapInfo *pixmaps[MAXPIXMAPNUM];
