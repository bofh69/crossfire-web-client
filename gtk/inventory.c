
/*
 * Showing and manipulating your inventory and what's in a 
 * container/at your feet. (But not autopickup.)
 */

#ifdef WIN32
#include <config.h>
#endif
#include "gx11.h"
#include "client.h" /* LOG */

#define bool uint8 /* Grr! Pick *something*, please! */

extern GdkColor gdk_grey;
extern GdkColor gdk_black;

/* *grumble* Neither of two C textbooks I checked gave an example of this. :S */
typedef bool (*itemfilter)(item * it);

typedef struct {
    item * cont; /* The container whose contents we're showing. */
    item * move_dest; /* The container you want to move things to when you right-click on them. */
    GtkWidget * list; /* GtkCList */
    GtkWidget * scroll_window; /* Scrolled-window widget holding list. */
    itemfilter shows;
    bool complete_rebuild:1; /* Dirty flag. */
    bool show_weight:1;
    bool highlight:1;
    bool show_flags:1;
    
    /* 
     * Image-column (and row height...) resizes to hold biggest face; 
     * good when standing on buildings (or monsters!).
     */
    bool face_column_resizes:1; 
    
    sint16 image_width;
    sint16 image_height;
} inventory_viewer;

/*
 * Creation
 */

static GList * views;

/* forward */
static void list_button_event(
    GtkWidget *gtklist, 
    gint row, gint column, 
    GdkEventButton *event, 
    inventory_viewer * view);


static inventory_viewer * new_inventory_viewer(item * container, itemfilter filter, item * move_dest) {
    inventory_viewer * ret;
    GtkWidget * list;
    GtkWidget * scroll_window;
    GtkStyle * liststyle;
    gchar *titles[] = {"?", "Name", "Weight"};
        
    scroll_window = gtk_scrolled_window_new (0,0);

    list = gtk_clist_new_with_titles(3, titles);

    g_assert(list != NULL);
    gtk_clist_set_column_width (GTK_CLIST(list), 0, image_size);
    gtk_clist_set_column_width (GTK_CLIST(list), 1, 150);
    gtk_clist_set_column_width (GTK_CLIST(list), 2, 50);
    
    gtk_clist_set_selection_mode (GTK_CLIST(list) , GTK_SELECTION_SINGLE);
    gtk_clist_set_row_height (GTK_CLIST(list), image_size); 
        
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    liststyle = gtk_rc_get_style (list);
    if (liststyle) {
      liststyle->bg[GTK_STATE_SELECTED] = gdk_grey;
      liststyle->fg[GTK_STATE_SELECTED] = gdk_black;
      gtk_widget_set_style (list, liststyle);
    }
      
    gtk_widget_show(list);
    
    gtk_container_add(GTK_CONTAINER(scroll_window), list);
    gtk_widget_show(scroll_window);

    ret = malloc(sizeof(inventory_viewer));
    ret->complete_rebuild = TRUE;
    ret->list = list;
    ret->scroll_window = scroll_window;
    ret->cont = container;
    ret->shows = filter;
    ret->show_weight = TRUE;
    ret->show_flags = TRUE;
    ret->highlight = FALSE;
    ret->move_dest = move_dest;
    ret->face_column_resizes = FALSE;
    ret->image_height = ret->image_width = image_size;
    views = g_list_first(g_list_append(views, ret));

    gtk_clist_set_button_actions(GTK_CLIST(list), 
        1, GTK_BUTTON_SELECTS);
    gtk_clist_set_button_actions(GTK_CLIST(list), 
        2, GTK_BUTTON_SELECTS);
    gtk_signal_connect(GTK_OBJECT(list), "select_row",
        GTK_SIGNAL_FUNC(list_button_event),
        ret);
    
    return ret;
}


/* -------------------------------------------------------------------------------------- */

/*
 * Old way only stored *animated* item to (widget, row); here, we'll record *every*
 * item. (And if someone wants to backport Wedel's icon-widget, make it (widget, type, location).)
 *
 * However, also record the animated items only for speed.
 *
 * TODO Test on a slower machine than the one I have right now; throw the profiler at it, figure
 * out if this is a bottleneck, and if it is which of n different ways of storing this information
 * is quickest.
 */
 
typedef struct {
    item * it;
    GList * viewers;
} item_delete;

static GList * item_to_widgets = NULL;
static GList * animated_items = NULL;

/*
 * Store and retrieve
 */

/* TODO Ideally, this goes away and we store the list in the item, (As a
void pointer, of course.) However, we need to be able to check *all*
known items; common doesn't really support that right now. */
static item_delete * item_to_widget_retrieve(item * op) {
    GList * p;
    item_delete * ret = NULL;
    
    for (p = g_list_first(item_to_widgets); p != NULL; p = g_list_next(p) ) {
        item_delete * record;
        
        record = (item_delete *)(p->data);
        
        if (record->it == op) {
            return record;
        }
    }    
    
    /* It's not on the list; we'll have to add one. */
    ret = malloc(sizeof(item_delete));
    
    ret->it = op;
    ret->viewers = NULL;
    
    item_to_widgets = g_list_first(g_list_prepend(item_to_widgets, ret));
    g_assert(item_to_widgets != NULL);

    return ret;
}

static GList * item_to_widget_retrieve_viewers(item * op) {
    return item_to_widget_retrieve(op)->viewers;
}

