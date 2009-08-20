const char * const rcsid_gtk2_info_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005-2008 Mark Wedel & Crossfire Development Team

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
 * @{
 * @name GTK V2 Font Style Definitions.
 * Font style support definitions for the info window.
 * Font style defines are indices into the font_style_names[] array.
 * The actual fonts that they are bound to are set up in the style file.
 */
#define FONT_NORMAL     0
#define FONT_ARCANE     1
#define FONT_STRANGE    2
#define FONT_FIXED      3
#define FONT_HAND       4
#define NUM_FONTS       5

/**
 * A mapping of font numbers to style based on the rcfile content.
 */
static char *font_style_names[NUM_FONTS] = {
    "info_font_normal",
    "info_font_arcane",
    "info_font_strange",
    "info_font_fixed",
    "info_font_hand"
};
/**
 * @} EndOf GTK V2 Font Style Definitions.
 */

/**
 * The number of supported message panes (normal + critical).  This define is
 * meant to support anything that iterates over all the information panels.
 * It does nothing to help remove or document hardcoded panel numbers
 * throughout the code.  FIXME:  Create defines for each panel and
 * replace panel numbers with the defines describing the panel.
 */
#define NUM_TEXT_VIEWS  2

extern  const char * const usercolorname[NUM_COLORS];

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

static void message_callback(int orig_color, int type, int subtype, char *message);
void draw_ext_info(int orig_color, int type, int subtype, char *message);

extern  const char * const colorname[NUM_COLORS];

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
 * @{
 * @name GTK V2 Client Output Count and Sync Definitions.
 * Supports a client-side implementation of what used to be provided by the
 * server output-count and output-sync commands.  These defines presently
 * control the way the system works.  The hardcoded values here are temporary
 * and shall give way to client commands and/or a GUI method of configuring
 * the system.  Turn off the output count/sync by setting MESSAGE_COUNT_MAX
 * to 1.  It should be safe to experiment with most values as long as none of
 * them are set less than 1, and as long as the two buffer sizes are set to
 * reasonable values (buffer sizes include the terminating null character).
 */
#define MESSAGE_BUFFER_COUNT 10         /**< The maximum number of messages
                                         *   to concurrently monitor for
                                         *   duplicate occurances.          */
#define MESSAGE_BUFFER_SIZE  56         /**< The maximum allowable size of
                                         *   messages that are checked for
                                         *   duplicate reduction.           */
#define COUNT_BUFFER_SIZE     8         /**< The maximum size of the tag
                                         *   that indicates the number of
                                         *   times a message occured while
                                         *   buffered.  Example:  " (4x)"   */
#define MESSAGE_COUNT_MAX    16         /**< The maximum number of times a
                                         *   buffered message may repeat
                                         *   before it is sent to a client
                                         *   panel for for display.         */
#define MESSAGE_AGE_MAX      16         /**< The maximum time in client
                                         *   ticks, that a message resides
                                         *   in a buffer before it is sent
                                         *   to a client panel for display.
                                         *   8 ticks is roughly 1 second.   */
/** @struct info_buffer_t
  * @brief A buffer record that supports suppression of duplicate messages.
  * This buffer holds data for messages that are monitored for suppression
  * of duplicates.  The buffer holds all data passed to message_callback(),
  * including type, subtype, suggested color, and the text.  Age and count
  * fields are provided to track the time a message is in the buffer, and
  * how many times it occured during the time it is buffered.
  */
struct info_buffer_t
{
  int  age;                             /**< The length of time a message
                                         *   spends in the buffer, measured
                                         *   in client ticks.  An age of -1
                                         *   indicates the buffer is empty
                                         *   and all other data is invalid.
                                         *   It is set 0 when a message
                                         *   is placed into the buffer.     */
  int  count;                           /**< The number of times a buffered
                                         *   message is detected while it
                                         *   is buffered.                   */
  int  orig_color;                      /**< Message data:  The suggested
                                         *   color to display the text in.  */
  int  type;                            /**< Message data:  Classification
                                         *   of the buffered message.       */
  int  subtype;                         /**< Message data:  Sub-class of
                                         *   the buffered message.          */
  char message[MESSAGE_BUFFER_SIZE      /**< Message data:  Message text.   */
               + COUNT_BUFFER_SIZE];     
} info_buffer[MESSAGE_BUFFER_COUNT];    /**< Several buffers that support
                                         *   suppression of duplicates even
                                         *   even when the duplicates are
                                         *   alternate with other messages. */
/**
 * @} EndOf GTK V2 Client Output Count/Sync Definitions.
 */

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

    if (memcmp(
        &style->fg[GTK_STATE_NORMAL],
        &base_style->fg[GTK_STATE_NORMAL],
        sizeof(GdkColor)))

        g_object_set(tag, "foreground-gdk", &style->fg[GTK_STATE_NORMAL], NULL);

    if (memcmp(
        &style->bg[GTK_STATE_NORMAL],
        &base_style->bg[GTK_STATE_NORMAL],
        sizeof(GdkColor)))

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
void info_get_styles(void)
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
        for (i = 0; i < sizeof(msg_type_names) / sizeof(Msg_Type_Names); i++) {
            if (msg_type_names[i].subtype > max_subtype)
                max_subtype = msg_type_names[i].subtype;
        }
        for (j = 0; j < NUM_TEXT_VIEWS; j++) {
            for (i = 0; i < MSG_TYPE_LAST; i++)
                info_pane[j].msg_type_tags[i] =
                     calloc(max_subtype + 1, sizeof(GtkTextTag*));

            for (i = 0; i < NUM_FONTS; i++)
                info_pane[j].font_tags[i] = NULL;

            for (i = 0; i < NUM_COLORS; i++)
                info_pane[j].color_tags[i] = NULL;
            /*
             * These tag definitions never change - we don't get them from the
             * settings file (maybe we should), so we only need to allocate
             * them once.
             */
            info_pane[j].bold_tag =
                gtk_text_buffer_create_tag(info_pane[j].textbuffer,
                    "bold", "weight", PANGO_WEIGHT_BOLD, NULL);

            info_pane[j].italic_tag =
                 gtk_text_buffer_create_tag(info_pane[j].textbuffer,
                     "italic", "style", PANGO_STYLE_ITALIC, NULL);

            info_pane[j].underline_tag =
                gtk_text_buffer_create_tag(info_pane[j].textbuffer,
                    "underline", "underline", PANGO_UNDERLINE_SINGLE, NULL);
            /*
             * This is really a convenience - we can pass multiple tags in when
             * drawing text, but once we pass in a NULL tag, that signifies no
             * more tags.  Rather than having to set up an array we pass in,
             * instead, we have this empty tag that we can pass is so that we
             * always have the same calling semantics, just differ what tags we
             * pass in.
             */
            if (!info_pane[j].default_tag)
                info_pane[j].default_tag =
                    gtk_text_buffer_create_tag(info_pane[j].textbuffer,
                        "default", NULL);
        }
        has_init = 1;
    }
    for (i = 0; i < NUM_TEXT_VIEWS; i++) {
        base_style[i] =
            gtk_rc_get_style_by_paths(
                gtk_settings_get_default(),
                NULL, "info_default", G_TYPE_NONE);
    }
    if (!base_style[0]) {
        LOG(LOG_INFO, "info.c::info_get_styles",
            "Unable to find base style info_default"
            " - will not process most info tag styles!");
    }

    has_style = 0;

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
        for (i = 0; i < sizeof(msg_type_names) / sizeof(Msg_Type_Names); i++) {
            int type, subtype;

            snprintf(style_name, sizeof(style_name), "msg_%s", msg_type_names[i].style_name);
            type =  msg_type_names[i].type;
            subtype = msg_type_names[i].subtype;

            tmp_style =
                gtk_rc_get_style_by_paths(
                    gtk_settings_get_default(), NULL, style_name, G_TYPE_NONE);

            for (j = 0; j < NUM_TEXT_VIEWS; j++) {
                /*
                 * If we have a style for this, update the tag that goes along
                 * with this.  If we don't have a tag for this style, create
                 * it.
                 */
                if (tmp_style) {
                    if (!info_pane[j].msg_type_tags[type][subtype]) {
                        info_pane[j].msg_type_tags[type][subtype] =
                            gtk_text_buffer_create_tag(
                                info_pane[j].textbuffer, NULL, NULL);
                    }
                    set_text_tag_from_style(
                        info_pane[j].msg_type_tags[type][subtype],
                        tmp_style, base_style[j]);
                    has_style = 1;
                } else {
                    /*
                     * No setting for this type/subtype, so remove tag if there
                     * is one.
                     */
                    if (info_pane[j].msg_type_tags[type][subtype]) {
                        gtk_text_tag_table_remove(
                            gtk_text_buffer_get_tag_table(
                                info_pane[j].textbuffer),
                            info_pane[j].msg_type_tags[type][subtype]);
                        info_pane[j].msg_type_tags[type][subtype] = NULL;
                    }
                }
            }
        }

        /*
         * Old message/color support.
         */
        for (i = 0; i < NUM_COLORS; i++) {
            snprintf(style_name, MAX_BUF, "info_%s", usercolorname[i]);

            tmp_style =
                gtk_rc_get_style_by_paths(
                    gtk_settings_get_default(), NULL, style_name, G_TYPE_NONE);

            for (j = 0; j < NUM_TEXT_VIEWS; j++) {
                if (tmp_style) {
                    if (!info_pane[j].color_tags[i]) {
                        info_pane[j].color_tags[i] =
                            gtk_text_buffer_create_tag(
                                info_pane[j].textbuffer, NULL, NULL);
                    }
                    set_text_tag_from_style(
                        info_pane[j].color_tags[i],
                        tmp_style, base_style[j]);
                } else {
                    if (info_pane[j].color_tags[i]) {
                        gtk_text_tag_table_remove(
                            gtk_text_buffer_get_tag_table(
                                info_pane[j].textbuffer),
                            info_pane[j].color_tags[i]);
                        info_pane[j].color_tags[i] = NULL;
                    }
                }
            }
        }

        /* Font type support */
        for (i = 0; i < NUM_FONTS; i++) {
            tmp_style =
                gtk_rc_get_style_by_paths(
                    gtk_settings_get_default(),
                    NULL, font_style_names[i], G_TYPE_NONE);

            for (j = 0; j < NUM_TEXT_VIEWS; j++) {
                if (tmp_style) {
                    if (!info_pane[j].font_tags[i]) {
                        info_pane[j].font_tags[i] =
                            gtk_text_buffer_create_tag(
                                info_pane[j].textbuffer, NULL, NULL);
                    }
                    set_text_tag_from_style(
                        info_pane[j].font_tags[i], tmp_style, base_style[j]);
                } else {
                    if (info_pane[j].font_tags[i]) {
                        gtk_text_tag_table_remove(
                            gtk_text_buffer_get_tag_table(
                                info_pane[j].textbuffer),
                            info_pane[j].font_tags[i]);
                        info_pane[j].font_tags[i] = NULL;
                    }
                }
            }
        }
    } else {
        /*
         * There is no base style - this should not normally be the case
         * with any real setting files, but certainly can be the case if the
         * user selected the 'None' setting.  So in this case, we just free all
         * the text tags.
         */
        has_style = 0;
        for (i = 0; i < sizeof(msg_type_names) / sizeof(Msg_Type_Names); i++) {
            int type, subtype;

            type = msg_type_names[i].type;
            subtype = msg_type_names[i].subtype;

            for (j = 0; j < NUM_TEXT_VIEWS; j++) {
                if (info_pane[j].msg_type_tags[type][subtype]) {
                    gtk_text_tag_table_remove(
                        gtk_text_buffer_get_tag_table(
                            info_pane[j].textbuffer),
                        info_pane[j].msg_type_tags[type][subtype]);
                    info_pane[j].msg_type_tags[type][subtype] = NULL;
                }
            }
        }
        for (i = 0; i < NUM_COLORS; i++) {
            for (j = 0; j < NUM_TEXT_VIEWS; j++) {
                if (info_pane[j].color_tags[i]) {
                    gtk_text_tag_table_remove(
                        gtk_text_buffer_get_tag_table(
                            info_pane[j].textbuffer),
                        info_pane[j].color_tags[i]);
                    info_pane[j].color_tags[i] = NULL;
                }
            }
        }
        /* Font type support */
        for (i = 0; i < NUM_FONTS; i++) {
            for (j = 0; j < NUM_TEXT_VIEWS; j++) {
                if (info_pane[j].font_tags[i]) {
                    gtk_text_tag_table_remove(
                        gtk_text_buffer_get_tag_table(
                            info_pane[j].textbuffer),
                        info_pane[j].font_tags[i]);
                    info_pane[j].font_tags[i] = NULL;
                }
            }
        }
    }
}

