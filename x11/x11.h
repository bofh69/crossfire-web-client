
#ifndef GX11_H
#define GX11_H

#include "client-types.h"

#define MAXPIXMAPNUM 10000

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
extern uint8 sdlimage,cache_images,split_windows,map_did_scroll,keepcache;
extern uint8 nopopups,updatekeycodes;;
extern int map_scale, icon_scale, updatelock;
extern char *facetoname[MAXPIXMAPNUM];

struct PixmapInfo {
  Pixmap pixmap,mask;
  Pixmap bitmap;
  long fg,bg;
};

extern struct PixmapInfo pixmaps[MAXPIXMAPNUM];
extern Display *display;
extern long screen_num;
extern uint8   image_size;
extern Window win_root,win_game;
extern GC gc_game;
extern Colormap colormap;
extern Window win_stats,win_message;


#endif
