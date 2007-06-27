char *rcsid_gtk_opengl_c =
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

/* This file implements an opengl version of the map renderer.  I've
 * tried to keep this platform generic, but there are just some things
 * that must be tied to the window system is is using, namely, window
 * creation and flipping the data buffers.  For that, on X, we use to
 * use glx - MSW 2005-03-12
 *
 * NOTE: Using dmalloc with opengl causes problems - it gets an
 * invalid allocation - I haven't dug through it, but my guess is that
 * some internal opengl/glx routine is doing something like a malloc(0)
 * which dmalloc catches.
 */

#include <config.h>

#ifdef HAVE_OPENGL

#include <client-types.h>

#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <windows.h>
#include <gdk/gdkwin32.h>
#endif
#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "image.h"
#include <client.h>
#include "mapdata.h"

#include "gtk2proto.h"

/* Start of Open GL includes */
#include <GL/gl.h>
#include <GL/glu.h>
#ifndef WIN32
#include <GL/glx.h>
#endif

extern int time_map_redraw;

#ifndef WIN32
static Display  *display;	/* X display & window for glx buffer swapping */
static Window	window;
#else
static HDC devicecontext;	/* Windows device context for windows buffer swapping */
#endif
static int	width=1, height=1;

/* This function does the generic initialization for opengl.
 * this function does not use any machine specicific calls.
 */
static void init_opengl_common()
{
    GLint texSize;

    /* Need to enable texture mapping */
    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);

    /* Need to enable alpha blending */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);

    #ifndef WIN32
    glViewport(0, 0, (float)width, (float)height);
    #else
    /* There is a bug somewhere that causes the viewport to be shifted up by
     * 25-MAPHEIGHT tiles when run in Windows.  Don't know yet what causes this,
     * but this is a bad hack to fix it. */
    glViewport(0, (use_config[CONFIG_MAPHEIGHT]-25)*32, (float)width, (float)height);
    #endif

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    /* Make the upper left 0,0 coordinate */
    glOrtho(0.0f,width,height,0.0f,-1.0f,1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glFlush();

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texSize);
    LOG(LOG_INFO,"gtk::opengl_common", "Maximum texture size is %d\n", texSize);

}


#ifndef WIN32
/*
 * GLX (X-Windows) specific OpenGL iniit
 */
void init_glx_opengl(GtkWidget* drawingarea)
{
    GLXContext	ctx;
    XVisualInfo *vi;
    int attrListDbl[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
	GLX_RED_SIZE, 4,
	GLX_GREEN_SIZE, 4,
	GLX_BLUE_SIZE, 4,
	GLX_ALPHA_SIZE, 4,
	GLX_DEPTH_SIZE, 16,
	None };
    XSetWindowAttributes attr;
    Visual *v;

    /* Need to tuck these away, because they are needed for glXSwappBuffers() */
    display = GDK_WINDOW_XDISPLAY(drawingarea->window);
    vi = glXChooseVisual(display,
			 gdk_x11_get_default_screen (), attrListDbl);

    width = drawingarea->allocation.width;
    height = drawingarea->allocation.height;

    /* On many systems, the default visual used by the display doesn't
     * have the features we need (alpha channel, etc).  The only way
     * around this is to create a subwindow with the ideal
     * visual.  As an oddity, we need to create a colormap
     * if using a different visual or we get a BadMatch error.
     */
    v = DefaultVisual(display, gdk_x11_get_default_screen ());
    attr.colormap= XCreateColormap(display, GDK_WINDOW_XID(drawingarea->window),
				   vi->visual, AllocNone);

    window = XCreateWindow(display, GDK_WINDOW_XID(drawingarea->window),
		0, 0, width, height, 0,
			   vi->depth,
			InputOutput,
			   vi->visual,
			   CWColormap, &attr);

    XMapWindow(display,window);

    if (!vi) {
        LOG(LOG_WARNING,"gtk::init_glx_opengl", "Could not get double buffered screen!\n");
    }

    ctx = glXCreateContext(display, vi, 0, GL_TRUE);

    if (!glXMakeCurrent(display, window, ctx)) {
        LOG(LOG_ERROR,"gtk::init_glx_opengl", "Could not set opengl context!\n");
	exit(1);
    }
    if (glXIsDirect(display, ctx))
        LOG(LOG_INFO,"gtk::init_glx_opengl", "Direct rendering is available!\n");
    else
        LOG(LOG_INFO,"gtk::init_glx_opengl", "Direct rendering is not available!\n");

}
#endif /* #ifndef WIN32 */


