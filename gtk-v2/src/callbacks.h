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
