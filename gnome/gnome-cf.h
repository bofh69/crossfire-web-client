
#ifndef GX11_H
#define GX11_H

#include "client-types.h"

typedef struct 
{
  int x;
  int y;
} PlayerPosition;

extern PlayerPosition pl_pos;

extern int fog_of_war;
extern int map_size;
extern int map_image_size, map_image_half_size;
extern int per_pixel_lighting;
extern int per_tile_lighting;
extern int show_grid;
extern uint8 sdlimage,split_windows,map_did_scroll,keepcache;
extern uint8 nopopups,updatekeycodes;;
extern int map_scale, icon_scale, updatelock;

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
#define MAXPIXMAPNUM 10000
typedef struct {
        long fg, bg;
        GdkPixbuf *gdkpixbuf;
        GdkPixmap *map_image;
        GdkBitmap *map_mask;
	uint16      map_width, map_height;
        GdkPixmap *icon_image;
        GdkBitmap *icon_mask;
} PixmapInfo;

extern PixmapInfo pixmaps[MAXPIXMAPNUM];

extern GtkWidget *drawable;
extern GdkGC *gc;
extern GdkBitmap    *dark1, *dark2, *dark3;
extern GdkPixmap    *dark;

#if 0
/* Some global widgetws */
extern GtkWidget    *gtkwin_root,*drawingarea,*run_label,*fire_label;
extern GtkWidget    *gtkwin_info;
extern GtkWidget    *entrytext, *counttext;
extern GdkPixmap    *mapwindow,*dark;
extern GdkGC	    *mapgc;
extern GtkWidget *ckentrytext, *ckeyentrytext, *cmodentrytext,*cnumentrytext, *cclist;
#endif

#endif
