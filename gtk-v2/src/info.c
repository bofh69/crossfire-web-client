char *rcsid_gtk2_info_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005-2007 Mark Wedel & Crossfire Development Team

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

    The author can be reached via e-mail to crossfire@metalforge.org
*/

/**
 * @file gtk-v2/src/info.c
 * This covers drawing text to the info window.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "client.h"

#include "image.h"
#include "gtk2proto.h"

#include "main.h"

/**
 * Index into the different fonts The actual font these are bound to is set up
 * the style file.
 */
#define FONT_NORMAL     0
#define FONT_ARCANE     1
#define FONT_STRANGE    2
#define FONT_FIXED      3
#define FONT_HAND       4
#define NUM_FONTS       5

/*
 * This is a mapping from the font numbers to the style to use from the rcfile.
 */
static char *font_style_names[NUM_FONTS] = {
    "info_font_normal", "info_font_arcane", "info_font_strange",
    "info_font_fixed", "info_font_hand"};

#define NUM_TEXT_VIEWS  2

extern  const char *usercolorname[NUM_COLORS];

struct Info_Pane
{
    GtkWidget       *textview;
    GtkWidget       *scrolled_window;
    GtkTextBuffer   *textbuffer;
    GtkTextMark     *textmark;
    GtkAdjustment   *adjustment;
    GtkTextTag      *color_tags[NUM_COLORS];
    GtkTextTag      *font_tags[NUM_FONTS];
    GtkTextTag      *bold_tag, *italic_tag, *underline_tag, *default_tag;
    GtkTextTag      **msg_type_tags[MSG_TYPE_LAST];
} info_pane[NUM_TEXT_VIEWS];

static void message_callback(int flag, int type, int subtype, char *message);

extern  char *colorname[NUM_COLORS];

/*
 * The idea behind the msg_type_names is to provide meaningful names that the
 * client can use to load/save these values, in particular, the gtk2 client
 * uses these to find styles on how to draw the different msg types.  We could
 * set this up as a two dimension array instead - that probably isn't as
 * efficient as the number of subtypes varies wildly.  The 0 subtypes are used
 * for general cases (describe the entire class of those message types).  Note
 * also that the names here are verbose - the actual code that uses these will
 * expand it further.  In practice, there should never be entries with both the
 * same type/subtype (each subtype should be unique) - if so, the results are
 * probably unpredictable on which one the code would use.
 */
#include "msgtypes.h"

static int max_subtype=0, has_style=0;

/**
 * Sets attributes in the text tag from a style.  Best I can gather, there is
 * no way to take all of the attributes from a style and apply them directly to
 * a text tag, hence this function to do the work.  GtkTextTags also know what
 * attributes are set and which are not set - thus, you can apply multiple tags
 * to the same text, and get all of the effects.  For styles, that isn't the
 * case - a style contains all of the information.  So this function also
 * compares the loaded style from the base style, and only sets the attributes
 * that are different.
 *
 * @param tag
 * text tag to set values on
 * @param style
 * style to get values from
 * @param base_style
 * base style for the widget to compare against
 */
void set_text_tag_from_style(GtkTextTag *tag, GtkStyle *style, GtkStyle *base_style)
{
    g_object_set(tag, "foreground-set", FALSE, NULL);
    g_object_set(tag, "background-set", FALSE, NULL);
    g_object_set(tag, "font-desc", NULL, NULL);

    if (memcmp(&style->fg[GTK_STATE_NORMAL], &base_style->fg[GTK_STATE_NORMAL], sizeof(GdkColor)))
        g_object_set(tag, "foreground-gdk", &style->fg[GTK_STATE_NORMAL], NULL);

    if (memcmp(&style->bg[GTK_STATE_NORMAL], &base_style->bg[GTK_STATE_NORMAL], sizeof(GdkColor)))
        g_object_set(tag, "background-gdk", &style->bg[GTK_STATE_NORMAL], NULL);

    if (style->font_desc != base_style->font_desc)
        g_object_set(tag, "font-desc", style->font_desc, NULL);
}

/**
 * Loads up values from the style file.  Note that the actual name of the style
 * file is set elsewhere.
 *
 * This function is designed so that it should be possible to call it multiple
 * times - it will release old style data and load up new values.  In this way,
 * a user should be able to change styles on the fly and have things work.
 */