/**
 * Initialize the information panels in the client.  These panels are the
 * client areas where text is drawn.
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
    for (i = 0; i < NUM_TEXT_VIEWS; i++) {
        snprintf(widget_name, MAX_BUF, "textview_info%d", i+1);
        info_pane[i].textview = glade_xml_get_widget(xml_tree, widget_name);

        snprintf(widget_name, MAX_BUF, "scrolledwindow_textview%d", i+1);

        info_pane[i].scrolled_window =
            glade_xml_get_widget(xml_tree, widget_name);

        gtk_text_view_set_wrap_mode(
            GTK_TEXT_VIEW(info_pane[i].textview), GTK_WRAP_WORD);

        info_pane[i].textbuffer =
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(info_pane[i].textview));

        info_pane[i].adjustment =
            gtk_scrolled_window_get_vadjustment(
                GTK_SCROLLED_WINDOW(info_pane[i].scrolled_window));

        gtk_text_buffer_get_end_iter(info_pane[i].textbuffer, &end);

        info_pane[i].textmark =
            gtk_text_buffer_create_mark(
                info_pane[i].textbuffer, NULL, &end, FALSE);

        gtk_widget_realize(info_pane[i].textview);
    }

    info_get_styles();
    info_buffer_init();

    /* Register callbacks for all message types */
    for (i = 0; i < MSG_TYPE_LAST; i++)
        setTextManager(i, message_callback);
}