static void item_to_widget_store(item * op, inventory_viewer * view) {
    item_delete * x;
    
    x = item_to_widget_retrieve(op);

    if (g_list_find(x->viewers, view) == NULL) {
        x->viewers = g_list_prepend(x->viewers, view);
    }
    g_assert(x->viewers != NULL);


    /* If it's animated, also stick it on the shortlist of animated items. */    
    if (op->animation_id > 0 && op->anim_speed) {
        /* Only stick it on if it's not already present. :S */
        if (g_list_find(animated_items, op) == NULL) {    
            animated_items = g_list_first(g_list_prepend(animated_items, op));
            g_assert(animated_items != NULL);
        }
    }    
}

/*
 * Remove
 */

static void remove_widget_one(gpointer item_and_widget_x, gpointer view_x) {      
    item_delete * item_and_widgets = (item_delete *)item_and_widget_x;
    
    item_and_widgets->viewers = g_list_remove(item_and_widgets->viewers, view_x);
}

static void item_to_widget_remove_widget(inventory_viewer * view) {
    g_list_foreach(item_to_widgets, remove_widget_one, view);
}

static void item_to_widget_remove_item(item * const op) {
    item * op_mangled = op;
    GList * search_return = NULL;

    if (item_to_widgets != NULL) {
        GList * victim_link = NULL;
        GList * i = NULL;        
        item_delete * victim = NULL;
        
        /* Look for the item_delete for this item. */
        for (i = item_to_widgets; i != NULL; i = g_list_next(i)) 
        {
            item_delete * x = (item_delete *)(i->data);
            if (x->it == op) {
                victim = x;
                victim_link = i;
                break;
            }
        }
        
        if (victim != NULL) {
            g_assert(victim_link != NULL);

            /* Remove the item_delete; free the widget-list first. */
            g_list_free(victim->viewers); 
            item_to_widgets = g_list_remove_link(item_to_widgets, victim_link);
        }        
    }
    
    /* Also nuke it from the animation list. (Hope g_list doesn't choke if it's not there.) */
    /*LOG(LOG_INFO, "inventory::item_to_widget_remove_item", 
        "removing %d (%s) %p", op->tag, op->d_name, op);*/
    animated_items = g_list_remove(animated_items, op_mangled);    
    search_return = g_list_find(animated_items, op);
    g_assert(search_return == NULL);
}

/*
 * Animate
 */

static void animate_item(gpointer view_x, gpointer item_x) {
    item * it = (item *)item_x;
    PixmapInfo * new_face = pixmaps[it->face];
    inventory_viewer * view = (inventory_viewer *) view_x;
    
    /* LOG(LOG_INFO, "inventory::animate_item", "Called"); */
    
    /* Don't update views that are going to be completely reconstructed anyway. */
    if (view->complete_rebuild) return;
    
    gtk_clist_set_pixmap(GTK_CLIST(view->list), 
        gtk_clist_find_row_from_data(GTK_CLIST(view->list), item_x), 0,
        (GdkPixmap*)new_face->icon_image,
        (GdkBitmap*)new_face->icon_mask);
}

static void animate_one_item(gpointer item_x, gpointer ignored) {
    item * it = (item *)item_x;
    GList * views = item_to_widget_retrieve_viewers(it);
    
    /* Is it animated? */
    g_assert(it->animation_id > 0 && it->anim_speed);
    
    it->last_anim++;
    
    /* Is it time to change the face yet? */
    if (it->last_anim < it->anim_speed) return;
    
    it->anim_state++;
    
    if (it->anim_state >= animations[it->animation_id].num_animations) {
      it->anim_state=0;
    }
    it->face = animations[it->animation_id].faces[it->anim_state];
    it->last_anim=0;

    /*LOG(LOG_INFO, "inventory::animate_one_item", "Animating %p: %s to %d", op, op->d_name, op->face);  */

    /* For each view the newly-updated item appears in, change its face. */
    g_list_foreach(views, animate_item, it);
}

static void animate_items(void) {
    g_list_foreach(animated_items, animate_one_item, NULL);    
}


static void item_changed_anim_hook(item * op) {
    /* HACK Make sure its presence or absence in the animated-items list is correct. */
    
    if (op->animation_id > 0 && op->anim_speed) {
        if (g_list_find(animated_items, op) == NULL) {
            animated_items = g_list_prepend(animated_items, op);
        }
    } else {
        animated_items = g_list_remove(animated_items, op);
    }
}


/* -------------------------------------------------------------------------------------- */

/*
 * (Re)building
 */

static void highlight_item(GtkWidget * list, item * it, gint row) {
    extern GdkColor root_color[16]; /* gx11.c; it'll probably change when Lally finishes his patch... */

    if (it->cursed || it->damned) {
        if (!it->magical) {
            gtk_clist_set_background (GTK_CLIST(list), row,
                &root_color[NDI_RED]);
        } else {
            gtk_clist_set_background (GTK_CLIST(list), row,
                &root_color[NDI_NAVY]);
        }
    }
    else if (it->magical) {
        gtk_clist_set_background (GTK_CLIST(list), row,
            &root_color[NDI_BLUE]);
    }
}

#define FMT_WEIGHT(buf, buf_size, it) snprintf(buf, buf_size, "%6.1f" , it->nrof * it->weight)