void info_get_styles()
{
    int i, j;
    static int has_init=0;
    GtkStyle    *tmp_style, *base_style[2];
    char    style_name[MAX_BUF];

    if (!has_init) {
        /*
         * We want to set up a 2 dimensional array of msg_type_tags to
         * correspond to all the types/subtypes, so looking up any value is
         * really easy.  We know the size of the types, but don't know the
         * number of subtypes - no single declared value.  So we just parse the
         * msg_type_names to find that, then know how big to make the other
         * dimension.  We could allocate different number of entries for each
         * type, but that makes processing a bit harder (no single value on the
         * number of subtypes), and this extra memory usage shouldn't really be
         * at all significant.
         */
        for (i=0; i<sizeof(msg_type_names) / sizeof(Msg_Type_Names); i++) {
            if (msg_type_names[i].subtype > max_subtype)
                max_subtype = msg_type_names[i].subtype;
        }
        for (j=0; j<NUM_TEXT_VIEWS; j++) {
            for (i=0; i < MSG_TYPE_LAST; i++)
                info_pane[j].msg_type_tags[i] = calloc(max_subtype+1, sizeof(GtkTextTag*));

            for (i=0; i<NUM_FONTS; i++)
                info_pane[j].font_tags[i] = NULL;
            for (i=0; i<NUM_COLORS; i++)
                info_pane[j].color_tags[i] = NULL;
            /*
             * These tag definitions never change - we don't get them from the
             * settings file (maybe we should), so we only need to allocate
             * them once.
             */
            info_pane[j].bold_tag = gtk_text_buffer_create_tag(info_pane[j].textbuffer, "bold",
                "weight", PANGO_WEIGHT_BOLD, NULL);

            info_pane[j].italic_tag = gtk_text_buffer_create_tag(info_pane[j].textbuffer, "italic",
                "style", PANGO_STYLE_ITALIC, NULL);

            info_pane[j].underline_tag = gtk_text_buffer_create_tag(info_pane[j].textbuffer, "underline",
                "underline", PANGO_UNDERLINE_SINGLE, NULL);

            /*
             * This is really a convenience - we can pass multiple tags in when
             * drawing text, but once we pass in a NULL tag, that signifies no
             * more tags.  Rather than having to set up an array we pass in,
             * instead, we have this empty tag that we can pass is so that we
             * always have the same calling semantics, just differ what tags we
             * pass in.
             */
            if (!info_pane[j].default_tag)
                info_pane[j].default_tag = gtk_text_buffer_create_tag(info_pane[j].textbuffer, "default",
                                                                      NULL);
        }
        has_init=1;
    }
    for (i=0; i<NUM_TEXT_VIEWS; i++) {
        base_style[i] = gtk_rc_get_style_by_paths(gtk_settings_get_default(), NULL,
                                      "info_default", G_TYPE_NONE);
    }
    if (!base_style[0]) {
        LOG(LOG_INFO, "info.c::info_get_styles", "Unable to find base style info_default - will not process most info tag styles!");
    }

    has_style=0;

    /*
     * If we don't have a base style tag, we can't process these other tags, as
     * we need to be able to do a difference, and doing a difference from
     * nothing (meaning, taking everything in style) still doesn't work really
     * well.
     */
    if (base_style[0]) {
        /*
         * This processes the type/subtype styles.  We look up the names in the
         * array to find what name goes to what number.
         */
        for (i=0; i<sizeof(msg_type_names) / sizeof(Msg_Type_Names); i++) {
            int type, subtype;

            sprintf(style_name,"msg_%s", msg_type_names[i].style_name);
            type =  msg_type_names[i].type;
            subtype = msg_type_names[i].subtype;

            tmp_style = gtk_rc_get_style_by_paths(gtk_settings_get_default(), NULL,
                                      style_name, G_TYPE_NONE);

            for (j=0; j<NUM_TEXT_VIEWS; j++) {
                /*
                 * If we have a style for this, update the tag that goes along
                 * with this.  If we don't have a tag for this style, create
                 * it.
                 */
                if (tmp_style) {
                    if (!info_pane[j].msg_type_tags[type][subtype]) {
                        info_pane[j].msg_type_tags[type][subtype] =
                            gtk_text_buffer_create_tag(info_pane[j].textbuffer, NULL, NULL);
                    }
                    set_text_tag_from_style(info_pane[j].msg_type_tags[type][subtype],
                                            tmp_style, base_style[j]);
                    has_style=1;
                } else {
                    /*
                     * No setting for this type/subtype, so remove tag if there
                     * is one.
                     */
                    if (info_pane[j].msg_type_tags[type][subtype]) {
                        gtk_text_tag_table_remove(gtk_text_buffer_get_tag_table(info_pane[j].textbuffer),
                                              info_pane[j].msg_type_tags[type][subtype]);
                        info_pane[j].msg_type_tags[type][subtype] = NULL;
                    }
                }
            }
        }

        /*
         * Old message/color support.
         */
        for (i=0; i<NUM_COLORS; i++) {
            snprintf(style_name, MAX_BUF, "info_%s", usercolorname[i]);
            tmp_style = gtk_rc_get_style_by_paths(gtk_settings_get_default(), NULL, style_name, G_TYPE_NONE);

            for (j=0; j<NUM_TEXT_VIEWS; j++) {
                if (tmp_style) {
                    if (!info_pane[j].color_tags[i]) {
                        info_pane[j].color_tags[i] = gtk_text_buffer_create_tag(info_pane[j].textbuffer,
                                                                NULL, NULL);
                    }
                    set_text_tag_from_style(info_pane[j].color_tags[i], tmp_style, base_style[j]);
                } else {
                    if (info_pane[j].color_tags[i]) {
                        gtk_text_tag_table_remove(gtk_text_buffer_get_tag_table(info_pane[j].textbuffer),
                                                  info_pane[j].color_tags[i]);
                        info_pane[j].color_tags[i] = NULL;
                    }
                }
            }
        }

        /* Font type support */
        for (i=0; i<NUM_FONTS; i++) {
            tmp_style = gtk_rc_get_style_by_paths(gtk_settings_get_default(), NULL,
                                  font_style_names[i], G_TYPE_NONE);

            for (j=0; j<NUM_TEXT_VIEWS; j++) {
                if (tmp_style) {
                    if (!info_pane[j].font_tags[i]) {
                        info_pane[j].font_tags[i] = gtk_text_buffer_create_tag(info_pane[j].textbuffer,
                                                                NULL, NULL);
                    }
                    set_text_tag_from_style(info_pane[j].font_tags[i], tmp_style, base_style[j]);
                } else {
                    if (info_pane[j].font_tags[i]) {
                        gtk_text_tag_table_remove(gtk_text_buffer_get_tag_table(info_pane[j].textbuffer),
                                                  info_pane[j].font_tags[i]);
                        info_pane[j].font_tags[i] = NULL;
                    }
                }
            }
        }
    } else {
        /*
         * There isn't any base style - this shouldn't normally be the case
         * with any real setting files, but certainly can be the case if the
         * user selected the 'None' setting.  So in this case, we just free all
         * the text tags.
         */
        has_style=0;
        for (i=0; i<sizeof(msg_type_names) / sizeof(Msg_Type_Names); i++) {
            int type, subtype;

            type =  msg_type_names[i].type;
            subtype = msg_type_names[i].subtype;

            for (j=0; j<NUM_TEXT_VIEWS; j++) {
                if (info_pane[j].msg_type_tags[type][subtype]) {
                    gtk_text_tag_table_remove(gtk_text_buffer_get_tag_table(info_pane[j].textbuffer),
                                      info_pane[j].msg_type_tags[type][subtype]);
                    info_pane[j].msg_type_tags[type][subtype] = NULL;
                }
            }
        }
        for (i=0; i<NUM_COLORS; i++) {
            for (j=0; j<NUM_TEXT_VIEWS; j++) {
                if (info_pane[j].color_tags[i]) {
                    gtk_text_tag_table_remove(gtk_text_buffer_get_tag_table(info_pane[j].textbuffer),
                                                  info_pane[j].color_tags[i]);
                    info_pane[j].color_tags[i] = NULL;
                }
            }
        }
        /* Font type support */
        for (i=0; i<NUM_FONTS; i++) {
            for (j=0; j<NUM_TEXT_VIEWS; j++) {
                if (info_pane[j].font_tags[i]) {
                    gtk_text_tag_table_remove(gtk_text_buffer_get_tag_table(info_pane[j].textbuffer),
                                                  info_pane[j].font_tags[i]);
                    info_pane[j].font_tags[i] = NULL;
                }
            }
        }
    }
}

