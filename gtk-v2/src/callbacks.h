#include <gtk/gtk.h>


gboolean
keyfunc                                (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

gboolean
keyrelfunc                             (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
menu_quit_character                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
menu_quit_program                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_dont_pickup_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_stop_before_pickup_activate    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_body_armor_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_boots_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_cloaks_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_gloves_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_helmets_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_shields_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_skillscrolls_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_normal_book_scrolls_activate   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_spellbooks_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_drinks_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_food_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_keys_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_magical_items_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_potions_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_valuables_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_wands_rods_horns_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_jewels_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
void
on_menu_flesh_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_all_weapons_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_missile_weapons_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_bows_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_arrows_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_pickup_off_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_5_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_10_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_15_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_20_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_25_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_35_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_40_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_45_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_ratio_50_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_not_cursed_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
menu_about                             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_drawingarea_map_expose_event        (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

gboolean
on_drawingarea_map_button_press_event  (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_drawingarea_magic_map_expose_event  (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

void
on_entry_commands_activate             (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_notebook_switch_page                (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

gboolean
on_inv_table_expose_event              (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

void
list_row_collapse                      (GtkTreeView     *treeview,
                                        GtkTreeIter     *iter,
                                        GtkTreePath     *path,
                                        gpointer         user_data);

void
on_treeview_metaserver_row_activated   (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);

void
on_metaserver_select_clicked           (GtkButton       *button,
                                        gpointer         user_data);

void
on_button_metaserver_quit_pressed      (GtkButton       *button,
                                        gpointer         user_data);

void
on_metaserver_text_entry_activate      (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_disconnect_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_disconnect_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_keybindings_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_keybinding_entry_key_key_press_event
                                        (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
on_keybinding_button_remove_clicked    (GtkButton       *button,
                                        gpointer         user_data);

void
on_keybinding_button_bind_clicked      (GtkButton       *button,
                                        gpointer         user_data);

void
on_keybinding_button_clear_clicked     (GtkButton       *button,
                                        gpointer         user_data);

void
on_keybinding_button_close_clicked     (GtkButton       *button,
                                        gpointer         user_data);

void
on_keybinding_button_update_clicked    (GtkButton       *button,
                                        gpointer         user_data);

void
on_keybinding_button_bind_clicked      (GtkButton       *button,
                                        gpointer         user_data);

void
on_spells_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_spell_cast_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_spell_invok_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_spell_close_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_spell_invoke_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_spell_options_activate              (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_spell_treeview_row_activated        (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);

void
on_config_button_save_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_config_button_apply_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_config_button_close_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_configure1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_configure_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save_window_position_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about_close_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_window_destroy_event                (GtkObject       *object,
                                        gpointer         user_data);

gboolean
on_metaserver_text_entry_key_press_event
                                        (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);
