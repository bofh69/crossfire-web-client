/*
 * static char *rcsid_image_c =
 *   "$Id$";
 */
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

/*
 * This file contains image related functions - this is a higher level up -
 * it mostly deals with the caching of the images, processing the image commands
 * from the server, etc.  This file is gtk specific - at least it returns
 * gtk pixmaps.
 */

#include "config.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <png.h>

/* Pick up the gtk headers we need */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_SDL
#include <SDL.h>
#include <SDL_image.h>
#endif

#include "client-types.h"
#include "gx11.h"
#include "client.h"

#include "gtkproto.h"

/* size for icons and map images, represented as a percentage */
int icon_scale=100, map_scale=100;

struct {
    char    *name;
    uint32  checksum;
    uint8   *png_data;
    uint32  width, height;
} private_cache[MAXPIXMAPNUM];

/* This holds the name we recieve with the 'face' command so we know what
 * to save it as when we actually get the face.
 */
char *facetoname[MAXPIXMAPNUM];


int use_private_cache=0, last_face_num=0;


/******************************************************************************
 *
 * Code related to face caching.
 *
 *****************************************************************************/

char facecachedir[MAX_BUF];

typedef struct Keys {
    uint8	flags;
    sint8	direction;
    KeySym	keysym;
    char	*command;
    struct Keys	*next;
} Key_Entry;


void requestface(int pnum, char *facename, char *facepath)
{
    char buf[MAX_BUF];

    facetoname[pnum] = strdup_local(facepath);
    sprintf(buf,"askface %d", pnum);
    cs_write_string(csocket.fd, buf, strlen(buf));
    /* Need to make sure we have the directory */
    sprintf(buf,"%s/%c%c", facecachedir, facename[0], facename[1]);
    if (access(buf,R_OK)) make_path_to_dir(buf);
}

/* Rotate right from bsd sum. */
#define ROTATE_RIGHT(c) if ((c) & 01) (c) = ((c) >>1) + 0x80000000; else (c) >>= 1;

/*#define CHECKSUM_DEBUG*/

/* These little helper functions just make the code below much more readable */
static void create_icon_image(uint8 *data, int pixmap_num)
{
    if (rgba_to_gdkpixmap(gtkwin_root->window, data, pixmaps[pixmap_num].icon_width, pixmaps[pixmap_num].icon_height,
		(GdkPixmap**)&pixmaps[pixmap_num].icon_image, (GdkBitmap**)&pixmaps[pixmap_num].icon_mask,
		gtk_widget_get_colormap(gtkwin_root)))
		    fprintf(stderr,"Unable to create scaled image, dest num = %d\n", pixmap_num);
}

/* These little helper functions just make the code below much more readable */
static void create_map_image(uint8 *data, int pixmap_num)
{
    pixmaps[pixmap_num].map_image = NULL;
    pixmaps[pixmap_num].map_mask = NULL;

    if (sdlimage) {
#if defined(HAVE_SDL)
    #if SDL_BYTEORDER == SDL_LIL_ENDIAN
	pixmaps[pixmap_num].map_image = SDL_CreateRGBSurfaceFrom(data, pixmaps[pixmap_num].map_width,
	        pixmaps[pixmap_num].map_height, 32, pixmaps[pixmap_num].map_width * 4,  0xff,
			0xff00, 0xff0000, 0xff000000);
    #else
	/* Big endian */
	pixmaps[pixmap_num].map_image = SDL_CreateRGBSurfaceFrom(data, pixmaps[pixmap_num].map_width,
	        pixmaps[pixmap_num].map_height, 32, pixmaps[pixmap_num].map_width * 4,  0xff000000,
			0xff0000, 0xff00, 0xff);

    #endif

#endif
    }
    else {
	rgba_to_gdkpixmap(gtkwin_root->window, data, pixmaps[pixmap_num].map_width, pixmaps[pixmap_num].map_height,
		(GdkPixmap**)&pixmaps[pixmap_num].map_image, (GdkBitmap**)&pixmaps[pixmap_num].map_mask,
		gtk_widget_get_colormap(gtkwin_root));
    }
}

/* Takes the pixmap to put the data into, as well as the rgba
 * data (ie, already loaded with png_to_data).  Scales and
 * stores the relevant data into the pixmap structure.
 * returns 1 on failure.
 */
