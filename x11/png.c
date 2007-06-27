const char *rcsid_x11_png_c =
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

/* This is a light weight png -> xpixmap function.  Most of the code is from
 * the example png documentation.
 * I wrote this because I could not find a simple function that did this -
 * most all libraries out there tended to do a lot more than I needed for
 * crossfire - in addition, imLib actually has bugs which prevents it from
 * rendering some images properly.
 *
 * This function is far from complete, but does the job and removes the need
 * for yet another library.
 */

#include <config.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <png.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "client-types.h"
#include "client.h"
#include "x11.h"

/* Defines for PNG return values */
/* These should be in a header file, but currently our calling functions
 * routines just check for nonzero return status and don't really care
 * why the load failed.
 */
#define PNGX_NOFILE	1
#define PNGX_OUTOFMEM	2
#define PNGX_DATA	3

static unsigned char *data_cp;
static int data_len, data_start;

static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    memcpy(data, data_cp + data_start, length);
    data_start += length;
}


uint8 *png_to_data(unsigned char *data, int len, int *width, int *height)
{
    uint8 *pixels=NULL;
    static png_bytepp	rows=NULL;
    static int rows_byte=0;

    png_structp	png_ptr;
    png_infop	info_ptr;
    int bit_depth, color_type, interlace_type, compression_type, y;

    data_len=len;
    data_cp = data;
    data_start=0;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
				     NULL, NULL, NULL);

    if (!png_ptr) {
	return NULL;
    }
    info_ptr = png_create_info_struct (png_ptr);

    if (!info_ptr) {
	png_destroy_read_struct (&png_ptr, NULL, NULL);
	return NULL;
    }
    if (setjmp (png_ptr->jmpbuf)) {
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	return NULL;
    }

    png_set_read_fn(png_ptr, NULL, user_read_data);
    png_read_info (png_ptr, info_ptr);

    /* Breaking these out instead of using png_get_IHDR fixes bug
     * 1249877 - problems on 64 bit systems (amd64 at least)
     */
    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    interlace_type = png_get_interlace_type(png_ptr, info_ptr);
    compression_type = png_get_compression_type(png_ptr, info_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE &&
            bit_depth <= 8) {

                /* Convert indexed images to RGB */
                png_set_expand (png_ptr);

    } else if (color_type == PNG_COLOR_TYPE_GRAY &&
                   bit_depth < 8) {

                /* Convert grayscale to RGB */
                png_set_expand (png_ptr);

    } else if (png_get_valid (png_ptr,
                                  info_ptr, PNG_INFO_tRNS)) {

                /* If we have transparency header, convert it to alpha
                   channel */
                png_set_expand(png_ptr);

    } else if (bit_depth < 8) {

                /* If we have < 8 scale it up to 8 */
                png_set_expand(png_ptr);


                /* Conceivably, png_set_packing() is a better idea;
                 * God only knows how libpng works
                 */
    }
        /* If we are 16-bit, convert to 8-bit */
    if (bit_depth == 16) {
                png_set_strip_16(png_ptr);
    }

        /* If gray scale, convert to RGB */
    if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                png_set_gray_to_rgb(png_ptr);
    }

        /* If interlaced, handle that */
    if (interlace_type != PNG_INTERLACE_NONE) {
                png_set_interlace_handling(png_ptr);
    }

    /* pad it to 4 bytes to make processing easier */
    if (!(color_type & PNG_COLOR_MASK_ALPHA))
	png_set_filler(png_ptr, 255, PNG_FILLER_AFTER);

    /* Update the info the reflect our transformations */
    png_read_update_info(png_ptr, info_ptr);

    /* re-read due to transformations just made */
    /* Breaking these out instead of using png_get_IHDR fixes bug
     * 1249877 - problems on 64 bit systems (amd64 at least)
     */
    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    interlace_type = png_get_interlace_type(png_ptr, info_ptr);
    compression_type = png_get_compression_type(png_ptr, info_ptr);

    pixels = (uint8*)malloc(*width * *height * 4);

    if (!pixels) {
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	fprintf(stderr,"Out of memory - exiting\n");
	exit(1);
    }

    /* the png library needs the rows, but we will just return the raw data */
    if (rows_byte == 0) {
	rows =(png_bytepp) malloc(sizeof(char*) * *height);
	rows_byte=*height;
    } else if (*height > rows_byte) {
	rows =(png_bytepp) realloc(rows, sizeof(char*) * *height);
	rows_byte=*height;
    }
    if (!rows) {
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	return NULL;
    }

    for (y=0; y<*height; y++)
	rows[y] = pixels + y * *width * 4;

    png_read_image(png_ptr, rows);
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);

    return pixels;
}


