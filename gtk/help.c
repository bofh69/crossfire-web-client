/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001-2005 Mark Wedel & Crossfire Development Team

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


#include <gtk/gtk.h>
#include "client-types.h"
#include "../common/p_cmd.h"

static GtkWidget *chelptext = NULL; /* HACK */

static void replace_text(const char * new_text) {
    gtk_text_set_point(GTK_TEXT(chelptext), 0);
    gtk_text_forward_delete(GTK_TEXT(chelptext), gtk_text_get_length(GTK_TEXT(chelptext)) );
    gtk_text_insert(GTK_TEXT(chelptext), NULL, &chelptext->style->black, NULL, new_text, -1);    
}

#define CLIENTHELP_LONG_LIST
/* Wossname. */
#define assumed_wrap 120
#define COLOSSAL_BUF 8192

/* HACK Mostly duplicated from common/p_cmd.c. */
static void set_default_text(void) {
    char out_buf[COLOSSAL_BUF];
    char tmp_buf[COLOSSAL_BUF];
    ConsoleCommand ** array_cc;
    ConsoleCommand * cc;
    int i;
    CommCat current_cat = COMM_CAT_MISC;
#ifndef CLIENTHELP_LONG_LIST
    size_t name_len;
    char line_buf[MAX_BUF];
    size_t line_len;

    line_buf[0] = '\0';
    line_len = 0;
#endif

/* HACK */
#define LINE(x) strncpy(tmp_buf, out_buf, COLOSSAL_BUF - 1); snprintf(out_buf, COLOSSAL_BUF - 1, "%s%s\n", tmp_buf, x);
        
    strcpy(out_buf, 
        "To get help on one of the commands listed below, enter the name in the text entry "
        "box below, and press Enter. To get back to this list, clear the text box and press "
        "Enter.\n"
        "\n"
        " === Client Side Command List === \n"
    );
    
    array_cc = get_cat_sorted_commands();
    
    for (i = 0; i < get_num_commands(); i++) {
        cc = array_cc[i];
        if (cc->cat != current_cat) {
            char buf[MAX_BUF];
            
#ifndef CLIENTHELP_LONG_LIST
            if (line_len > 0) {
                LINE(line_buf);
                line_buf[0] = '\0';
                line_len = 0;
            }
#endif

            snprintf(buf, MAX_BUF - 1, "%s Commands:", get_category_name(cc->cat));
            LINE(buf);
            current_cat = cc->cat; 
        }
        
#ifdef CLIENTHELP_LONG_LIST
        if (cc->desc != NULL) {
            char buf[MAX_BUF];
            snprintf(buf, MAX_BUF - 1, "%s - %s", cc->name, cc->desc);
            LINE(buf);
        } else {
            LINE(cc->name);
        }
#else
        name_len = strlen(cc->name);
        
        if (strlen(cc->name) > MAX_BUF) {
            LINE(cc->name);
        } else if (name_len > assumed_wrap) {
            LINE(line_buf);
            LINE(cc->name);
            line_len = 0;
        } else if (line_len + name_len > assumed_wrap) {
            LINE(line_buf);
            strncpy(line_buf, cc->name, name_len + 1);
            line_len = name_len;
        } else {
            if (line_len > 0) {
                strncat(line_buf, " ", 2);
                line_len += 1;
            }
            strncat(line_buf, cc->name, name_len + 1);
            line_len += name_len;
        }
#endif
    }
    
#ifndef CLIENTHELP_LONG_LIST
    /* Dump dangling commands. Been there, got the fencepost.
    Or is it a gap? */
    if (line_len > 0) {
        LINE(line_buf);
    }
#endif
    
    replace_text(out_buf);
}