int create_and_rescale_image_from_data(int pixmap_num, uint8 *rgba_data, int width, int height)
{
    int nx, ny;
    uint8 *png_tmp;

    /* In all cases, the icon images are in native form. */
    if (icon_scale != 100) {
	nx=width;
	ny=height;
	png_tmp = rescale_rgba_data(rgba_data, &nx, &ny, icon_scale);
	pixmaps[pixmap_num].icon_width = nx;
	pixmaps[pixmap_num].icon_height = ny;
	create_icon_image(png_tmp, pixmap_num);
	free(png_tmp);
    }
    else {
	pixmaps[pixmap_num].icon_width = width;
	pixmaps[pixmap_num].icon_height = height;
	create_icon_image(rgba_data, pixmap_num);
    }

    /* We could try to be more intelligent if icon_scale matched map_scale,
     * but this shouldn't be called too often, and this keeps the code
     * simpler.
     */
    if (map_scale != 100) {
	nx=width;
	ny=height;
	png_tmp = rescale_rgba_data(rgba_data, &nx, &ny, map_scale);
	pixmaps[pixmap_num].map_width = nx;
	pixmaps[pixmap_num].map_height = ny;
	create_map_image(png_tmp, pixmap_num);
	if (!sdlimage) free(png_tmp);
    } else {
	pixmaps[pixmap_num].map_width = width;
	pixmaps[pixmap_num].map_height = height;
	create_map_image(rgba_data, pixmap_num);
    }
    /* Not ideal, but basically, if it is missing the map or icon image, presume
     * something failed.
     */
    if (!pixmaps[pixmap_num].icon_image || !pixmaps[pixmap_num].map_image) return 1;
    return 0;
}


/* This is common for both face1 and face commands. */
void finish_face_cmd(int pnum, uint32 checksum, int has_sum, char *face)
{
    char buf[MAX_BUF];
    int fd,len;
    uint32 nx,ny;
    uint8 data[65536], *png_tmp;
    uint32 newsum=0;

    /* Check private cache first */
    sprintf(buf,"%s/.crossfire/gfx/%s", getenv("HOME"), face);
    strcat(buf,".png");

    if ((fd=open(buf, O_RDONLY))!=-1) {
	len=read(fd, data, 65535);
	close(fd);
	has_sum=0;  /* Maybe not really true, but we want to use this image
		     * and not request a replacement.
		     */
    } else {

	/* Hmm.  Should we use this file first, or look in our home
	 * dir cache first?
	 */
	if (use_private_cache) {
	    len = find_face_in_private_cache(face, checksum);
	    if ( len > 0 ) {
		create_and_rescale_image_from_data(pnum, private_cache[len].png_data,
				private_cache[len].width, private_cache[len].height);
		/* we may want to find a better match */
		if (private_cache[len].checksum == checksum ||
		    !has_sum || keepcache) return;
	    }
	}

	/* To prevent having a directory with 2000 images, we do a simple
	 * split on the first 2 characters.
	 */
	sprintf(buf,"%s/%c%c/%s.png", facecachedir, face[0], face[1],face);

	if ((fd=open(buf, O_RDONLY))==-1) {
	    requestface(pnum, face, buf);
	    return;
	}
	len=read(fd, data, 65535);
	close(fd);
    }

    if (has_sum && !keepcache) {
	for (fd=0; fd<len; fd++) {
	    ROTATE_RIGHT(newsum);
	    newsum += data[fd];
	    newsum &= 0xffffffff;
	}

	if (newsum != checksum) {
#ifdef CHECKSUM_DEBUG
	    fprintf(stderr,"finish_face_command: checksums differ: %s, %x != %x\n",
		    face, newsum, checksum);
#endif
	    requestface(pnum, face, buf);
#ifdef CHECKSUM_DEBUG
	} else {
	    fprintf(stderr,"finish_face_command: checksums match: %s, %x == %x\n",
		    face, newsum, checksum);
#endif
	}
    }

    if (!(png_tmp = png_to_data(data, len, &nx, &ny))) {
	fprintf(stderr,"Got error on png_to_data, file=%s\n",buf);
	requestface(pnum, face, buf);
    }
    if (create_and_rescale_image_from_data(pnum, png_tmp,nx, ny)) {
	    fprintf(stderr,"Got error on create_and_rescale_image_from_data, file=%s\n",buf);
	    requestface(pnum, face, buf);
    }
}


/* This code is somewhat from the crossedit/xutil.c.
 * What we do is create a private copy of all the images
 * for ourselves.  Then, if we get a request to GDK_DISPLAY()
 * a new image, we see if we have it in this cache.
 *
 * This is only supported for PNG images.  I see now reason
 * to support the older image formats since they will be 
 * going away.
 * Note that this does not actually create/render the image, rather it
 * just stores the png data that will then get used to create something
 * later on.  This allows this code to be more general, and also takes
 * into account potential resizing issues.
 */

