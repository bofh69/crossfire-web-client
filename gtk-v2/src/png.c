char *rcsid_gtk2_png_c =
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


#include <config.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <png.h>
#include <client-types.h>
#include <client.h>

/* Pick up the gtk headers we need */
#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#endif
#include <gdk/gdkkeysyms.h>


/* Defines for PNG return values */
/* These should be in a header file, but currently our calling functions
 * routines just check for nonzero return status and don't really care
 * why the load failed.
 */
#define PNGX_NOFILE	1
#define PNGX_OUTOFMEM	2
#define PNGX_DATA	3

static uint8 *data_cp;
static int data_len, data_start;

static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    memcpy(data, data_cp + data_start, length);
    data_start += length;
}

uint8 *png_to_data(uint8 *data, int len, uint32 *width, uint32 *height)
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

	
    if (setjmp (png_jmpbuf(png_ptr))) {
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	return NULL;
    }


    png_set_read_fn(png_ptr, NULL, user_read_data);
    png_read_info (png_ptr, info_ptr);

	/*
	 * This seems to bug on at least one system (other than mine)
	 * http://www.metalforge.net/cfmb/viewtopic.php?t=1085
	 *
	 * I think its actually a bug in libpng. This function dies with an 
	 * error based on image width. However I've produced a work around
	 * using the indivial functions. Repeated below.
	 * 
    png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)width, (png_unit_32*)height, &bit_depth,
		 &color_type, &interlace_type, &compression_type, &filter_type);
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
	/*
	 * See above for error description
    png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)width, (png_uint_32*)height, &bit_depth,
		 &color_type, &interlace_type, &compression_type, &filter_type);
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
	LOG(LOG_CRITICAL,"gtk::png_to_data","Out of memory - exiting");
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
	LOG(LOG_CRITICAL,"gtk::rescale_rgba_data","Image too big");
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


guchar rgb[512*512*3];	/* Make this especially big to support larger images in the future */

/* This takes data that has already been converted into RGBA format (via
 * png_to_data above perhaps) and creates a GdkPixmap and GdkBitmap out
 * of it.
 * Return non zero on error (currently, no checks for error conditions is done 
 */
int rgba_to_gdkpixmap(GdkWindow *window, uint8 *data,int width, int height,
		   GdkPixmap **pix, GdkBitmap **mask, GdkColormap *colormap)
{
    GdkGC	*gc, *gc_alpha;
    int		has_alpha=0, alpha;
    GdkColor  scolor;
    int x,y;

    *pix = gdk_pixmap_new(window, width, height, -1);

    gc=gdk_gc_new(*pix);
    gdk_gc_set_function(gc, GDK_COPY);

    *mask=gdk_pixmap_new(window, width, height,1);
    gc_alpha=gdk_gc_new(*mask);
    
    scolor.pixel=1;
    gdk_gc_set_foreground(gc_alpha, &scolor);
    gdk_draw_rectangle(*mask, gc_alpha, 1, 0, 0, width, height);

    scolor.pixel=0;
    gdk_gc_set_foreground(gc_alpha, &scolor);

    /* we need to draw the alpha channel.  The image may not in fact
     * have alpha, but no way to know at this point other than to try
     * and draw it.
     */
    for (y=0; y<height; y++) {
	for (x=0; x<width; x++) {
	    alpha = data[(y * width + x) * 4 +3];
	    /* Transparent bit */
	    if (alpha==0) {
		gdk_draw_point(*mask, gc_alpha, x, y);
		has_alpha=1;
	    }
	}
    }

    gdk_draw_rgb_32_image(*pix, gc,  0, 0, width, height, GDK_RGB_DITHER_NONE, data, width*4);
    if (!has_alpha) {
	gdk_pixmap_unref(*mask);
	*mask = NULL;
    }

    gdk_gc_destroy(gc_alpha);
    gdk_gc_destroy(gc);
    return 0;
}

/* This takes data that has already been converted into RGBA format (via
 * png_to_data above perhaps) and creates a GdkPixbuf
 * of it.
 * Return non zero on error (currently, no checks for error conditions is done 
 */
int rgba_to_gdkpixbuf(uint8 *data,int width, int height,GdkPixbuf **pix)
{
    int		rowstride;
    guchar  *pixels, *p;
    int x,y;

#if 0
    /* I'm not sure why this doesn't work, since it seems
     * the data should be in the right format, but it doesn't.
     */
    *pix = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB,
		    TRUE, 8, width, height, width * 4, NULL, NULL);
    return 0;