static void rebuild_our_widget(inventory_viewer * view) {
    item * it;
    char buffer[3][MAX_BUF];
    char *columns[3];
    gfloat scrollbar_pos; /* Copying from gx11.c, etc etc. */
    GtkWidget * scroll_window = NULL;
    GtkWidget * list = NULL;
    uint16 mh = image_size, mw = image_size;

    g_assert(view != NULL);
    g_assert(view->complete_rebuild);
    
    scroll_window = view->scroll_window;
    list = view->list;

    /* GtkAdjustment doesn't give any indirect way of extracting that value. :( */
    scrollbar_pos = 
      gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll_window))->value;
    gtk_clist_freeze(GTK_CLIST(list));
    gtk_clist_clear(GTK_CLIST(list));
    
    columns[0] = buffer[0];
    columns[1] = buffer[1];
    
    for (it = view->cont->inv; it != NULL; it = it->next) {
	PixmapInfo * pixmap = pixmaps[it->face];
        gint row; 
        
        if (!view->shows(it)) continue;
        
        if (view->face_column_resizes) {
            if (pixmap->icon_width > mw) mw = pixmap->icon_width;
            if (pixmap->icon_height > mh) mh = pixmap->icon_height;
        }
    
        /* TODO safe_strcat! Perhaps use glib's string functions? */    
	strcpy (buffer[0]," "); 
	strcpy (buffer[1], it->d_name);
	
	if (view->show_flags) {
            strcat (buffer[1], it->flags);
        }
        	
        if (view->show_weight && !(it->weight < 0)) {
            FMT_WEIGHT(buffer[2], MAX_BUF, it);
            columns[2] = buffer[2];
        } else {
            columns[2] = " ";
        }
        
        row = gtk_clist_append(GTK_CLIST(list), columns);
          
        /* Set original pixmap */
        gtk_clist_set_pixmap (GTK_CLIST (list), row, 0,
                        (GdkPixmap*)pixmap->icon_image,
                        (GdkBitmap*)pixmap->icon_mask); 
        
        gtk_clist_set_row_data (GTK_CLIST(list), row, it); 

	item_to_widget_store(it, view);
	
	if (view->highlight) {
	    highlight_item(list, it, row);
	}
    }
    
    if (view->face_column_resizes) {
        if (view->image_width != mw) {
            gtk_clist_set_column_width(GTK_CLIST(list), 0, mw);
            view->image_width = mw;
        }
        if (view->image_height != mh) {
            gtk_clist_set_row_height(GTK_CLIST(list), mh);
            view->image_height = mh;
        }
    }
    
    /* Ok, stuff is drawn, now replace the scrollbar positioning as far as possible */
    gtk_adjustment_set_value(
        GTK_ADJUSTMENT(
          gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll_window))
        ),
        scrollbar_pos);
    gtk_clist_thaw(GTK_CLIST(list));
    
    view->complete_rebuild = FALSE;
}

/* 
 * Updates and animation
 */
 
/* forward */
static bool view_visible(inventory_viewer * view);

static void item_tick_per_view(gpointer data, gpointer user_data_ignored) {
    inventory_viewer * view;
    view = (inventory_viewer *)data;
    
    if (redraw_needed) {
        /*LOG(LOG_INFO, "inventory::item_tick_per_view", "%p redraw_needed", view); */
    
        /* Faces have changed (gtk/image.c). Sadly, cache.c isn't more granular than this, so we
        can only update *all* the faces. */
        
        /* TODO If the inventory isn't otherwise dirty, only cycle through the faces. 
        For the moment, rebuild the entire list. */
        view->complete_rebuild = TRUE;
        
        /* Do not clear the flag; that's done in gx11.c::do_timeout(). 
        In any case, we'd smash it for the other views. :S */
    }
    
    /* 
     * HACK
     * If visible() ever returns false, remember to manually update the widget when
     * it becomes visible! 
     */
    if (!view_visible(view)) return;
    
    if (view->complete_rebuild) {
        /*LOG(LOG_INFO, "inventory::item_tick_per_view", "rebuild %p on timeout", view);*/
        rebuild_our_widget(view);
    } 
}

static void itemview_tick(void) {
    animate_items(); 
    
    g_list_foreach(views, item_tick_per_view, NULL);
}

/* TODO Another optimization; if an item is new (to the container),
we only need to dirty the views that will show the item. */

static void item_changed_one(gpointer view_x, gpointer op_x) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    item * it = (item *)op_x;

    /* TODO Finer-grained checking, so only the affected panels
    complete_rebuild, and for *big* fenceposting, only add a row. */
            
    if (view->cont == it->env) {
        /* TODO My brother says he can do better. */
        view->complete_rebuild = TRUE;
        /*LOG(LOG_INFO, "inventory::item_changed_one", "%p dirtied", view); */
    } else {
        /*LOG(LOG_INFO, "inventory::item_changed_one", "%p not container", view); */
    } 

}

void item_event_item_changed(item * op) { 
    item_changed_anim_hook(op); 

    /*LOG(LOG_INFO, "inventory::item_event_item_changed", "Changed: %d %s %d", op->tag, op->d_name, op->face); */
    g_list_foreach(views, item_changed_one, (gpointer)op);
}



static void container_clearing_one(gpointer view_x, gpointer op_x) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    item * it = (item *)op_x;
    
    /* TODO We'd have to tweak for recursive-search in a tree widget, maybe. */
    if (view->cont == it) {
        if (!view->complete_rebuild) {
            view->complete_rebuild = TRUE;
            /* Wonder if at any later stage pass view? */
            item_to_widget_remove_widget(view); 
            /*LOG(LOG_INFO, "inventory::container_clearing_one", "%p dirtied", view);*/
        } else {
            /*LOG(LOG_INFO, "inventory::container_clearing_one", "%p already dirty", view);*/
        }
    } else {
        /*LOG(LOG_INFO, "inventory::container_clearing_one", "%p not container", view);*/
    } 
}

void item_event_container_clearing(item * op) {
    /*LOG(LOG_INFO, "inventory::item_event_container_clearing", "Clearing: %d %s %d", op->tag, op->d_name, op->face); */
    g_list_foreach(views, container_clearing_one, (gpointer)op);
}