/**
 * initializes the info displays.  The info displays are the area where
 * text is drawn.
 *
 * @param window_root
 * Parent (root) window of the application.
 */
void info_init(GtkWidget *window_root)
{
    int i;
    GtkTextIter end;
    char    widget_name[MAX_BUF];
    GladeXML *xml_tree;

    xml_tree = glade_get_widget_tree(GTK_WIDGET(window_root));
    for (i=0; i < NUM_TEXT_VIEWS; i++) {
        snprintf(widget_name, MAX_BUF, "textview_info%d", i+1);
        info_pane[i].textview = glade_xml_get_widget(xml_tree, widget_name);

        snprintf(widget_name, MAX_BUF, "scrolledwindow_textview%d", i+1);
        info_pane[i].scrolled_window =
            glade_xml_get_widget(xml_tree, widget_name);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(info_pane[i].textview), GTK_WRAP_WORD);
        info_pane[i].textbuffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(info_pane[i].textview));
        info_pane[i].adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(info_pane[i].scrolled_window));

        gtk_text_buffer_get_end_iter(info_pane[i].textbuffer, &end);
        info_pane[i].textmark =  gtk_text_buffer_create_mark(info_pane[i].textbuffer, NULL, &end, FALSE);
        gtk_widget_realize(info_pane[i].textview);
    }

    info_get_styles();

    /* Register callbacks for all message types */
    for (i=0; i<MSG_TYPE_LAST; i++)
        setTextManager(i,message_callback);
}