/**
 * Adds some data to the text buffer of the specified information panel using
 * the appropriate tags to provide the desired formatting.  Note that the style
 * within the users theme determines how a particular type/subtype is drawn.
 *
 * @param pane
 * The client message panel to write a message to.
 * @param message
 * A pointer to some text to display in a client message window.
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
                           int bold, int italic,
                           int font, char *color, int underline)
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
        for (color_num = 0; color_num < NUM_COLORS; color_num++)
            if (!strcasecmp(usercolorname[color_num], color))
                break;
        if (color_num < NUM_COLORS)
            color_tag = info_pane[pane].color_tags[color_num];
    }
    if (!color_tag)
        color_tag = info_pane[pane].default_tag;

    /*
     * Following block of code deals with the type/subtype.  First, we check
     * and make sure the passed in values are legal.  If so, first see if there
     * is a particular style for the type/subtype combo, if not, fall back to
     * one just for the type.
     */
    type_tag = info_pane[pane].default_tag;

    if (type >= MSG_TYPE_LAST
    || subtype >= max_subtype
    || type < 0 || subtype < 0 ) {
        LOG(LOG_ERROR, "info.c::add_to_textbuf",
            "type (%d) >= MSG_TYPE_LAST (%d) or "
            "subtype (%d) >= max_subtype (%d)\n",
            type, MSG_TYPE_LAST, subtype, max_subtype);
    } else {
        if (info_pane[pane].msg_type_tags[type][subtype])
            type_tag = info_pane[pane].msg_type_tags[type][subtype];
        else if (info_pane[pane].msg_type_tags[type][0])
            type_tag = info_pane[pane].msg_type_tags[type][0];
    }

    gtk_text_view_get_visible_rect(
        GTK_TEXT_VIEW(info_pane[pane].textview), &rect);

    if ((info_pane[pane].adjustment->value + rect.height)
        >= info_pane[pane].adjustment->upper)
            scroll_to_end = 1;

    gtk_text_buffer_get_end_iter(info_pane[pane].textbuffer, &end);

    gtk_text_buffer_insert_with_tags(
        info_pane[pane].textbuffer, &end, message, strlen(message),
        bold ? info_pane[pane].bold_tag : info_pane[pane].default_tag,
        italic ? info_pane[pane].italic_tag : info_pane[pane].default_tag,
        underline ? info_pane[pane].underline_tag : info_pane[pane].default_tag,
        info_pane[pane].font_tags[font] ?
            info_pane[pane].font_tags[font] : info_pane[pane].default_tag,
        color_tag, type_tag, NULL);

    if (scroll_to_end)
        gtk_text_view_scroll_mark_onscreen(
            GTK_TEXT_VIEW(info_pane[pane].textview), info_pane[pane].textmark);
}