static void item_deleting_one(gpointer view_x, gpointer op_x) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    item * it = (item *)op_x; 
    
    if (it->env != view->cont) {    
        /*LOG(LOG_INFO, "inventory::item_deleting_one", "%p not container", view);*/
        return;
    } 
    
    if (view->complete_rebuild) {
        /*LOG(LOG_INFO, "inventory::item_deleting_one", "%p already dirty", view);*/
        return;
    } 
    
    /*LOG(LOG_INFO, "inventory::item_deleting_one", "%p removing row", view);*/

    if (view->face_column_resizes) {
        PixmapInfo * it_face = pixmaps[it->face];
        
        /* Special handling to shrink the image column if the 'responsible' face vanishes. */
        
        if (it_face->icon_width == image_size && it_face->icon_height == image_size) {
            ; /* The column will never get smaller than image_size. */
        } else if (it_face->icon_width < view->image_width 
            && it_face->icon_height < view->image_height) {
            ; /* This face isn't a cause of either of the maximums. */    
        } else {            
            PixmapInfo * tmp_face;
            item * tmp_item;
            uint16 mw = image_size, mh = image_size;
            
            /* TODO Refactor with rebuild_our_widget. */
            
            /* it_face requires one of the dimensions to be that large.
            See if removing it changes the requirements. */
            
            for (tmp_item = view->cont->inv; tmp_item != NULL; tmp_item = tmp_item->next) {
                if (tmp_item == it) continue;
                
                if (!view->shows(it)) continue;
                
                tmp_face = pixmaps[tmp_item->face];
                
                if (tmp_face->icon_width > mw) mw = tmp_face->icon_width;
                if (tmp_face->icon_height > mh) mh = tmp_face->icon_height;
            }
            
            /* mw, mh hold the size requirement for every shown item bar op. */
            
            if (view->image_width != mw) {
                gtk_clist_set_column_width(GTK_CLIST(view->list), 0, mw);
                view->image_width = mw;
            }
            if (view->image_height != mh) {
                gtk_clist_set_row_height(GTK_CLIST(view->list), mh);
                view->image_height = mh;
            }
        }                                
    }

    /* Remove the row containing the item. */                
    gtk_clist_remove(GTK_CLIST(view->list),
        gtk_clist_find_row_from_data(GTK_CLIST(view->list), op_x)
    );
}

void item_event_item_deleting(item * op) {
    /*LOG(LOG_INFO, "inventory::item_event_item_deleting", "Deleting: %d %s %d", op->tag, op->d_name, op->face); */
    g_list_foreach(views, item_deleting_one, (gpointer)op);
    /* Among other things, prevent animating the now-Missing item. */
    item_to_widget_remove_item(op);
}


/* 
 * Configuration
 */

/* *Could* do these without rebuilding widget, but they almost never happen,
   so not a problem? */

static void inventory_viewer_set_show_weight(inventory_viewer * view, bool show_weight) {
    if (view->show_weight == show_weight) {
        return;
    }
    
    view->complete_rebuild = TRUE;
    view->show_weight = show_weight;
}

static void inventory_viewer_set_highlight(inventory_viewer * view, bool highlight) {
    if (view->highlight == highlight) {
        return;
    }
    
    view->complete_rebuild = TRUE;
    view->highlight = highlight;
}

static void inventory_viewer_set_show_flags(inventory_viewer * view, bool show_flags) {
    if (view->show_flags == show_flags) {
        return;
    }
    
    view->complete_rebuild = TRUE;
    view->show_flags = show_flags;
}

static void inventory_viewer_set_container(inventory_viewer * view, item * new_cont) {
    if (view->cont == new_cont) return;
    
    /*LOG(LOG_INFO, "inventory::inventory_viewer_set_container", "%p dirtied", view);*/
    view->cont = new_cont;
    view->complete_rebuild = TRUE;
}


/* 
 * Handle mouse presses in the lists 
 */
 
#include "gtkproto.h" /* draw_info */
static void list_button_event(
    GtkWidget *gtklist, 
    gint row, gint column, 
    GdkEventButton *event, 
    inventory_viewer * view)
{
    item *it;
    it = gtk_clist_get_row_data (GTK_CLIST(gtklist), row);
    gtk_clist_unselect_row (GTK_CLIST(gtklist), row, 0);
        
    if (event->button==1) {
        if (event->state & GDK_SHIFT_MASK)
          toggle_locked(it);
        else
          client_send_examine (it->tag);     

    }
    if (event->button==2) {
        if (event->state & GDK_SHIFT_MASK)
          send_mark_obj(it);
        else
          client_send_apply (it->tag);
    }
    if (event->button==3) {
        if (it->locked) {
            draw_info ("This item is locked. To drop it, first unlock by shift+leftclicking on it.",
		NDI_BLACK);
        } else {
            cpl.count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(counttext));
            client_send_move (view->move_dest->tag, it->tag, cpl.count);
            if (!use_config[CONFIG_POPUPS]) { /* TODO I see no popping up here? */
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(counttext),0.0);
                cpl.count=0;
            }
        }          
    }  
}



/****************************************************************************
 *
 * Everything below is specific to the inventory and look windows; everything
 * above should be general.
 *
 * If you want to change, or especially *add*, inventory viewers, you should
 * only need to modify things below this comment.
 *
 * (Viewing multiple *containers* possibly requires changes in 
 * common and the *server*...)
 *
 ****************************************************************************/

/*
 * Creation (and destruction)
 */

