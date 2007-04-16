const char *rcsid_gtk_map_c =
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

/* This file handles the map related code - both in terms of allocation,
 * insertion of new objects, and actual rendering (although the 
 * sdl rendering is in the sdl file
 */

#include <config.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <png.h>

/* Pick up the gtk headers we need */
#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <time.h>
#include <gdk/gdkwin32.h>
#endif
#include <gdk/gdkkeysyms.h>

#include "client-types.h"
#include "gx11.h"
#include "client.h"
#include "gtkproto.h"
#include "mapdata.h"


/* Start of map handling code.
 * For the most part, this actually is not window system specific,
 * but certainly how the client wants to store this may vary.
 */

struct Map the_map;

/*
 * Added for fog of war. Current size of the map structure in memory.
 * We assume a rectangular map so this is the length of one side.
 * command.c needs to know about this so not static 
 * FIX ME: Don't assume rectangle
 */

PlayerPosition pl_pos;

/*
 * Request a map update from the server. This is to circumvent a bug in some
 * server versions.
 */
void reset_map()
{
    cs_print_string(csocket.fd, "mapredraw");
}
    
static void draw_pixmap(int srcx, int srcy, int dstx, int dsty, int clipx, int clipy,
			void *mask, void *image, int sizex, int sizey)
{
    gdk_gc_set_clip_mask(mapgc, mask);
    gdk_gc_set_clip_origin(mapgc, clipx, clipy);
    gdk_draw_pixmap(mapwindow, mapgc, image, srcx, srcy, dstx, dsty, sizex, sizey);
}

int display_mapscroll(int dx, int dy)
    {
#ifdef HAVE_SDL
    if (use_config[CONFIG_DISPLAYMODE]==CFG_DM_SDL)
	return sdl_mapscroll(dx,dy);
    else
#endif
    return 0;
}

/* Draw anything in adjacent squares that could smooth on given square
 * mx,my square to smooth on. you should not call this function to
 * smooth on a 'completly black' square.
 * layer layer to examine (we smooth only one layer at a time)
 * picx,picy place on the mapwindow to draw
 */
void drawsmooth (int mx,int my,int layer,int picx,int picy){
    static int dx[8]={0,1,1,1,0,-1,-1,-1};
    static int dy[8]={-1,-1,0,1,1,1,0,-1};
    static int bweights[8]={2,0,4,0,8,0,1,0};
    static int cweights[8]={0,2,0,4,0,8,0,1};
    static int bc_exclude[8]={
                 1+2,/*north exclude northwest (bit0) and northeast(bit1)*/
                 0,
                 2+4,/*east exclude northeast and southeast*/
                 0,
                 4+8,/*and so on*/
                 0,
                 8+1,
                 0
                };
    int partdone[8]={0,0,0,0,0,0,0,0};
    int slevels[8];
    int sfaces[8];
    int i,lowest,weight,weightC;
    int emx,emy;
    int smoothface;

    if (the_map.cells[mx][my].heads[layer].face == 0
         || !CAN_SMOOTH(the_map.cells[mx][my],layer) )
        return;

    for (i=0;i<8;i++){
        emx=mx+dx[i];
        emy=my+dy[i];
        if ( (emx<0) || (emy<0) || (the_map.x<=emx) || (the_map.y<=emy)){
            slevels[i]=0;
            sfaces[i]=0; /*black picture*/
        }
        else if (the_map.cells[emx][emy].smooth[layer]<=the_map.cells[mx][my].smooth[layer]){
            slevels[i]=0;
            sfaces[i]=0; /*black picture*/
        }else{      
            slevels[i]=the_map.cells[emx][emy].smooth[layer];
            sfaces[i]=pixmaps[the_map.cells[emx][emy].heads[layer].face]->smooth_face;
        }                    
    }
    /* ok, now we have a list of smoothlevel higher than current square.
     * there are at most 8 different levels. so... let's check 8 times
     * for the lowest one (we draw from botto to top!).
     */
    lowest=-1;
    while (1){
        lowest = -1;
        for (i=0;i<8;i++){
            if ( (slevels[i]>0) && (!partdone[i]) &&
                ((lowest<0) || (slevels[i]<slevels[lowest]))
               )
                    lowest=i;    
        }
        if (lowest<0)
            break;   /*no more smooth to do on this square*/
        /*printf ("hey, must smooth something...%d\n",sfaces[lowest]);*/
        /*here we know 'what' to smooth*/
        /* we need to calculate the weight
         * for border and weight for corners.
         * then we 'markdone' 
         * the corresponding squares
         */
        /*first, the border, which may exclude some corners*/
        weight=0;
        weightC=15; /*works in backward. remove where there is nothing*/
        /*for (i=0;i<8;i++)
            cornermask[i]=1;*/
        for (i=0;i<8;i++){ /*check all nearby squares*/
            if ( (slevels[i]==slevels[lowest]) &&
                 (sfaces[i]==sfaces[lowest])){
                partdone[i]=1;
                weight=weight+bweights[i];
                weightC&=~bc_exclude[i];
            }else{
                /*must rmove the weight of a corner if not in smoothing*/
                weightC&=~cweights[i];
            }
            
        }
        /*We can't do this before since we need the partdone to be adjusted*/
        if (sfaces[lowest]<=0)
            continue;  /*Can't smooth black*/
        smoothface=sfaces[lowest];
        if (smoothface<=0){
            continue;  /*picture for smoothing not yet available*/
        }
        /* now, it's quite easy. We must draw using a 32x32 part of
         * the picture smoothface. 
         * This part is located using the 2 weights calculated: 
         * (32*weight,0) and (32*weightC,32)
         */
        if ( (!pixmaps[smoothface]->map_image) ||
             (pixmaps[smoothface] == pixmaps[0]))
            continue;   /*don't have the picture associated*/
        if (weight>0){
	    draw_pixmap(
		weight*map_image_size, 0,
		picx, picy,
		picx-weight*map_image_size, picy,
		pixmaps[smoothface]->map_mask, pixmaps[smoothface]->map_image, map_image_size, map_image_size);
        }
        if (weightC>0){
	    draw_pixmap(
		weightC*map_image_size, map_image_size,
		picx, picy,
		picx-weightC*map_image_size, picy-map_image_size,
		pixmaps[smoothface]->map_mask, pixmaps[smoothface]->map_image, map_image_size, map_image_size);
        }
    }/*while there's some smooth to do*/
}

