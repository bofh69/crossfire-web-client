#ifndef ITEM_H
#define ITEM_H

/*
 *  These should probably be in a separate file
 */
typedef unsigned int    uint32;
typedef signed int      sint32;
typedef unsigned short  uint16;
typedef signed short    sint16;
typedef unsigned char   uint8;
typedef signed char     sint8;

/*
 *  Use static buffer for object names. Item names are changing so
 *  often that mallocing them it just a waste of time. Also there is
 *  probably some upper limits for names that client can show, Note
 *  that total number of items is small (<100) so this don't even
 *  waste too much memory
 */
#define NAME_LEN	64
#define copy_name(t,f) strncpy(t, f, NAME_LEN-1)

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
    sint32 nrof;		/* number of items */
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
    uint8   type;		/* Item type for ordering */
} item;


extern void free_all_items (item *);
extern void dump_inv (item *);
extern item *locate_item (sint32);
extern void remove_item (item *);
extern void remove_item_inventory (item *);
extern item *create_new_item (item *, sint32);
extern item *player_item (void);
extern item *map_item (void);

/*
 *  A few macros to make clear interface 
 *  These will change (especially update_item and add_new_item)
 */
#define delete_item(tag) remove_item(locate_item(tag))
#define delete_item_inventory(tag) remove_item_inventory(locate_item(tag))


#define add_new_item(tag,loc,name,weight,face,flags) \
    set_item_values(create_new_item (locate_item(loc),tag), \
		    name,weight,face,flags)

#endif /* ITEM_H */
