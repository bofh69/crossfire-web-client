char *rcsid_gtk_sdl_c =
    "$Id$";

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

#include "config.h"

#ifdef HAVE_SDL

#include <client-types.h>
#include <SDL.h>
#include <SDL_image.h>

/* Pick up the gtk headers we need */
#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#endif
#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "image.h"
#include <client.h>
#include "gtk2proto.h"
#include "mapdata.h"

/* Actual SDL surface the game view is painted on */
SDL_Surface* mapsurface;
static SDL_Surface* lightmap;
static SDL_Surface* fogmap;
static char *redrawbitmap;

extern int time_map_redraw;


/* Move some of the SDL code to this file here.  This makes it easier to share
 * between the gnome and gtk client.  It also reduces the length of both the gx11.c
 * and gnome.c file.  It also is more readable, as not as many #ifdef SDL.. #endif
 * constructs are needed.
 * Note that there may still be some SDL code in gx11.c - some areas are embedded
 * so much that it is not easy to remove.
 */

/* these should generally be included by the file including this file. */
#include <SDL.h>
#include <SDL_image.h>


static void do_SDL_error(const char *SDL_function, const char *file, int line)
{
  LOG(LOG_CRITICAL,SDL_function,"SDL error in file %s line %d\n%s",
	   file, line, SDL_GetError());
  SDL_Quit();
  exit( 1);
}

/*
 * Set the pixel at (x, y) to the given value
 * NOTE: The surface must be locked before calling this!
 * This function is directly grabbed from the SDL docs.
 * Note this is not currently used, but is useful enough
 * that it should be included.
 */
static void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
	} else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
	}
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}