#include "pixmaps/all.xpm"
#include "pixmaps/hand.xpm"
#include "pixmaps/hand2.xpm"
#include "pixmaps/coin.xpm"
#include "pixmaps/skull.xpm"
#include "pixmaps/mag.xpm"
#include "pixmaps/nonmag.xpm"
#include "pixmaps/lock.xpm"
#include "pixmaps/unlock.xpm"

static bool show_all(item * ignored) { return TRUE; }
static bool show_applied(item * it) { return it->applied; }
static bool show_unapplied(item * it) { return !(it->applied); }
static bool show_unpaid(item * it) { return it->unpaid; }
static bool show_cursed(item * it) { return it->cursed || it->damned; }
static bool show_magical(item * it) { return it->magical; }
static bool show_nonmagical(item * it) { return !(it->magical); }
static bool show_locked(item * it) { return it->locked; }
static bool show_unlocked(item * it) { return !(it->locked); }

typedef struct {
  const char *name;
  const char *const *xpm;
  itemfilter filter;
  bool highlight;
} fixed_tab_init;

/* TODO Dynamic!!!. */
#define TYPE_LISTS 9

/* These are used to create the inventory tabs. */
static fixed_tab_init fixed_tabs[TYPE_LISTS] = {
  { "all", all_xpm, show_all, TRUE },
  { "applied", hand_xpm, show_applied, FALSE },
  { "unapplied", hand2_xpm, show_unapplied, FALSE },
  { "unpaid", coin_xpm, show_unpaid, FALSE },
  { "cursed", skull_xpm, show_cursed, FALSE },
  { "magical", mag_xpm, show_magical, FALSE },
  { "nonmagical", nonmag_xpm, show_nonmagical, FALSE },
  { "locked", lock_xpm, show_locked, FALSE },
  { "unlocked", unlock_xpm, show_unlocked, TRUE }
};

/* TODO Maybe I should move these two into the itemlist structs? */
static GList * inv_viewers = NULL;
static inventory_viewer * look_viewer = NULL;
/* The inventory_viewers created from the entries above that highlight,
plus look_viewer. */
static GList * highlit_inv_viewers = NULL;

static GtkWidget * look_widget = NULL;
static GtkWidget * inv_notebook = NULL;



/* 
 * Destroy the current views when the client is toggled between splitwindow and onewindow
 * mode (or vice versa). 
 */
 
static void add_removal_victim(gpointer view_x, gpointer victim_views_p_x) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    GList ** victim_views_p = (GList **) victim_views_p_x;
    if (g_list_find(*victim_views_p, view) == NULL) {
        *victim_views_p = g_list_prepend(*victim_views_p, view);
    }
}

static void nuke_view(gpointer view_x, gpointer notused) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    views = g_list_remove(views, view);
    item_to_widget_remove_widget(view);
    free(view);
}

void inventory_splitwin_toggling(void) {
    GList * victim_views = NULL;

    /* Eeek! Need to throw away all sorts of things. */
    
    /* We need to get of everything in inv_viewers, everything
    in highlit_inv_viewers, and the look_viewer; however, we also
    need to free them exactly once. add_removal_victim effectively
    creates a set.*/
    g_list_foreach(inv_viewers, add_removal_victim, &victim_views);
    g_list_foreach(highlit_inv_viewers, add_removal_victim, &victim_views);
    add_removal_victim(look_viewer, &victim_views);
    
    /* Free the views. */
    g_list_foreach(victim_views, nuke_view, NULL);
    g_list_free(victim_views);

    /* Zero the values; it's like the client just started. */
    /* We presume widgets'll be taken care of by GTK widget-destroy functions. */    
    look_viewer = NULL;    
    look_widget = NULL;
    inv_viewers = NULL;
    inv_notebook = NULL; 
    highlit_inv_viewers = NULL;    
}

/*
 * Resizing all the columns when the widget's size is changed.
 */

static void resize_left_widget_one(gpointer view_x, gpointer total_width_x) {
    inventory_viewer  * view = (inventory_viewer *)view_x;
    gint total_width = GPOINTER_TO_INT(total_width_x);
    
    if (view == NULL) {
        /* Weird. This is never set as a signal handler, but somehow gtk1.2
           decides to call this with a NULL view when going to split windows.
        */
        return;
    }
    
    gtk_clist_set_column_width(GTK_CLIST(view->list),
        1, total_width - view->image_width);
    
    /*LOG(LOG_INFO, "inventory::resize_left_widget_one", "view %p image_width %d", view, view->image_width);*/
}

/* I mean, the minimum width that won't cause infinite recursion is dependent on the
width of the scrollbar and possibly the font used in the title widgets; all that could
change if you switch GTK-engine-thingy, so hard-coding this value is silly. On the other
hand, I don't know any alternative, aside from using clist's automatic sizing features. 

... and why in ding dong would making it smaller (70) *add* the horizontal scrollbar?
*/
#define MAGIC_SAFE_WIDTH 75

static void resize_left_widgets(GtkWidget *widget, GtkAllocation *event) {
    static gint old_total_width = 0;
    inventory_viewer * hack; 
    gint total_width; 
    
    /* If GTK can unexpectedly call resize_left_widget_one() as a signal
       handler when the inventory widgets temporarily don't exist, then we
       might as well watch our tail here, too. */
    if (inv_viewers == NULL) return;
    if (look_viewer == NULL) return;
    
    /* HACK Extract the first inventory-viewer. */
    hack = (inventory_viewer *)(inv_viewers->data);
    total_width = GTK_CLIST(hack->list)->clist_window_width - MAGIC_SAFE_WIDTH; 
    
    if (old_total_width == total_width) return;
    old_total_width = total_width;

    g_list_foreach(inv_viewers, resize_left_widget_one, GINT_TO_POINTER(total_width));
    resize_left_widget_one(look_viewer, GINT_TO_POINTER(total_width));
}



