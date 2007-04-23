/*
 * static char *rcsid_item_h =
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

#ifndef ITEM_H
#define ITEM_H

/*
 *  Use static buffer for object names. Item names are changing so
 *  often that mallocing them it just a waste of time. Also there is
 *  probably some upper limits for names that client can show, Note
 *  that total number of items is small (<100) so this don't even
 *  waste too much memory
 */
#define NAME_LEN	128
#define copy_name(t,f) strncpy(t, f, NAME_LEN-1); t[NAME_LEN-1]=0;

#define NO_ITEM_TYPE		30000
/*
 *  item structure keeps all information what player 
 *  (= client) knows about items in its inventory
 */
typedef struct item_struct {
    struct item_struct *next;	/* next item in inventory */
    struct item_struct *prev;	/* previous item in inventory */
    struct item_struct *env;	/* which items inventory is this item */
    struct item_struct *inv;	/* items inventory */
    char d_name[NAME_LEN];	/* item's full name w/o status information */
    char s_name[NAME_LEN];	/* item's singular name as sent to us */
    char p_name[NAME_LEN];	/* item's plural name as sent to us */
    char flags[NAME_LEN];	/* item's status information */
    sint32 tag;			/* item identifier (0 = free) */
    uint32 nrof;		/* number of items */
    float weight;		/* how much item weights */
    sint16 face;		/* index for face array */
    uint16 animation_id;	/* Index into animation array */
    uint8 anim_speed;		/* how often to animate */
    uint8 anim_state;		/* last face in sequence drawn */
    uint16 last_anim;		/* how many ticks have passed since we last animated */
    uint16 magical:1;		/* item is magical */
    uint16 cursed:1;		/* item is cursed */
    uint16 damned:1;		/* item is damned */
    uint16 unpaid:1;		/* item is unpaid */
    uint16 locked:1;		/* item is locked */
    uint16 applied:1;		/* item is applied */
    uint16 open:1;		/* container is open */
    uint16 was_open:1;		/* container was open */
    uint16 inv_updated:1;	/* item's inventory is updated, this is set
				   when item's inventory is modified, draw 
				   routines can use this to redraw things */
    uint8 apply_type;		/* how item is applied (worn/wield/etc) */
    uint32 flagsval;		/* unmodified flags value as sent from the server*/
    uint16   type;		/* Item type for ordering */
} item;

/* Toolkits implement these. */
extern void item_event_item_deleting(item * it);
extern void item_event_container_clearing(item * container);
/* TODO More fine-grained event - but how to handle it? */
extern void item_event_item_changed(item * it);
extern int can_write_spell_on(item* it);

#endif /* ITEM_H */