#ifdef WIN32
/*
 * WGL (MS Windows) specific OpenGL init
 */
void init_wgl_opengl(GtkWidget* drawingarea)
{
    HGLRC glctx;
    HDC dctx;
    int pixelformat;
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),          //size of structure
        1,                                      //default version
        PFD_DRAW_TO_WINDOW |                    //window drawing support
        PFD_SUPPORT_OPENGL |                    //opengl support
        PFD_DOUBLEBUFFER,                       //double buffering support
        PFD_TYPE_RGBA,                          //RGBA color mode
        16,                                     //16 bit color mode
        0, 0, 0, 0, 0, 0,                       //ignore color bits
        4,                                      //4 bits alpha buffer
        0,                                      //ignore shift bit
        0,                                      //no accumulation buffer
        0, 0, 0, 0,                             //ignore accumulation bits
        16,                                     //16 bit z-buffer size
        0,                                      //no stencil buffer
        0,                                      //no aux buffer
        PFD_MAIN_PLANE,                         //main drawing plane
        0,                                      //reserved
        0, 0, 0                                 //layer masks ignored
    };

    width = drawingarea->allocation.width;
    height = drawingarea->allocation.height;

    dctx = GetDC(GDK_WINDOW_HWND(drawingarea->window));
    devicecontext = dctx;

    /* Get the closest matching pixel format to what we specified and set it */
    pixelformat = ChoosePixelFormat(dctx, &pfd);
    SetPixelFormat(dctx, pixelformat, &pfd);

    glctx = wglCreateContext(dctx);
    wglMakeCurrent(dctx, glctx);
}
#endif /* #ifdef WIN32 */


/*
 * Takes te GtkWindow to draw on - this should always be 'drawingarea'
 * Calls the correct platform-specific initialization code, then the generic.
 */
void init_opengl(GtkWidget* drawingarea)
{

    #ifndef WIN32
    init_glx_opengl(drawingarea);
    #else
    init_wgl_opengl(drawingarea);
    #endif
    init_opengl_common();
}


/* We set up a table of darkness - when opengl draws the
 * first layer, it fills this in - in this way, we have a table
 * of all the darkness values that we should use - this
 * makes dealing with darkness much faster, as we don't have
 * to see if the space is has valid darkness, etc.
 * We add 2 to the value to take into acount the padding of
 * an extra space (thus, don't need special logic for
 * final row/column).  That is +1 value.  but the last row
 * also has the right/bottom side vertices, and that is where
 * the other +1 comes from
 */

static uint16 map_darkness[(MAP_MAX_SIZE+2)*2][(MAP_MAX_SIZE+2)*2];

/* This is darkness to use if we have no darkness information.
 * 0 makes sense for standard behaviour, but I find setting this
 * to 255 makes for some interesting smoothing effects relative
 * to the edge of the screen and blocked spaces.  I'm not
 * sure if anything other than 0 and 255 would be useful.
 */
#define DEFAULT_DARKNESS    0


/* This lights a space in opengl mode.  It is effectively the same
 * quality as the sdl per pixel lighting, but opengl makes the work
 * much easier.  Basically, we divide the space into 4 smaller
 * subspaces.  We then draw these 4 squares, setting the color
 * of the different vertices on a average of our space
 * lightness and that of the neighboring space for the outside
 * vertices.  For the inside (middle) one, we just use our
 * value.  Note that this code uses the light info of all 8 neighboring
 * spaces.
 *
 * I draw all the squares starting in the upper left position and
 * then going to upper right, bottom right, bottom left
 *
 * As a note, for GlColor4ub, 255 is full opaque, 0 fully transparent
 */