/* rescale_png_image takes png data and scales it accordingly.
 * This function is based on pnmscale, but has been modified to support alpha
 * channel - instead of blending the alpha channel, it takes the most opaque
 * value - blending it is not likely to give sane results IMO - for any image
 * that has transparent information, if we blended the alpha, the result would
 * be the edges of that region being partially transparent.
 * This function has also been re-written to use more static data - in the
 * case of the client, it will be called thousands of times, so it doesn't make
 * sense to free the data and then re-allocate it.
 *
 * For pixels that are fully transparent, the end result after scaling is they
 * will be tranparent black.  This is a needed effect for blending to work properly.
 *
 * This function returns a new pointer to the scaled image data.  This is
 * malloc'd data, so should be freed at some point to prevent leaks.
 * This function does not modify the data passed to it - the caller is responsible
 * for freeing it if it is no longer needed.
 *
 * function arguments:
 * data: PNG data - really, this is any 4 byte per pixel data, in RGBA format.
 * *width, *height: The source width and height.  These values are modified
 *   to contain the new image size.
 * scale: percentage size that new image should be.  100 is a same size
 *    image - values larger than 100 will result in zoom, values less than
 *    100 will result in a shrinkage.
 */

/* RATIO is used to know what units scale is - in this case, a percentage, so
 * it is set to 100
 */
#define RATIO	100

#define MAX_IMAGE_WIDTH		1024
#define MAX_IMAGE_HEIGHT	1024
#define BPP 4

