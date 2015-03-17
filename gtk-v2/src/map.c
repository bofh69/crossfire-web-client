/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, see the
 * 'LICENSE' and 'COPYING' files.
 *
 * The authors can be reached via e-mail to crossfire-devel@real-time.com
 */

/**
 * @file
 * Handles map related code in terms of allocation, insertion of new objects,
 * and actual rendering (although the sdl rendering is in the sdl file
 */

#include "client.h"

#include <gtk/gtk.h>

#include "image.h"
#include "main.h"
#include "mapdata.h"
#include "gtk2proto.h"

static gboolean map_updated = FALSE;

// Declarations for local event-handling functions.
static gboolean map_button_event(GtkWidget *widget,
        GdkEventButton *event, gpointer user_data);
static gboolean map_expose_event(GtkWidget *widget,
        GdkEventExpose *event, gpointer user_data);

/*
 * Added for fog of war. Current size of the map structure in memory.
 * We assume a rectangular map so this is the length of one side.
 * command.c needs to know about this so not static
 * FIX ME: Don't assume rectangle
 */

PlayerPosition pl_pos;

int map_image_size = DEFAULT_IMAGE_SIZE;
int map_image_half_size = DEFAULT_IMAGE_SIZE / 2;

static GtkWidget *map_drawing_area;

GtkWidget *map_notebook;

/*
 * This should really be one of the CONFIG values, or perhaps a checkbox
 * someplace that displays frame rate.
 */
gboolean time_map_redraw = FALSE;

/**
 * Calculate and set desired map size based on map window size.
 */
static void map_check_resize() {
    int w = map_drawing_area->allocation.width / map_image_size;
    int h = map_drawing_area->allocation.height / map_image_size;
    w = (w > MAP_MAX_SIZE) ? MAP_MAX_SIZE : w;
    h = (h > MAP_MAX_SIZE) ? MAP_MAX_SIZE : h;

    if (w != want_config[CONFIG_MAPWIDTH] || h != want_config[CONFIG_MAPHEIGHT]) {
        want_config[CONFIG_MAPWIDTH] = w;
        want_config[CONFIG_MAPHEIGHT] = h;
        client_mapsize(w, h);
    }
}

/**
 * Callback for map window resize event.
 */
static void map_configure_event(GtkWidget *widget,
        GdkEventConfigure *event, gpointer data) {
    map_check_resize();
}

/**
 * This initializes the stuff we need for the map.
 *
 * @param window_root The client's main playing window.
 */