static void opengl_light_space(int x, int y, int mx, int my)
{
    if (use_config[CONFIG_DARKNESS] == CFG_LT_TILE) {
	/* If we don't have darkness, or it isn't dark, don't do anything */
	if (!the_map.cells[mx][my].have_darkness || the_map.cells[mx][my].darkness==0) return;

	glColor4ub(0, 0, 0,  the_map.cells[mx][my].darkness);
	glBegin(GL_QUADS);
	glVertex3i(x * map_image_size, y * map_image_size, 0);
	glVertex3i((x+1) * map_image_size, y * map_image_size, 0);
	glVertex3i((x+1) * map_image_size, (y+1) * map_image_size, 0);
	glVertex3i(x * map_image_size, (y+1) * map_image_size, 0);
	glEnd();
    }

    /* do the upper left area */
    glBegin(GL_QUADS);

    glColor4ub(0, 0, 0,  map_darkness[x*2][y*2]);
    glVertex3i(x * map_image_size, y * map_image_size, 0);

    glColor4ub(0, 0, 0,   map_darkness[x*2 + 1][y*2]);
    glVertex3i(x * map_image_size + map_image_half_size, y * map_image_size, 0);

    glColor4ub(0, 0, 0,   map_darkness[x*2 + 1][y*2 + 1]);
    glVertex3i( x * map_image_size + map_image_half_size, y * map_image_size + map_image_half_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2][y*2+1]);
    glVertex3i(x * map_image_size, y * map_image_size + map_image_half_size, 0);

    glEnd();

    /* Repeat for upper right area */
    glBegin(GL_QUADS);

    glColor4ub(0, 0, 0,  map_darkness[x*2+1][y*2]);
    glVertex3i(x * map_image_size + map_image_half_size, y * map_image_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2+2][y*2]);
    glVertex3i((x +1 ) * map_image_size, y * map_image_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2+2][y*2+1]);
    glVertex3i((x+1) * map_image_size, y * map_image_size + map_image_half_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2+1][y*2+1]);
    glVertex3i( x * map_image_size + map_image_half_size, y * map_image_size + map_image_half_size, 0);

    glEnd();

    /* Repeat for lower left area */
    glBegin(GL_QUADS);


    glColor4ub(0, 0, 0,  map_darkness[x*2][y*2+1]);
    glVertex3i(x * map_image_size, y * map_image_size + map_image_half_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2+1][y*2+1]);
    glVertex3i( x * map_image_size + map_image_half_size, y * map_image_size + map_image_half_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2+1][y*2+2]);
    glVertex3i(x * map_image_size + map_image_half_size, (y + 1) * map_image_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2][y*2+2]);
    glVertex3i( x * map_image_size, (y +1)* map_image_size, 0);

    glEnd();

    /* Repeat for lower right area */
    glBegin(GL_QUADS);

    glColor4ub(0, 0, 0,  map_darkness[x*2+1][y*2+1]);
    glVertex3i( x * map_image_size + map_image_half_size, y * map_image_size + map_image_half_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2+2][y*2+1]);
    glVertex3i((x+1) * map_image_size, y * map_image_size + map_image_half_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2+2][y*2+2]);
    glVertex3i((x+1) * map_image_size, (y +1)* map_image_size, 0);

    glColor4ub(0, 0, 0,  map_darkness[x*2+1][y*2+2]);
    glVertex3i(x * map_image_size + map_image_half_size, (y + 1) * map_image_size, 0);

    glEnd();

}


/* some basics.  dx, dy are coordinate pairs for offsets. bweights and cweights
 * are bitmasks that determine the face to draw (or'd together)
 */

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

/* vertices are floats.  this sets the value appropriately for
 * us to multiply the x coordinate by.
 */
#define	    TEXTURE_OFFSET  1.0/16.0

/* Draw anything in adjacent squares that could smooth on given square
 * mx,my square to smooth on. you should not call this function to
 * smooth on a 'completly black' square. (simply for visual result)
 * layer layer to examine (we smooth only one layer at a time)
 * dst place on the mapwindow to draw
 */