uint8 *rescale_rgba_data(uint8 *data, int *width, int *height, int scale)
{
    static int xrow[BPP * MAX_IMAGE_WIDTH], yrow[BPP*MAX_IMAGE_HEIGHT];
    static uint8 *nrows[MAX_IMAGE_HEIGHT];

    /* Figure out new height/width */
    int new_width = *width  * scale / RATIO, new_height = *height * scale / RATIO;

    int sourcerow=0, ytoleft, ytofill, xtoleft, xtofill, dest_column=0, source_column=0, needcol,
	destrow=0;
    int x,y;
    uint8 *ndata;
    uint8 r,g,b,a;

    if (*width > MAX_IMAGE_WIDTH || new_width > MAX_IMAGE_WIDTH
    || *height > MAX_IMAGE_HEIGHT || new_height > MAX_IMAGE_HEIGHT)
    {
	fprintf(stderr, "Image too big\n");
	exit(0);
    }

    /* clear old values these may have */
    memset(yrow, 0, sizeof(int) * *height * BPP);

    ndata = (uint8*)malloc(new_width * new_height * BPP);

    for (y=0; y<new_height; y++)
	nrows[y] = (png_bytep) (ndata + y * new_width * BPP);

    ytoleft = scale;
    ytofill = RATIO;

    for (y=0,sourcerow=0; y < new_height; y++) {
	memset(xrow, 0, sizeof(int) * *width * BPP);
	while (ytoleft < ytofill) {
	    for (x=0; x< *width; ++x) {
		/* Only want to copy the data if this is not a transperent pixel.
		 * If it is transparent, the color information is has is probably
		 * bogus, and blending that makes the results look worse.
		 */
		if (data[(sourcerow * *width + x)*BPP+3] > 0 ) {
		    yrow[x*BPP] += ytoleft * data[(sourcerow * *width + x)*BPP]/RATIO;
		    yrow[x*BPP+1] += ytoleft * data[(sourcerow * *width + x)*BPP+1]/RATIO;
		    yrow[x*BPP+2] += ytoleft * data[(sourcerow * *width + x)*BPP+2]/RATIO;
		}
		/* Alpha is a bit special - we don't want to blend it -
		 * we want to take whatever is the more opaque value.
		 */
		if (data[(sourcerow * *width + x)*BPP+3] > yrow[x*BPP+3])
		    yrow[x*BPP+3] = data[(sourcerow * *width + x)*BPP+3];
	    }
	    ytofill -= ytoleft;
	    ytoleft = scale;
	    if (sourcerow < *height)
		sourcerow++;
	}

	for (x=0; x < *width; ++x) {
	    if (data[(sourcerow * *width + x)*BPP+3] > 0 ) {
		xrow[x*BPP] = yrow[x*BPP] + ytofill * data[(sourcerow * *width + x)*BPP] / RATIO;
		xrow[x*BPP+1] = yrow[x*BPP+1] + ytofill * data[(sourcerow * *width + x)*BPP+1] / RATIO;
		xrow[x*BPP+2] = yrow[x*BPP+2] + ytofill * data[(sourcerow * *width + x)*BPP+2] / RATIO;
	    }
	    if (data[(sourcerow * *width + x)*BPP+3] > xrow[x*BPP+3])
		xrow[x*BPP+3] = data[(sourcerow * *width + x)*BPP+3];
	    yrow[x*BPP]=0; yrow[x*BPP+1]=0; yrow[x*BPP+2]=0; yrow[x*BPP+3]=0;
	}

	ytoleft -= ytofill;
	if (ytoleft <= 0) {
	    ytoleft = scale;
	    if (sourcerow < *height)
		sourcerow++;
	}

	ytofill = RATIO;
	xtofill = RATIO;
	dest_column = 0;
	source_column=0;
	needcol=0;
	r=0; g=0; b=0; a=0;

	for (x=0; x< *width; x++) {
	    xtoleft = scale;

	    while (xtoleft >= xtofill) {
		if (needcol) {
		    dest_column++;
		    r=0; g=0; b=0; a=0;
		}

		if (xrow[source_column*BPP+3] > 0) {
		    r += xtofill * xrow[source_column*BPP] / RATIO;
		    g += xtofill * xrow[1+source_column*BPP] / RATIO;
		    b += xtofill * xrow[2+source_column*BPP] / RATIO;
		}
		if (xrow[3+source_column*BPP] > a)
		    a = xrow[3+source_column*BPP];

		nrows[destrow][dest_column * BPP] = r;
		nrows[destrow][1+dest_column * BPP] = g;
		nrows[destrow][2+dest_column * BPP] = b;
		nrows[destrow][3+dest_column * BPP] = a;
		xtoleft -= xtofill;
		xtofill = RATIO;
		needcol=1;
	    }

	    if (xtoleft > 0 ){
		if (needcol) {
		    dest_column++;
		    r=0; g=0; b=0; a=0;
		    needcol=0;
		}

		if (xrow[3+source_column*BPP] > 0) {
		    r += xtoleft * xrow[source_column*BPP] / RATIO;
		    g += xtoleft * xrow[1+source_column*BPP] / RATIO;
		    b += xtoleft * xrow[2+source_column*BPP] / RATIO;
		}
		if (xrow[3+source_column*BPP] > a)
		    a = xrow[3+source_column*BPP];

		xtofill -= xtoleft;
	    }
	    source_column++;
	}

	if (xtofill > 0 ) {
	    source_column--;
	    if (xrow[3+source_column*BPP] > 0) {
		r += xtofill * xrow[source_column*BPP] / RATIO;
		g += xtofill * xrow[1+source_column*BPP] / RATIO;
		b += xtofill * xrow[2+source_column*BPP] / RATIO;
	    }
	    if (xrow[3+source_column*BPP] > a)
		a = xrow[3+source_column*BPP];
	}

	/* Not positve, but without the bound checking for dest_column,
	 * we were overrunning the buffer.  My guess is this only really
	 * showed up if when the images are being scaled - there is probably
	 * something like half a pixel left over.
	 */
	if (!needcol && (dest_column < new_width)) {
	    nrows[destrow][dest_column * BPP] = r;
	    nrows[destrow][1+dest_column * BPP] = g;
	    nrows[destrow][2+dest_column * BPP] = b;
	    nrows[destrow][3+dest_column * BPP] = a;
	}
	destrow++;
    }
    *width = new_width;
    *height = new_height;
    return ndata;
}