/**
 * Adds some data to the text buffer, using the appropriate tags to provide the
 * desired formatting.  Note that the style within the users theme determines
 * how a particular type/subtype is drawn.
 *
 * @param pane
 * @param message
 * @param type
 * The message type - see the MSG_TYPE values in newclient.h
 * @param subtype
 * Message subtype - see MSG_TYPE_..._... values in newclient.h
 * @param bold
 * If true, should be in bold text.
 * @param italic
 * If true, should be in italic text
 * @param font
 * Which font number to use - this is resolved to actual font style
 * based on the users theme file.
 * @param color
 * string version of the color
 * @param underline
 * If true, should underline the text.
 */
static void add_to_textbuf(int pane, char *message,
                           int type, int subtype,
                           int bold, int italic, int font, char *color, int underline)
{
    GtkTextIter end;
    GdkRectangle rect;
    int scroll_to_end=0, color_num;
    GtkTextTag      *color_tag=NULL, *type_tag=NULL;

    /*
     * Lets see if the defined color matches any of our defined colors.  If we
     * get a match, set color_tag.  If color_tag is null, we either don't have
     * a match, we don't have a defined tag for the color, or we don't have a
     * color, use the default tag.  It would be nice to know if color is a sub
     * value set with [color] tag, or is part of the message itself - if we're
     * just being passed NDI_RED in the draw_ext_info from the server, we
     * really don't care about that - the type/subtype styles should really be
     * what determines what color to use.
     */
    if (color) {
        for (color_num=0; color_num < NUM_COLORS; color_num++)
            if (!strcasecmp(usercolorname[color_num], color)) break;

        if (color_num < NUM_COLORS) color_tag = info_pane[pane].color_tags[color_num];
    }
    if (!color_tag) color_tag = info_pane[pane].default_tag;

    /*
     * Following block of code deals with the type/subtype.  First, we check
     * and make sure the passed in values are legal.  If so, first see if there
     * is a particular style for the type/subtype combo, if not, fall back to
     * one just for the type.
     */
    type_tag = info_pane[pane].default_tag;
    if (type >= MSG_TYPE_LAST || subtype >= max_subtype || type < 0 || subtype < 0 ) {
        LOG(LOG_ERROR, "info.c::add_to_textbuf", "type (%d) >= MSG_TYPE_LAST (%d) or subtype (%d) >= max_subtype (%d)\n",
            type, MSG_TYPE_LAST, subtype, max_subtype);
    } else {
        if (info_pane[pane].msg_type_tags[type][subtype]) type_tag = info_pane[pane].msg_type_tags[type][subtype];
        else if (info_pane[pane].msg_type_tags[type][0]) type_tag = info_pane[pane].msg_type_tags[type][0];
    }

    gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(info_pane[pane].textview), &rect);
    if ((info_pane[pane].adjustment->value + rect.height) >= info_pane[pane].adjustment->upper ) scroll_to_end=1;

    gtk_text_buffer_get_end_iter(info_pane[pane].textbuffer, &end);

    gtk_text_buffer_insert_with_tags(info_pane[pane].textbuffer, &end, message , strlen(message),
             bold?info_pane[pane].bold_tag : info_pane[pane].default_tag,
             italic?info_pane[pane].italic_tag : info_pane[pane].default_tag,
             underline?info_pane[pane].underline_tag : info_pane[pane].default_tag,
             info_pane[pane].font_tags[font]?info_pane[pane].font_tags[font] : info_pane[pane].default_tag,
             color_tag, type_tag,
             NULL);

    if (scroll_to_end)
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(info_pane[pane].textview), info_pane[pane].textmark);
}