static void drawsmooth_opengl (int x, int y, int mx, int my, int layer) {
    int partdone[8]={0,0,0,0,0,0,0,0}, slevels[8], sfaces[8], i,
	weight,weightC, emx,emy, smoothface, dosmooth, lowest, havesmooth;

    dosmooth=0;
    for (i=0;i<8;i++){
        emx=mx+dx[i];
        emy=my+dy[i];

        if ( (emx<0) || (emy<0) || (emx >= the_map.x) || (emy >= the_map.y)){
            slevels[i]=0;
            sfaces[i]=0; /*black picture*/
        }
        else if (the_map.cells[emx][emy].smooth[layer]<=the_map.cells[mx][my].smooth[layer] ||
	    the_map.cells[emx][emy].heads[layer].face == 0){
            slevels[i]=0;
            sfaces[i]=0; /*black picture*/
        } else{
            slevels[i]=the_map.cells[emx][emy].smooth[layer];
            sfaces[i]=pixmaps[the_map.cells[emx][emy].heads[layer].face]->smooth_face;
            dosmooth++;
	}
    }

    /* slevels[] & sfaces[] contain the smooth level.  dosmooth is the number if
     * spaces that need to be smoothed.  lowlevel is the lowest level to smooth.
     */

    /* havesmooth is how many faces we have smoothed */
    havesmooth=0;

    while (havesmooth < dosmooth) {
	lowest=-1;
	for (i=0;i<8;i++){
	    if ( (slevels[i]>0) && (!partdone[i]) && ((lowest<0) || (slevels[i]<slevels[lowest])))
		lowest=i;
	}
        if (lowest<0)
            break;   /*no more smooth to do on this square*/

	/* the weight values is a bitmask that determines what image we draw */
        weight=0;
        weightC=15; /*works in backward. remove where there is nothing*/

        for (i=0;i<8;i++){ /*check all nearby squares*/
            if (slevels[i]==slevels[lowest] && sfaces[i] == sfaces[lowest]) {
                partdone[i]=1;
                weight=weight+bweights[i];
                weightC&=~bc_exclude[i];
		havesmooth++;
            } else{
                /*must rmove the weight of a corner if not in smoothing*/
                weightC&=~cweights[i];
            }
	}

        smoothface=sfaces[lowest];
        if (smoothface<=0){
            continue;  /*picture for smoothing not yet available*/
        }

        /* now, it's quite easy. We must draw using a 32x32 part of
         * the picture smoothface.
         * This part is located using the 2 weights calculated:
         * (32*weight,0) and (32*weightC,32)
         */

        if ( (!pixmaps[smoothface]->map_texture) || (pixmaps[smoothface] == pixmaps[0]))
            continue;   /*don't have the picture associated*/

	if (the_map.cells[mx][my].cleared)
	    glBindTexture(GL_TEXTURE_2D, pixmaps[smoothface]->fog_texture);
	else
	    glBindTexture(GL_TEXTURE_2D, pixmaps[smoothface]->map_texture);

	/* The values of 0.0f and 0.5f are hardcoded, but as of now, it is
	 * a known fact that there are 2 rows of data in the smoothing images,
	 * so that should be OK.
	 */
	if (weight) {
	    glBegin(GL_QUADS);

	    glTexCoord2f(TEXTURE_OFFSET * weight, 0.0f);
	    glVertex3i(x * map_image_size, y * map_image_size, 0);

	    glTexCoord2f(TEXTURE_OFFSET * (weight+1), 0.0f);
	    glVertex3i((x+1) * map_image_size, y * map_image_size, 0);

	    glTexCoord2f(TEXTURE_OFFSET * (weight+1), 0.5f);
	    glVertex3i((x+1) * map_image_size, (y+1) * map_image_size, 0);

	    glTexCoord2f(TEXTURE_OFFSET * weight, 0.5f);
	    glVertex3i(x * map_image_size, (y+1) * map_image_size, 0);

	    glEnd();
	}
	if (weightC) {
	    glBegin(GL_QUADS);

	    glTexCoord2f(TEXTURE_OFFSET * weight, 0.5f);
	    glVertex3i(x * map_image_size, y * map_image_size, 0);

	    glTexCoord2f(TEXTURE_OFFSET * (weight+1), 0.5f);
	    glVertex3i((x+1) * map_image_size, y * map_image_size, 0);

	    glTexCoord2f(TEXTURE_OFFSET * (weight+1), 1.0f);
	    glVertex3i((x+1) * map_image_size, (y+1) * map_image_size, 0);

	    glTexCoord2f(TEXTURE_OFFSET * weight, 1.0f);
	    glVertex3i(x * map_image_size, (y+1) * map_image_size, 0);

	    glEnd();
	}
    } /*while there's some smooth to do*/
}

static void draw_smoothing(int layer)
{
    int x, y, mx, my;

    for(y = use_config[CONFIG_MAPHEIGHT]+MAX_MAP_OFFSET; y>=0; y--) {
	for(x = use_config[CONFIG_MAPWIDTH]+MAX_MAP_OFFSET; x>=0; x--) {

	    mx = x + pl_pos.x;
	    my = y + pl_pos.y;

	    if ( the_map.cells[mx][my].heads[layer].face!=0 &&
		CAN_SMOOTH(the_map.cells[mx][my],layer))
		drawsmooth_opengl(x, y, mx, my, layer);
	}
    }
}