/*
 * Redrawing only the active inventory view on idle-tick..
 */

/* HACK Only the current inventory tab is rebuilt on each tick. */
static bool view_visible(inventory_viewer * view) {
    GList * i;

    /* Bottom widget. */
    if (view == look_viewer) return TRUE;
    
    /* If it's an inv_viewer, if its the visible notebook page. */
    for(i = inv_viewers; i != NULL; i = g_list_next(i)) {
        if (i->data == view) {
            return view->scroll_window == gtk_notebook_get_nth_page(
                GTK_NOTEBOOK(inv_notebook),
                gtk_notebook_get_current_page(GTK_NOTEBOOK(inv_notebook)));
        }
    }
    
    /* assume */
    return TRUE;
}

/* As the wossname of the above, when the visible tab is changed, rebuild it if needed. 
Tied to widgets by mod_one_widget(). */
static void redraw_on_show(GtkWidget * a, GdkEventVisibility * event, gpointer view_x) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    
    if (!view->complete_rebuild) return;
    
    /*{
        char * x = "unexpected";

        if (event->state == GDK_VISIBILITY_UNOBSCURED) {
            x = "unobscured";
        } else if (event->state == GDK_VISIBILITY_PARTIAL) {
            x = "partial";
        } else if (event->state == GDK_VISIBILITY_FULLY_OBSCURED) {
            x = "obscured";
        }
        
        LOG(LOG_INFO, "inventory::redraw_on_show", "rebuilding %p (visibility changed to %s)", view, x);
    }*/
    
    rebuild_our_widget(view);
}



static GtkWidget * get_inv_widget(void) {
    fixed_tab_init * i;
    GtkStyle *tabstyle;
    GdkPixmap *labelgdkpixmap;
    GdkBitmap *labelgdkmask;
    GtkWidget *tablabel;
    inventory_viewer * view;
    
    if (inv_notebook != NULL) {
       return inv_notebook;
    }
            
    inv_notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (inv_notebook), GTK_POS_TOP );
    
    for (i = fixed_tabs; i - fixed_tabs < TYPE_LISTS; i++) {
        tabstyle = gtk_widget_get_style(gtkwin_root);

	labelgdkpixmap = gdk_pixmap_create_from_xpm_d(
            gtkwin_root->window,
            &labelgdkmask,
            &tabstyle->bg[GTK_STATE_NORMAL],
            (gchar **)  i->xpm );

        tablabel = gtk_pixmap_new (labelgdkpixmap, labelgdkmask);
        gtk_widget_show (tablabel);
        
        view = new_inventory_viewer(cpl.ob, i->filter, cpl.below); /* player to ground */
        view->face_column_resizes = FALSE;
                
        inventory_viewer_set_highlight(view, i->highlight);
        highlit_inv_viewers = g_list_append(highlit_inv_viewers, view);
        
        inv_viewers = g_list_append(inv_viewers, view);

        gtk_notebook_append_page (GTK_NOTEBOOK (inv_notebook), 
	    view->scroll_window, 
            tablabel);        
        
        /* 
         * Attach events to make some extra behaviours...
         */
        
        gtk_signal_connect(GTK_OBJECT(view->list), 
            "size-allocate",
            (GtkSignalFunc)resize_left_widgets,
            NULL);

        /* Since the program will automatically adjust these, any changes
         * the user makes can get obliterated, so just don't let the user
         * make changes.
         */
        gtk_clist_set_column_resizeable(GTK_CLIST(view->list), 0, FALSE);
        gtk_clist_set_column_resizeable(GTK_CLIST(view->list), 1, FALSE);
        gtk_clist_set_column_resizeable(GTK_CLIST(view->list), 2, FALSE);


        /* Only the visible tab redraws on inventory_tick(); redraw_on_show
        redraws dirty tabs when they *become* visible. */
        gtk_signal_connect(GTK_OBJECT(view->list),
            "visibility-notify-event",
            (GtkSignalFunc)redraw_on_show,
            view);
        
         gtk_widget_add_events(view->list, GDK_VISIBILITY_NOTIFY_MASK);        
    }

    gtk_widget_show(inv_notebook);    
    
    return inv_notebook;
}    

    

static GtkWidget *get_look_widget(void) {
    if (look_widget != NULL) {
        return look_widget;
    }

    look_viewer = new_inventory_viewer(cpl.below, show_all, cpl.ob); /* ground to player */
    look_viewer->highlight = TRUE;
    look_viewer->face_column_resizes = TRUE;
    highlit_inv_viewers = g_list_append(highlit_inv_viewers, look_viewer);
    
    look_widget = look_viewer->scroll_window;
    
    return look_widget;
}

/* 
 * Now slap the labels on around the invwidgets. 
 */

itemlist look_list, inv_list;

GtkWidget *closebutton;

/* forward */
static void close_container_callback(item *op);