/**
 * A message processor that accepts messages along with meta information color
 * and type.  The message type and subtype are analyzed to select font and
 * other text attributes.  All gtk-v2 client messages pass through this
 * processor before being output.  Before addition of the output buffering
 * feature, this was the message callback function.  It is a separate function
 * so that it can be called both by the callback, and but buffer maintenance
 * functions.
 *
 * Client-sourced messages generally should be passed directly to this handler
 * instead of to the callback.  This will save some overhead as the callback
 * implements a system that coalesces duplicate messages - a feature that is
 * not really applicable to most messages that do not come from the server.
 *
 * @param orig_color
 * A suggested text color that may change based on message type/subtype.
 * @param type
 * The message type. See the MSG_TYPE definitions in newclient.h
 * @param subtype
 * Message subtype.  See MSG_TYPE_..._... values in newclient.h
 * @param message
 * The message text.
 */
void draw_ext_info(int orig_color, int type, int subtype, char *message) {
    char *marker, *current, *original;
    int bold=0, italic=0, font=0, underline=0;
    int pane=0;       /**< Which pane the incoming message should go to.
                       */
    char *color=NULL; /**< Only if we get a [color] tag should we care,
                       *   otherwise, the type/subtype should dictate color
                       *   (unless no style set!)
                       */

    current = strdup(message);
    original = current;         /* Just so we know what to free */

    /*
     * Route messages to different information panels based on the type of the
     * message text.  By default, messages go to the main information panel.
     * Certain message types are considered critical, and they are rerouted to
     * the secondary message panel.  See MSG_TYPE definitions in newclient.h
     * for a complete listing of the available types.  msgtypes.h also helps
     * clarify what different message types are used for.
     */
    if (type == MSG_TYPE_ATTRIBUTE
    ||  type == MSG_TYPE_COMMUNICATION
    ||  type == MSG_TYPE_DIALOG
    ||  type == MSG_TYPE_VICTIM)
    {
        /* Critical messages */
        pane = 1;
    } else {
        /* All other messages */
        pane = 0;
    }

    /*
     * If there is no style information, or if a specific style has not been
     * set for the type/subtype of this message, allow orig_color to set the
     * color of the text.  The orig_color handling here adds compatibility
     * with former draw_info() calls that gave a color hint.  The color hint
     * still works now in the event that the theme has not set a style for
     * the message type.
     */
    if (! has_style || info_pane[pane].msg_type_tags[type][subtype] == 0) {
        if (orig_color <0 || orig_color>NUM_COLORS) {
            LOG(LOG_ERROR, "info.c::message_callback",
                "Passed invalid color from server: %d, max allowed is %d\n",
                orig_color, NUM_COLORS);
            orig_color = 0;
        } else {
            /*
             * Not efficient - we have a number, but convert it to a string, at
             * which point add_to_textbuf() converts it back to a number :(
             */
            color = (char*)usercolorname[orig_color];
        }
    }

    while ((marker = strchr(current, '[')) != NULL) {
        *marker = 0;

        if (strlen(current) > 0)
            add_to_textbuf(pane,
                current, type, subtype, bold, italic, font, color, underline);

        current = marker + 1;

        if ((marker = strchr(current, ']')) == NULL) {
            free(original);
            return;
        }

        *marker = 0;
        if (!strcmp(current, "b"))               bold = TRUE;
        else if (!strcmp(current,  "/b"))        bold = FALSE;
        else if (!strcmp(current,  "i"))         italic = TRUE;
        else if (!strcmp(current,  "/i"))        italic = FALSE;
        else if (!strcmp(current,  "ul"))        underline = TRUE;
        else if (!strcmp(current,  "/ul"))       underline = FALSE;
        else if (!strcmp(current,  "fixed"))     font = FONT_FIXED;
        else if (!strcmp(current,  "arcane"))    font = FONT_ARCANE;
        else if (!strcmp(current,  "hand"))      font = FONT_HAND;
        else if (!strcmp(current,  "strange"))   font = FONT_STRANGE;
        else if (!strcmp(current,  "print"))     font = FONT_NORMAL;
        else if (!strcmp(current,  "/color"))    color = NULL;
        else if (!strncmp(current, "color=", 6)) color = current + 6;
        else
            LOG(LOG_INFO, "info.c::message_callback",
                "unrecognized tag: [%s]\n", current);

        current = marker + 1;
    }

    add_to_textbuf(
        pane, current, type, subtype, bold, italic, font, color, underline);

    add_to_textbuf(
        pane, "\n", type, subtype, bold, italic, font, color, underline);

    free(original);
}