/**
 * Unlike the gtk client, we don't do anything tricky like popups with
 * different message types.  However, we will choose different fonts, etc,
 * based on this information - for this reason, we just use one callback, and
 * change those minor things based on the callback.  We also need to parse the
 * data.
 *
 * @param orig_color
 * @param type
 * @param subtype
 * @param message
 */
static void message_callback(int orig_color, int type, int subtype, char *message) {
    char *marker, *current, *original;
    int bold=0, italic=0, font=0, underline=0;
    char *color=NULL; /**< Only if we get a [color] tag should we care,
                       * otherwise, the type/subtype should dictate color
                       * (unless no style set!)
                       */

    current = strdup(message);
    original = current;         /* Just so we know what to free */

    if (!has_style) {
        if (orig_color <0 || orig_color>NUM_COLORS) {
            LOG(LOG_ERROR,"info.c::message_callback", "Passed invalid color from server: %d, max allowed is %d\n",
                orig_color, NUM_COLORS);
            orig_color=0;
        } else {
            /*
             * Not really efficient - we have a number, but convert it to a
             * string, at which point the add_to_textbuf will convert it back
             * to a number :(
             */
            color=(char*)usercolorname[orig_color];
        }
    }

    while ((marker = strchr(current,'['))!= NULL) {
        *marker = 0;

        if (strlen(current) > 0)
            add_to_textbuf( 0,current, type, subtype, bold, italic, font, color, underline);

        current=marker+1;
        if ((marker = strchr(current,']')) == NULL) {
            free(original);
            return;
        }
        *marker = 0;
        if (!strcmp(current,"b"))               bold = TRUE;
        else if (!strcmp(current,"/b"))         bold = FALSE;
        else if (!strcmp(current,"i"))          italic = TRUE;
        else if (!strcmp(current,"/i"))         italic = FALSE;
        else if (!strcmp(current,"ul"))         underline = TRUE;
        else if (!strcmp(current,"/ul"))        underline = FALSE;
        else if (!strcmp(current,"fixed"))      font = FONT_FIXED;
        else if (!strcmp(current,"arcane"))     font = FONT_ARCANE;
        else if (!strcmp(current,"hand"))       font = FONT_HAND;
        else if (!strcmp(current,"strange"))    font = FONT_STRANGE;
        else if (!strcmp(current,"print"))      font = FONT_NORMAL;
        else if (!strcmp(current,"/color"))     color = NULL;
        else if (!strncmp(current,"color=",6))  color = current + 6;
        else LOG(LOG_INFO, "info.c::message_callback",
                 "unrecognized tag: [%s]\n", current);
        current = marker+1;

    }
    add_to_textbuf( 0,current, type, subtype, bold, italic, font, color, underline);
    add_to_textbuf( 0, "\n", type, subtype, bold, italic, font, color, underline);
    free(original);
}