void get_look_display(GtkWidget *frame) 
{
  GtkWidget *vbox1;
  GtkWidget *hbox1;
  
  look_list.env = cpl.below;
  strcpy (look_list.title, "You see:");
  strcpy (look_list.last_title, look_list.title);
  strcpy (look_list.last_weight, "0");
  strcpy (look_list.last_maxweight, "0");
  look_list.show_weight = TRUE;
  look_list.weight_limit = 0;
    

  vbox1 = gtk_vbox_new(FALSE, 0);/*separation here*/
  gtk_container_add (GTK_CONTAINER(frame), vbox1);

  hbox1 = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start (GTK_BOX(vbox1),hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);

  closebutton = gtk_button_new_with_label ("Close");
  gtk_signal_connect_object (GTK_OBJECT (closebutton), "clicked",
			       GTK_SIGNAL_FUNC(close_container_callback),
			       NULL);
  gtk_widget_set_sensitive(closebutton, FALSE);
  gtk_box_pack_start (GTK_BOX(hbox1),closebutton, FALSE, FALSE, 2);
  gtk_widget_show (closebutton);
  gtk_tooltips_set_tip (tooltips, closebutton, 
      "This will close an item if you have one open.", 
      NULL);

  look_list.label = gtk_label_new ("You see:");
  gtk_box_pack_start (GTK_BOX(hbox1),look_list.label, TRUE, FALSE, 2);
  gtk_widget_show (look_list.label);

  look_list.weightlabel = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX(hbox1),look_list.weightlabel, TRUE, FALSE, 2);
  gtk_widget_show (look_list.weightlabel);

  look_list.maxweightlabel = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX(hbox1),look_list.maxweightlabel, TRUE, FALSE, 2);
  gtk_widget_show (look_list.maxweightlabel);

  gtk_box_pack_start(GTK_BOX(vbox1), get_look_widget(), TRUE, TRUE, 0);

  gtk_widget_show (vbox1);
}

/* Used for getting and dropping more than 1 of an item, and for dimension door
   and jumping lengths. */
GtkWidget *counttext;

static void count_callback(GtkWidget *widget, GtkWidget *entry)
{
    const gchar *count_text;
    extern GtkWidget * gtkwin_info_text;

    count_text = gtk_entry_get_text(GTK_ENTRY(counttext));
    cpl.count = atoi (count_text);
    gtk_widget_grab_focus (GTK_WIDGET(gtkwin_info_text)); /* I wonder why this isn't entrytext? */
}

void get_inv_display(GtkWidget *frame)
{
  GtkWidget *vbox2;
  GtkWidget *hbox1;
  GtkWidget *invlabel;
  GtkAdjustment *adj;

  strcpy (inv_list.title, "Inventory:");
  strcpy (inv_list.last_title, inv_list.title);
  strcpy (inv_list.last_weight, "0");
  strcpy (inv_list.last_maxweight, "0");
  inv_list.env = cpl.ob;
  inv_list.show_weight = TRUE;
  inv_list.weight_limit = 0;
  
  vbox2 = gtk_vbox_new(FALSE, 0); /* separation here */
  
  gtk_container_add (GTK_CONTAINER(frame), vbox2); 

  hbox1 = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start (GTK_BOX(vbox2),hbox1, FALSE, FALSE, 0);
  gtk_widget_show (hbox1);


  inv_list.label = gtk_label_new ("Inventory:");
  gtk_box_pack_start (GTK_BOX(hbox1),inv_list.label, TRUE, FALSE, 2);
  gtk_widget_show (inv_list.label);

  inv_list.weightlabel = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX(hbox1),inv_list.weightlabel, TRUE, FALSE, 2);
  gtk_widget_show (inv_list.weightlabel);


  inv_list.maxweightlabel = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX(hbox1),inv_list.maxweightlabel, TRUE, FALSE, 2);
  gtk_widget_show (inv_list.maxweightlabel);

  invlabel = gtk_label_new ("Count:");
  gtk_box_pack_start (GTK_BOX(hbox1),invlabel, FALSE, FALSE, 5);
  gtk_widget_show (invlabel);

  adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 100000.0, 1.0,
                                                  100.0, 0.0);
  counttext = gtk_spin_button_new (adj, 1.0, 0);

  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (counttext), FALSE);
  gtk_widget_set_usize (counttext, 65, 0);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (counttext),
				     GTK_UPDATE_ALWAYS);
   gtk_signal_connect(GTK_OBJECT(counttext), "activate",
		     GTK_SIGNAL_FUNC(count_callback),
		     counttext);


  gtk_box_pack_start (GTK_BOX (hbox1),counttext, FALSE, FALSE, 0);

  gtk_widget_show (counttext);
  gtk_tooltips_set_tip (tooltips, counttext, 
      "This sets the number of items you wish to pickup or drop. You can also use the keys 0-9 to set it.", 
      NULL);

  gtk_box_pack_start (GTK_BOX(vbox2), get_inv_widget(), TRUE, TRUE, 0);
  gtk_widget_show (vbox2);
}



/* commandline toggle tab */

void command_show (const char *params) {
    int i;

    if(params == NULL)  {
	/* Shouldn't need to get current page, but next_page call is not wrapping
	 * like the docs claim it should.
	 */
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(inv_notebook))==TYPE_LISTS-1) {
	    gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), 0);
	} else {
	    gtk_notebook_next_page(GTK_NOTEBOOK(inv_notebook));
        }
        return;
    } 
    
    for (i = 0; i < TYPE_LISTS; i++) {
        /* Prefix match */
        if (!strncmp(params, fixed_tabs[i].name, strlen(params))) {
            gtk_notebook_set_page(GTK_NOTEBOOK(inv_notebook), i);
            return;
        }
    }
}

/*
 * Label updates.
 */


