const char *rcsid_common_item_c =
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


#ifdef WIN32
#include <windows.h>
#endif
#include <ctype.h>	/* needed for isdigit */
#include <client.h>
#include <item.h>
#include <newclient.h>
#include <external.h>
#include <script.h>

static item *free_items;	/* the list of free (unused) items */
static item *player, *map;	/* these lists contains rest of items */
				/* player = pl->ob, map = pl->below */

#define NROF_ITEMS 50		/* how many items are reserved initially */
				/* for the item spool */



#include <item-types.h>
/* This should be modified to read the definition from a file */
void init_item_types(void)
{
}



/* This uses the item_types table above.  We try to figure out if
 * name has a match above.  Matching is done pretty loosely - however
 * we try to match the start of the name because that is more reliable.
 * We return the 'type' (matching array element above), 255 if no match
 * (so unknown objects put at the end)
 */

uint8 get_type_from_name(const char *name)
{
    int type, pos;

    for (type=0; type < NUM_ITEM_TYPES; type++) {
	pos=0;
	while (item_types[type][pos]!=NULL) {
	    /* Only search at start of line */
	    if (item_types[type][pos][0]=='^') {
		if (!strncasecmp(name, item_types[type][pos]+1, strlen( item_types[type][pos]+1)))
		    return type;
	    }
	    /* String anywhere in name */
	    else if (strstr(name, item_types[type][pos])!=NULL) {
#if 0
		fprintf(stderr,"Returning type %d for %s\n", type, name);
#endif
		return type;
	    }
	    pos++;
	}
    }
    LOG(LOG_WARNING,"common::get_type_from_name","Could not find match for %s", name);
    return 255;
}


/* Does what is says - inserts newitem before the object.
 * the parameters can not be null
 */
static void insert_item_before_item(item *newitem, item *before)
{

    if (before->prev) {
	before->prev->next = newitem;
    } else {
	newitem->env->inv = newitem;
    }

    newitem->prev = before->prev;

    before->prev = newitem;
    newitem->next = before;

    if (newitem->env) 
	newitem->env->inv_updated = 1;
}


/* Item it has gotten an item type, so we need to resort its location */

void update_item_sort(item *it)
{
    item *itmp, *last=NULL;

    /* If not in some environment or the map, return */
    /* Sorting on the map doesn't work.  In theory, it would be nice,
     * but the server really must know the map order for things to
     * work.
     */
    if (!it->env || it->env==it || it->env==map) return;

    /* If we are already sorted properly, don't do anything further.
     * this is prevents the order of the inventory from changing around
     * if you just equip something.
     */

    if (it->prev && it->prev->type == it->type &&
	it->prev->locked == it->locked && 
	!strcasecmp(it->prev->s_name, it->s_name)) return;

    if (it->next && it->next->type == it->type &&
	it->next->locked == it->locked && 
	!strcasecmp(it->next->s_name, it->s_name)) return;

    /* Remove this item from the list */
    if (it->prev)	it->prev->next = it->next;
    if (it->next)	it->next->prev = it->prev;
    if (it->env->inv==it)   it->env->inv = it->next;

    for (itmp = it->env->inv; itmp!=NULL; itmp=itmp->next) {
	last = itmp;

	/* If the next item is higher in the order, insert here */
	if (itmp->type > it->type) {
	    insert_item_before_item(it, itmp);
	    return;
	}
	else if (itmp->type == it->type) {
#if 0
	    /* This could be a nice idea, but doesn't work very well if you
	     * have a few unidentified wands, as the position of a wand
	     * which you know the effect will move around as you equip others.
	     */
	    /* Hmm.  We can actually use the tag value of the items to reduce
	     * this a bit - do this by grouping, but if name is equal, then
	     * sort by tag.  Needs further investigation.
	     */

	    /* applied items go first */
	    if (itmp->applied) continue;
	    /* put locked items before others */
	    if (itmp->locked && !it->locked) continue;
#endif

	    /* Now alphabetise */
	    if (strcasecmp(itmp->s_name, it->s_name) < 0) continue;

	    /* IF we got here, it means it passed all our sorting tests */
	    insert_item_before_item(it, itmp);
	    return;
	}
    }
    /* No match - put it at the end */

    /* If there was a previous item, update pointer.  IF no previous
     * item, we need to update the environment to point to us */
    if (last) last->next = it;
    else 
	it->env->inv = it;

    it->prev = last;
    it->next = NULL;
}

/* Stolen from common/item.c */
/*
 * get_number(integer) returns the text-representation of the given number
 * in a static buffer.  The buffer might be overwritten at the next
 * call to get_number().
 * It is currently only used by the query_name() function.
 */