static XImage   *ximage;
static int rmask=0, bmask=0,gmask=0,need_color_alloc=0, rshift=16, bshift=0, gshift=8,
    rev_rshift=0, rev_gshift=0, rev_bshift=0;
static int colors_alloced=0, private_cmap=0, colormap_size;
struct Pngx_Color_Values {
    unsigned char   red, green, blue;
    long    pixel_value;
} *color_values;

#define COLOR_FACTOR       3
#define BRIGHTNESS_FACTOR  1

/* This function is used to find the pixel and return it
 * to the caller.  We store what pixels we have already allocated
 * and try to find a match against that.  The reason for this is that
 * XAllocColor is a very slow routine. Before my optimizations,
 * png loading took about 140 seconds, of which 60 seconds of that
 * was in XAllocColor calls.
 */
long pngx_find_color(Display *display, Colormap *cmap, int red, int green, int blue)
{

    int i, closeness=0xffffff, close_entry=-1, tmpclose;
    XColor  scolor;

    for (i=0; i<colors_alloced; i++) {
	if ((color_values[i].red == red) && (color_values[i].green == green) &&
	    (color_values[i].blue == blue)) return color_values[i].pixel_value;

	tmpclose = COLOR_FACTOR * (abs(red - color_values[i].red) +
				   abs(green - color_values[i].green) +
				   abs(blue - color_values[i].blue)) +
		    BRIGHTNESS_FACTOR * abs((red + green + blue) -
				(color_values[i].red + color_values[i].green + color_values[i].blue));

	/* I already know that 8 bit is not enough to hold all the PNG colors,
	 * so lets do some early optimization
	 */
	if (tmpclose < 3) return color_values[i].pixel_value;
	if (tmpclose < closeness) {
	    closeness = tmpclose;
	    close_entry = i;
	}
    }

    /* If the colormap is full, no reason to do anything more */
    if (colors_alloced == colormap_size)
	return color_values[close_entry].pixel_value;


    /* If we get here, we haven't cached the color */

    scolor.red = (red << 8) + red;
    scolor.green = (green << 8) + green;
    scolor.blue = (blue << 8) + blue;


again:
    if (!XAllocColor(display, *cmap, &scolor)) {
	if (!private_cmap) {
	    fprintf(stderr,"Going to private colormap after %d allocs\n", colors_alloced);
	    *cmap = XCopyColormapAndFree(display, *cmap);
	    private_cmap=1;
	    goto again;
	}
	else {
#if 0
	    fprintf(stderr,"Unable to allocate color %d %d %d, %d colors alloced, will use closenss value %d\n",
		    red, green, blue, colors_alloced, closeness);
#endif
	    colors_alloced = colormap_size;	/* Colormap is exhausted */
	    return color_values[close_entry].pixel_value;
	}
    }
    color_values[colors_alloced].red = red;
    color_values[colors_alloced].green = green;
    color_values[colors_alloced].blue = blue;
    color_values[colors_alloced].pixel_value= scolor.pixel;
    colors_alloced++;
    return scolor.pixel;
}