/**
 * @defgroup GTKv2OutputCountSync GTK V2 client output count/sync functions.
 * @{
 */

/**
 * Output count/sync message buffer initialization to set all buffers empty.
 * Called only once at client start from info_init(), the function initializes
 * all message buffers to the empty state (age == -1). When buffers are marked
 * empty, all other data in the buffer is considered invalid and is ignored,
 * so there is no need to clear and set up the other fields in the buffer
 * structure.
 */
void info_buffer_init() {
    int loop;                           

    for (loop = 0; loop < MESSAGE_BUFFER_COUNT; loop += 1)
        info_buffer[loop].age = -1;
};

/**
 * Displays output count/sync system buffered message text with times found.
 * Whenever a message must be ejected from the output count/sync system
 * buffers, this function is called.  If the message was seen multiple times
 * while buffered, an occurance tag is appended to the message text.  After
 * submitting the message for display, the buffer is marked empty.
 * @param id
 * The output count/sync message buffer to flush (0 - MESSAGE_BUFFER_COUNT).
 */
void info_buffer_flush(const int id) {
    char times[COUNT_BUFFER_SIZE];      /* Duplicate message count string.  */

    /*
     * Only report the number of times the message was seen if it was more
     * than once.
     */
    if (info_buffer[id].count > 1) {
        snprintf(times, COUNT_BUFFER_SIZE, " (%ux)", info_buffer[id].count);
        strcat(info_buffer[id].message, times);
    }
    /*
     * Output the message.
     */
    draw_ext_info(
        info_buffer[id].orig_color,
        info_buffer[id].type,
        info_buffer[id].subtype,
        info_buffer[id].message);
    /*
     * Mark the buffer empty.
     */
    info_buffer[id].age = -1;
};