static void overlay_grid( int re_init, int ax, int ay)
{

  static SDL_Surface* grid_overlay;

  static int first_pass;

  int x= 0;
  int y= 0;
  SDL_Rect dst;
  Uint32 *pixel;
  SDL_PixelFormat* fmt;

  /* Need to convert back to screen coordinates */
  ax-= pl_pos.x;
  ay-= pl_pos.y;
  
  if( re_init == TRUE)
    {
      if( grid_overlay)
	SDL_FreeSurface( grid_overlay);

      first_pass= 0;
      grid_overlay= NULL;
    }

  if( grid_overlay == NULL)
    {
      grid_overlay= SDL_CreateRGBSurface( SDL_HWSURFACE|SDL_SRCALPHA, 
					  use_config[CONFIG_MAPWIDTH]*map_image_size,
					  use_config[CONFIG_MAPHEIGHT]*map_image_size,
					  mapsurface->format->BitsPerPixel,
					  mapsurface->format->Rmask,
					  mapsurface->format->Gmask,
					  mapsurface->format->Bmask,
					  mapsurface->format->Amask);
      if( grid_overlay == NULL)
	do_SDL_error( "CreateRGBSurface", __FILE__, __LINE__);

      grid_overlay= SDL_DisplayFormatAlpha( grid_overlay);

      first_pass= 0;
    }

  /* 
   * If this is our first time drawing the grid, we need to build up the 
   * grid overlay
   */
  if( first_pass== 0)
    {

      /* Red pixels around the edge and along image borders
       * fully transparent pixels everywhere else
       */
      
      fmt= grid_overlay->format;
      for( x= 0; x < map_image_size*use_config[CONFIG_MAPWIDTH]; x++)
	{
	  for( y= 0; y < map_image_size*use_config[CONFIG_MAPHEIGHT]; y++)
	    {
	      /* FIXME: Only works for 32 bit displays right now */
	      pixel= (Uint32*)grid_overlay->pixels+y*grid_overlay->pitch/4+x;

	      if( x == 0 || y == 0 || 
		  ((x % map_image_size) == 0) || ((y % map_image_size) == 0 ) ||
		  y == use_config[CONFIG_MAPHEIGHT]*map_image_size-1 || x == use_config[CONFIG_MAPWIDTH]*map_image_size -1 )
		{
		  *pixel= SDL_MapRGBA( fmt, 255, 0, 0, SDL_ALPHA_OPAQUE);
		}
	      else 
		{
		  *pixel= SDL_MapRGBA( fmt, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
		}
	    }
	}
      first_pass= 1;

      /* 
       * If this is our first pass then we need to overlay the entire grid
       * now. Otherwise we just update the tile we are on
       */
      dst.x= 0;
      dst.y= 0;
      dst.w= map_image_size*use_config[CONFIG_MAPWIDTH];
      dst.h= map_image_size*use_config[CONFIG_MAPHEIGHT];
      SDL_BlitSurface( grid_overlay, NULL, mapsurface, &dst);
    } 
  else 
    {
      dst.x= ax* map_image_size;
      dst.y= ay* map_image_size;
      dst.w= map_image_size;
      dst.h= map_image_size;
      /* One to one pixel mapping of grid and mapsurface so we
       * can share the SDL_Rect
       */
      SDL_BlitSurface( grid_overlay, &dst, mapsurface, &dst);
    }

  return;
}

/*
 * Takes two args, the first is the GtkWindow to draw on, this should always
 * be 'drawingarea'. The second is a boolean, if 0 then the whole
 * SDL system in initialized or reinited if already run once before,
 * if non zero then only the lightmap is rebuilt, if we switch between
 * per-pixel or per-tile lighting 
 */
void init_SDL( GtkWidget* sdl_window, int just_lightmap)
{

    char SDL_windowhack[32];

    if( just_lightmap == 0) {
	g_assert( sdl_window != NULL);
	if( SDL_WasInit( SDL_INIT_VIDEO) != 0) {
	    if( lightmap)
		SDL_FreeSurface( lightmap);
	    if( mapsurface)
		SDL_FreeSurface( mapsurface);
	    SDL_Quit();
	}

	/* 
	 * SDL hack to tell SDL which xwindow to paint onto 
	 */

#ifndef WIN32
	sprintf( SDL_windowhack, "SDL_WINDOWID=%ld",
	       GDK_WINDOW_XWINDOW(sdl_window->window) );
#else
        sprintf( SDL_windowhack, "SDL_WINDOWID=%ld",
                GDK_WINDOW_HWND(sdl_window->window) );
#endif
	putenv( SDL_windowhack);
      
	if( SDL_Init( SDL_INIT_VIDEO) < 0)
	{
	    LOG(LOG_CRITICAL,"gtk::init_SDL", "Could not initialize SDL: %s", SDL_GetError());
	    gtk_main_quit();
	}

	mapsurface= SDL_SetVideoMode( map_image_size*use_config[CONFIG_MAPWIDTH], map_image_size*use_config[CONFIG_MAPHEIGHT], 0, 
				    SDL_HWSURFACE|SDL_DOUBLEBUF);

	if( mapsurface == NULL)
	{
	    do_SDL_error( "SetVideoMode", __FILE__, __LINE__);
	}

	if( fogmap)
	    SDL_FreeSurface( fogmap);

	fogmap= SDL_CreateRGBSurface( SDL_HWSURFACE|SDL_SRCALPHA, map_image_size,
				map_image_size,
				mapsurface->format->BitsPerPixel,
				mapsurface->format->Rmask,
				mapsurface->format->Gmask,
				mapsurface->format->Bmask,
				mapsurface->format->Amask);

	if( fogmap == NULL)
	{
	    do_SDL_error( "SDL_CreateRGBSurface", __FILE__, __LINE__);
	}

	/* 
	 * This is a persurface alpha value, not an alpha channel value.
	 * So this surface doesn't actually need a full alpha channel
	 */
	if( SDL_SetAlpha( fogmap, SDL_SRCALPHA|SDL_RLEACCEL, 128) < 0)
	{
	    do_SDL_error( "SDL_SetAlpha", __FILE__, __LINE__);
	}
    }

    if( just_lightmap != 0 && lightmap)
	SDL_FreeSurface( lightmap);
  
    lightmap= SDL_CreateRGBSurface( SDL_HWSURFACE|SDL_SRCALPHA, map_image_size,
				  map_image_size,
				  mapsurface->format->BitsPerPixel,
				  mapsurface->format->Rmask,
				  mapsurface->format->Gmask,
				  mapsurface->format->Bmask,
				  mapsurface->format->Amask);
    if( lightmap == NULL)
    {
	do_SDL_error( "SDL_CreateRGBSurface", __FILE__, __LINE__);
    }
  
    if(use_config[CONFIG_LIGHTING] != CFG_LT_TILE)
    {
	/* Convert surface to have a full alpha channel if we are doing
	 * per-pixel lighting */
	lightmap= SDL_DisplayFormatAlpha( lightmap);
	if( lightmap == NULL)
	{
	    do_SDL_error( "DisplayFormatAlpha", __FILE__, __LINE__);
	}
    }

    if(use_config[CONFIG_SHOWGRID] == TRUE)
    {
	overlay_grid( TRUE, 0, 0);
    }
    /* We make this a bit bigger than the actual map - thus, there
     * is a 1 space pad in all directions.  This enables us
     * to store a value in that area without having to do checks to
     * see if we are at the edge of the map - doing a store vs 4
     * checks is going to be much faster.
     */
    redrawbitmap = malloc(sizeof(char) * (MAP_MAX_SIZE +2)* (MAP_MAX_SIZE+2));
}


/* Draw a alpha square on lightmap. Topleft corner is at startx,starty. 
 * values for topleft, topright, bottomleft,bottomright corners are knowns
 * This use bilinear interpolation for other points. Width and heights are given
 * for surrouding known values square. Interpolation is done in a small square whose
 * coordinates are given by start{x|y} and end{x|y}
 * dest{x|y} is top-left corner in destination map.
 *                             Tchize 22 May 2004
 *
 * Note - profile shows this is a very costly function - of a small run,
 * 77% of the time of the cpu time for the client was in this function.
 */
 
void drawquarterlightmap_sdl(int tl, int tr, int bl, int br,                /*colors*/
			     int width, int height,                         /*color square size*/
			     int startx, int starty, int endx, int endy,    /*interpolation region*/
			     int destx, int desty){                         /*where in lightmap to save result*/
	int x,y;
	int top,bottom,val;
	for (x=startx;x<endx;x++){
		top= ((x*(tr-tl))/ width)+tl;    /*linear interpolation for top color*/
		bottom= ((x*(br-bl))/ width)+bl;  /*linear interpolation for bottom color*/
		for (y=starty;y<endy;y++){
			val=((y*(bottom-top))/height)+top; /*linear interpolation between top and bottom*/
			if (val>255)
				val=255;
			if (val<0)
				val=0;
			/*printf("writing pel at %d,%d\n",destx+x,desty+y);*/
			putpixel(lightmap, destx+x-startx, desty+y-starty,
				SDL_MapRGBA(lightmap->format, 0, 0, 0, val));
		}
	}
}

/* Do the lighting on a per pixel basis.
 * x and y are coordinates on the drawable map surfaces (but in terms of
 * spaces, not pixels).  mx and my are indexes into the
 * the_map.cells[][] array.
 * All the below goes out and figures lighting for each pixel on
 * the space, and creates a surface (with alpha) that is then put on
 * top of the exiting map space.
 *
 * TODO: I think it is possible to greatly speed this up by using
 * pre-generated darkness masks.  Doing all the possibilities 
 * would be 3125 images (5 positions, each with 5 values, 5^5),
 * Doing it based on quadrants would only reduce that to 1024.
 * But I _think_ it may be possible to do this with just 64 images
 * (2^5 + one 90 degree rotation of the same) based on quadrants.
 * ie, do a 16x16 image with the 5 gradiants (0,64,128,255 at the
 * left, and each of those values at the right).  Then do the same
 * idea for top and bottom.  For any single quadrant, you would
 * then merge thse two values (which can be done with a fast blit),
 * corresponding to the right values, and you do the same thing for
 * the other four quadrants.  Note this only works so long as
 * 4 lighting values are used - if more are added, this quickly
 * breaks.  Also, if lighting colored effects are desired,
 * this also doesn't work very well.
 *
 * For now, I've just kept the old logic. MSW 2001-10-09
 */

/* See note below about ALPHA_FUDGE - used to adjust lighting effects some */

#define ALPHA_FUDGE(x)  (2*(x) / 3)
#define GENDARK(x,y) ( (((x)&(y) & 1) == 1)?255:0 )
static void do_sdl_per_pixel_lighting(int x, int y, int mx, int my)
{

    int dark0, dark1, dark2, dark3, dark4;
    SDL_Rect dst;

    /* I use dark0 -> dark4 in the order to keep it similar to
     * the old code.
     */
    dark0 = the_map.cells[mx][my].darkness;

    if (y-1 < 0 || !the_map.cells[mx][my-1].have_darkness) dark1 = dark0;
    else dark1 = the_map.cells[mx][my-1].darkness;

    if (x+1 >= use_config[CONFIG_MAPWIDTH] || !the_map.cells[mx+1][my].have_darkness) dark2 = dark0;
    else dark2 = the_map.cells[mx+1][my].darkness;

    if (y+1 >= use_config[CONFIG_MAPHEIGHT] || !the_map.cells[mx][my+1].have_darkness) dark3 = dark0;
    else dark3 = the_map.cells[mx][my+1].darkness;

    if (x-1 < 0 || !the_map.cells[mx-1][my].have_darkness) dark4 = dark0;
    else dark4 = the_map.cells[mx-1][my].darkness;

    /* If they are all the same, processing is easy
     *
     * Note, the best lightining algorithm also uses diagonals
     * so we should check the diagonals are same too
     * We don't check for now, simply do all raw computation on best mode
     * Tchize 19 may 2004
     */
    if (dark0 == dark1 && dark0 == dark2 && dark0 == dark3 && dark0 == dark4 && (use_config[CONFIG_LIGHTING] != CFG_LT_PIXEL_BEST)) {
	dst.x = x * map_image_size;
	dst.y = y * map_image_size;
	dst.w = map_image_size;
	dst.h = map_image_size;

	if (dark0 == 255) {
	    SDL_FillRect(mapsurface,&dst, SDL_MapRGB(mapsurface->format, 0, 0, 0));
	} else if (the_map.cells[mx][my].darkness != 0) {
	    SDL_FillRect(lightmap,NULL, SDL_MapRGBA(lightmap->format, 0, 0, 0, the_map.cells[mx][my].darkness));
	    SDL_BlitSurface(lightmap, NULL, mapsurface, &dst);
	}
	return;
    }


    if (use_config[CONFIG_LIGHTING] == CFG_LT_PIXEL ) {
	/* This almost works as well as the per pixel code below, but does have some various
	 * artifacts in the drawing.  It uses the same logic as the per pixel code below,
	 * bit since SDL does the blit, the alpha handling ends up being different
	 * (I think it ends up being additive).  This results in the darkness being
	 * darker, but you also don't get the smooth effects.  If you divide all the values
	 * by 2 (change ALPHA_FUDGE), the blending is smooth, but now the things are not dark
	 * enough, so the blending aganst solid black spaces does not look good.
	 * The reason this code is of interest is that on my system, it is about 50%
	 * faster than the code below (25 ms to darkness the church in the starting
	 * town vs 50 ms for the code further down)
	 * Setting ALPHA_FUDGE to 2/3 seems to reduce the artifacts described above
	 * to fairly minimal levels, while still keeping things dark enough.
	 * MSW 2001-10-12
	 */

	int i;

	if (dark1 == dark0) {
	    /* If we don't have usable darkness at the top, then this entire region
	    * should be the same value.  Likewise, if the top value and center value
	    * are the same, we can do the entire region.
	    */
	    dst.x=0;
	    dst.y=0;
	    dst.w = map_image_size;
	    dst.h = map_image_half_size;
	    SDL_FillRect(lightmap, &dst, SDL_MapRGBA(lightmap->format, 0, 0, 0, ALPHA_FUDGE(dark0)));
	}
	else for (i=0; i<map_image_half_size; i++) {
	    /* Need to do it line by line */

	    dst.x = 0;
	    dst.y = i;
	    dst.w = map_image_size;
	    dst.h = 1;
	    SDL_FillRect(lightmap, &dst, SDL_MapRGBA(lightmap->format, 0, 0, 0, 
			ALPHA_FUDGE((map_image_half_size - i) * dark1 + i * dark0)/map_image_half_size));

	}
	/* All the following blocks are basically the same as above, just different
	 * darkness areas.
	 */
	if (dark3 == dark0) {
	    dst.x=0;
	    dst.y=map_image_half_size;
	    dst.w = map_image_size;
	    dst.h = map_image_half_size;
	    SDL_FillRect(lightmap, &dst, SDL_MapRGBA(lightmap->format, 0, 0, 0, ALPHA_FUDGE(dark0)));
	}
	else for (i=map_image_half_size; i<map_image_size; i++) {
	    /* Need to do it line by line */

	    dst.x = 0;
	    dst.y = i;
	    dst.w = map_image_size;
	    dst.h = 1;
	    SDL_FillRect(lightmap, &dst, SDL_MapRGBA(lightmap->format, 0, 0, 0, 
			ALPHA_FUDGE(dark0*(map_image_size-i) + dark3*(i-map_image_half_size)) / map_image_half_size));
	}
	/* Blit this to the screen now.  Otherwise, we need to look at the alpha values
	 * and re-average.
	 */

	dst.x= x * map_image_size; 
	dst.y= y * map_image_size;
	SDL_BlitSurface(lightmap, NULL, mapsurface, &dst);

	if (dark4 == dark0) {
	    dst.x=0;
	    dst.y=0;
	    dst.w = map_image_half_size;
	    dst.h = map_image_size;
	    SDL_FillRect(lightmap, &dst, SDL_MapRGBA(lightmap->format, 0, 0, 0, ALPHA_FUDGE(dark0)));
	}
	else for (i=0; i<map_image_half_size; i++) {
	    /* Need to do it line by line */
	    dst.x = i;
	    dst.y = 0;
	    dst.w = 1;
	    dst.h = map_image_size;
	    SDL_FillRect(lightmap, &dst, SDL_MapRGBA(lightmap->format, 0, 0, 0,
			ALPHA_FUDGE(dark4*(map_image_half_size-i) + dark0*i) / map_image_half_size));
	}
	if (dark2 == dark0) {
	    dst.x=map_image_half_size;
	    dst.y=0;
	    dst.w = map_image_half_size;
	    dst.h = map_image_size;
	    SDL_FillRect(lightmap, &dst, SDL_MapRGBA(lightmap->format, 0, 0, 0, ALPHA_FUDGE(dark0)));
	}
	else for (i=map_image_half_size; i<map_image_size; i++) {
	    /* Need to do it line by line */

	    dst.x = i;
	    dst.y = 0;
	    dst.w = 1;
	    dst.h = map_image_size;
	    SDL_FillRect(lightmap, &dst, SDL_MapRGBA(lightmap->format, 0, 0, 0, 
			ALPHA_FUDGE(dark0*(map_image_size-i) + dark2*(i-map_image_half_size)) / map_image_half_size));
	}
	dst.x= x * map_image_size; 
	dst.y= y * map_image_size;
	SDL_BlitSurface(lightmap, NULL, mapsurface, &dst);
    } else if (use_config[CONFIG_LIGHTING] == CFG_LT_PIXEL_BEST ) {
#if 0
	int dx,dy;
	static  int *darkx=NULL, *darky=NULL,darkx_allocated=0;

	/* Generated stored for the darkx[] array.  Do it dynamically, but
	 * only allocate if the size needs to be expanded to keep performance
	 * better.  darkx could be null in the initial case, but realloc should
	 * just treat that as a malloc (so according to the man page)
	 */
	if (map_image_size > darkx_allocated) {
	    darkx = realloc(darkx, map_image_size * sizeof(int));
	    darky = realloc(darky, map_image_size * sizeof(int));
	    darkx_allocated = map_image_size;
	}
			
	for( dx= 0; dx < map_image_half_size; dx++)
	    darkx[dx]= (dark4*(map_image_half_size-dx) + dark0*dx) / map_image_half_size;
	for( dx= map_image_half_size; dx < map_image_size; dx++)
	    darkx[dx] = (dark0*(map_image_size-dx) + dark2*(dx-map_image_half_size)) / map_image_half_size;

	for( dy= 0; dy < map_image_half_size; dy++)
	    darky[dy]= (dark1*(map_image_half_size-dy) + dark0*dy) / map_image_half_size;
	for( dy= map_image_half_size; dy < map_image_size; dy++)
	    darky[dy] = (dark0*(map_image_size-dy) + dark3*(dy-map_image_half_size)) / map_image_half_size;

	SDL_LockSurface( lightmap);

	for (dx=0; dx<map_image_size; dx++)
	    for (dy=0; dy<map_image_size; dy++) 
		putpixel(lightmap, dx, dy, SDL_MapRGBA(lightmap->format, 0, 0, 0,(darkx[dx] + darky[dy])/2));
#else
	/*we need additionnal surrounding infos*/
	int dark5, dark6, dark7, dark8;
	if ( (y-1 < 0) || (x+1 >= use_config[CONFIG_MAPWIDTH])
		|| !the_map.cells[mx+1][my-1].have_darkness) dark5 = (dark1+dark2)>>1; /*(fast div 2)*/
	else dark5 = the_map.cells[mx+1][my-1].darkness;

	if ( (x+1 >= use_config[CONFIG_MAPWIDTH]) 
		|| (y+1 >= use_config[CONFIG_MAPHEIGHT])
		|| !the_map.cells[mx+1][my+1].have_darkness) dark6 = (dark2+dark3)>>1;
	else dark6 = the_map.cells[mx+1][my+1].darkness;

	if ( (y+1 >= use_config[CONFIG_MAPHEIGHT]) || (x-1 < 0)
		|| !the_map.cells[mx-1][my+1].have_darkness) dark7 = (dark3+dark4)>>1;
	else dark7 = the_map.cells[mx-1][my+1].darkness;
	
	if ( (x-1 < 0) || (y-1 < 0)
		|| !the_map.cells[mx-1][my-1].have_darkness) dark8 = (dark4+dark1)>>1;
	else dark8 = the_map.cells[mx-1][my-1].darkness;
	/*upper left lightmap quarter*/
	drawquarterlightmap_sdl(dark8, dark1, dark4, dark0,                /*colors*/
			     map_image_size, map_image_size,               /*color square size*/
			     map_image_half_size, map_image_half_size, map_image_size, map_image_size,    /*interpolation region*/
			     0, 0);                         /*where in lightmap to save result*/
	/*upper right lightmap quarter*/
	drawquarterlightmap_sdl(dark1, dark5, dark0, dark2,                /*colors*/
			     map_image_size, map_image_size,               /*color square size*/
			     0, map_image_half_size, map_image_half_size, map_image_size,    /*interpolation region*/
			     map_image_half_size, 0);                         /*where in lightmap to save result*/
	/*bottom left lightmap quarter*/
	drawquarterlightmap_sdl(dark4, dark0, dark7, dark3,                /*colors*/
			     map_image_size, map_image_size,               /*color square size*/
			     map_image_half_size, 0, map_image_size, map_image_half_size,    /*interpolation region*/
			     0, map_image_half_size);                         /*where in lightmap to save result*/
	/*bottom right lightmap quarter*/
	drawquarterlightmap_sdl(dark0, dark2, dark3, dark6,                /*colors*/
			     map_image_size, map_image_size,               /*color square size*/
			     0, 0, map_image_half_size, map_image_half_size,    /*interpolation region*/
			     map_image_half_size, map_image_half_size);                         /*where in lightmap to save result*/
#endif
	dst.w= map_image_size;
	dst.h= map_image_size;
	dst.x= x * map_image_size; 
	dst.y= y * map_image_size;
	SDL_UnlockSurface(lightmap);
	SDL_BlitSurface(lightmap, NULL, mapsurface, &dst);	
    }
}
/* Draw anything in adjacent squares that could smooth on given square
 * mx,my square to smooth on. you should not call this function to
 * smooth on a 'completly black' square. (simply for visual result)
 * layer layer to examine (we smooth only one layer at a time)
 * dst place on the mapwindow to draw
 */
static void drawsmooth_sdl (int mx,int my,int layer,SDL_Rect dst){
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
    SDL_Rect src;

    if (the_map.cells[mx][my].heads[layer].face == 0
    || !CAN_SMOOTH(the_map.cells[mx][my], layer)) {
        return;
    }

    src.w=dst.w;
    src.h=dst.h;

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
            src.x=map_image_size*weight;
            src.y=0;
            if (the_map.cells[mx][my].cleared) {
                if (SDL_BlitSurface(pixmaps[smoothface]->fog_image,
                        &src, mapsurface, &dst))
                    do_SDL_error( "BlitSurface", __FILE__, __LINE__);
            } else {
                if (SDL_BlitSurface(pixmaps[smoothface]->map_image,
                        &src, mapsurface, &dst))
                    do_SDL_error( "BlitSurface", __FILE__, __LINE__);
            }
        }
        if (weightC>0){
            src.x=map_image_size*weightC;
            src.y=map_image_size;
            if (the_map.cells[mx][my].cleared) {
                if (SDL_BlitSurface(pixmaps[smoothface]->fog_image,
                        &src, mapsurface, &dst))
                    do_SDL_error( "BlitSurface", __FILE__, __LINE__);
            } else {
                if (SDL_BlitSurface(pixmaps[smoothface]->map_image,
                        &src, mapsurface, &dst))
                    do_SDL_error( "BlitSurface", __FILE__, __LINE__);
            }
        }
    }/*while there's some smooth to do*/
}