static void chelp_entry_callback(GtkWidget * cargo_cult_ignored, GtkWidget * topic_entry) {
    char buf[MAX_BUF];
    const gchar * topic;
    const ConsoleCommand * cc;
    /* LOG(LOG_INFO, "chelp_entry_callback", "Got %s", gtk_entry_get_text(GTK_ENTRY(topic_entry))); */

    topic = gtk_entry_get_text(GTK_ENTRY(topic_entry));
    /* TODO Select it, in case typing replaces selection. */
    
    if (topic == NULL || strlen(topic) <= 0) {
         set_default_text();
         return;
    }
        
    cc = find_command(topic);

    if (cc == NULL) {
        snprintf(buf, MAX_BUF - 1, "No command '%s' found.", topic);
        replace_text(buf);
    } else {
        char out_buf[COLOSSAL_BUF];
        const char * extended;

        if (cc->desc != NULL) {
            snprintf(buf, MAX_BUF - 1, "%s - %s",
                cc->name,
                cc->desc);
        } else {
            snprintf(buf, MAX_BUF - 1, cc->name);
        }

        if (cc->helpfunc == NULL) {
            extended = "This command is undocumented.";
        } else { 
            extended = cc->helpfunc();
            if (extended == NULL) {
                extended = "This command is not yet documented.";
            }
        }

        snprintf(out_buf, COLOSSAL_BUF - 1, "%s Command:\n%s\n\n%s", 
            get_category_name(cc->cat),
            buf,
            extended);
        replace_text(out_buf);
    }
}

static GtkWidget *gtkwin_chelp = NULL;

void chelpdialog(GtkWidget *widget) {
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *helpbutton;
  GtkWidget *vscrollbar;
  GtkWidget * topic_entry;
  GtkWidget * lblCommand;
  /*  GtkStyle *style;*/


  if(gtkwin_chelp == NULL) {
    
    gtkwin_chelp = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_position (GTK_WINDOW (gtkwin_chelp), GTK_WIN_POS_CENTER);
    gtk_widget_set_usize (gtkwin_chelp,400,300);
    gtk_window_set_title (GTK_WINDOW (gtkwin_chelp), "Crossfire Client-Side Command Help");
    gtk_window_set_policy (GTK_WINDOW (gtkwin_chelp), TRUE, TRUE, FALSE);

    gtk_signal_connect (GTK_OBJECT (gtkwin_chelp), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &gtkwin_chelp);
    
    gtk_container_border_width (GTK_CONTAINER (gtkwin_chelp), 1);
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER(gtkwin_chelp),vbox);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    chelptext = gtk_text_new (NULL, NULL);
    gtk_text_set_editable (GTK_TEXT (chelptext), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox),chelptext, TRUE, TRUE, 0);
    gtk_widget_show (chelptext);

    vscrollbar = gtk_vscrollbar_new (GTK_TEXT (chelptext)->vadj);
    gtk_box_pack_start (GTK_BOX (hbox),vscrollbar, FALSE, FALSE, 0);
 
    gtk_widget_show (vscrollbar);
    gtk_widget_show (hbox);

    hbox = gtk_hbox_new(FALSE, 2);
    lblCommand = gtk_label_new("Command: ");
    gtk_box_pack_start(GTK_BOX(hbox), lblCommand, FALSE, FALSE, 0);

    topic_entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (hbox), topic_entry, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(topic_entry), "activate",
		     GTK_SIGNAL_FUNC(chelp_entry_callback),
		     topic_entry);
    /* TODO Make it a combo box? No? */
    
    helpbutton = gtk_button_new_with_label ("Close");
    gtk_signal_connect_object (GTK_OBJECT (helpbutton), "clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       GTK_OBJECT (gtkwin_chelp));
    gtk_box_pack_end (GTK_BOX (hbox), helpbutton, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (lblCommand);
    gtk_widget_show (topic_entry);
    gtk_widget_show (helpbutton);
    gtk_widget_show (hbox);

    gtk_widget_show (vbox);
    gtk_widget_show (gtkwin_chelp);
    set_default_text();
  }
  else { 
    gdk_window_raise (gtkwin_chelp->window);
  }
}