#else
    *pix  = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);

    rowstride =  gdk_pixbuf_get_rowstride(*pix);
    pixels = gdk_pixbuf_get_pixels(*pix);

    for (y=0; y<height; y++) {
	for (x=0; x<width; x++) {
	    p = pixels + y * rowstride + x * 4;
	    p[0] = data[4*(x + y * width)];
	    p[1] = data[4*(x + y * width) + 1 ];
	    p[2] = data[4*(x + y * width) + 2 ];
	    p[3] = data[4*(x + y * width) + 3 ];

	}
    }

    return 0;
#endif
}


int png_to_gdkpixmap(GdkWindow *window, uint8 *data, int len,
		   GdkPixmap **pix, GdkBitmap **mask, GdkColormap *colormap)
{
    static uint8 *pixels=NULL;
    static int pixels_byte=0, rows_byte=0;
    static png_bytepp	rows=NULL;
    unsigned long width, height;
    png_structp	png_ptr;
    png_infop	info_ptr;
    int bit_depth, color_type, interlace_type, compression_type, filter_type,
	bpp, x,y,has_alpha,i,alpha;
    GdkColor  scolor;
    GdkGC	*gc, *gc_alpha;

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
	png_destroy_read_struct (&png_ptr, &info_ptr,NULL);
	return PNGX_DATA;
    }
    has_alpha=0;
    png_set_read_fn(png_ptr, NULL, user_read_data);
    png_read_info (png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
		 &color_type, &interlace_type, &compression_type, &filter_type);

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
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
		 &color_type, &interlace_type, &compression_type, &filter_type);
    if (color_type & PNG_COLOR_MASK_ALPHA)
                bpp = 4;
    else
                bpp = 3;

    /* Allocate the memory we need once, and increase it if necessary.
     * This is more efficient the allocating this block of memory every time.
     */
    if (pixels_byte==0) {
	pixels_byte = width * height * bpp;
	pixels = (uint8*)malloc(pixels_byte);
    } else if ((width * height * bpp) > pixels_byte) {
	pixels_byte =width * height * bpp;
	/* Doing a free/malloc is probably more efficient -
	 * we don't care about the old data in this
	 * buffer.
	 */
	free(pixels);
	pixels= (uint8*)malloc(pixels_byte);
    }

    if (!pixels) {
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	pixels_byte=0;
	return PNGX_OUTOFMEM;
    }
    if (rows_byte == 0) {
	rows =(png_bytepp) malloc(sizeof(char*) * height);
	rows_byte=height;
    } else if (height > rows_byte) {
	rows =(png_bytepp) realloc(rows, sizeof(char*) * height);
	rows_byte=height;
    }
    if (!rows) {
	png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
	pixels_byte=0;
	return PNGX_OUTOFMEM;
    }

    for (y=0; y<height; y++) 
	rows[y] = pixels + y * width * bpp;

    png_read_image(png_ptr, rows);
#if 0
    fprintf(stderr,"image is %d X %d, bpp=%d, color_type=%d\n",
	    width, height, bpp, color_type);
#endif

    *pix = gdk_pixmap_new(window, width, height, -1);


    gc=gdk_gc_new(*pix);
    gdk_gc_set_function(gc, GDK_COPY);

    if (color_type & PNG_COLOR_MASK_ALPHA) {
	*mask=gdk_pixmap_new(window, width, height,1);
	gc_alpha=gdk_gc_new(*mask);
	gdk_gc_set_function(gc_alpha, GDK_COPY);

	scolor.pixel=1;
	gdk_gc_set_foreground(gc_alpha, &scolor);
	gdk_draw_rectangle(*mask, gc_alpha, 1, 0, 0, width, height);

	scolor.pixel=0;
	gdk_gc_set_foreground(gc_alpha, &scolor);
	has_alpha=1;
    }
    else {
        *mask = NULL;
        gc_alpha = NULL;    /* Prevent compile warnings */
    }
    i=0;
    for (y=0; y<height; y++) {
	for (x=0; x<width; x++) {
	    rgb[i++]=rows[y][x*bpp];	/* red */
	    rgb[i++]=rows[y][x*bpp+1];	/* green */
	    rgb[i++]=rows[y][x*bpp+2];	/* blue */
	    if (has_alpha) {
		alpha = rows[y][x*bpp+3];
		/* Transparent bit */
		if (alpha==0) {
		    gdk_draw_point(*mask, gc_alpha, x, y);
		}
	    }
	}
    }
    gdk_draw_rgb_image(*pix, gc,  0, 0, 32, 32, GDK_RGB_DITHER_NONE, rgb, 32*3);
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    if (has_alpha)
	gdk_gc_destroy(gc_alpha);
    gdk_gc_destroy(gc);
    return 0;
}

