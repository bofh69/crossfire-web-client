/* config.c */
extern void main_window_destroyed(void);
extern void configdialog(GtkWidget *widget);
extern void load_defaults(void);
extern void save_defaults(void);
/* gx11.c */
extern void gtk_command_history(int direction);
extern void gtk_complete_command(void);
extern void draw_prompt(const char *str);
extern void draw_info(const char *str, int color);
extern void draw_color_info(int colr, const char *buf);
extern void draw_stats(int redraw);
extern void reset_stat_bars(void);
extern void resize_resistance_table(int resists_show);
extern void draw_message_window(int redraw);
extern void cclist_button_event(GtkWidget *gtklist, gint row, gint column, GdkEventButton *event);
extern void menu_clear(void);
extern void client_exit(void);
extern void create_windows(void);
extern void set_scroll(const char *s);
extern void set_autorepeat(const char *s);
extern int get_info_width(void);
extern void do_clearlock(void);
extern void x_set_echo(void);
extern void draw_info_windows(void);
extern int do_timeout(void);
extern int gtk_checkchilds(void);
extern void draw_magic_map(void);
extern void magic_map_flash_pos(void);
extern void get_window_coord(GtkWidget *win, int *x, int *y, int *wx, int *wy, int *w, int *h);
extern void save_winpos(void);
extern void set_window_pos(void);
extern int init_windows(int argc, char **argv);
extern void display_map_doneupdate(int redraw, int notice);
extern void display_map_newmap(void);
extern void resize_map_window(int x, int y);
extern void display_map_startupdate(void);
extern char *get_metaserver(void);
extern void gtkLogListener(LogEntry *le);
extern void gLogHandler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);
extern int main(int argc, char *argv[]);
/* inventory.c */
extern void item_event_item_changed(item *op);
extern void item_event_container_clearing(item *op);
extern void item_event_item_deleting(item *op);
extern void inventory_splitwin_toggling(void);
extern void get_look_display(GtkWidget *frame);
extern void get_inv_display(GtkWidget *frame);
extern void command_show(const char *params);
extern void update_list_labels(itemlist *l);
extern void set_weight_limit(uint32 wlim);
extern void set_show_weight(const char *s);
extern void itemlist_set_show_icon(itemlist *l, int new_setting);
extern void set_show_icon(const char *s);
extern void set_look_list_env(item *op);
extern void open_container(item *op);
extern void close_container(item *op);
extern void inventory_tick(void);
/* help.c */
extern void chelpdialog(GtkWidget *widget);
/* image.c */
extern int create_and_rescale_image_from_data(Cache_Entry *ce, int pixmap_num, uint8 *rgba_data, int width, int height);
extern void addsmooth(uint16 face, uint16 smooth_face);
extern int associate_cache_entry(Cache_Entry *ce, int pixnum);
extern void reset_image_data(void);
extern void image_update_download_status(int start, int end, int total);
extern void get_map_image_size(int face, uint8 *w, uint8 *h);
/* keys.c */
extern void init_keys(void);
extern void bind_key(const char *params);
extern void unbind_key(const char *params);
extern void keyrelfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window);
extern void keyfunc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window);
extern void draw_keybindings(GtkWidget *keylist);
extern void bind_callback(GtkWidget *gtklist, GdkEventButton *event);
extern void ckeyunbind(GtkWidget *gtklist, GdkEventButton *event);
extern void ckeyentry_callback(GtkWidget *widget, GdkEventKey *event, GtkWidget *window);
extern void ckeyclear(void);
/* map.c */
extern void reset_map(void);
extern int display_mapscroll(int dx, int dy);
extern void drawsmooth(int mx, int my, int layer, int picx, int picy);
extern void gtk_draw_map(int redraw);
/* png.c */
extern uint8 *png_to_data(uint8 *data, int len, uint32 *width, uint32 *height);
extern uint8 *rescale_rgba_data(uint8 *data, int *width, int *height, int scale);
extern int rgba_to_gdkpixmap(GdkWindow *window, uint8 *data, int width, int height, GdkPixmap **pix, GdkBitmap **mask, GdkColormap *colormap);
extern int png_to_gdkpixmap(GdkWindow *window, uint8 *data, int len, GdkPixmap **pix, GdkBitmap **mask, GdkColormap *colormap);
/* sdl.c */
extern void init_SDL(GtkWidget *sdl_window, int just_lightmap);
extern void drawquarterlightmap_sdl(int tl, int tr, int bl, int br, int width, int height, int startx, int starty, int endx, int endy, int destx, int desty);
extern void sdl_gen_map(int redraw);
extern int sdl_mapscroll(int dx, int dy);
/* sound.c */
extern void signal_pipe(int i);
extern int init_sounds(void);
extern void SoundCmd(unsigned char *data, int len);
/* text.c */
extern void init_text_callbacks(void);
extern media_state write_media(GtkText* textarea, const char* message);
extern media_state write_media_with_state(GtkText* textarea, const char* message,media_state current_state);
extern const char* getMOTD(void);
extern const char* get_rules(void);
extern news_entry* get_news(void);
extern void cleanup_textmanagers(void);