/* update_redrawbitmap() - replacment of sdl_square_need_redraw logic.
 * use of sdl_square_need_redraw is relatively inefficient becuase
 * it is called for every space (hence function call overhead),
 * but also has 4 checks to make sure the neighbor space is within valid
 * range, and if non tile mode, performs that check at least 4 times
 * per space.
 * This is much more efficient, because our redrawbitmap array is
 * large enough we don't need those checks - we know we are always safe
 * to go one outside the bounds (hence, the +1 in the coordinate
 * values)
 */
static void update_redrawbitmap(void)
{
    int mx,my, x,y;

    memset(redrawbitmap, 0, (use_config[CONFIG_MAPWIDTH]+2) * (use_config[CONFIG_MAPHEIGHT]+2));

    for( x= 0; x<use_config[CONFIG_MAPWIDTH]; x++) {
	for(y = 0; y<use_config[CONFIG_MAPHEIGHT]; y++) {
	    mx = x + pl_pos.x;
	    my = y + pl_pos.y;

	    /* Basically, we need to check the conditions that require this space.
	     * to be redrawn.  We store this in redrawbitmap, because storing
	     * in the_map[][].need_update would cause a cascade effect, of space
	     * 1,0 need an update, so we thing 2,0 needs an update due to smoothing/
	     * like, which causes 3,0 to be updated, etc.  Having our own
	     * memory area allows the memset above, which is an optimized routine
	     * to clear memory.
	     */
	    if (the_map.cells[mx][my].need_update) {
		redrawbitmap[x + 1 + (y+1) * use_config[CONFIG_MAPWIDTH]] = 1;
		/* If this space has changed, and using non tile lighting,
		 * we need to update the neighbor spaces.  Ideally, we'd
		 * have a flag just to denote lighting changes, since
		 * that is handled on a different surface anyways.
		 */
		if (use_config[CONFIG_LIGHTING] == CFG_LT_PIXEL || 
		    use_config[CONFIG_LIGHTING] == CFG_LT_PIXEL_BEST) {
		    /* This is where having redrawbitmap bigger pays off - don't have
		     * to check to see if values are within redrawbitmap is within bounds
		     */
		    redrawbitmap[x  + (y+1) * use_config[CONFIG_MAPWIDTH]] = 1;
		    redrawbitmap[x + 2 + (y+1) * use_config[CONFIG_MAPWIDTH]] = 1;
		    redrawbitmap[x + 1 + (y) * use_config[CONFIG_MAPWIDTH]] = 1;
		    redrawbitmap[x + 1 + (y+2) * use_config[CONFIG_MAPWIDTH]] = 1;
		}
		/* In best mode, have to update diaganols in addition*/
		if (use_config[CONFIG_LIGHTING] == CFG_LT_PIXEL_BEST) {
		    redrawbitmap[x  + (y) * use_config[CONFIG_MAPWIDTH]] = 1;
		    redrawbitmap[x + 2 + (y) * use_config[CONFIG_MAPWIDTH]] = 1;
		    redrawbitmap[x +  (y+2) * use_config[CONFIG_MAPWIDTH]] = 1;
		    redrawbitmap[x + 2 + (y+2) * use_config[CONFIG_MAPWIDTH]] = 1;
		}
	    }
	    else if (the_map.cells[mx][my].need_resmooth) {
		redrawbitmap[x + 1 + (y+1) * use_config[CONFIG_MAPWIDTH]] = 1;
	    }
	}
    }
}

