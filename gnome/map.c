
#include <config.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <png.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "client-types.h"
#include "gnome-cf.h"
#include "client.h"
#include "gnomeproto.h"


/* Start of map handling code.
 * For the most part, this actually is not window system specific,
 * but certainly how the client wants to store this may vary.
 */

struct Map the_map;
uint8	map_did_scroll=0;

/*
 * Added for fog of war. Current size of the map structure in memory.
 * We assume a rectangular map so this is the length of one side.
 * command.c needs to know about this so not static 
 * FIX ME: Don't assume rectangle
 */

int map_size= 0;
PlayerPosition pl_pos;

/* gnome does not currently support this - just turn it off to keep the
 * code more common
 */
static uint8 sdlimage=0;

/*
 * Takes three args, first is a return value that is a pointer
 * we should put map info into. Next two are map dimensions.
 * This function supports non rectangular maps but the client
 * pretty much doesn't. The caller is responsible for freeing
 * the data. I have the caller pass in a map struct instead of
 * returning a pointer because I didn't want to change all the
 * the_map.cells to the_map->cells...
 * The returned map memory is zero'ed.
 */
void allocate_map( struct Map* new_map, int ax, int ay)
{

  int i= 0;

  if( new_map == NULL)
    return;

  if( ax < 1 || ay < 1) {
    new_map->cells= NULL;
    return;
  }

  new_map->cells= (struct MapCell**)calloc( sizeof( struct MapCell*) * ay 
					    + sizeof( struct MapCell) *
					    map_size * map_size, 1);
  if( new_map->cells == NULL)
    return;

  /* Skip past the first row of pointers to rows and assign the start of
   * the actual map data
   */
  new_map->cells[0]= (struct MapCell*)((char*)new_map->cells + 
				       (sizeof( struct MapCell*) * ay));

  /* Finish assigning the beginning of each row relative to the first row
   * assigned above
   */
  for( i= 0; i < ay; i++) 
    {
      new_map->cells[i]= new_map->cells[0] + ( i * ax);
    }

  new_map->x= ax;
  new_map->y= ay;

  return;
}

/*
 * Clears out all the cells in the current view (which is 
 * the whole map if not using fog_of_war, and request
 * a map update from the server 
 */
void reset_map()
{
    if( fog_of_war == TRUE)
    {
	int x= 0;
	int y= 0;
	pl_pos.x= the_map.x/2;
	pl_pos.y= the_map.y/2;
	memset( the_map.cells[0], 0, 
		sizeof( struct MapCell) * the_map.x * the_map.y);
	for( x= pl_pos.x; x < (pl_pos.x + mapx); x++) 
	{
	    for( y= pl_pos.y; y < (pl_pos.y + mapy); y++)
	    {
		the_map.cells[x][y].need_update= 1;
	    }
	}
    }
    else
    {
	int x= 0;
	int y= 0;
	memset( the_map.cells[0], 0, 
		sizeof( struct MapCell) * the_map.x * the_map.y);
	for( x= 0; x < mapx; x++)
	{
	    for( y= 0; y < mapy; y++)
	    {
		the_map.cells[x][y].need_update= 1;
	    }
	}
    }
    
    cs_print_string(csocket.fd, "mapredraw");
    
    return;
}

void display_map_clearcell(long x,long y)
{
    if( fog_of_war == TRUE)
    {
	/* we don't want to clear out the values yet. We will do that
	 * next time we try to write some data to this tile. For now
	 * we just mark that it has been cleared. Also mark it for
	 * update so we can draw the proper fog cell over it
	 */
	x+= pl_pos.x;
	y+= pl_pos.y;
	the_map.cells[x][y].cleared= 1;
	the_map.cells[x][y].need_update= 1;
    }
    else 
    {
	int i;
	the_map.cells[x][y].count = 0;
	the_map.cells[x][y].darkness = 0;
	the_map.cells[x][y].need_update = 1;
	the_map.cells[x][y].have_darkness = 0;
	the_map.cells[x][y].cleared= 0;
	for (i=0; i<MAXFACES; i++)
	    the_map.cells[x][y].faces[i] = -1;  /* empty/blank face */
    }

    return;
}