static void display_mapcell(int ax, int ay, int mx, int my)
{
    int layer;
    int face;

    /* First, we need to black out this space. */
    for (layer=0; layer<MAXLAYERS; layer++) {
        face = mapdata_face(ax, ay, layer);
        if ((face > 0) && (!pixmaps[face]->map_mask))
            break;
    }
    /* Only draw rectangle if all faces have transparency */
    if (layer==MAXLAYERS)
        gdk_draw_rectangle(mapwindow, drawingarea->style->black_gc, TRUE, ax*map_image_size, ay*map_image_size, map_image_size, map_image_size);


    /* now draw the different layers.  Only draw if using fog of war or the
     * space isn't clear.
     */
    if (use_config[CONFIG_FOGWAR] || !the_map.cells[mx][my].cleared) {
	for (layer=0; layer<MAXLAYERS; layer++) {
	    int sx, sy;

	    /* draw single-tile faces first */
	    face = mapdata_face(ax, ay, layer);
	    if (face > 0 && pixmaps[face]->map_image != NULL) {
		int w = pixmaps[face]->map_width;
		int h = pixmaps[face]->map_height;
		draw_pixmap(
		    w-map_image_size, h-map_image_size,
		    ax*map_image_size, ay*map_image_size,
		    ax*map_image_size+map_image_size-w, ay*map_image_size+map_image_size-h,
		    pixmaps[face]->map_mask, pixmaps[face]->map_image, map_image_size, map_image_size);

		if ( use_config[CONFIG_SMOOTH])
		    drawsmooth(mx, my, layer, ax*map_image_size, ay*map_image_size);
	    }
	    /* Sometimes, it may happens we need to draw the smooth while there
	     * is nothing to draw at that layer (but there was something at lower
	     * layers). This is handled here. The else part is to take into account
	     * cases where the smooth as already been handled 2 code lines before
	     */
	    else if (use_config[CONFIG_SMOOTH] && the_map.cells[mx][my].need_resmooth)
		drawsmooth (mx, my, layer, ax*map_image_size, ay*map_image_size);
		    
	    /* draw big faces last (should overlap other objects) */
	    face = mapdata_bigface(ax, ay, layer, &sx, &sy);
	    if (face > 0 && pixmaps[face]->map_image != NULL) {
		/* This is pretty messy, because images are not required to be
		 * an integral multiplier of the image size.  There
		 * are really 4 main variables:
		 * source[xy]: From where within the pixmap to start grabbing pixels.
		 * off[xy]: Offset from space edge on the visible map to start drawing pixels.
		 *   off[xy] also determines how many pixels to draw (map_image_size - off[xy])
		 * clip[xy]: Position of the clipmask.  The position of the clipmask is always
		 *   at the upper left of the image as we drawn it on the map, so for any
		 *   given big image, it will have the same values for all the pieces.  However
		 *   we need to re-construct that location based on current location.
		 *
		 * For a 32x72 image, it would be drawn like follows:
		 *		    sourcey	    offy
		 * top space:	    0		    24
		 * middle space:    8		    0
		 * bottom space:    40		    0
		 */
		int dx, dy, sourcex, sourcey, offx, offy, clipx, clipy;

		dx = pixmaps[face]->map_width % map_image_size;
		offx = dx?(map_image_size -dx):0;
		clipx = (ax - sx)*map_image_size + offx;

		if (sx) {
		    sourcex = sx * map_image_size - offx ;
		    offx=0;
		} else {
		    sourcex=0;
		}

		dy = pixmaps[face]->map_height % map_image_size;
		offy = dy?(map_image_size -dy):0;
		clipy = (ay - sy)*map_image_size + offy;

		if (sy) {
		    sourcey = sy * map_image_size - offy;
		    offy=0;
		} else {
		    sourcey=0;
		}

		draw_pixmap(
		    sourcex,  sourcey,
		    ax*map_image_size+offx, ay*map_image_size + offy,
		    clipx, clipy,
		    pixmaps[face]->map_mask, pixmaps[face]->map_image,
		    map_image_size - offx, map_image_size - offy);
	    }
	} /* else for processing the layers */
    }

    /* If this is a fog cell, do darknening of the space.
     * otherwise, process light/darkness - only do those if not a 
     * fog cell.
     */
    if (use_config[CONFIG_FOGWAR] && the_map.cells[mx][my].cleared) {
	draw_pixmap(0, 0, ax*map_image_size, ay*map_image_size, ax*map_image_size, ay*map_image_size, dark1, dark, map_image_size, map_image_size);
    }
    else if (the_map.cells[mx][my].darkness > 192) { /* Full dark */
	gdk_draw_rectangle (mapwindow, drawingarea->style->black_gc,
	    TRUE,map_image_size*ax, map_image_size*ay,
	    map_image_size, map_image_size);
    } else if (the_map.cells[mx][my].darkness> 128) {
	draw_pixmap(0, 0, ax*map_image_size, ay*map_image_size, ax*map_image_size, ay*map_image_size, dark1, dark, map_image_size, map_image_size);
    } else if (the_map.cells[mx][my].darkness> 64) {
	draw_pixmap(0, 0, ax*map_image_size, ay*map_image_size, ax*map_image_size, ay*map_image_size, dark2, dark, map_image_size, map_image_size);
    } else if (the_map.cells[mx][my].darkness> 1) {
	draw_pixmap(0, 0, ax*map_image_size, ay*map_image_size, ax*map_image_size, ay*map_image_size, dark3, dark, map_image_size, map_image_size);
    }
}