int init_pngx_loader(Display *display)
{
    int pad,depth;
    XVisualInfo xvinfo, *xvret;
    Visual *visual;

    depth = DefaultDepth(display, DefaultScreen(display));
    visual = DefaultVisual(display, DefaultScreen(display));
    xvinfo.visualid = XVisualIDFromVisual(visual);
    xvret = XGetVisualInfo(display, VisualIDMask, &xvinfo, &pad);
    if (pad != 1) {
	fprintf(stderr,"XGetVisual found %d matching visuals?\n", pad);
	return 1;
    }
    rmask = xvret -> red_mask;
    gmask = xvret -> green_mask;
    bmask = xvret -> blue_mask;
    /* We need to figure out how many bits to shift.  Thats what this
     * following block of code does.  We can't presume to use just
     * 16, 8, 0 bits for RGB respectively, as if you are on 16 bit,
     * that is not correct.  There may be a much easier way to do this -
     * it is just bit manipulation.  Note that we want to preserver
     * the most significant bits, so these shift values can very
     * well be negative, in which case we need to know that -
     * the shift operators don't work with negative values.
     * An example is 5 bits for blue - in that case, we really
     * want to shfit right (>>) by 3 bits.
     */
    rshift=0;
    if (rmask) {
	while (!((1 << rshift) & rmask)) rshift++;
	while (((1 << rshift) & rmask)) rshift++;
	rshift -= 8;
	if (rshift < 0 ) {
	    rev_rshift=1;
	    rshift = -rshift;
	}
    }
    gshift=0;
    if (gmask) {
	while (!((1 << gshift) & gmask)) gshift++;
	while (((1 << gshift) & gmask)) gshift++;
	gshift -= 8;
	if (gshift < 0 ) {
	    rev_gshift=1;
	    gshift = -gshift;
	}
    }
    bshift=0;
    if (bmask) {
	while (!((1 << bshift) & bmask)) bshift++;
	while (((1 << bshift) & bmask)) bshift++;
	bshift -= 8;
	if (bshift < 0 ) {
	    rev_bshift=1;
	    bshift = -bshift;
	}
    }


    if (xvret->class==PseudoColor) {
	need_color_alloc=1;
	if (xvret->colormap_size>256) {
	    fprintf(stderr,"One a pseudocolor visual, but colormap has %d entries?\n", xvret->colormap_size);
	}
	color_values=malloc(sizeof(struct Pngx_Color_Values) * xvret->colormap_size);
	colormap_size = xvret->colormap_size-1;	/* comparing # of alloced colors against this */
    }
    XFree(xvret);

    if (depth>16) pad = 32;
    else if (depth > 8) pad = 16;
    else pad = 8;

    ximage = XCreateImage(display, visual,
		      depth,
		      ZPixmap, 0, 0,
		      MAX_IMAGE_SIZE, MAX_IMAGE_SIZE,  pad, 0);
    if (!ximage) {
	fprintf(stderr,"Failed to create Ximage\n");
	return 1;
    }
    ximage->data = malloc(ximage->bytes_per_line * MAX_IMAGE_SIZE);
    if (!ximage->data) {
	fprintf(stderr,"Failed to create Ximage data\n");
	return 1;
    }
    return 0;
}


