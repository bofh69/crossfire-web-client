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

#include <math.h>
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

PlayerPosition pl_pos;

extern int predict_alpha;

int map_image_size = DEFAULT_IMAGE_SIZE;
int map_image_half_size = DEFAULT_IMAGE_SIZE / 2;

static GtkWidget *map_drawing_area;

GtkWidget *map_notebook;

/*
 * This should really be one of the CONFIG values, or perhaps a checkbox
 * someplace that displays frame rate.
 */
bool time_map_redraw = false;

/**
 * Calculate and set desired map size based on map window size.
 */
void map_check_resize() {
    if (!GTK_IS_WIDGET(map_drawing_area)) {
        // Called by config_check(), but main window layout not yet loaded.
        return;
    }

    GtkAllocation size;
    gtk_widget_get_allocation(map_drawing_area, &size);
    int scaled_size = map_image_size * use_config[CONFIG_MAPSCALE] / 100;
    int w = size.width / scaled_size + 1;
    int h = size.height / scaled_size + 1;
    w = (w > MAP_MAX_SIZE) ? MAP_MAX_SIZE : w;
    h = (h > MAP_MAX_SIZE) ? MAP_MAX_SIZE : h;

    // If request would be even, make it odd so player is centered.
    if (w % 2 == 0) {
        w += 1;
    }

    if (h % 2 == 0) {
        h += 1;
    }

    if (w != want_config[CONFIG_MAPWIDTH] || h != want_config[CONFIG_MAPHEIGHT]) {
        want_config[CONFIG_MAPWIDTH] = w;
        want_config[CONFIG_MAPHEIGHT] = h;
        if (csocket.fd) {
            client_mapsize(w, h);
        }
    }
}

/**
 * This initializes the stuff we need for the map.
 *
 * @param window_root The client's main playing window.
 */
void map_init(GtkWidget *window_root) {
    static gulong map_button_handler = 0;
    map_drawing_area = GTK_WIDGET(gtk_builder_get_object(
                window_xml, "drawingarea_map"));
    map_notebook = GTK_WIDGET(gtk_builder_get_object(
                window_xml, "map_notebook"));

    g_signal_connect(map_drawing_area, "configure_event",
            G_CALLBACK(map_check_resize), NULL);
    g_signal_connect(map_drawing_area, "expose_event",
            G_CALLBACK(map_expose_event), NULL);

    // Enable event masks and set callbacks to handle mouse events.
    // If its already connected (e.g. on a second login), then skip
    // an additional association.
    if (!g_signal_handler_is_connected(map_drawing_area, map_button_handler)) {
        gtk_widget_add_events(map_drawing_area,
            GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
        map_button_handler = g_signal_connect(map_drawing_area, "event",
            G_CALLBACK(map_button_event), NULL);
    }

    // Set our image sizes.
    // IIRC, atoi stops at the first nonnumeric char, so the x in the size will be the end.
    if (face_info.facesets[face_info.faceset].size != NULL) {
        image_size = atoi(face_info.facesets[face_info.faceset].size);
        map_image_size  = image_size; // These should be the same.
        map_image_half_size = map_image_size / 2;
    } else {
        LOG(LOG_ERROR, "map_init", "Invalid faceset size from server");
    }
    // If we are not on the default size, we need to resize pixmaps[0].
    if (map_image_size != DEFAULT_IMAGE_SIZE) {
        int nx = map_image_size, ny = map_image_size;
        guint8 *png_tmp = rescale_rgba_data(pixmaps[0]->map_image, &nx, &ny, use_config[CONFIG_MAPSCALE]);
        // Try to affect pixmap[0] in-place, since it is referenced extensively.
        pixmaps[0]->icon_width = nx;
        pixmaps[0]->icon_height = ny;
        pixmaps[0]->full_icon_width = nx;
        pixmaps[0]->full_icon_height = ny;
        do_new_image(png_tmp, pixmaps[0]);
    }

    // Set map size based on window size and show widget.
    map_check_resize();
    gtk_widget_show(map_drawing_area);
}

/**
 * Draw a pixmap to the given map tile on screen.
 * @param ax Map cell on-screen x-coordinate
 * @param ay Map cell on-screen y-coordinate
 */
static void draw_pixmap(cairo_t *cr, PixmapInfo *pixmap, int ax, int ay, int off_x, int off_y) {
    const int dest_x = ax * map_image_size;
    const int dest_y = ay * map_image_size;
    cairo_set_source_surface(cr, pixmap->map_image, dest_x + global_offset_x + off_x, dest_y + global_offset_y + off_y);
    cairo_paint(cr);
}

static void draw_smooth_pixmap(cairo_t* cr, PixmapInfo* pixmap,
        const int sx, const int sy, const int dx, const int dy) {
    const int src_x = map_image_size * sx;
    const int src_y = map_image_size * sy;
    const int dest_x = map_image_size * dx + global_offset_x;
    const int dest_y = map_image_size * dy + global_offset_y;
    cairo_set_source_surface(cr, pixmap->map_image, dest_x - src_x, dest_y - src_y);
    cairo_rectangle(cr, dest_x, dest_y, map_image_size, map_image_size);
    cairo_fill(cr);
}

int display_mapscroll(int dx, int dy) {
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

        if (weight > 0) {
            draw_smooth_pixmap(cr, pixmaps[smoothface], weight, 0, picx, picy);
        }

        if (weightC > 0) {
            draw_smooth_pixmap(cr, pixmaps[smoothface], weightC, 1, picx, picy);
        }
    }
}