/**
 * Adds a line to the info window.  note that with the textbufs, it seems you
 * need to manually set it to the bottom of the screen - otherwise, the
 * scrollbar just stays at the top.  However, I could see this not being ideal
 * if you are trying to scroll back while new stuff comes in.
 *
 * @param str
 * @param color
 */

void draw_info(const char *str, int color) {
    int ncolor = color;
    GtkTextIter end;
    GdkRectangle rect;
    int scroll_to_end=0;

    if (ncolor==NDI_WHITE) {
        ncolor=NDI_BLACK;
    }

    /*
     * This seems more complicated than it should be, but we need to see if the
     * window is scrolled at the end.  If it is, we want to keep scrolling it
     * down with new info.  If not, we don't want to change position -
     * otherwise, it makes it very difficult to look back at the old info (like
     * old messages missed during combat, looking at the shop listing while
     * people are chatting, etc) We need to find out the position before
     * putting in new text - otherwise, that operation will mess up our
     * position, and not give us right info.
     */
    gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(info_pane[0].textview), &rect);
    if ((info_pane[0].adjustment->value + rect.height) >= info_pane[0].adjustment->upper ) scroll_to_end=1;

    if (color == NDI_BLACK) {
        gtk_text_buffer_get_end_iter(info_pane[0].textbuffer, &end);
        gtk_text_buffer_insert_with_tags(info_pane[0].textbuffer, &end, str , strlen(str), info_pane[0].color_tags[ncolor], NULL);
        gtk_text_buffer_insert(info_pane[0].textbuffer, &end, "\n" , 1);

        if (scroll_to_end)
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(info_pane[0].textview), info_pane[0].textmark);
    } else {
        gtk_text_buffer_get_end_iter(info_pane[0].textbuffer, &end);
        gtk_text_buffer_insert_with_tags(info_pane[0].textbuffer, &end, str , strlen(str), info_pane[0].color_tags[ncolor], NULL);
        gtk_text_buffer_insert(info_pane[0].textbuffer, &end, "\n" , 1);

        if (scroll_to_end)
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(info_pane[0].textview), info_pane[0].textmark);

        gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(info_pane[1].textview), &rect);
        if ((info_pane[1].adjustment->value + rect.height) >= info_pane[1].adjustment->upper ) scroll_to_end=1;
        else scroll_to_end=0;

        gtk_text_buffer_get_end_iter(info_pane[1].textbuffer, &end);
        gtk_text_buffer_insert_with_tags(info_pane[1].textbuffer, &end, str , strlen(str), info_pane[1].color_tags[ncolor], NULL);
        gtk_text_buffer_insert(info_pane[1].textbuffer, &end, "\n" , 1);

        if (scroll_to_end)
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(info_pane[1].textview), info_pane[1].textmark);
    }
}

/**
 *
 * @param colr
 * @param buf
 */
void draw_color_info(int colr, const char *buf){
        draw_info(buf,colr);
}

/**
 * Clears all the message.  Not sure why someone would use it,
 * but it is called from the common area, so might as well
 * support it.
 */
void menu_clear() {
    int i;

    for (i=0; i < NUM_TEXT_VIEWS; i++) {
        gtk_text_buffer_set_text(info_pane[i].textbuffer, "", 0);
    }
}

/**
 * All the following are 'dummy' functions.  Basically, there are callbacks to
 * these from the common area, but they are not implemented in gtk, either
 * because it makes no sense (set_scroll for example), or because it may not be
 * technically possible to do so if we limit ourselves to proper GTK2 code (Eg,
 * don't mess with the internals of X or platform specific issues)
 *
 * @param s
 */
void set_scroll(char *s)
{
}

/**
 *
 @param s
 */
void set_autorepeat(char *s)
{
}

/**
 * This is used by the common help system to determine when to wrap.  Should be
 * able to get width of window, and divide by character width - however, still
 * not perfect if we are using a variable width font.  Actually, gtk can do
 * word wrapping for us, so maybe the real fix is to have it to the word
 * wrapping and just run a sufficiently large value.
 * FIXME: should be better than hardcoded value.
 *
 * @return
 * The width of the info window in characters.
 */
int get_info_width()
{
    return 40;
}