int png_to_xpixmap(Display *display, Drawable draw, unsigned char *data, int len,
		   Pixmap *pix, Pixmap *mask, Colormap *cmap,
		   unsigned long *width, unsigned long *height)
{
    static uint8 *pixels=NULL;
    static int pixels_byte=0, rows_byte=0;
    static png_bytepp	rows=NULL;

    png_structp	png_ptr=NULL;
    png_infop	info_ptr=NULL;
    int bit_depth, color_type, interlace_type, compression_type,
	red,green,blue, lastred=-1, lastgreen=-1, lastblue=-1,alpha,bpp, x,y,
	has_alpha, cmask, lastcmask, lastcolor;
    GC	gc, gc_alpha;


    data_len=len;
    data_cp = data;
    data_start=0;
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
				     NULL, NULL, NULL);
    if (!png_ptr) {
	return PNGX_OUTOFMEM;
    }
    info_ptr = png_create_info_struct (png_ptr);

    if (!info_ptr) {
	png_destroy_read_struct (&png_ptr, NULL, NULL);
	return PNGX_OUTOFMEM;
    }
    if (setjmp (png_ptr->jmpbuf)) {
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	return PNGX_DATA;
    }
    /* put these here to prevent compiler warnings about them getting
     * clobbered by setjmp
     */
    has_alpha=0;
    cmask=-1;
    lastcmask=-1;
    lastcolor=-1;

    png_set_read_fn(png_ptr, NULL, user_read_data);
    png_read_info (png_ptr, info_ptr);

    /* re-read due to transformations just made */
    /* Breaking these out instead of using png_get_IHDR fixes bug
     * 1249877 - problems on 64 bit systems (amd64 at least)
     */
    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    interlace_type = png_get_interlace_type(png_ptr, info_ptr);
    compression_type = png_get_compression_type(png_ptr, info_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE &&
            bit_depth <= 8) {

                /* Convert indexed images to RGB */
                png_set_expand (png_ptr);

    } else if (color_type == PNG_COLOR_TYPE_GRAY &&
                   bit_depth < 8) {

                /* Convert grayscale to RGB */
                png_set_expand (png_ptr);

    } else if (png_get_valid (png_ptr,
                                  info_ptr, PNG_INFO_tRNS)) {

                /* If we have transparency header, convert it to alpha
                   channel */
                png_set_expand(png_ptr);

    } else if (bit_depth < 8) {

                /* If we have < 8 scale it up to 8 */
                png_set_expand(png_ptr);


                /* Conceivably, png_set_packing() is a better idea;
                 * God only knows how libpng works
                 */
    }
        /* If we are 16-bit, convert to 8-bit */
    if (bit_depth == 16) {
                png_set_strip_16(png_ptr);
    }

        /* If gray scale, convert to RGB */
    if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                png_set_gray_to_rgb(png_ptr);
    }

        /* If interlaced, handle that */
    if (interlace_type != PNG_INTERLACE_NONE) {
                png_set_interlace_handling(png_ptr);
    }

    /* Update the info the reflect our transformations */
    png_read_update_info(png_ptr, info_ptr);
    /* re-read due to transformations just made */
    /* re-read due to transformations just made */

    /* Breaking these out instead of using png_get_IHDR fixes bug
     * 1249877 - problems on 64 bit systems (amd64 at least)
     */
    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    interlace_type = png_get_interlace_type(png_ptr, info_ptr);
    compression_type = png_get_compression_type(png_ptr, info_ptr);

    if (color_type & PNG_COLOR_MASK_ALPHA)
                bpp = 4;
    else
                bpp = 3;

    /* Allocate the memory we need once, and increase it if necessary.
     * This is more efficient the allocating this block of memory every time.
     */
    if (pixels_byte==0) {
	pixels_byte =*width * *height * bpp;
	pixels = (uint8*)malloc(pixels_byte);
    } else if ((*width * *height * bpp) > pixels_byte) {
	pixels_byte =*width * *height * bpp;
	pixels=realloc(pixels, pixels_byte);
    }

    if (!pixels) {
	pixels_byte=0;
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	return PNGX_OUTOFMEM;
    }
    if (rows_byte == 0) {
	rows =(png_bytepp) malloc(sizeof(char*) * *height);
	rows_byte=*height;
    } else if (*height > rows_byte) {
	rows =(png_bytepp) realloc(rows, sizeof(char*) * *height);
	rows_byte=*height;
    }
    if (!rows) {
	pixels_byte=0;
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	return PNGX_OUTOFMEM;
    }

    for (y=0; y<*height; y++)
	rows[y] = pixels + y * (*width) * bpp;

    png_read_image(png_ptr, rows);
#if 0
    fprintf(stderr,"image is %d X %d, bpp=%d, color_type=%d\n",
	    *width, *height, bpp, color_type);
#endif

    *pix = XCreatePixmap(display, draw, *width, *height,
			DefaultDepth(display,  DefaultScreen(display)));

    gc=XCreateGC(display, *pix, 0, NULL);
    XSetFunction(display, gc, GXcopy);
    XSetPlaneMask(display, gc, AllPlanes);

    if (color_type & PNG_COLOR_MASK_ALPHA) {
	/* The foreground/background colors are not really
	 * colors, but rather values to set in the mask.
	 * The values used below work properly on at least
	 * 8 bit and 16 bit display - using things like
	 * blackpixel & whitepixel does NO work on
	 * both types of display.
	 */
	*mask=XCreatePixmap(display ,draw, *width, *height,1);
	gc_alpha=XCreateGC(display, *mask, 0, NULL);
	XSetFunction(display, gc_alpha, GXcopy);
	XSetPlaneMask(display, gc_alpha, AllPlanes);
	XSetForeground(display, gc_alpha, 1);
	XFillRectangle(display, *mask, gc_alpha, 0, 0, *width, *height);
	XSetForeground(display, gc_alpha, 0);
	has_alpha=1;
    }
    else {
	*mask = None;
	gc_alpha = None;    /* Prevent compile warnings */
    }

    for (y=0; y<*height; y++) {
	for (x=0; x<*width; x++) {
	    red=rows[y][x*bpp];
	    green=rows[y][x*bpp+1];
	    blue=rows[y][x*bpp+2];
	    if (has_alpha) {
		alpha = rows[y][x*bpp+3];
		/* Transparent bit */
		if (alpha==0) {
		    XDrawPoint(display, *mask, gc_alpha, x, y);
		}
	    }
	    if (need_color_alloc) {
		/* We only use cmask to avoid calling pngx_find_color repeatedly.
		 * when the color has not changed from the last pixel.
		 */
		if ((lastred != red) && (lastgreen != green) && (lastblue != blue)) {
		    lastcolor = pngx_find_color(display, cmap, red, green, blue);
		    lastcmask = cmask;
		}
		XPutPixel(ximage, x, y, lastcolor);
	    } else {
		if ((lastred != red) && (lastgreen != green) && (lastblue != blue)) {
		    if (rev_rshift) red >>= rshift;
		    else red <<= rshift;
		    if (rev_gshift) green >>= gshift;
		    else green <<= gshift;
		    if (rev_bshift) blue >>= bshift;
		    else blue <<= bshift;

		    cmask = (red & rmask) | (green  & gmask) | (blue  & bmask);
		}
		XPutPixel(ximage, x, y, cmask);
	    }
	}
    }

    XPutImage(display, *pix, gc, ximage, 0, 0, 0, 0, 32, 32);
    if (has_alpha)
	XFreeGC(display, gc_alpha);
    XFreeGC(display, gc);
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return 0;
}


