/*
 * static char *rcsid_external_h =
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
extern void display_map_clearcell(long x, long y);
extern void display_map_addbelow(long x, long y, long face);
extern void display_map_startupdate(void);
extern void display_map_doneupdate(int redraw);
extern void set_map_darkness(int x, int y, uint8 darkness);
extern void set_map_face(int x, int y, int layer, int face);
extern void display_mapscroll(int dx, int dy);
extern void draw_magic_map(void);

/* Info related functions */
extern void draw_info(const char *str, int color);
extern void draw_color_info(int colr, const char *buf);
extern void draw_prompt(const char *str);
extern void x_set_echo(void);
extern void set_scroll(char *s);

/* Stats related commands */
extern void draw_stats(int redraw);
extern void draw_message_window(int redraw);
/* this should really just set a field in the stats, and let the
 * client figure the new weight limit out
 */
extern void set_weight_limit(uint32 wlim);

/* Image related functions */
extern int display_willcache(void);
extern void finish_face_cmd(int pnum, uint32 checksum, int has_sum, char *face);
extern void display_newpng(long face, char *buf, long buflen);

/* Item related commands */
extern void open_container(item *op);
extern void close_container(item *op);
extern void set_show_icon(char *s);
extern void set_show_weight(char *s);

/* Keybinding relatated commands - this probably should not be a callback */
extern void bind_key(char *params);
extern void unbind_key(char *params);

/* Misc commands */
extern void save_winpos(void);
extern void save_defaults(void);
extern void command_show(char *params);