int ReadImages() {

    int		len,i,num ;
    FILE	*infile;
    char	*cp, databuf[10000], *last_cp=NULL;
    unsigned long  x;

    if (image_file[0] == 0) return 0;

    if (!cache_images) {
	cache_images=1;	    /* we want face commands from server */
	keepcache=TRUE;	    /* Reduce requests for new image */
    }

    if ((infile = fopen(image_file,"r"))==NULL) {
        fprintf(stderr,"Unable to open %s\n", image_file);
	return 0;
    }
    for (i=0; i<MAXPIXMAPNUM; i++)
	private_cache[0].name = NULL;

    i=0;
    while (fgets(databuf,MAX_BUF,infile)) {

	/* First, verify that that image header line is OK */
        if(strncmp(databuf,"IMAGE ",6)!=0) {
	    fprintf(stderr,"ReadImages:Bad image line - not IMAGE, instead\n%s",databuf);
	    return 0;
	}
        num = atoi(databuf+6);
        if (num<0 || num > MAXPIXMAPNUM) {
            fprintf(stderr,"Pixmap number less than zero: %d, %s\n",num, databuf);
            return 0;
	}
	/* Skip accross the number data */
	for (cp=databuf+6; *cp!=' '; cp++) ;
	len = atoi(cp);
	if (len==0 || len>10000) {
	    fprintf(stderr,"ReadImages: length not valid: %d\n%s",
                    len,databuf);
                return 0;
	}
	/* We need the name so that when an FaceCmd comes in, we can look for
	 the matching name.
	 */
	while (*cp!=' ' && *cp!='\n') cp++; /* skip over len */

	/* We only want the last component of the name - not the full path */
	while (*cp != '\n') {
	    if (*cp == '/') last_cp = cp+1; /* don't want the slah either */
	    cp++;
	}
	*cp = '\0';	/* Clear newline */

	private_cache[num].name = strdup_local(last_cp);

	if (fread(databuf, 1, len, infile)!=len) {
           fprintf(stderr,"read_client_images: Did not read desired amount of data, wanted %d\n%s",
                    len, databuf);
                    return 0;
	}
	private_cache[num].checksum=0;
	for (x=0; x<len; x++) {
	    ROTATE_RIGHT(private_cache[num].checksum);
	    private_cache[num].checksum += databuf[x];
	    private_cache[num].checksum &= 0xffffffff;
	}
	if (num > last_face_num) last_face_num = num;

	private_cache[num].png_data = png_to_data((uint8 *)databuf, len, &private_cache[num].width,
			&private_cache[num].height);
	if (!private_cache[num].png_data) {
	    fprintf(stderr,"Got error on png_to_data, image num %d, name %s\n",num, private_cache[num].name);
	    free(private_cache[num].name);
	    private_cache[num].name=NULL;
	    private_cache[num].checksum=0;
	}
    }
    fclose(infile);
    use_private_cache=1;
    return 0;
}

/* try to find a face in our private cache.  We return the face
 * number if we find one, -1 for no face match
 */
int find_face_in_private_cache(char *face, int checksum)
{
    int i;

    for (i=1; i<=last_face_num; i++)
	if (!strcmp(face, private_cache[i].name)) {
	    return i;
	}
    return -1;
}

/* We can now connect to different servers, so we need to clear out
 * any old images.  We try to free the data also to prevent memory
 * leaks.
 * This could be more clever, ie, if we're caching images and go to
 * a new server and get a name, we should try to re-arrange our cache
 * or the like.
 */

void reset_image_data()
{
    int i;

    for (i=1; i<MAXPIXMAPNUM; i++) {
	if (pixmaps[i].icon_image) {
	    gdk_pixmap_unref(pixmaps[i].icon_image);
	    pixmaps[i].icon_image=NULL;
	    if (pixmaps[i].icon_mask) {
		gdk_pixmap_unref(pixmaps[i].icon_mask);
		pixmaps[i].icon_mask=NULL;
	    }
	}
	if (cache_images && facetoname[i]!=NULL) {
	    free(facetoname[i]);
	    facetoname[i]=NULL;
	}
	if (pixmaps[i].map_image) {
#ifdef HAVE_SDL
	    if (sdlimage) {
		SDL_FreeSurface(pixmaps[i].map_image);
		free(((SDL_Surface*)pixmaps[i].map_image)->pixels);
	    }
	    else

#endif
	    {
		gdk_pixmap_unref(pixmaps[i].map_image);
		pixmaps[i].map_image=NULL;
		if (pixmaps[i].map_mask) {
		    gdk_pixmap_unref(pixmaps[i].map_mask);
		    pixmaps[i].icon_mask=NULL;
		}
	    }
	}
    }
    /*
    memset(&the_map, 0, sizeof(struct Map));
    */
    memset( the_map.cells[0], 0, sizeof( sizeof( struct MapCell)*
					 the_map.x * the_map.y ));
}