/* like png_to_xpixmap above, but the data has already been decompressed
 * into rgba data.
 */

int rgba_to_xpixmap(Display *display, Drawable draw, uint8 *pixels,
		   Pixmap *pix, Pixmap *mask, Colormap *cmap,
		   unsigned long width, unsigned long height)
{
    int red,green,blue, lastred=-1, lastgreen=-1, lastblue=-1,alpha,x,y,
	cmask=-1, lastcmask, lastcolor=-1;
    GC	gc, gc_alpha;

    *pix = XCreatePixmap(display, draw, width, height,
			DefaultDepth(display,  DefaultScreen(display)));

    gc=XCreateGC(display, *pix, 0, NULL);
    XSetFunction(display, gc, GXcopy);
    XSetPlaneMask(display, gc, AllPlanes);

    /* The foreground/background colors are not really
     * colors, but rather values to set in the mask.
     * The values used below work properly on at least
     * 8 bit and 16 bit display - using things like
     * blackpixel & whitepixel does NO work on
     * both types of display.
     */

    *mask=XCreatePixmap(display ,draw, width, height,1);
    gc_alpha=XCreateGC(display, *mask, 0, NULL);
    XSetFunction(display, gc_alpha, GXcopy);
    XSetPlaneMask(display, gc_alpha, AllPlanes);
    XSetForeground(display, gc_alpha, 1);
    XFillRectangle(display, *mask, gc_alpha, 0, 0, width, height);
    XSetForeground(display, gc_alpha, 0);

    for (y=0; y<height; y++) {
	for (x=0; x<width; x++) {
	    red=    pixels[(y * width + x)*4];
	    green=  pixels[(y * width + x)*4 + 1];
	    blue=   pixels[(y * width + x)*4 + 2];
	    alpha = pixels[(y * width + x)*4 + 3];
	    if (alpha==0) {
		XDrawPoint(display, *mask, gc_alpha, x, y);
	    }
	    if (need_color_alloc) {
		/* We only use cmask to avoid calling pngx_find_color repeatedly.
		 * when the color has not changed from the last pixel.
		 */
		if ((lastred != red) && (lastgreen != green) && (lastblue != blue)) {
		    lastcolor = pngx_find_color(display, cmap, red, green, blue);
		    lastcmask = cmask;
		}
		XPutPixel(ximage, x, y, lastcolor);
	    } else {
		if ((lastred != red) && (lastgreen != green) && (lastblue != blue)) {
		    if (rev_rshift) red >>= rshift;
		    else red <<= rshift;
		    if (rev_gshift) green >>= gshift;
		    else green <<= gshift;
		    if (rev_bshift) blue >>= bshift;
		    else blue <<= bshift;

		    cmask = (red & rmask) | (green  & gmask) | (blue  & bmask);
		}
		XPutPixel(ximage, x, y, cmask);
	    }
	}
    }

    XPutImage(display, *pix, gc, ximage, 0, 0, 0, 0, width, height);
    XFreeGC(display, gc_alpha);
    XFreeGC(display, gc);
    return 0;
}