/**
 * Output count/sync buffer maintainer adds buffer time and output messages.
 * For every tick that data sits in a message buffer, age the message so it
 * eventually gets displayed.  If the data in a buffer reaches the maximum
 * permissible age or message occurance count, it is ejected and displayed.
 */
void info_buffer_tick() {
    int loop;

    for (loop = 0; loop < MESSAGE_BUFFER_COUNT; loop += 1) {
        if (info_buffer[loop].age > -1) {
            if ((info_buffer[loop].age < MESSAGE_AGE_MAX)
            &&  (info_buffer[loop].count < MESSAGE_COUNT_MAX))
                /*
                 * The buffer has data in it, and has not reached maximum age,
                 * so bump the age up a notch.
                 */
                info_buffer[loop].age += 1;
            else
                /*
                 * The data has been in the buffer too long, so display it in
                 * the client (and report how many times it was seen while in
                 * the buffer.
                 */
                info_buffer_flush(loop);
        }
    }
}

/**
 * A callback to accept messages along with meta information color and type.
 * Unlike the GTK V1 client, we don't do anything tricky like popups with
 * different message types.  However, we will choose different fonts, etc,
 * based on this information - for this reason, we just use one callback, and
 * change those minor things based on the callback.  The message is parsed to
 * handle embedded style codes.
 *
 * Client-sourced messages should be passed directly to draw_ext_info() and
 * not through the callback.  MSG_TYPE_CLIENT messages are deliberately not
 * buffered here because they are generally unique, adminstrative messages
 * that should not be delayed.
 *
 * @param orig_color
 * A suggested text color that may change based on message type/subtype.
 * @param type
 * The message type. See the MSG_TYPE definitions in newclient.h
 * @param subtype
 * Message subtype.  See MSG_TYPE_..._... values in newclient.h
 * @param message
 * The message text.
 */