void map_init(GtkWidget *window_root) {
    map_drawing_area = GTK_WIDGET(gtk_builder_get_object(
                window_xml, "drawingarea_map"));
    map_notebook = GTK_WIDGET(gtk_builder_get_object(
                window_xml, "map_notebook"));

    g_signal_connect(map_drawing_area, "configure_event",
            G_CALLBACK(map_configure_event), NULL);
    g_signal_connect(map_drawing_area, "expose_event",
            G_CALLBACK(map_expose_event), NULL);

    // Enable event masks and set callbacks to handle mouse events.
    gtk_widget_add_events(map_drawing_area,
            GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(map_drawing_area, "event",
            G_CALLBACK(map_button_event), NULL);

    // Set map size based on window size and show widget.
    map_check_resize();
    gtk_widget_show(map_drawing_area);

    switch (use_config[CONFIG_DISPLAYMODE]) {
#ifdef HAVE_SDL
    case CFG_DM_SDL:
        init_SDL(map_drawing_area,0);
        break;
#endif

#ifdef HAVE_OPENGL
    case CFG_DM_OPENGL:
        init_opengl(map_drawing_area);
        break;
#endif
    }
}

/**
 * Request a map update from the server. This is to circumvent a bug in some
 * server versions.
 * @todo remove
 */
void reset_map() {
}

/**
 * Draw a pixmap to the given map tile on screen.
 * @param ax Map cell on-screen x-coordinate
 * @param ay Map cell on-screen y-coordinate
 */
static void draw_pixmap(cairo_t *cr, PixmapInfo *pixmap, int ax, int ay) {
    cairo_set_source_surface(cr, pixmap->map_image,
            ax * map_image_size, ay * map_image_size);
    cairo_paint(cr);
}

int display_mapscroll(int dx, int dy) {
#ifdef HAVE_SDL
    if (use_config[CONFIG_DISPLAYMODE] == CFG_DM_SDL) {
        return sdl_mapscroll(dx,dy);
    } else
#endif
        return 0;
}

/**
 * Draw anything in adjacent squares that could smooth on given square
 *
 * @param mx
 * @param my Square to smooth on.
 * You should not call this function to smooth on a 'completely black' square.
 * @param layer Layer to examine (we smooth only one layer at a time)
 * @param picx
 * @param picy Place on the map_drawing_area->window to draw
 */
static void drawsmooth(cairo_t *cr, int mx, int my, int layer, int picx, int picy) {
    static int dx[8]= {0,1,1,1,0,-1,-1,-1};
    static int dy[8]= {-1,-1,0,1,1,1,0,-1};
    static int bweights[8]= {2,0,4,0,8,0,1,0};
    static int cweights[8]= {0,2,0,4,0,8,0,1};
    static int bc_exclude[8]= {
        1+2,/*north exclude northwest (bit0) and northeast(bit1)*/
        0,
        2+4,/*east exclude northeast and southeast*/
        0,
        4+8,/*and so on*/
        0,
        8+1,
        0
    };
    int partdone[8]= {0,0,0,0,0,0,0,0};
    int slevels[8];
    int sfaces[8];
    int i,weight,weightC;
    int emx,emy;
    int smoothface;
    int hasFace = 0;
    for (i=0; i<=layer; i++) {
        hasFace |= mapdata_cell(mx, my)->heads[i].face;
    }
    if (!hasFace || !mapdata_can_smooth(mx, my, layer)) {
        return;
    }
    for (i=0; i<8; i++) {
        emx=mx+dx[i];
        emy=my+dy[i];
        if (!mapdata_contains(emx, emy)) {
            slevels[i]=0;
            sfaces[i]=0; /*black picture*/
        } else if (mapdata_cell(emx, emy)->smooth[layer] <= mapdata_cell(mx, my)->smooth[layer]) {
            slevels[i]=0;
            sfaces[i]=0; /*black picture*/
        } else {
            slevels[i]=mapdata_cell(emx, emy)->smooth[layer];
            sfaces[i]=pixmaps[mapdata_cell(emx, emy)->heads[layer].face]->smooth_face;
        }
    }
    /*
     * Now we have a list of smoothlevel higher than current square.  There are
     * at most 8 different levels. so... check 8 times for the lowest one (we
     * draw from bottom to top!).
     */
    while (1) {
        int lowest = -1;
        for (i=0; i<8; i++) {
            if ( (slevels[i]>0) && (!partdone[i]) &&
                    ((lowest<0) || (slevels[i]<slevels[lowest]))
               ) {
                lowest=i;
            }
        }
        if (lowest<0) {
            break;    /*no more smooth to do on this square*/
        }
        /*printf ("hey, must smooth something...%d\n",sfaces[lowest]);*/
        /* Here we know 'what' to smooth
         *
         * Calculate the weight for border and weight for corners.  Then
         * 'markdone' the corresponding squares
         *
         * First, the border, which may exclude some corners
         */
        weight=0;
        weightC=15; /*works in backward. remove where there is nothing*/
        /*for (i=0;i<8;i++)
            cornermask[i]=1;*/
        for (i=0; i<8; i++) { /*check all nearby squares*/
            if ( (slevels[i]==slevels[lowest]) &&
                    (sfaces[i]==sfaces[lowest])) {
                partdone[i]=1;
                weight=weight+bweights[i];
                weightC&=~bc_exclude[i];
            } else {
                /*must rmove the weight of a corner if not in smoothing*/
                weightC&=~cweights[i];
            }
        }
        /*We can't do this before since we need the partdone to be adjusted*/
        if (sfaces[lowest]<=0) {
            continue;    /*Can't smooth black*/
        }
        smoothface=sfaces[lowest];
        if (smoothface<=0) {
            continue;  /*picture for smoothing not yet available*/
        }
        /*
         * now, it's quite easy. We must draw using a 32x32 part of the picture
         * smoothface.  This part is located using the 2 weights calculated:
         * (32*weight,0) and (32*weightC,32)
         */
        if ( (!pixmaps[smoothface]->map_image) ||
                (pixmaps[smoothface] == pixmaps[0])) {
            continue;    /*don't have the picture associated*/
        }

        // @todo Fix smoothing here.
        if (weight > 0) {
            draw_pixmap(cr, pixmaps[smoothface], picx, picy);

            /*
            draw_pixmap(
                weight*map_image_size, 0,
                picx, picy,
                picx-weight*map_image_size, picy,
                pixmaps[smoothface]->map_mask, pixmaps[smoothface]->map_image, map_image_size, map_image_size);
            */
        }

        if (weightC > 0) {
            draw_pixmap(cr, pixmaps[smoothface], picx, picy);

            /*
            draw_pixmap(
                weightC*map_image_size, map_image_size,
                picx, picy,
                picx-weightC*map_image_size, picy+map_image_size,
                pixmaps[smoothface]->map_mask, pixmaps[smoothface]->map_image, map_image_size, map_image_size);
            */
        }
    }
}

/**
 * Draw a single map layer to the given cairo context.
 */
static void map_draw_layer(cairo_t *cr, int layer) {
    for (int x = 0; x < use_config[CONFIG_MAPWIDTH]; x++) {
        for (int y = 0; y < use_config[CONFIG_MAPHEIGHT]; y++) {
            // Translate on-screen coordinates to virtual map coordinates.
            int mx = pl_pos.x + x, my = pl_pos.y + y;

            // Skip current cell if not visible and not using fog of war.
            if (!use_config[CONFIG_FOGWAR] && mapdata_cell(mx, my)->cleared) {
                continue;
            }

            int dx, dy, face = mapdata_face_info(mx, my, layer, &dx, &dy);
            if (face > 0 && pixmaps[face]->map_image != NULL) {
                draw_pixmap(cr, pixmaps[face], x + dx, y + dy);
            }
            /*
            * Sometimes, it may happens we need to draw the smooth while there
            * is nothing to draw at that layer (but there was something at
            * lower layers). This is handled here. The else part is to take
            * into account cases where the smooth as already been handled 2
            * code lines before
            */
            if (use_config[CONFIG_SMOOTH]) {
                drawsmooth(cr, mx, my, layer, x * map_image_size, y * map_image_size);
            }
        }
    }
}

/**
 * Draw darkness layer to a location on screen.
 */
static void mapcell_draw_darkness(cairo_t *cr, int ax, int ay, int mx, int my) {
    cairo_rectangle(cr, ax * map_image_size, ay * map_image_size,
            map_image_size, map_image_size);

    double opacity = mapdata_cell(mx, my)->darkness / 192.0 * 0.6;

    if (use_config[CONFIG_FOGWAR] && mapdata_cell(mx, my)->cleared) {
        opacity += 0.15;
    }

    cairo_set_source_rgba(cr, 0, 0, 0, opacity);
    cairo_fill(cr);
}

/**
 * Redraw the entire map using GTK.
 */
static void gtk_map_redraw(gboolean redraw) {
    if (!redraw && !map_updated) {
        return;
    }

    int width = map_drawing_area->allocation.width;
    int height = map_drawing_area->allocation.height;

    // Create double buffer and associated graphics context.
    cairo_surface_t *cst =
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(cst);

    // Blank graphics context with a solid black background.
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    for (int layer = 0; layer < MAXLAYERS; layer++) {
        map_draw_layer(cr, layer);
    }

    for (int x = 0; x < use_config[CONFIG_MAPWIDTH]; x++) {
        for (int y = 0; y < use_config[CONFIG_MAPHEIGHT]; y++) {
            // Determine the 'virtual' map coordinates.
            int mx = pl_pos.x + x, my = pl_pos.y + y;
            mapcell_draw_darkness(cr, x, y, mx, my);
            mapdata_cell(mx, my)->need_update = 0;
            mapdata_cell(mx, my)->need_resmooth = 0;
        }
    }

    cairo_destroy(cr);

    // Copy the double buffer on the map drawing area.
    cairo_t *map_cr = gdk_cairo_create(map_drawing_area->window);
    cairo_set_source_surface(map_cr, cst, 0, 0);
    cairo_paint(map_cr);
    cairo_destroy(map_cr);

    cairo_surface_destroy(cst);
}

/**
 * The player has changed maps, so any info we have (for fog of war) is bogus,
 * so clear out all that old info.
 */
void display_map_newmap(void)
{
    reset_map();
}

/**
 * Resize_map_window is a NOOP for the time being - not sure if it will in fact
 * need to do something, since there are scrollbars for the map window now.
 * Note - this is note a window resize request, but rather process the size
 * (in spaces) of the map - is received from server.
 */
void resize_map_window(int x, int y)
{
    /* We do an implicit clear, since after a resize, there may be some
     * left over pixels at the edge which will not get drawn on by map spaces.
     */
    gdk_window_clear(map_drawing_area->window);
    draw_map(TRUE);
}

/**
 * Draw the map window using the appropriate backend.
 * @param redraw If true, the entire screen must be redrawn.
 */
void draw_map(gboolean redraw) {
    gint64 t_start, t_end;

    if (time_map_redraw) {
        t_start = g_get_monotonic_time();
    }

    switch (use_config[CONFIG_DISPLAYMODE]) {
#ifdef HAVE_SDL
    case CFG_DM_SDL:
        sdl_gen_map(redraw);
        break;
#endif

#ifdef HAVE_OPENGL
    case CFG_DM_OPENGL:
        opengl_gen_map(redraw);
        break;
#endif

    default:
        gtk_map_redraw(redraw);
        break;
    }

    if (time_map_redraw) {
        t_end = g_get_monotonic_time();
        gint64 elapsed = t_end - t_start;
        printf("%"G_GINT64_FORMAT"\n", elapsed);
    }
}

static gboolean map_expose_event(GtkWidget *widget, GdkEventExpose *event,
        gpointer user_data) {
    draw_map(TRUE);
    return FALSE;
}

/**
 * Given a relative tile coordinate, determine its compass direction.
 * @param dx Relative 'x' coordinate
 * @param dy Relative 'y' coordinate
 * @return 0 if x and y are both zero, 1-8 for each compass direction
 */
static int relative_direction(int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return 0;
    } else if (dx == 0 && dy < 0) {
        return 1;
    } else if (dx > 0 && dy < 0) {
        return 2;
    } else if (dx > 0 && dy == 0) {
        return 3;
    } else if (dx > 0 && dy > 0) {
        return 4;
    } else if (dx == 0 && dy > 0) {
        return 5;
    } else if (dx < 0 && dy > 0) {
        return 6;
    } else if (dx < 0 && dy == 0) {
        return 7;
    } else if (dx < 0 && dy < 0) {
        return 8;
    } else {
        g_assert_not_reached();
    }
}