/* This generates a map in Opengl mode.
 *
 * This is based loosely on the SDL code.  We re-draw the entire
 * map every time this is called
 *
 * Note that at current time, most of the options are not available.  On
 * my system, opengl is blindingly fast compared to other drawing methods -
 * for the 25x25 map, sdl/pixmap took almost 100% of cpu time (athlon mp 2000).
 * On opengl, cpu time is less than 10%.  And that is with redrawing the entire
 * map each time.  As such, the more complex drawing options should still be
 * easily handled on any system that does have opengl.
 * (for reference, glxgears gets about 380 fps on my system)
 * MSW 2005-03-14
 */

void opengl_gen_map(int redraw) {
    long elapsed1, elapsed2;
    struct timeval tv1, tv2,tv3;
    int mx,my, layer,x,y, d1, d2, d3, num_dark, got_smooth, face, t1, t2;

    if (time_map_redraw)
	gettimeofday(&tv1, NULL);

    glClear(GL_COLOR_BUFFER_BIT);

    /* Need to set this, as the darkness logic could have reset this.
     * since darkness is done after all other drawing, we can do it just
     * once here.
     */
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);


    /* We draw every space every time this is called.
     * We draw from bottom right to top left - this
     * makes stacking of big images work as expected.
     * we also draw all of one layer before doing the
     * next layer.  This really shouldn't effect
     * performance all that much - all that is being
     * changed is the order of the loops
     * we add MAX_MAP_OFFSET so that big objects off the edge of the map are drawn.
     */
    for (layer=0; layer<=MAXLAYERS; layer++) {

	got_smooth=0;

	if (layer == MAXLAYERS) {
	    /* the top layer is the darkness processing - turn off
	     * the texture pattern so darkness works as expected.
	     */
	    glBindTexture(GL_TEXTURE_2D, 0);
	}
	for(y = use_config[CONFIG_MAPHEIGHT]+MAX_MAP_OFFSET; y>=0; y--) {
	    for(x = use_config[CONFIG_MAPWIDTH]+MAX_MAP_OFFSET; x>=0; x--) {
		/* mx,my represent the spaces on the 'virtual' map (ie, the_map structure).
		 * x and y (from the for loop) represent the visable screen.
		 */
		mx = x + pl_pos.x;
		my = y + pl_pos.y;

		/* if we get here, this denotes a special/top layer.  This is the
		 * time to do lighting, smoothing, etc.
		 */
		if (layer == MAXLAYERS) {
		    /* If off the map, don't need to do anything, because the code here
		     * doesn't do anything that would extend onto the map.
		     */
		    if (x >= use_config[CONFIG_MAPWIDTH] || y >= use_config[CONFIG_MAPHEIGHT]) continue;

		    /* One could make the case it doesn't make sense to light fog of war
		     * spaces, but I find visually it looks a lot nicer if you do -
		     * otherwise, they are too bright relative to the spaces around.
		     * them.
		     */
		    if (use_config[CONFIG_DARKNESS]) {
			opengl_light_space(x, y, mx, my);
		    }

		} else {
		    /* only do this in the better lighting modes.  Fortunately,
		     * the CFG_LT_.. values are ordered from worst to best, to
		     * are >= check works just fine, right now.
		     */
		    if (layer == 0 && use_config[CONFIG_DARKNESS] >= CFG_LT_PIXEL &&
~			x <= use_config[CONFIG_MAPWIDTH] && y <= use_config[CONFIG_MAPHEIGHT]) {

			/* The darkness code is similar to the per pixel SDL code.
			 * as such, each square we process needs to know the values for the
			 * intersection points - there is a lot of redundant calculation
			 * in this if done a square at a time, so instead, we can calculate
			 * the darkness point of all the vertices here.  We calculate the
			 * upper/left 4 vertices.
			 *
			 * SDL actually gets better results I think - perhaps because
			 * the per pixel lighting uses a different algorithym - we basically
			 * let opengl do the blending.  But the results we use here, while
			 * perhaps not as nice, certainly look better than per tile lighting.
			 */
			if (the_map.cells[mx][my].have_darkness)
			    map_darkness[x*2 + 1][y*2 + 1] = the_map.cells[mx][my].darkness;
			else
			    map_darkness[x*2 + 1][y*2 + 1] = DEFAULT_DARKNESS;

			d1 = DEFAULT_DARKNESS;	/* square to left */
			d2 = DEFAULT_DARKNESS;	/* square to upper left */
			d3 = DEFAULT_DARKNESS;	/* square above */
			num_dark=1; /* Number of adjoining spaces with darkness */

			if (x>0 && the_map.cells[mx-1][my].have_darkness) {
			    d1 = the_map.cells[mx-1][my].darkness;
			    num_dark++;
			}

			if (x>0 && y>0 && the_map.cells[mx-1][my-1].have_darkness) {
			    d2 = the_map.cells[mx-1][my-1].darkness;
			    num_dark++;
			}

			if (y>0 && the_map.cells[mx][my-1].have_darkness) {
			    d3 = the_map.cells[mx][my-1].darkness;
			    num_dark++;
			}
#if 0
			/* If we don't have darkness, we want to use our value and not
			 * average.  That is because the value we average against is 0 -
			 * this results in lighter bands next to areas we won't have darkness
			 * info for.
			 */
			map_darkness[x*2][y*2] = (d1 + d2 +d3 + map_darkness[x*2 + 1][y*2 + 1]) / num_dark;

			if (d1) {
			    map_darkness[x*2][y*2 + 1] = (d1 + map_darkness[x*2 + 1][y*2 + 1]) / 2;
			} else {
			    map_darkness[x*2][y*2 + 1] = map_darkness[x*2 + 1][y*2 + 1];
			}

			if (d3) {
			    map_darkness[x*2 +1 ][y*2] = (d3 + map_darkness[x*2 + 1][y*2 + 1]) / 2;
			} else {
			    map_darkness[x*2 + 1][y*2] = map_darkness[x*2 + 1][y*2 + 1];
			}
#else
			/* This block does a 'max' darkness - I think it gives the best results,
			 * which is why by default it is the one used.
			 */
			map_darkness[x*2][y*2] = MAX( MAX(d1, d2), MAX(d3, map_darkness[x*2 + 1][y*2 + 1]));
			map_darkness[x*2][y*2 + 1] = MAX(d1, map_darkness[x*2 + 1][y*2 + 1]);
			map_darkness[x*2 + 1][y*2] = MAX(d3, map_darkness[x*2 + 1][y*2 + 1]);
#endif

		    }

		    if (the_map.cells[mx][my].heads[layer].face) {
			if (pixmaps[the_map.cells[mx][my].heads[layer].face]->map_texture) {
			    int nx, ny;

			    /* nx, ny are the location of the top/left side of the image to draw. */

			    nx = (x+1) * map_image_size - pixmaps[the_map.cells[mx][my].heads[layer].face]->map_width;
			    ny = (y+1) * map_image_size - pixmaps[the_map.cells[mx][my].heads[layer].face]->map_height;

			    /* if both nx and ny are outside visible area, don't need to do anything more */
			    if (nx > width && ny > height) continue;

			    /* There are some issues with this - it is really the head of the
			     * object that is determining fog of war logic.  I don't have good solution
			     * to that, other than to live with it.
			     */
			    if (the_map.cells[mx][my].cleared)
				glBindTexture(GL_TEXTURE_2D, pixmaps[the_map.cells[mx][my].heads[layer].face]->fog_texture);
			    else
				glBindTexture(GL_TEXTURE_2D, pixmaps[the_map.cells[mx][my].heads[layer].face]->map_texture);

			    glBegin(GL_QUADS);

			    glTexCoord2f(0.0f, 0.0f);
			    glVertex3i(nx, ny, 0);

			    glTexCoord2f(1.0f, 0.0f);
			    glVertex3i( (x+1) * map_image_size, ny, 0);

			    glTexCoord2f(1.0f, 1.0f);
			    glVertex3i( (x+1) * map_image_size, (y+1) * map_image_size, 0);

			    glTexCoord2f(0.0f, 1.0f);
			    glVertex3i(nx, (y+1) * map_image_size, 0);

			    glEnd();
			}
			if (use_config[CONFIG_SMOOTH] && CAN_SMOOTH(the_map.cells[mx][my],layer) &&
			    the_map.cells[mx][my].heads[layer].face !=0) {

			    got_smooth=1;
			}
		    }
		    if ((face=mapdata_bigface_head(x, y, layer, &t1, &t2))!=0) {
			if (pixmaps[face]->map_texture) {
			    int nx, ny;

			    /* nx, ny are the location of the top/left side of the image to draw. */

			    nx = (x+1) * map_image_size - pixmaps[face]->map_width;
			    ny = (y+1) * map_image_size - pixmaps[face]->map_height;

			    /* if both nx and ny are outside visible area, don't need to do anything more */
			    if (nx > width && ny > height) continue;

			    /* There are some issues with this - it is really the head of the
			     * object that is determining fog of war logic.  I don't have good solution
			     * to that, other than to live with it.
			     */
			    if (the_map.cells[mx][my].cleared)
				glBindTexture(GL_TEXTURE_2D, pixmaps[face]->fog_texture);
			    else
				glBindTexture(GL_TEXTURE_2D, pixmaps[face]->map_texture);

			    glBegin(GL_QUADS);

			    glTexCoord2f(0.0f, 0.0f);
			    glVertex3i(nx, ny, 0);

			    glTexCoord2f(1.0f, 0.0f);
			    glVertex3i( (x+1) * map_image_size, ny, 0);

			    glTexCoord2f(1.0f, 1.0f);
			    glVertex3i( (x+1) * map_image_size, (y+1) * map_image_size, 0);

			    glTexCoord2f(0.0f, 1.0f);
			    glVertex3i(nx, (y+1) * map_image_size, 0);

			    glEnd();
			}
		    } /* If this space has a valid face */
		} /* If last layer/else not last layer */
	    } /* for x loop */
	} /* for y loop */

	/* Because of our handling with big images, we can't easily draw this
         * when drawing the space - we may want to smooth onto another space
	 * in which the ground hasn't been drawn yet, so we have to do smoothing
	 * at the end of each layer.
	 * We use the got_smooth variable to know if there is in fact any smoothing
	 * to do - for many layers, this is not likely to be set, so we can save
	 * work by not doing this.
	 */
	if (got_smooth)
	    draw_smoothing(layer);
    }

    if (time_map_redraw)
	gettimeofday(&tv2, NULL);

    #ifndef WIN32
    glXSwapBuffers(display, window);
    #else
    SwapBuffers(devicecontext);
    #endif

    if (time_map_redraw) {
	gettimeofday(&tv3, NULL);
	elapsed1 = (tv2.tv_sec - tv1.tv_sec)*1000000 + (tv2.tv_usec - tv1.tv_usec);
	elapsed2 = (tv3.tv_sec - tv2.tv_sec)*1000000 + (tv3.tv_usec - tv2.tv_usec);

	/* I care about performance for 'long' updates, so put the check in to make
	 * these a little more noticable */
	if ((elapsed1 + elapsed2)>10000)
	    LOG(LOG_INFO,"gtk::opengl_gen_map","gen took %7ld, flip took %7ld, total = %7ld",
		    elapsed1, elapsed2, elapsed1 + elapsed2);
    }
} /* opengl_gen_map function */