static void message_callback(int orig_color, int type, int subtype, char *message) {
    int search;                         /* Loop for searching the buffers.  */
    int found;                          /* Which buffer a message is in.    */
    int empty;                          /* The ID of an empty buffer.       */
    int oldest;                         /* Oldest buffered message found.   */
    int oldage;                         /* Age of oldest buffered message.  */

    /*
     * Message buffering is a feature that may be enabled or disabled by the
     * player.  If the system is turned off, simply forward all messages to
     * the output handler with no additional processing.
     *
     * A legacy switch to prevent message folding is to set the color of the
     * message to NDI_UNIQUE.
     *
     * Messages sourced by the client are generally administrative in nature,
     * and should not be buffered.
     *
     * The system also declines to buffer messages over a set length as most
     * messages that need coalescing are short.  Most messages that are long
     * are usually unique and should not be delayed.  >= allows for the null
     * at the end of the string in the buffer. IE. If the buffer size is 40,
     * only 39 chars can be put into it to ensure room for a null character.
     */
    if ((MESSAGE_COUNT_MAX <= 1)        /* The player buffer on/off switch. */
    ||  (type <  MSG_TYPE_SKILL)
    ||  (type == MSG_TYPE_CLIENT)
    ||  (type == MSG_TYPE_COMMUNICATION)
    ||  (orig_color == NDI_UNIQUE)
    ||  (strlen(message) >= MESSAGE_BUFFER_SIZE)) {
        /*
         * If the message buffering feature is off, simply pass the message on
         * to the parser that will determine the panel routing and style.
         */
        draw_ext_info(orig_color, type, subtype, message);
    } else {
        empty  = -1;       /* Default:  Buffers are empty until proven full */
        found  = -1;       /* Default:  Incoming message is not in a buffer */
        oldest = -1;       /* Default:  Oldest buffered message is unknown  */
        oldage = -1;       /* Default:  Oldest message age is not known     */

        for (search = 0; search < MESSAGE_BUFFER_COUNT; search += 1) {
            /*
             * Find an empty buffer if one exists.  If the message is not
             * currently buffered, we need to find a place to put it.
             * All fields (except age) of an empty buffer are invalid.
             */
            if (info_buffer[search].age == -1) {
                /*
                 * We only care about finding the first empty buffer.
                 */
                if (empty < 0) {
                    empty = search;
                }
            } else {
                /*
                 * The buffer is not empty, so process it to find the oldest
                 * buffered message so if we get a new message but no buffers
                 * are free, we can dump the oldest message to make room.
                 */
                if (info_buffer[search].age > oldage) {
                    oldest = search;
                    oldage = info_buffer[search].age;
                }
                /*
                 * Also check to see if the incoming message matches the
                 * buffer, but only if the buffer is full.
                 */
                if ((info_buffer[search].age > -1)
                &&  ! strcmp(message, info_buffer[search].message)) {
                    found  = search;
                }
            }
        }

        if FALSE {
            LOG(LOG_DEBUG, "info.c::message_callback", "\n           "
                "type: %d-%d empty: %d found: %d oldest: %d oldage: %d",
                    type, subtype, empty, found, oldest, oldage);
        }

        /*
         * If the current message is already buffered, then increment the
         * message count and exit, otherwise add the message to the buffer.
         */
        if (found > -1) {
            info_buffer[found].count += 1;
        } else {
            /*
             * The message was not found in the buffer, so check if there is
             * an available buffer.  If not, dump the oldest buffer to make
             * room, then mark it empty.
             */
            if (empty == -1) {
                if (oldest > -1) {
                    /*
                     * The oldest buffer is getting kicked out of the buffer
                     * to make room for a new message coming in.  Move it into
                     * a buffer that will be output to the player, and then
                     * mark the buffer empty so the new message can go in.
                     */
                    draw_ext_info(
                        info_buffer[oldest].orig_color,
                        info_buffer[oldest].type,
                        info_buffer[oldest].subtype,
                        info_buffer[oldest].message);
                    info_buffer[oldest].age = -1;
                    empty = oldest;
                } else {
                    LOG(LOG_ERROR, "info.c::message_callback",
                        "Buffer full; oldest unknown", strlen(message));
                }
            }
            /*
             * There should always be an empty buffer at this point, but just
             * in case, recheck before putting the new message in the buffer.
             * Do not log another error as one was just logged, but instead
             * just output the message that came in without passing it through
             * the buffer system.
             */
            if (empty > -1) {
               /*
                * Copy the incoming message to the empty buffer.
                */
                info_buffer[empty].age = 0;
                info_buffer[empty].count = 1;
                info_buffer[empty].orig_color = orig_color;
                info_buffer[empty].type = type;
                info_buffer[empty].subtype = subtype;
                strcpy(info_buffer[empty].message, message);
            } else {
                /*
                 * Something went wrong.  The message could not be buffered,
                 * so print it out directly.
                 */
                draw_ext_info(orig_color, type, subtype, message);
            }
        }
    }
}

/**
 * @} */ /* EndOf GTKv2OutputCountSync
 */

/**
 * Clears all the message panels.  It is not clear why someone would use it,
 * but is called from the common area, and so is supported here.
 */
void menu_clear(void) {
    int i;

    for (i=0; i < NUM_TEXT_VIEWS; i++) {
        gtk_text_buffer_set_text(info_pane[i].textbuffer, "", 0);
    }
}

/**
 * A stub function that does nothing.  These are callbacks used by the common
 * code, but they are not implemented in GTK, either because it makes no sense
 * (set_scroll for example), or because it may not be technically possible to
 * do so if we limit ourselves to proper GTK2 code (Eg, don't mess with the
 * internals of X or platform specific issues)
 *
 * @param s
 */
void set_scroll(const char *s)
{
}

/**
 * A stub function that does nothing.  These are callbacks used by the common
 * code, but they are not implemented in GTK, either because it makes no sense
 * (set_scroll for example), or because it may not be technically possible to
 * do so if we limit ourselves to proper GTK2 code (Eg, don't mess with the
 * internals of X or platform specific issues)
 *
 @param s
 */
void set_autorepeat(const char *s)
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
int get_info_width(void)
{
    return 40;
}
