/*
 * static char *rcsid_external_h =
 *   "$Id$";
 */
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001,2006 Mark Wedel & Crossfire Development Team

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

/* This file contains external calls that the common area makes
 * callbacks to.
 * This was really a quick hack done to allow some seperation.
 * Really, these should be set via callbacks that the client
 * can make to the library.  Many of these probably should
 * never really be callbacks in any case, or be more general
 */

/* Sound functions */
extern void SoundCmd(unsigned char *data,  int len);

/* Map window related functions */
extern void resize_map_window(int x, int y);
extern void display_map_newmap(void);
extern void display_map_addbelow(long x, long y, long face);
extern void display_map_startupdate(void);
extern void display_map_doneupdate(int redraw, int notice);
extern int display_mapscroll(int dx, int dy);
extern void draw_magic_map(void);

/* Info related functions */
extern void draw_info(const char *str, int color);
extern void draw_color_info(int colr, const char *buf);
extern void draw_prompt(const char *str);
extern void x_set_echo(void);
extern void set_scroll(const char *s);
extern void set_autorepeat(const char *s);
extern void menu_clear(void);
extern int get_info_width(void);

/* Stats related commands */
extern void draw_stats(int redraw);
extern void draw_message_window(int redraw);

/* this should really just set a field in the stats, and let the
 * client figure the new weight limit out
 */
extern void set_weight_limit(uint32 wlim);

/* Image related functions */
extern int display_willcache(void);
extern int create_and_rescale_image_from_data(Cache_Entry *ce, int pixmap_num, uint8 *rgba_data, int width, int height);
extern uint8 *png_to_data(uint8 *data, int len, uint32 *width, uint32 *height);
extern int associate_cache_entry(Cache_Entry *ce, int pixnum);
extern void image_update_download_status(int start, int end, int total);
extern void get_map_image_size(int face, uint8 *w, uint8 *h);
extern void addsmooth(uint16 face, uint16 smooth_face);

/* Item related commands */
extern void open_container(item *op);
extern void close_container(item *op);
extern void set_show_icon(const char *s);
extern void set_show_weight(const char *s);

/* Keybinding relatated commands - this probably should not be a callback */
extern void bind_key(const char *params);
extern void unbind_key(const char *params);

/* Misc commands */
extern void save_winpos(void);
extern void save_defaults(void);
extern void command_show(const char *params);
extern void client_tick(uint32 tick);
extern void cleanup_connection();