static void display_mapcell(int ax, int ay, int mx, int my)
{
    SDL_Rect dst, src;
    int layer;

    /* First, we need to black out this space. */
    dst.x = ax*map_image_size;
    dst.y = ay*map_image_size;
    dst.w = map_image_size; 
    dst.h = map_image_size;
    SDL_FillRect(mapsurface, &dst, SDL_MapRGB(mapsurface->format, 0, 0, 0));

    /* now draw the different layers.  Only draw if using fog of war or the
     * space isn't clear.
     */
    if (use_config[CONFIG_FOGWAR] || !the_map.cells[mx][my].cleared) {
        for (layer=0; layer<MAXLAYERS; layer++) {
            int sx, sy;

            /* draw single-tile faces first */
            int face = mapdata_face(ax, ay, layer);
            if (face > 0 && pixmaps[face]->map_image != NULL) {
                int w = pixmaps[face]->map_width;
                int h = pixmaps[face]->map_height;
                /* add one to the size values to take into account the actual width of the space */
                src.x = w-map_image_size;
                src.y = h-map_image_size;
                src.w = map_image_size;
                src.h = map_image_size;
                dst.x = ax*map_image_size;
                dst.y = ay*map_image_size;
                if (the_map.cells[mx][my].cleared) {
                    if (SDL_BlitSurface(pixmaps[face]->fog_image, &src, mapsurface, &dst))
                        do_SDL_error( "BlitSurface", __FILE__, __LINE__);
                } else {
                    if (SDL_BlitSurface(pixmaps[face]->map_image, &src, mapsurface, &dst))
                        do_SDL_error( "BlitSurface", __FILE__, __LINE__);
                }

                if ( use_config[CONFIG_SMOOTH])
                    drawsmooth_sdl (mx,my,layer,dst);
            }
            /* Sometimes, it may happens we need to draw the smooth while there
             * is nothing to draw at that layer (but there was something at lower
             * layers). This is handled here. The else part is to take into account
             * cases where the smooth as already been handled 2 code lines before
             */
            else if (use_config[CONFIG_SMOOTH] && the_map.cells[mx][my].need_resmooth)
                drawsmooth_sdl (mx,my,layer,dst);

            /* draw big faces last (should overlap other objects) */
            face = mapdata_bigface(ax, ay, layer, &sx, &sy);
            if (face > 0 && pixmaps[face]->map_image != NULL) {
		/* We have to handle images that are not an equal
		 * multiplier of map_image_size.  See
		 * display_mapcell() in gtk-v2/src/map.c for
		 * more details on this logic, since it is basically
		 * the same.
		 */
		int dx, dy, sourcex, sourcey, offx, offy;

                dx = pixmaps[face]->map_width % map_image_size;
                offx = dx?(map_image_size -dx):0;

                if (sx) {
                    sourcex = sx * map_image_size - offx ;
                    offx=0;
		} else {
                    sourcex=0;
		}

                dy = pixmaps[face]->map_height % map_image_size;
                offy = dy?(map_image_size -dy):0;

                if (sy) {
                    sourcey = sy * map_image_size - offy;
                    offy=0;
		} else {
                    sourcey=0;
		}

                src.x = sourcex;
                src.y = sourcey;
                src.w = map_image_size - offx;
                src.h = map_image_size - offy;
                dst.x = ax*map_image_size + offx;
                dst.y = ay*map_image_size + offy;
                if (the_map.cells[mx][my].cleared) {
                    if (SDL_BlitSurface(pixmaps[face]->fog_image, &src, mapsurface, &dst))
                        do_SDL_error( "BlitSurface", __FILE__, __LINE__);
                } else {
                    if (SDL_BlitSurface(pixmaps[face]->map_image, &src, mapsurface, &dst))
                        do_SDL_error( "BlitSurface", __FILE__, __LINE__);
                }
            } /* else for processing the layers */
        }
    }

    if (use_config[CONFIG_LIGHTING] == CFG_LT_TILE) {
        dst.x = ax*map_image_size;
        dst.y = ay*map_image_size;
        dst.w = map_image_size;
        dst.h = map_image_size;

        /* Note - Instead of using a lightmap, I just fillrect
         * directly onto the map surface - I would think this should be
         * faster
         */
        if (the_map.cells[mx][my].darkness == 255) {
            SDL_FillRect(mapsurface,&dst, SDL_MapRGB(mapsurface->format, 0, 0, 0));
        } else if (the_map.cells[mx][my].darkness != 0) {
            SDL_SetAlpha(lightmap, SDL_SRCALPHA|SDL_RLEACCEL, the_map.cells[mx][my].darkness);
            SDL_BlitSurface(lightmap, NULL, mapsurface, &dst);
        }
    } else if (use_config[CONFIG_LIGHTING] == CFG_LT_PIXEL || use_config[CONFIG_LIGHTING] == CFG_LT_PIXEL_BEST) {
        do_sdl_per_pixel_lighting(ax, ay, mx, my);
    }
}

