/* $Id$ */
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

    The author can be reached via e-mail to crossfire-devel@real-time.com
*/

#ifndef MAP_H
#define MAP_H

#include "client-types.h"


/** The protocol supports 10 layers, so set MAXLAYERS accordingly.
 */
#define MAXLAYERS 10

/**
 * Maximum size of view area a server could support.
 */
#define MAX_VIEW 64

/* Map1 only used 3 layers.  Trying to use 10 seems to cause
 * problems for that code.
 */
#define MAP1_LAYERS 3

struct MapCellLayer {
    sint16 face;
    sint8 size_x;
    sint8 size_y;

    /* Link into animation information.
     * animation is provided to us from the server in the map2 command.
     * animation_speed is also provided.
     * animation_left is how many ticks until animation changes - generated
     *  by client.
     * animation_phase is current phase.
     */
    sint16  animation;
    uint8   animation_speed;
    uint8   animation_left;
    uint8   animation_phase;
};

/** The heads[] in the mapcell is used for single part objects
 * or the head piece for multipart.  The easiest way to think about
 * it is that the heads[] contains the map information as specifically
 * sent from the server.  For the heads value, the size_x and size_y
 * represent how many spaces (up and to the left) that image extends
 * into.
 * The tails are values that the client fills in - if we get
 * a big head value, we fill in the tails value so that the display
 * logic can easily redraw one space.  In this case, the size_ values
 * are offsets that point to the head.  In this way, the draw logic
 * can look at the size of the image, look at these values, and
 * know what portion of it to draw.
 */
struct MapCell
{
    struct MapCellLayer heads[MAXLAYERS];
    struct MapCellLayer tails[MAXLAYERS];
    uint16 smooth[MAXLAYERS];
    uint8 darkness;         /* darkness: 0=fully illuminated, 255=pitch blank */
    uint8 need_update:1;    /* set if tile should be redrawn */
    uint8 have_darkness:1;  /* set if darkness information was set */
    uint8 need_resmooth:1;  /* same has need update but for smoothing only */
    uint8 cleared:1;        /* If set, this is a fog cell. */
};

struct Map
{
    /* Store size of map so we know if map_size has changed
     * since the last time we allocated this;
     */
    int x;
    int y;

    struct MapCell **cells;
};


extern struct Map the_map;


/**
 * Initializes the module. Allocates memory for the_map. This functions must be
 * called before any other function is used.
 */
void mapdata_init(void);

/**
 * Resets all stored information.
 */
void mapdata_reset(void);

/**
 * Sets the current display size. Must be called whenever a new display size
 * was negotiated with the server.
 */
void mapdata_set_size(int viewx, int viewy);

/**
 * Scrolls the map view. Must be called whenever a map_scroll command was
 * received from the server.
 */
void mapdata_scroll(int dx, int dy);

/**
 * Clears the map view. Must be called whenever a newmap command was received
 * from the server.
 */
void mapdata_newmap(void);

/**
 * Checks whether the given coordinates are within the current display size (as
 * set by mapdata_set_size).
 */
int mapdata_is_inside(int x, int y);

/**
 * Returns the face to display at a given location. This function returns the
 * "head" information, i.e. the face information sent by the server.
 */
sint16 mapdata_face(int x, int y, int layer);

/**
 * Returns the face to display at a given location. This function returns the
 * "tail" information, i.e. big faces expanded by the client.
 *
 * *ww and *hh return the offset of the current tile relative to the head;
 * 0 <= *ww < (width of face), 0 <= *hh < (height of face).
 *
 * When drawing the map view, this function must be used instead than a direct
 * access to the_map.cells[]. This is because the_map.cells[] eventually still
 * contains obsolete (fog of war) big face information; this function detects
 * and clears such faces.
 */
sint16 mapdata_bigface(int x, int y, int layer, int *ww, int *hh);

#endif