/* Rather than put a bunch of opengl code in the image.c file,
 * it instead calls these routines for the image creation
 * logic.
 */
void create_opengl_map_image(uint8 *data, PixmapInfo *pi)
{
    static uint8 *newdata;
    static int size=0;
    int nwidth, nheight, numshifts, i;
    uint8 *data_to_use = data, *l;
    uint32 g, *p;

    /* the width and height of textures has to be a power of 2.
     * so 32x32 and 64x64 images work, but 96x96 does not.
     * The logic below basically figures out if the width we have
     * is in fact a power of two or not, and if not, the next power
     * of 2 up that is.
     */
    for (nwidth = pi->map_width, numshifts=0; nwidth >1; nwidth >>=1, numshifts++) ;
    nwidth <<= numshifts;
    if (nwidth != pi->map_width) nwidth <<=1;

    for (nheight = pi->map_height, numshifts=0; nheight >1; nheight >>=1, numshifts++) ;
    nheight <<= numshifts;
    if (nheight != pi->map_height) nheight <<=1;


    /* Below deals with cases where the pixmap is not a power of 2.
     * The 'proper' opengl way of dealing with such textures is to make a mipmap -
     * this is basically a resized version up/down to the nearest power of two,
     * which is then rescaled when it is used to actually texture something - this
     * means it is scaled up once, then scaled down again.  For most opengl apps,
     * this probably isn't a big deal, because the surface the texture being applied
     * applied to is going to cause distortion.  However, in this client, we are doing
     * a 1:1 paste operation, so that scaling will certainly cause some distortion.
     * instead, I take the approach make a bigger image, but fill the extra bits with
     * transparent alpha bits.  this way, we have no loss of quality.
     */
    if (pi->map_width != nwidth || pi->map_height != nheight) {
	int y;
	uint8	*datastart;

	/* Use a static buffer to hold image data, so we don't have to
	 * keep allocating/deallocating, but need to make sure it is
	 * big enough.
	 */
	if (nwidth * nheight * 4 > size) {
	    size = nwidth * nheight * 4;
	    newdata = realloc(newdata, size);
	}
	/* Fill the top portion of the image with empty/transparent data.  Also,
	 * set up datastart to point to where we should start filling in the
	 * rest of the data
	 */
	if (nheight > pi->map_height) {
	    memset(newdata, 0, (nheight - pi->map_height) * nwidth * 4);
	    datastart = newdata + (nheight - pi->map_height) * nwidth * 4;
	} else
	    datastart = newdata;

	for (y =0; y < pi->map_height; y++) {
	    memset(datastart + y * nwidth * 4, 0, (nwidth - pi->map_width) * 4);
	    memcpy(datastart + y * nwidth * 4 + (nwidth - pi->map_width) * 4,
		data + y * pi->map_width * 4, pi->map_width * 4);
	}
	data_to_use = newdata;
	pi->map_width = nwidth;
	pi->map_height = nheight;
    }



    glGenTextures(1, &pi->map_texture);
    glBindTexture(GL_TEXTURE_2D, pi->map_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, 4, pi->map_width, pi->map_height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, data_to_use);

    /* Generate a fog image.  This isn't 100% efficient, because
     * we copy data we may need to modify, but makes the code simpler in
     * in the case above where we've had to change the size of the image.
     */

    if (pi->map_width * pi->map_height * 4 > size) {
	size = pi->map_width * pi->map_height * 4;
	newdata = realloc(newdata, size);
    }

    /* In this case, newdata does not contain a copy of the data - make one */
    if (data_to_use != newdata) {
	    memcpy(newdata, data, pi->map_height *  pi->map_width * 4);
    }

    for (i=0; i < pi->map_width * pi->map_height; i++) {
	l = (uint8 *) (newdata + i*4);
	g = MAX(*l, *(l+1));
	g = MAX(g, *(l+2));
	p = (uint32*) newdata + i;
        *p = g | (g << 8) | (g << 16) | (*(l + 3) << 24);
    }

    glGenTextures(1, &pi->fog_texture);
    glBindTexture(GL_TEXTURE_2D, pi->fog_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, 4, pi->map_width, pi->map_height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, newdata);

}