char *get_number(uint32 i) {
static char numbers[21][20] = {
  "no","a","two","three","four","five","six","seven","eight","nine","ten",
  "eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen",
  "eighteen","nineteen","twenty"
};
  static char buf[MAX_BUF];

  if(i<0)
  {
	  sprintf(buf,"negative");
	  return buf;
  }

  if(i<=20)
    return numbers[i];
  else {
    sprintf(buf,"%d",i);
    return buf;
  }
}


/*
 *  new_item() returns pointer to new item which
 *  is allocated and initialized correctly
 */
static item *new_item(void)
{
    item *op = malloc (sizeof(item));

    if (! op) 
	exit(0);

    op->next = op->prev = NULL;
    copy_name (op->d_name, "");
    copy_name (op->s_name, "");
    copy_name (op->p_name, "");
    op->inv = NULL;
    op->env = NULL;
    op->tag = 0;
    op->face = 0;
    op->weight = 0;
    op->magical = op->cursed = op->damned = 0;
    op->unpaid = op->locked = op->applied = 0;
    op->flagsval=0;
    op->animation_id=0;
    op->last_anim=0;
    op->anim_state=0;
    op->nrof=0;
    op->open=0;
    op->type=NO_ITEM_TYPE;
    op->inv_updated = 0;
    return op;
}

/*
 *  alloc_items() returns pointer to list of allocated objects
 */
static item *alloc_items (int nrof) {
    item *op, *list;
    int i;

    list = op = new_item();

    for (i=1; i<nrof; i++) {
	op->next = new_item();
	op->next->prev = op;
	op = op->next;
    }
    return list;
}

/*
 *  free_items() frees all allocated items from list
 */
void free_all_items (item *op) {
    item *tmp;

    while (op) {
	if (op->inv)
	    free_all_items (op->inv);  
	tmp = op->next;
	free(op);
	op = tmp;
    }
}

/*
 *  Recursive function, used by locate_item()
 */
static item *locate_item_from_item (item *op, sint32 tag)
{
    item *tmp;

    for (; op; op=op->next)
	if (op->tag == tag)
	    return op;
	else if (op->inv && (tmp = locate_item_from_item (op->inv, tag)))
	    return tmp;

    return NULL;
}

/*
 *  locate_item() returns pointer to the item which tag is given 
 *  as parameter or if item is not found returns NULL
 */
item *locate_item (sint32 tag)
{
    item *op;

    if (tag == 0)
	return map;

    if ((op=locate_item_from_item(map->inv, tag)) != NULL)
	return op;
    if ((op=locate_item_from_item(player, tag)) != NULL)
	return op;

    if (cpl.container && (cpl.container->tag == tag))
	return cpl.container;
    return NULL;
}

/*
 *  remove_item() inserts op the the list of free items
 *  Note that it don't clear all fields in item
 */
void remove_item (item *op)
{
    /* IF no op, or it is the player */
    if (!op || op==player || op==map) return;
    
    item_event_item_deleting(op);
    
    op->env->inv_updated = 1;

    /* Do we really want to do this? */
    if (op->inv && op != cpl.container)
	remove_item_inventory (op);

    if (op->prev) {
	op->prev->next = op->next;
    } else {
	op->env->inv = op->next;
    }
    if (op->next) {
	op->next->prev = op->prev;
    }

    if (cpl.container == op) return;	/* Don't free this! */

    /* add object to a list of free objects */
    op->next = free_items;
    if (op->next != NULL)
	op->next->prev = op;
    free_items = op;

    /* Clear all these values, since this item will get re-used */
    op->prev = NULL;
    op->env = NULL;
    op->tag = 0;
    copy_name (op->d_name, "");
    copy_name (op->s_name, "");
    copy_name (op->p_name, "");
    op->inv = NULL;
    op->env = NULL;
    op->tag = 0;
    op->face = 0;
    op->weight = 0;
    op->magical = op->cursed = op->damned = 0;
    op->unpaid = op->locked = op->applied = 0;
    op->flagsval=0;
    op->animation_id=0;
    op->last_anim=0;
    op->anim_state=0;
    op->nrof=0;
    op->open=0;
    op->type=NO_ITEM_TYPE;
}

/*
 *  remove_item_inventory() recursive frees items inventory
 */
void remove_item_inventory (item *op)
{
    if ( !op )
        return;
    
    item_event_container_clearing(op);
        
    op->inv_updated = 1;
    while (op->inv)
	remove_item (op->inv);
}

/*
 *  add_item() adds item op to end of the inventory of item env
 */
static void add_item (item *env, item *op) 
{
    item *tmp;
    
    for (tmp = env->inv; tmp && tmp->next; tmp=tmp->next)
	;

    op->next = NULL;
    op->prev = tmp;
    op->env = env;
    if (!tmp) {
	env->inv = op;
    } else {
	if (tmp->next)
	    tmp->next->prev = op;
	tmp->next = op;
    }
}

