/**
 * @file
 */

#include <stdbool.h>

#ifndef MAP_H
#define MAP_H

/** The protocol supports 10 layers, so set MAXLAYERS accordingly.
 */
#define MAXLAYERS 10

/**
 * Maximum size of view area a server could support.
 */
#define MAX_VIEW 64

/* Map1 only used 3 layers.  Trying to use 10 seems to cause
 * problems for that code.
 */
#define MAP1_LAYERS 3

struct MapCellLayer {
    gint16 face;
    gint8 size_x;
    gint8 size_y;

    /* Link into animation information.
     * animation is provided to us from the server in the map2 command.
     * animation_speed is also provided.
     * animation_left is how many ticks until animation changes - generated
     *  by client.
     * animation_phase is current phase.
     */
    gint16  animation;
    guint8   animation_speed;
    guint8   animation_left;
    guint8   animation_phase;
};

/** The heads[] in the mapcell is used for single part objects
 * or the head piece for multipart.  The easiest way to think about
 * it is that the heads[] contains the map information as specifically
 * sent from the server.  For the heads value, the size_x and size_y
 * represent how many spaces (up and to the left) that image extends
 * into.
 * The tails are values that the client fills in - if we get
 * a big head value, we fill in the tails value so that the display
 * logic can easily redraw one space.  In this case, the size_ values
 * are offsets that point to the head.  In this way, the draw logic
 * can look at the size of the image, look at these values, and
 * know what portion of it to draw.
 */
struct MapCell
{
    struct MapCellLayer heads[MAXLAYERS];
    struct MapCellLayer tails[MAXLAYERS];
    guint16 smooth[MAXLAYERS];
    guint8 darkness;         /* darkness: 0=fully illuminated, 255=pitch black */
    guint8 need_update:1;    /* set if tile should be redrawn */
    guint8 have_darkness:1;  /* set if darkness information was set */
    guint8 need_resmooth:1;  /* same has need update but for smoothing only */
    guint8 cleared:1;        /* If set, this is a fog cell. */
};

struct Map
{
    /* Store size of map so we know if map_size has changed
     * since the last time we allocated this;
     */
    int x;
    int y;

    struct MapCell **cells;
};

struct MapCell *mapdata_cell(int x, int y);
bool mapdata_contains(int x, int y);
void mapdata_size(int *x, int *y);
bool mapdata_can_smooth(int x, int y, int layer);

/**
 * Initializes the module. Allocates memory for the_map. This functions must be
 * called before any other function is used.
 */
void mapdata_init(void);

/**
 * Sets the current display size. Must be called whenever a new display size
 * was negotiated with the server.
 */
void mapdata_set_size(int viewx, int viewy);

/**
 * Scrolls the map view. Must be called whenever a map_scroll command was
 * received from the server.
 */
void mapdata_scroll(int dx, int dy);

/**
 * Clears the map view. Must be called whenever a newmap command was received
 * from the server.
 */
void mapdata_newmap(void);

/**
 * Checks whether the given coordinates are within the current display size (as
 * set by mapdata_set_size).
 */
int mapdata_is_inside(int x, int y);

/**
 * Returns the face to display at a given location. This function returns the
 * "head" information, i.e. the face information sent by the server.
 */
gint16 mapdata_face(int x, int y, int layer);

/**
 * Returns the face to display at a given location. This function returns the
 * "tail" information, i.e. big faces expanded by the client.
 *
 * *ww and *hh return the offset of the current tile relative to the head;
 * 0 <= *ww < (width of face), 0 <= *hh < (height of face).
 *
 * When drawing the map view, this function must be used instead than a direct
 * access to the_map.cells[]. This is because the_map.cells[] eventually still
 * contains obsolete (fog of war) big face information; this function detects
 * and clears such faces.
 */
gint16 mapdata_bigface(int x, int y, int layer, int *ww, int *hh);

void mapdata_clear_space(int x, int y);
void mapdata_set_check_space(int x, int y);
void mapdata_set_darkness(int x, int y, int darkness);
void mapdata_set_smooth(int x, int y, int smooth, int layer);
void mapdata_clear_old(int x, int y);
void mapdata_set_face_layer(int x, int y, gint16 face, int layer);
void mapdata_set_anim_layer(int x, int y, guint16 anim, guint8 anim_speed, int layer);
gint16 mapdata_bigface_head(int x, int y, int layer, int *ww, int *hh);
void mapdata_animation(void);

#endif