/* This generates a map in SDL mode.
 *
 * I had to totally change the logic on how we do this in SDL mode -
 * to support variable sized images, the old method of generating each
 * space does not work, as one space may spill over to the other.
 * Instead, we first blit the bottom layer, then the layer above
 * that, and so on.  This results in the map being drawn a bit
 * more correctly.  In fact, that logic actually isn't needed, as
 * with the new map commands, we know the offset and size of the
 * images.
 *
 * The logic here only redraws spaces that change.  The logic in the
 * common/commands.c files the odd layers with links for 'big images'.
 * for objects on these layers, we look at the size_x and size_y values
 * to determine the offset from which we should be blitting.
 *
 * Old notes, but left in:
 * The performance here is very good in most cases - about 30 ms (on my system)
 * is used just for my flip at the bottom of the function, drawing only what
 * is needed generally saves a lot of time (<15 ms in most cases) compared to the
 * 80-120 ms usually needed on a 15x15 map.
 */

void sdl_gen_map(int redraw) {
    int x, y, num_spaces=0, num_drawn=0;
    struct timeval tv1, tv2, tv3;
    long elapsed1, elapsed2;

    if (time_map_redraw)
	gettimeofday(&tv1, NULL);

    update_redrawbitmap();

    for( x= 0; x<use_config[CONFIG_MAPWIDTH]; x++) {
	for(y = 0; y<use_config[CONFIG_MAPHEIGHT]; y++) {
	    num_spaces++;

	    /* This will be updated in the for loop above for
	     * whatever conditions that need this space to be redrawn
	     */
	    if (redraw || redrawbitmap[x + 1 + (y+1) * use_config[CONFIG_MAPWIDTH]]) {
		num_drawn++;
		display_mapcell(x, y, pl_pos.x+x, pl_pos.y+y);
		the_map.cells[pl_pos.x+x][pl_pos.y+y].need_update = 0;
		the_map.cells[pl_pos.x+x][pl_pos.y+y].need_resmooth = 0;
	    }
	}
	    }

    if (time_map_redraw)
	gettimeofday(&tv2, NULL);

    SDL_Flip(mapsurface);

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
} /* sdl_gen_map function */