void print_darkness()
{

    int x= 0;
    int y= 0;

    for( y= 0; y < mapy; y++)
    {
	for( x= 0; x < mapx; x++)
	{
	    if( the_map.cells[x][y].count== 0)
		fprintf( stderr, "[ - ]");
	    else
		fprintf( stderr, "[%3d]", the_map.cells[x][y].darkness);
	}
	fprintf( stderr, "\n");
    }
}

void print_map()
{
    int x= 0;
    int y= 0;
    int z= 0;

    int local_mapx;
    int local_mapy;

    if( fog_of_war == TRUE)
    {
	local_mapx= pl_pos.x + mapx;
	local_mapy= pl_pos.y + mapy;
	printf( " Current X pos: %d -- Current Y pos: %d\n", 
		pl_pos.x, pl_pos.y);
    }
    else 
    {
	local_mapx= mapx;
	local_mapy= mapy;
    }

    fprintf( stderr, "-----------------------\n");
    for( y= (fog_of_war == TRUE ? pl_pos.y : 0); y < local_mapy; y++)
    {
	for( z= 0; z < MAXFACES; z++)
	{
	    for( x= (fog_of_war == TRUE ? pl_pos.x : 0); x < local_mapx; x++)
	    {
		if( the_map.cells[x][y].count == 0)
		    fprintf( stderr, "[ -- ]");
		else 
		    fprintf( stderr, "[%4d]", the_map.cells[x][y].faces[z]);
	    }
	    fprintf( stderr, "\n");
	}
	fprintf( stderr, "\n");
    }
    fprintf( stderr, "-----------------------\n");
    return;
}

void set_map_darkness(int x, int y, uint8 darkness)
{
  if( fog_of_war == TRUE)
  {
      x+= pl_pos.x;
      y+= pl_pos.y;
  }

    the_map.cells[x][y].have_darkness = 1;
    if (darkness != (255 - the_map.cells[x][y].darkness )) {
	the_map.cells[x][y].darkness = 255 - darkness;
	the_map.cells[x][y].need_update = 1;
	/* pretty ugly - since the light code with pngximage uses
	 * neighboring spaces to adjust the darkness, we now need to
	 * let the neighbors know they should update their darkness
	 * now.
	 */
	if (sdlimage) {
	    if (x-1>0) the_map.cells[x-1][y].need_update = 1;
	    if (y-1>0) the_map.cells[x][y-1].need_update = 1;
	    if (x+1<mapx) the_map.cells[x+1][y].need_update = 1;
	    if (y+1<mapy) the_map.cells[x][y+1].need_update = 1;
	}
    }
}

/* sets the face at layer to some value.  We just can't
 * restact arbitrarily, as the server now sends faces only
 * for layers that change, and not the entire space.
 */
void set_map_face(int x, int y, int layer, int face)
{
  if( fog_of_war == TRUE)
  {
      x+= pl_pos.x;
      y+= pl_pos.y;
  }

  if( (fog_of_war == TRUE) && (the_map.cells[x][y].cleared == 1) )
  {
      /* This cell has been cleared previously but now we are 
       * writing new data to do. So we have to clear it for real now 
       */
      int i= 0;
      the_map.cells[x][y].count= 0;
      the_map.cells[x][y].darkness= 0;
      the_map.cells[x][y].need_update= 1;
      the_map.cells[x][y].have_darkness= 0;
      the_map.cells[x][y].cleared= 0;
      for (i=0; i<MAXFACES; i++)
	  the_map.cells[x][y].faces[i]= -1;  /* empty/blank face */
  }

  the_map.cells[x][y].faces[layer] = face;
  if ((layer+1) > the_map.cells[x][y].count)
    the_map.cells[x][y].count = layer+1;
  the_map.cells[x][y].need_update = 1;
  the_map.cells[x][y].have_darkness = 1;
}


void display_map_addbelow(long x,long y,long face)
{

    if( fog_of_war == TRUE) 
    {
	x+= pl_pos.x;
	y+= pl_pos.y;
    }
    
    if( (fog_of_war == TRUE) && (the_map.cells[x][y].cleared == 1) )
    {
	/* This cell has been cleared previously but now we are 
	 * writing new data to do. So we have to clear it for real now 
	 */
	int i= 0;
	the_map.cells[x][y].count= 0;
	the_map.cells[x][y].darkness= 0;
	the_map.cells[x][y].need_update= 1;
	the_map.cells[x][y].have_darkness= 0;
	the_map.cells[x][y].cleared= 0;
	for (i=0; i<MAXFACES; i++)
	    the_map.cells[x][y].faces[i]= -1;  /* empty/blank face */
    }

    the_map.cells[x][y].faces[the_map.cells[x][y].count] = face&0xFFFF;
    the_map.cells[x][y].count ++;
    the_map.cells[x][y].need_update = 1;
}