/* Takes the pixmap to put the data into, as well as the rgba
 * data (ie, already loaded with png_to_data).  Scales and
 * stores the relevant data into the pixmap structure.
 * returns 1 on failure.
 */
int create_and_rescale_image_from_data(Cache_Entry *ce, int pixmap_num, uint8 *rgba_data, int width, int height)
{
    struct PixmapInfo  *pi;

    if (pixmap_num <= 0 || pixmap_num >= MAXPIXMAPNUM)
	return 1;

    if (pixmaps[pixmap_num] != pixmaps[0]) {
	XFreePixmap(display, pixmaps[pixmap_num]->pixmap);
	if (pixmaps[pixmap_num]->mask)
	    XFreePixmap(display, pixmaps[pixmap_num]->mask);
	free(pixmaps[pixmap_num]);
	pixmaps[pixmap_num] = pixmaps[0];
    }

    pi = malloc(sizeof(struct PixmapInfo));
    if (rgba_to_xpixmap(display, win_game, rgba_data, &pi->pixmap,
		   &pi->mask, &colormap, width, height) != 0) {
	free(pi);
	return 1;
    }

    if (!pi->pixmap || !pi->mask) {
	if (pi->pixmap)
	    XFreePixmap(display, pi->pixmap);
	if (pi->mask)
	    XFreePixmap(display, pi->mask);
	free(pi);
	return 1;
    }

    pi->width = width / image_size;
    pi->height = height / image_size;

    if (ce) {
	ce->image_data = pi;
    }
    pixmaps[pixmap_num] = pi;
    return 0;
}

void get_map_image_size(int face, uint8 *w, uint8 *h)
{
    /* This function is not implemented yet, so just return default values */
    if (face < 0 || face >= MAXPIXMAPNUM) {
	*w = 1;
	*h = 1;
    }
    else {
	*w = pixmaps[face]->width;
	*h = pixmaps[face]->height;
    }
}


#if PNG_MAIN

int main(int argc, char *argv[])
{
    Display *disp;
    char    data[256],buf[1024*1024];
    int	    fd,i,z, alpha;
    unsigned long  width, height;
    Window  window;
    XSetWindowAttributes    wattrs;
    Pixmap  pix, mask;
    GC	    gc;

    if (argc!=2) {
	fprintf(stderr,"Usage: %s <filename>\n", argv[0]);
	exit(1);
    }

    if (!(disp=XOpenDisplay(NULL))) {
	fprintf(stderr,"Unable to open display\n");
	exit(1);
    }

    wattrs.backing_store = WhenMapped;
    wattrs.background_pixel = WhitePixel(disp, DefaultScreen(disp));

    window=XCreateWindow(disp, DefaultRootWindow(disp), 0, 0,
	 32, 32, 0, CopyFromParent, InputOutput, CopyFromParent,
	CWBackingStore|CWBackPixel, &wattrs);

    i = open(argv[1], O_RDONLY);
    z=read(i, buf, 1024*1024);
    close(i);
    fprintf(stderr,"Read %d bytes from %s\n", z, argv[1]);
    png_to_xpixmap(disp, window, buf, z, &pix, &mask, DefaultColormap(disp,
				DefaultScreen(disp)), &width, &height);
    XResizeWindow(disp, window, width, height);
    if (!window) {
	fprintf(stderr,"Unable to create window\n");
	exit(1);
    }
    if (mask) XShapeCombineMask(disp,window,ShapeBounding,0,0,mask,ShapeSet);
    XMapWindow(disp, window);
    XFlush(disp);
    gc=XCreateGC(disp, pix, 0, NULL);
    if (mask)
	XSetClipMask(disp, gc, mask);

    /* A simple way to display the image without needing to worry
     * about exposures and redraws.
     */
    for (i=0; i<30; i++) {
	XCopyArea(disp, pix, window, gc, 0, 0, width, height, 0 , 0);
	XFlush(disp);
	sleep(1);
    }

    exit(0);
}
#endif