/**
 * Draw a single map layer to the given cairo context.
 */
static void map_draw_layer(cairo_t *cr, int layer, int pass, int mx_start, int nx, int my_start, int ny, int off_x, int off_y) {
    for (int x = 0; x <= nx; x++) {
        for (int y = 0; y <= ny; y++) {
            // Translate on-screen coordinates to virtual map coordinates.
            const int mx = mx_start + x;
            const int my = my_start + y;

            // Skip current cell if not visible and not using fog of war.
            if (!use_config[CONFIG_FOGWAR] && mapdata_cell(mx, my)->cleared) {
                continue;
            }

            if (pass == 0) {
                int dx, dy, face = mapdata_face_info(mx, my, layer, &dx, &dy);
                if (face > 0 && pixmaps[face]->map_image != NULL) {
                    draw_pixmap(cr, pixmaps[face], x + dx, y + dy, off_x, off_y);
                }
            } else if (pass == 1) {
                if (use_config[CONFIG_SMOOTH]) {
                    drawsmooth(cr, mx, my, layer, x + off_x, y + off_y);
                }
            }
        }
    }
}

/**
 * Draw darkness layer to a location on screen.
 */
static void mapcell_draw_darkness(cairo_t *cr, int ax, int ay, int mx, int my) {
    cairo_rectangle(cr, ax * map_image_size + global_offset_x, ay * map_image_size + global_offset_y,
            map_image_size, map_image_size);

    double opacity = mapdata_cell(mx, my)->darkness / 192.0 * 0.6;

    if (use_config[CONFIG_FOGWAR] && mapdata_cell(mx, my)->cleared) {
        opacity += 0.2;
    }

    cairo_set_source_rgba(cr, 0, 0, 0, opacity);
    cairo_fill(cr);
}

/**
 * Redraw the entire map using GTK.
 */