/* 
 * Returns true if virtual view is about to butt up against 
 * the side of the virtual map on the next scroll
 * Only used for fog of war code
 */
static int need_recenter_map( int dx, int dy)
{
    
    if( pl_pos.x + dx + mapx >= the_map.x ||
	pl_pos.y + dy + mapy >= the_map.y ||
	pl_pos.x + dx <= 0                ||
	pl_pos.y + dy <= 0 )
    {
	return TRUE;
    }
    
    return FALSE;
}

/*
 * Only used in fog of war code.
 * Will recenter the virtual coordinates of the player view 
 * to the center of the map and try to keep as much current
 * state in memory as possible
 * If view is already close to center it won't move it
 */
static void recenter_virtual_map_view( struct Map *map)
{
    static struct Map tmpmap;
    struct MapCell **tmpcells;
    int y_shift= 0;
    int x_shift= 0;
    int x= 0, y= 0;

    if( map == NULL)
	return;


    if( tmpmap.cells == NULL)
    {
	allocate_map( &tmpmap, map->x, map->y);
    }

    /* 
     * If mapsize changed, allocate a new map
     */
    if( tmpmap.x != map->x || tmpmap.y != map->y)
    {
	free(tmpmap.cells);

	allocate_map( &tmpmap, map->x, map->y);
    }


    /*
     * If we are less then 1/4 away from either edge of the virtual map
     * or the next move would push us up against the edge (for small
     * virtual maps with large views this could happen before our 0,0 view
     * coordinate is within 1/4 of the edge) we shift to the center.
     */
    if( pl_pos.x <= (map->x/4) || pl_pos.x >= (map->x*3/4) ||
	pl_pos.x + mapx + 1 >= map->x )
    {
	x_shift= map->x/2 - pl_pos.x;
    }
    if( pl_pos.y <= (map->y/4) || pl_pos.y >= (map->y*3/4) ||
	pl_pos.y + mapy + 1 >= map->y )
    {
	y_shift= map->y/2 - pl_pos.y;
    }


    if( x_shift == 0 && y_shift == 0)
	return;

    for( x= 0; x < map->x; x++)
    {
	if( x + x_shift >= map->x || x + x_shift < 0)
	    continue;

	for( y= 0; y < map->y; y++)
	{
	    if( y + y_shift >= map->y || y + y_shift < 0)
		continue;

	    memcpy( (char*)&tmpmap.cells[x + x_shift][y + y_shift],
		    (char*)&map->cells[x][y],
		    sizeof( struct MapCell) );
	}
    }


    pl_pos.x+= x_shift;
    pl_pos.y+= y_shift;


    /*
     * Swap cell arrays then zero out the old cells to avoid another big memcopy
     */
    tmpcells= map->cells;
    map->cells= tmpmap.cells;
    tmpmap.cells= tmpcells;

    memset( (char*)&tmpmap.cells[0][0], 0,
	    sizeof( struct MapCell) * tmpmap.x * tmpmap.y);

    return;
}

  
void display_mapscroll(int dx,int dy)
{
    int x,y;
    static struct Map newmap;
    int local_mapx= 0, local_mapy= 0;

    if( fog_of_war == TRUE) 
    {
	/* We don't need to memcopy any of this stuff around cause 
	 * we are keeping it in memory. We do need to update our
	 * virtual position though
	 */
	
	if( need_recenter_map( dx, dy) == TRUE) 
	{
	    recenter_virtual_map_view( &the_map);
	}
	
	pl_pos.x+= dx;
	pl_pos.y+= dy;
	local_mapx= pl_pos.x + mapx;
	local_mapy= pl_pos.y + mapy;
	
	/*
	 * For cells about to enter the view, mark them as
	 * needing an update. Cells that are already in 
	 * view don't need to be updated since we just memcpy
	 * the image data around. This is needed for proper 
	 * drawing of blank or black tiles coming into view
	 */
	for( x= pl_pos.x; x < pl_pos.x + mapx; x++) {
	    for( y= pl_pos.y; y < pl_pos.y + mapy; y++) {
		if( (x + dx) < pl_pos.x || (x + dx) >= (mapx + pl_pos.x) ||
		    (y + dy) < pl_pos.y || (y + dy) >= (mapy + pl_pos.y) ) 
		{
		    if( x < 0 || y < 0 || x >= the_map.x ||
			y >= the_map.y)
		    {
			continue;
		    }
		    
		    the_map.cells[x][y].need_update= 1;
		    the_map.cells[x][y].cleared= 1;
		}
	    } /* for y */
	} /* for x */
    }
    else 
    {
	local_mapx= mapx;
	local_mapy= mapy;
    }

    if( newmap.cells == NULL)
	allocate_map( &newmap, map_size, map_size);

    /* Check to see if map_size changed since we allocated newmap */
    if( newmap.x != map_size) 
    {
	free(newmap.cells);
	
	allocate_map( &newmap, map_size, map_size);
    }
    
    if( fog_of_war == FALSE) {
      for(x=0;x<mapx;x++) {
	for(y=0;y<mapy;y++) {
	  /* In case its own of range, set the count to zero */
	  if (x+dx < 0 || x+dx >= mapx ||y+dy < 0 || y+dy >= mapy) {
	    memset((char*)&newmap.cells[x][y], 0, sizeof(struct MapCell));
	    /*
	     * Changed my smacfiggen 6/20/2001 -- When new cells come onto
	     * the map and we aren't using the new map command we want to 
	     * mark these as updated or else blank tiles get blitted with 
	     * old info.
	     *
	     */
	    if ( !map1cmd)
	      newmap.cells[x][y].need_update=1;
	  } else {
	    memcpy((char*)&(newmap.cells[x][y]), (char*)&(the_map.cells[x+dx][y+dy]),
		   sizeof(struct MapCell));
	    /* if using pngximage, we will instead set the map_did_scroll
	     * to 1 - we don't want to regen the backing image
	     */
	    if( !sdlimage) {
		newmap.cells[x][y].need_update=1;
	    }
	  }
	}
      }
      memcpy((char*)the_map.cells[0],(char*)newmap.cells[0],
	     sizeof(struct MapCell)*newmap.x*newmap.y );
    }
#if 0
    if (sdlimage) 
	sdl_mapscroll(dx,dy);
#endif
	
/*    fprintf(stderr,"scroll command: %d %d\n", dx, dy);*/
}