void opengl_free_pixmap(PixmapInfo *pi)
{
    if (pi->map_texture) {
	glDeleteTextures(1, &pi->map_texture);
	pi->map_texture=0;
    }
    if (pi->fog_texture) {
	glDeleteTextures(1, &pi->fog_texture);
	pi->fog_texture=0;
    }
}

#include "../../pixmaps/question.111"
void create_opengl_question_mark()
{

    GLubyte question[question_height][question_width][4];
    int xb, x, y, offset=0;

    /* We want data in rgba format.  So convert the question bits
     * to an rgba format.  We only need to do this once
     */
    for (y=0; y<question_height; y++) {
	for (xb=0; xb<question_width/8; xb++) {
	    for (x=0; x<8; x++) {
		if (question_bits[offset] & (1 << x)) {
		    question[y][xb * 8 + x][0] = 255;
		    question[y][xb * 8 + x][1] = 255;
		    question[y][xb * 8 + x][2] = 255;
		    question[y][xb * 8 + x][3] = 255;
		} else {
		    question[y][xb * 8 + x][0] = 0;
		    question[y][xb * 8 + x][1] = 0;
		    question[y][xb * 8 + x][2] = 0;
		    question[y][xb * 8 + x][3] = 0;
		}
	    }
	    offset++;
	}
    }

    glGenTextures(1, &pixmaps[0]->map_texture);
    glBindTexture(GL_TEXTURE_2D, pixmaps[0]->map_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, 4, question_width, question_height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, &question[0][0][0]);

    glGenTextures(1, &pixmaps[0]->fog_texture);
    glBindTexture(GL_TEXTURE_2D, pixmaps[0]->fog_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, 4, question_width, question_height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, &question[0][0][0]);
}

#endif
