/*
 * static char *rcsid_rcsid_h =
 *   "$Id$";
 */

extern const char* rcsid_gtk_config_c;
extern const char* rcsid_gtk_gx11_c;
extern const char* rcsid_gtk_image_c;
extern const char* rcsid_gtk_keys_c;
extern const char* rcsid_gtk_map_c;
extern const char* rcsid_gtk_png_c;
extern const char* rcsid_gtk_sdl_c;
extern const char* rcsid_gtk_sound_c;
#define HAS_GTK_RCSID
#define INIT_GTK_RCSID \
    const char *gtk_rcsid[]={\
    "$Id$",\
    rcsid_gtk_config_c,\
    rcsid_gtk_gx11_c,\
    rcsid_gtk_image_c,\
    rcsid_gtk_keys_c,\
    rcsid_gtk_map_c,\
    rcsid_gtk_png_c,\
    rcsid_gtk_sdl_c,\
    rcsid_gtk_sound_c,\
    NULL}

/*#ifdef HAVE_SDL*/