void update_list_labels (itemlist *l)
{
    char weight[MAX_BUF];
    char max_weight[MAX_BUF];
  
    /* draw title and put stuff in widgets */

    if ( strcmp( l->title, l->last_title ) ) {
        strcpy(l->last_title, l->title);
        strcpy(weight,l->title);
        gtk_label_set (GTK_LABEL(l->label), weight);
        gtk_widget_draw (l->label, NULL);
    }

    if(l->env->weight < 0 || !l->show_weight) {
	    strcpy(weight, " ");
	    strcpy(max_weight, " ");
    }
    else if (!l->weight_limit) {
	    sprintf (weight, "%6.1f",l->env->weight);
        strcpy (max_weight, " ");
    } else {
	    sprintf (weight, "%6.1f",l->env->weight);
	    sprintf (max_weight, "/ %4d",l->weight_limit / 1000);
    }

    if ( strcmp( weight, l->last_weight ) ) {
        strcpy(l->last_weight, weight);
    	gtk_label_set (GTK_LABEL(l->weightlabel), weight);
        gtk_widget_draw (l->weightlabel, NULL);
    }
    if ( strcmp( max_weight, l->last_maxweight ) ) {
        strcpy(l->last_maxweight, max_weight);
	    gtk_label_set (GTK_LABEL(l->maxweightlabel), max_weight);
        gtk_widget_draw (l->maxweightlabel, NULL);
    }
    
    l->env->inv_updated = FALSE;
}

/*
 *  update_list_labels() redraws inventory and look window labels when necessary
 *  
 *  (Maybe somewhat more often; look_list doesn't always have a weight shown, possibly.)
 */
static void update_lists_labels(void)
{
  if (inv_list.env->inv_updated) {
    update_list_labels (&inv_list);
  } 

  if (look_list.env->inv_updated) {
    update_list_labels (&look_list);
  }
  
}



/*
 * Events
 */
 
void set_weight_limit (uint32 wlim)
{
    inv_list.weight_limit = wlim;
    update_list_labels(&inv_list);
}

/* toggle weight */

static void set_show_weight_inv_one(gpointer view_x, gpointer new_setting) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    inventory_viewer_set_show_weight(view, GPOINTER_TO_INT(new_setting));
}

void set_show_weight (const char *s)
{
    if (s == NULL || *s == 0 || strncmp ("inventory", s, strlen(s)) == 0) {
	inv_list.show_weight = ! inv_list.show_weight; /* toggle */
	update_list_labels (&inv_list);
	g_list_foreach(inv_viewers, set_show_weight_inv_one, GINT_TO_POINTER((int)inv_list.show_weight));
    } else if (strncmp ("look", s, strlen(s)) == 0) {
	look_list.show_weight = ! look_list.show_weight; /* toggle */
	update_list_labels (&look_list);
	inventory_viewer_set_show_weight(look_viewer, look_list.show_weight);
    }
}



/* toggle flags */

static void set_flags_one(gpointer view_x, gpointer show_flags) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    inventory_viewer_set_show_flags(view, GPOINTER_TO_INT(show_flags));
}

static void set_inv_flags(bool show_flags) {
    g_list_foreach(inv_viewers, set_flags_one, GINT_TO_POINTER((int)show_flags));
}

static void set_look_flags(bool show_flags) {
    inventory_viewer_set_show_flags(look_viewer, show_flags);
}

void itemlist_set_show_icon(itemlist * l, int new_setting) {
   if (l->show_icon == new_setting) return;
   
   /* HACK */
   if (l == &inv_list) {
       set_inv_flags(!new_setting);
   } else if (l == &look_list) {
       set_look_flags(!new_setting);
   } else {
      g_assert(l == &inv_list || l == &look_list);
   }
   
   l->show_icon = new_setting;
}

void set_show_icon (const char *s)
{
    if (s == NULL || *s == 0 || strncmp ("inventory", s, strlen(s)) == 0) {
        itemlist_set_show_icon(&inv_list, !inv_list.show_icon); /* toggle */
    } else if (strncmp ("look", s, strlen(s)) == 0) {
        itemlist_set_show_icon(&look_list, !look_list.show_icon); /* toggle */
    }
}


/* when containers are opened and shut */

static void set_look_list_env_one(gpointer view_x, gpointer new_look_x) {
    inventory_viewer * view = (inventory_viewer *)view_x;
    item * new_look = (item *)new_look_x;
    
    view->move_dest = new_look;
}

/* Also called on disconnect. */
void set_look_list_env(item * op) {
    if (look_list.env == op) return;

    look_list.env = op;

    inventory_viewer_set_container(look_viewer, op);
    
    g_list_foreach(inv_viewers, set_look_list_env_one, op);
}


void open_container (item *op) {
  set_look_list_env(op); 
  sprintf (look_list.title, "%s:", op->d_name);
  gtk_widget_set_sensitive(closebutton, TRUE);

  update_list_labels (&look_list);
}

void close_container(item *op)
{
  if (look_list.env != cpl.below) {
    if (use_config[CONFIG_APPLY_CONTAINER])
	client_send_apply (look_list.env->tag);
    set_look_list_env(cpl.below);
    strcpy (look_list.title, "You see:");
    gtk_widget_set_sensitive(closebutton, FALSE);
    update_list_labels (&look_list);
  }
}

/* This is basically the same as above, but is used for the callback
 * of the close button.  As such, it has to always send the apply.
 * However, since its a callback, its not like we can just easily
 * pass additional parameters.
 */
static void close_container_callback(item *op)
{
  if (look_list.env != cpl.below) {
    client_send_apply (look_list.env->tag);
    set_look_list_env(cpl.below);
    strcpy (look_list.title, "You see:");
    gtk_widget_set_sensitive(closebutton, FALSE);

    update_list_labels (&look_list);
  }
}

/* --- */

/* Called by gx11::do_timeout(). */
void inventory_tick() {
    update_lists_labels();
    itemview_tick();
}