int sdl_mapscroll(int dx, int dy)
{
    /* Don't sdl_gen_map should take care of the redraw */

    /* a copy of what pngximage does except sdl specfic
     * mapsurface->pitch is the length of a scanline in bytes 
     * including alignment padding
     */
    SDL_LockSurface( mapsurface);
    if( dy < 0) {
	int offset= mapsurface->pitch * (-dy*map_image_size);
	memmove( mapsurface->pixels + offset, mapsurface->pixels, 
		     mapsurface->pitch * (mapsurface->h + dy*map_image_size) );
    }
    else if( dy > 0) {
	int offset= mapsurface->pitch * (dy*map_image_size);
	memmove( mapsurface->pixels,  mapsurface->pixels + offset,
		     mapsurface->pitch * (mapsurface->h - dy*map_image_size) );
    }

    if (dx) {
	int y;
	for( y= 0; y < mapsurface->h; y++) {
	    if( dx < 0) {
		char* start_of_row= mapsurface->pixels + mapsurface->pitch * y;
		int offset= ( mapsurface->format->BytesPerPixel * map_image_size * -dx);
		memmove( start_of_row + offset, start_of_row,
			     mapsurface->pitch - offset);
	    }
	    else {
		char* start_of_row= mapsurface->pixels + mapsurface->pitch * y; 
		int offset= ( mapsurface->format->BytesPerPixel * map_image_size * dx);
		memmove( start_of_row, start_of_row + offset,
			     mapsurface->pitch - offset);
	    }
	}
    }
    SDL_UnlockSurface( mapsurface);
	    
    return 1;
}

#endif