/*
 *  create_new_item() returns pointer to a new item, inserts it to env 
 *  and sets its tag field and clears locked flag (all other fields
 *  are unitialized and may contain random values)
 */
item *create_new_item (item *env, sint32 tag)
{
    item *op;

    if (!free_items)
	free_items = alloc_items (NROF_ITEMS);

    op = free_items;
    free_items = free_items->next;
    if (free_items)
	free_items->prev = NULL;

    op->tag = tag;
    op->locked = 0;
    if (env) add_item (env, op);
    return op;
}

int num_free_items()
{
    item *tmp;
    int count=0;

    for (tmp=free_items; tmp; tmp=tmp->next)
	count++;
    return count;
}

/*
 *  Hardcoded now, server could send these at initiation phase.
 */
static const char *const apply_string[] = {
    "", " (readied)", " (wielded)", " (worn)", " (active)", " (applied)"
};

static void set_flag_string (item *op)
{
    op->flags[0] = 0;

    if (op->locked) 
	strcat (op->flags, " *");
    if (op->apply_type) {
	if (op->apply_type < sizeof (apply_string) / sizeof(apply_string[0])) 
	    strcat (op->flags, apply_string[op->apply_type]);
	else 
	    strcat (op->flags, " (undefined)");
    }
    if (op->open)
	strcat (op->flags, " (open)");
    if (op->damned)
	strcat (op->flags, " (damned)");
    if (op->cursed)
	strcat (op->flags, " (cursed)");
    if (op->magical)
	strcat (op->flags, " (magic)");
    if (op->unpaid)
	strcat (op->flags, " (unpaid)");
}

static void get_flags (item *op, uint16 flags)
{
    op->was_open = op->open;
    op->open    = flags & F_OPEN    ? 1 : 0;
    op->damned  = flags & F_DAMNED  ? 1 : 0;
    op->cursed  = flags & F_CURSED  ? 1 : 0;
    op->magical = flags & F_MAGIC   ? 1 : 0;
    op->unpaid  = flags & F_UNPAID  ? 1 : 0;
    op->applied = flags & F_APPLIED ? 1 : 0;
    op->locked  = flags & F_LOCKED  ? 1 : 0;
    op->flagsval= flags;
    op->apply_type = flags & F_APPLIED;
    set_flag_string(op);
}



void set_item_values (item *op, char *name, sint32 weight, uint16 face, 
		      uint16 flags, uint16 anim, uint16 animspeed,
		      uint32 nrof, uint16 type) 
{
    int resort=1;

    if (!op) {
	printf ("Error in set_item_values(): item pointer is NULL.\n");
	return;
    }
    /* Program always expect at least 1 object internall */
    if (nrof==0) nrof=1;

    if (*name!='\0') {
	copy_name(op->s_name, name);

	/* Unfortunately, we don't get a length parameter, so we just have
	 * to assume that if it is a new server, it is giving us two piece
	 * names.
	 */
	if (csocket.sc_version>=1024) {
	    copy_name(op->p_name, name+strlen(name)+1);
	}
	else { /* If not new version, just use same for both */
	    copy_name(op->p_name, name);
	}

	/* Necessary so that d_name is updated below */
	op->nrof = nrof + 1;
    } else {
	resort=0;	/* no name - don't resort */
    }

    if (op->nrof != nrof) {
        if (nrof !=1 ) {
	    sprintf(op->d_name, "%s %s", get_number(nrof), op->p_name);
	} else {
	    strcpy(op->d_name, op->s_name);
	}
        op->nrof = nrof;
    }

    if (op->env) op->env->inv_updated = 1;
    op->weight = (float) weight / 1000;
    op->face = face;
    op->animation_id = anim;
    op->anim_speed=animspeed;
    op->type = type;
    get_flags (op, flags);

    /* We don't sort the map, so lets not bother figuring out the
     * type.  Likewiwse, only figure out item type if this
     * doesn't have a type (item2 provides us with a type
     */
    if (op->env != map && op->type == NO_ITEM_TYPE) {
	op->type =get_type_from_name(op->s_name);
    }
    if (resort) update_item_sort(op);
    
    item_event_item_changed(op);
}

void toggle_locked (item *op)
{
    SockList sl;
    uint8 buf[MAX_BUF];

    if (op->env->tag == 0)
	return;	/* if item is on the ground, don't lock it */

    sprintf((char*)buf,"lock %c %d",!op->locked,op->tag);
    script_monitor_str((char*)buf);
    SockList_Init(&sl, buf);
    SockList_AddString(&sl, "lock "); 
    SockList_AddChar(&sl, !op->locked);
    SockList_AddInt(&sl, op->tag);
    SockList_Send(&sl, csocket.fd);
}