/*
 * Clears all map data - this is only called when we have lost our connection
 * to a server - this way bogus data won't be around when we connect
 * to the new server
 */
void reset_map_data()
{
    if( fog_of_war == TRUE)
    {
	int x= 0;
	int y= 0;
	pl_pos.x= the_map.x/2;
	pl_pos.y= the_map.y/2;
	memset( the_map.cells[0], 0, 
		sizeof( struct MapCell) * the_map.x * the_map.y);
	for( x= pl_pos.x; x < (pl_pos.x + mapx); x++) 
	{
	    for( y= pl_pos.y; y < (pl_pos.y + mapy); y++)
	    {
		the_map.cells[x][y].need_update= 1;
	    }
	}
    }
    else
    {
	int x= 0;
	int y= 0;
	memset( the_map.cells[0], 0, 
		sizeof( struct MapCell) * the_map.x * the_map.y);
	for( x= 0; x < mapx; x++)
	{
	    for( y= 0; y < mapy; y++)
	    {
		the_map.cells[x][y].need_update= 1;
	    }
	}
    }
}

/* gtk -> gnome mapping */

#define mapgc	gc
/* This draws the map using GdkPixmaps/GdkBitmaps.  It is based off of sdl_gen_map.
 * We draw on a GdkPixmap drawable (mapwindow), which we then copy to the 
 * visible area on the screen.  Doing this reduces flickering that otherwise happen
 * if we draw directly to the window.
 * The performance of this function seems to be very good - presumably because
 * most of this data is stored in the x-server in a format native to it.  Also,
 * per pixel manipulations are not done - either the entire pixel is copied,
 * or it is not, no blending is done.  This means that true alpha blending
 * is not supported, but nothing uses that now anyways.
 */