void gtk_draw_map(int redraw) {
    int mx, my;
    int x, y;
    struct timeval tv1, tv2,tv3;
    long elapsed1, elapsed2;

    if (time_map_redraw)
	gettimeofday(&tv1, NULL);

    for(x = 0; x < use_config[CONFIG_MAPWIDTH]; x++) {
	for(y = 0; y < use_config[CONFIG_MAPHEIGHT]; y++) {
	    /* mx,my represent the spaces on the 'virtual' map (ie, the_map structure).
	     * x and y (from the for loop) represent the visable screen.
	     */
	    mx = pl_pos.x+x;
	    my = pl_pos.y+y;
	    if (redraw
	    || the_map.cells[mx][my].need_update
	    || the_map.cells[mx][my].need_resmooth) {
		display_mapcell(x, y, mx, my);
		the_map.cells[mx][my].need_update=0;
		the_map.cells[mx][my].need_resmooth=0;
	    }
	}
    }

    if (time_map_redraw)
	gettimeofday(&tv2, NULL);

    gdk_draw_pixmap(drawingarea->window, drawingarea->style->black_gc, mapwindow,
	0, 0, 0, 0, use_config[CONFIG_MAPWIDTH] * map_image_size, use_config[CONFIG_MAPHEIGHT] * map_image_size);
    if (time_map_redraw) {
	gettimeofday(&tv3, NULL);
	elapsed1 = (tv2.tv_sec - tv1.tv_sec)*1000000 + (tv2.tv_usec - tv1.tv_usec);
	elapsed2 = (tv3.tv_sec - tv2.tv_sec)*1000000 + (tv3.tv_usec - tv2.tv_usec);

	/* I care about performance for 'long' updates, so put the check in to make
	 * these a little more noticable */
	if ((elapsed1 + elapsed2)>10000)
	    LOG(LOG_INFO,"gtk::sdl_gen_map","gen took %7ld, flip took %7ld, total = %7ld",
		    elapsed1, elapsed2, elapsed1 + elapsed2);
    }
}