void send_mark_obj (item *op) {
    SockList sl;
    uint8 buf[MAX_BUF];

    if (op->env->tag == 0)
	return;	/* if item is on the ground, don't mark it */

    sprintf((char*)buf,"mark %d",op->tag);
    script_monitor_str((char*)buf);
    SockList_Init(&sl, buf);
    SockList_AddString(&sl, "mark ");
    SockList_AddInt(&sl, op->tag);
    SockList_Send(&sl, csocket.fd);
}


item *player_item ()
{
    player = new_item(); 
    return player;
}

item *map_item ()
{
    map = new_item();
    map->weight = -1;
    return map;
}

/* Upates an item with new attributes. */
void update_item(int tag, int loc, char *name, int weight, int face, int flags,
		 int anim, int animspeed, uint32 nrof, int type)
{
    item *ip = locate_item(tag), *env=locate_item(loc);

    /* Need to do some special handling if this is the player that is
     * being updated.
     */
    if (player->tag==tag) {
	copy_name (player->d_name, name);
	/* I don't think this makes sense, as you can have
	 * two players merged together, so nrof should always be one
	 */
	player->nrof = nrof;
	player->weight = (float) weight / 1000;
	player->face = face;
	get_flags (player, flags);
	if (player->inv) player->inv->inv_updated = 1;
	player->animation_id = anim;
	player->anim_speed = animspeed;
	player->nrof = nrof;
    }
    else { 
	if (ip && ip->env != env) {
	    remove_item(ip);
	    ip=NULL;
	}
	set_item_values(ip?ip:create_new_item(env,tag), name, weight, face, flags,
			anim, animspeed,nrof, type);
    }
}


/*
 *  Prints players inventory, contain extra information for debugging purposes
 * This isn't pretty, but is only used for debugging, so it doesn't need to be.
 */
void print_inventory (item *op)
{
    char buf[MAX_BUF];
    char buf2[MAX_BUF];
    item *tmp;
    static int l = 0;
#if 0
    int info_width = get_info_width();
#else
    /* A callback for a debugging command seems pretty pointless.  If anything,
     * it may be more useful to dump this out to stderr
     */
    int info_width = 40;
#endif

    if (l == 0) {
	sprintf (buf, "%s's inventory (%d):", op->d_name, op->tag);
	sprintf (buf2, "%-*s%6.1f kg", info_width - 10, buf, op->weight);
	draw_info (buf2,NDI_BLACK);
    }

    l += 2;
    for (tmp = op->inv; tmp; tmp=tmp->next) {
	sprintf (buf, "%*s- %d %s%s (%d)", l - 2, "", tmp->nrof, tmp->d_name,
		 tmp->flags, tmp->tag);
	sprintf (buf2, "%-*s%6.1f kg", info_width - 8 - l, buf, tmp->nrof*tmp->weight);
	draw_info (buf2,NDI_BLACK);
	if (tmp->inv)
	    print_inventory (tmp);
    }
    l -= 2;
}

/* Check the objects, animate the ones as necessary */
void animate_objects()
{
    item *ip;
    int got_one=0;

    /* Animate players inventory */
    for (ip=player->inv; ip; ip=ip->next) {
	if (ip->animation_id>0 && ip->anim_speed) {
	    ip->last_anim++;
	    if (ip->last_anim>=ip->anim_speed) {
		ip->anim_state++;
		if (ip->anim_state >= animations[ip->animation_id].num_animations)
		    ip->anim_state=0;
		ip->face = animations[ip->animation_id].faces[ip->anim_state];
		ip->last_anim=0;
		got_one=1;
	    }
	}
    }
#ifndef GTK_CLIENT
    if (got_one) player->inv_updated=1;
#endif
    if (cpl.container) {
	/* Now do a container if one is active */
	for (ip=cpl.container->inv; ip; ip=ip->next) {
	    if (ip->animation_id>0 && ip->anim_speed) {
		ip->last_anim++;
		if (ip->last_anim>=ip->anim_speed) {
		    ip->anim_state++;
		    if (ip->anim_state >= animations[ip->animation_id].num_animations)
			ip->anim_state=0;
		    ip->face = animations[ip->animation_id].faces[ip->anim_state];
		    ip->last_anim=0;
		    got_one=1;
		}
	    }
	}
	if (got_one) cpl.container->inv_updated=1;
    } else {
	/* Now do the map (look window) */
	for (ip=cpl.below->inv; ip; ip=ip->next) {
	    if (ip->animation_id>0 && ip->anim_speed) {
		ip->last_anim++;
		if (ip->last_anim>=ip->anim_speed) {
		    ip->anim_state++;
		    if (ip->anim_state >= animations[ip->animation_id].num_animations)
			ip->anim_state=0;
		    ip->face = animations[ip->animation_id].faces[ip->anim_state];
		    ip->last_anim=0;
		    got_one=1;
		}
	    }
	}
	if (got_one) cpl.below->inv_updated=1;
    }
}