/*#define TIME_MAP_REDRAW*/
void gtk_draw_map()
{
    int x,y,onlayer,layer, dst_x, dst_y;
#ifdef TIME_MAP_REDRAW
    struct timeval tv1, tv2, tv3;
    long elapsed1, elapsed2;
    gettimeofday(&tv1, NULL);
#endif

    gdk_draw_rectangle (drawable->window, drawable->style->black_gc,
		TRUE, 0,0, map_image_size*mapx, map_image_size * mapy);

    for (onlayer=MAXFACES-1; onlayer>=0; onlayer--) {
	if (map1cmd) layer = (MAXFACES -1)  - onlayer;
        else layer = onlayer;

	/* Fog not currently supported, so don't need to worry about that complexity */
	for( x= mapx-1; x>= 0; x--) {
            for(y = mapy-1; y >= 0; y--) {
                /* there must be a real face for this layer, and we must have data for that face. */
                if ((the_map.cells[x][y].faces[layer]>0) && 
		    pixmaps[the_map.cells[x][y].faces[layer]].map_image) {
                    /* Figure out how much data is being copied, and adjust the origin accordingly.
                     * This probably needs additional checking in case the image extends beyond the
                     * map boundries.
                     */
                    dst_x = (x+1) * map_image_size - pixmaps[the_map.cells[x][y].faces[layer]].map_width;
                    dst_y = (y+1) * map_image_size - pixmaps[the_map.cells[x][y].faces[layer]].map_height;
		    gdk_gc_set_clip_mask (mapgc, pixmaps[the_map.cells[x][y].faces[layer]].map_mask);
		    gdk_gc_set_clip_origin(mapgc, dst_x, dst_y);
		    gdk_draw_pixmap(drawable->window, mapgc,
				    pixmaps[the_map.cells[x][y].faces[layer]].map_image,
				    0, 0, dst_x, dst_y, 
				    pixmaps[the_map.cells[x][y].faces[layer]].map_width,
				    pixmaps[the_map.cells[x][y].faces[layer]].map_height);
		}
               /* On last past, do our special processing, like applying darkness */
                if (onlayer==0) {
                    the_map.cells[x][y].need_update=0;
		    if (the_map.cells[x][y].darkness > 192) { /* Full dark */
			gdk_draw_rectangle (drawable->window, drawable->style->black_gc,
					    TRUE,map_image_size*x, map_image_size *y,
					    map_image_size, map_image_size);
		    } else if (the_map.cells[x][y].darkness> 128) {
			gdk_gc_set_clip_mask(mapgc, dark1);
			gdk_gc_set_clip_origin(mapgc, x * map_image_size, y*map_image_size);
			gdk_draw_pixmap(drawable->window, mapgc, dark, 0, 0,
					x * map_image_size, y*map_image_size, map_image_size, map_image_size);
		    } else if (the_map.cells[x][y].darkness> 64) {
			gdk_gc_set_clip_mask(mapgc, dark2);
			gdk_gc_set_clip_origin(mapgc, x * map_image_size, y*map_image_size);
			gdk_draw_pixmap(drawable->window, mapgc, dark, 0, 0,
					x * map_image_size, y*map_image_size, map_image_size, map_image_size);
		    } else if (the_map.cells[x][y].darkness> 1) {
			gdk_gc_set_clip_mask(mapgc, dark3);
			gdk_gc_set_clip_origin(mapgc, x * map_image_size, y*map_image_size);
			gdk_draw_pixmap(drawable->window, mapgc, dark, 0, 0,
					x * map_image_size, y*map_image_size, map_image_size, map_image_size);
		    }
		}
	    } /* for y */
	} /* for x */
    } /* for layers */

#ifdef TIME_MAP_REDRAW
    gettimeofday(&tv2, NULL);
#endif
    gdk_draw_pixmap(drawable->window, drawable->style->black_gc, drawable->window,
		    0, 0, 0, 0, mapx * map_image_size, mapy * map_image_size);

#ifdef TIME_MAP_REDRAW
    gettimeofday(&tv3, NULL);
    elapsed1 = (tv2.tv_sec - tv1.tv_sec)*1000000 + (tv2.tv_usec - tv1.tv_usec);
    elapsed2 = (tv3.tv_sec - tv2.tv_sec)*1000000 + (tv3.tv_usec - tv2.tv_usec);

    /* I care about performance for 'long' updates, so put the check in to make
     * these a little more noticable */
    if ((elapsed1 + elapsed2)>10000)
        fprintf(stderr,"gtk_draw_map: gen took %7ld, flip took %7ld, total = %7ld\n",
                    elapsed1, elapsed2, elapsed1 + elapsed2);
#endif
}