/**
 * Handle a mouse event in the drawing area.
 */
static gboolean map_button_event(GtkWidget *widget,
        GdkEventButton *event, gpointer user_data) {
    // Determine the tile of the mouse event, relative to the player.
    int dx = ((int)event->x - 2) / map_image_size - (use_config[CONFIG_MAPWIDTH] / 2);
    int dy = ((int)event->y - 2) / map_image_size - (use_config[CONFIG_MAPHEIGHT] / 2);
    int dir = relative_direction(dx, dy);

    switch (event->button) {
        case 1:
            if (event->type == GDK_BUTTON_PRESS) {
                look_at(dx,dy);
            }
            break;
        case 2:
            if (event->type == GDK_BUTTON_RELEASE) {
                clear_fire();
            } else {
                fire_dir(dir);
            }
            break;
        case 3:
            if (event->type == GDK_BUTTON_RELEASE) {
                stop_run();
            } else {
                run_dir(dir);
            }
            break;
    }

    return FALSE;
}

/**
 * This isn't used - it is basically a prequel - we know we got a map command
 * from the server, but have digested it all yet.  This can be useful if there
 * is info we know we need to store away or the like before it is destroyed,
 * but there isn't anything like that for the gtk client.
 */
void display_map_startupdate(void)
{
}

/**
 * This is called after the map has been all digested.  this should perhaps be
 * removed, and left to being done from from the main event loop.
 *
 * @param redraw If set, force redraw of all tiles.
 * @param notice If set, another call will follow soon.
 */
void display_map_doneupdate(int redraw, int notice)
{
    map_updated |= redraw || !notice;
}