static void gtk_map_redraw() {
    if (!map_updated) {
        return;
    }

    GtkAllocation size;
    gtk_widget_get_allocation(map_drawing_area, &size);

    // Effective dimensions in pixels, i.e. after adjusting for map scale
    float scale = use_config[CONFIG_MAPSCALE]/100.0;
    const double ew = size.width / scale;
    const double eh = size.height / scale;

    // Number of tiles to show in x and y dimensions
    const int nx = (int)ceilf(ew / map_image_size);
    const int ny = (int)ceilf(eh / map_image_size);

    // Current viewport dimensions as sent by server, in squares
    const int vw = use_config[CONFIG_MAPWIDTH];
    const int vh = use_config[CONFIG_MAPHEIGHT];

    // The server always centers the player in the viewport. However, if our
    // drawing area shows more tiles than the viewport, then the player is
    // no longer centered. Correct that here.
    const int mx_start = (nx > vw) ? pl_pos.x - (nx - vw)/2 : pl_pos.x;
    const int my_start = (ny > vh) ? pl_pos.y - (ny - vh)/2 : pl_pos.y;

    // Create double buffer and associated graphics context.
    cairo_surface_t *cst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ew, eh);
    cairo_t *cr = cairo_create(cst);

    // Blank graphics context with a solid black background.
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, ew, eh);
    cairo_fill(cr);

    for (int layer = 0; layer < MAXLAYERS; layer++) {
        map_draw_layer(cr, layer, 0, mx_start, nx, my_start, ny, 0, 0);
        map_draw_layer(cr, layer, 1, mx_start, nx, my_start, ny, 0, 0);
    }

    for (int x = 0; x <= nx; x++) {
        for (int y = 0; y <= ny; y++) {
            // Translate on-screen coordinates to virtual map coordinates.
            const int mx = mx_start + x;
            const int my = my_start + y;

            mapcell_draw_darkness(cr, x, y, mx, my);
            mapdata_cell(mx, my)->need_update = 0;
            mapdata_cell(mx, my)->need_resmooth = 0;
        }
    }

    cairo_destroy(cr);

    // Copy the double buffer on the map drawing area.
    cairo_t *map_cr = gdk_cairo_create(gtk_widget_get_window(map_drawing_area));
    if (use_config[CONFIG_MAPSCALE] != 100) {
        cairo_scale(map_cr, scale, scale);
    }
    cairo_set_source_surface(map_cr, cst, 0, 0);
    if (use_config[CONFIG_MAPSCALE] > 100) {
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    }
    cairo_paint(map_cr);
    cairo_destroy(map_cr);

    cairo_surface_destroy(cst);
}

/**
 * Resize_map_window is a NOOP for the time being - not sure if it will in fact
 * need to do something, since there are scrollbars for the map window now.
 * Note - this is note a window resize request, but rather process the size
 * (in spaces) of the map - is received from server.
 */
void resize_map_window(int x, int y) {
}

static void update_global_offset() {
    int dx, dy;
    dx = ((want_offset_x*map_image_size) - global_offset_x)*predict_alpha/100.0;
    dy = ((want_offset_y*map_image_size) - global_offset_y)*predict_alpha/100.0;
    global_offset_x += dx;
    global_offset_y += dy;

    // Snap global offset to zero when we're at the right square.
    if (want_offset_x == 0 && want_offset_y == 0) {
        global_offset_x = 0;
        global_offset_y = 0;
    }
}

/**
 * Draw the map window using the appropriate backend.
 */
void draw_map() {
    gint64 t_start, t_end;
    t_start = g_get_monotonic_time();

    update_global_offset();
    gtk_map_redraw();

    t_end = g_get_monotonic_time();
    gint64 elapsed = t_end - t_start;
    if (time_map_redraw) {
        printf("profile/redraw,%"G_GINT64_FORMAT"\n", elapsed);
    }

    const unsigned int target_redraw = 100000;
    if (elapsed > target_redraw) {
        LOG(LOG_DEBUG, "draw_map", "Increasing mapscale to %d to reduce draw time below %u us",
                use_config[CONFIG_MAPSCALE], target_redraw);
        use_config[CONFIG_MAPSCALE] += 5;
        map_check_resize();
    }
}

static gboolean map_expose_event(GtkWidget *widget, GdkEventExpose *event,
        gpointer user_data) {
    draw_map();
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
    const float tile_size = map_image_size * use_config[CONFIG_MAPSCALE]/100.0;
    int dx = ((int)event->x - 2) / tile_size - (use_config[CONFIG_MAPWIDTH] / 2);
    int dy = ((int)event->y - 2) / tile_size - (use_config[CONFIG_MAPHEIGHT] / 2);
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
 * This is called after the map has been all digested.  this should perhaps be
 * removed, and left to being done from from the main event loop.
 *
 * @param redraw If set, force redraw of all tiles.
 * @param notice If set, another call will follow soon.
 */
void display_map_doneupdate(int redraw, int notice) {
    map_updated |= redraw || !notice;
}
