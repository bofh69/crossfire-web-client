char *rcsid_gtk_text_c =
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
#include "config.h"
#include <stdio.h>
#include <gtk/gtk.h>
#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include <gdk/gdkwin32.h>
#endif
#include "client.h"
#include "gx11.h"
#include "gtkproto.h"
char* NO_TITLE="[no title]";
GtkWidget* book_root = NULL;
GtkWidget* book_notes = NULL;
GdkBitmap* btnClose_bm = NULL;
GdkPixmap* btnClose_pm = NULL;

void prepare_book_window(){
    if (!book_root){
        #include "pixmaps/close.xpm"
        book_root= gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(book_root),"books");
        book_notes = gtk_notebook_new();
        gtk_notebook_set_tab_pos(GTK_NOTEBOOK(book_notes),GTK_POS_LEFT);
        gtk_container_add(GTK_CONTAINER(book_root),book_notes);
        gtk_widget_show(GTK_WIDGET(book_notes));  
        gtk_widget_show(GTK_WIDGET(book_root));              
        btnClose_pm = gdk_pixmap_create_from_xpm_d(book_root->window,&btnClose_bm,
                &gtk_widget_get_style(book_root)->bg[GTK_STATE_NORMAL],
                (gchar**)close_xpm);
            
        gtk_signal_connect (GTK_OBJECT (book_root), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &book_root);      
        gtk_window_set_default_size(GTK_WINDOW(book_root),500,600);
        gtk_window_set_position(GTK_WINDOW(book_root),GTK_WIN_POS_CENTER);
    }
}
/**
 * Parse message, extract multimedia information, and push
 * as appropriate in the GtkText
 */
char *arcane_medium_fontname[]={
    "-*-cuneifontlight-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-linotext-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-blackforest-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-becker-*-*-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-arnoldboecklin-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-caligula-*-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
char *hand_medium_fontname[]={
    "-*-dobkinscript-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-coronetscript-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-muriel-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-genoa-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-parkavenue-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-rechtmanscript-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
};
char *strange_medium_fontname[]={
    "-*-annstone-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-shalomstick-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    
};
char *print_medium_fontname[]={
    "-*-arial-medium-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-bookman-light-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-agate-normal-r-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
char *print_bold_fontname[]={
    "-*-arial-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-bookman-demi-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-agate-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
char *print_italic_fontname[]={
    "-*-arial-medium-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-bookman-light-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-agate-normal-i-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
char *print_italicbold_fontname[]={
    "-*-arial-bold-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-bookman-demi-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-agate-bold-i-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
char *fixed_medium_fontname[]={
    "-*-fixed-medium-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-courrier-medium-*-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-andale mono-medium-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
char *fixed_bold_fontname[]={
    "-*-fixed-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-courrier-bold-*-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-andale mono-medium-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
char *fixed_italic_fontname[]={
    "-*-fixed-medium-o-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-courrier-medium-o-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-andale mono-medium-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
char *fixed_italicbold_fontname[]={
    "-*-fixed-bold-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-courrier-bold-o-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-andale mono-*-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
GdkFont* load_a_font(char **font_list){
    GdkFont* result;
    int i;
    for (i=0; font_list[i]!=NULL;i++){
        result = gdk_font_load(font_list[i]);
        if (result != NULL){
            LOG(LOG_INFO,"gtk::load_a_font","Loaded font %s.",font_list[i]);
            return result;
        }
    }
    return NULL;
}
GdkFont* style_fixed[4];
GdkFont* style_arcane[4];
GdkFont* style_hand[4];
GdkFont* style_strange[4];
GdkFont* style_print[4];
#define STYLE_BOLD 1
#define STYLE_ITALIC 2
int style_inited = 0;

void init_fonts(){
    if (!style_inited){
        style_fixed[0]=load_a_font(fixed_medium_fontname);
        style_fixed[1]=load_a_font(fixed_bold_fontname);
        style_fixed[2]=load_a_font(fixed_italic_fontname);
        style_fixed[3]=load_a_font(fixed_italicbold_fontname);
        
        style_arcane[0]=load_a_font(arcane_medium_fontname);
        style_arcane[1]=style_arcane[2]=style_arcane[3]=style_arcane[0];
        
        style_hand[0]= load_a_font(hand_medium_fontname);
        style_hand[1]=style_hand[2]=style_hand[3]=style_hand[0];
        
        style_strange[0] = load_a_font(strange_medium_fontname);
        style_strange[1]=style_strange[2]=style_strange[3]=style_strange[0];
        
        style_print[0]=load_a_font(print_medium_fontname);
        style_print[1]=load_a_font(print_bold_fontname);
        style_print[2]=load_a_font(print_italic_fontname);
        style_print[3]=load_a_font(print_italicbold_fontname);
        style_inited=1;
    }
}
void write_media(GtkText* textarea, char* message){   
     
    char *current, *marker, *original;
    int flavor = 0;
    GdkFont** current_style;
    GdkColor* fore = NULL;
    init_fonts();
    current_style = style_print;
    current=malloc(strlen(message)+1);
    if (current==NULL){
        LOG(LOG_ERROR,"gtk::write_media","couldn't alloc memory for string manipualtion. Dropping media\n");
        return;
    }
    strcpy(current,message);
    original=current;
    while( (marker=strchr(current,'['))!=NULL){
        *marker='\0';
        gtk_text_insert(textarea,current_style[flavor],fore,NULL,current,marker-current);
        current = marker+1;
        if ( (marker = strchr(current,']')) ==NULL)
            return;  
        *marker='\0';
        if (!strcmp(current,"b"))
            flavor |=STYLE_BOLD;
        else if (!strcmp(current,"i"))
            flavor |=STYLE_ITALIC;
        else if (!strcmp(current,"/b"))
            flavor &=!STYLE_BOLD;
        else if (!strcmp(current,"/i"))
            flavor &=!STYLE_ITALIC;
        else if (!strcmp(current,"fixed"))
            current_style = style_fixed;
        else if (!strcmp(current,"arcane"))
            current_style = style_arcane;
        else if (!strcmp(current,"hand"))
            current_style = style_hand;
        else if (!strcmp(current,"strange"))
            current_style = style_strange;
        else if (!strcmp(current,"print"))
            current_style = style_print;
        else
            printf("unidentified message: %s",current);
        current=marker+1;
    }
    gtk_text_insert(textarea,current_style[flavor],fore,NULL,current,marker-current);
    free(original);
}
void add_book(char* title, char* message){
    GtkWidget *content,*label,*hbox, *scroll, *panel, *close, *closepic;
    prepare_book_window();
    
    content = gtk_text_new(NULL,NULL);
    gtk_text_set_editable(GTK_TEXT(content),FALSE);
    gtk_text_set_word_wrap(GTK_TEXT(content),FALSE);
    gtk_text_set_line_wrap(GTK_TEXT(content),FALSE);
    write_media(GTK_TEXT(content),message);
    
    panel = gtk_vbox_new(FALSE,0);
    close = gtk_button_new();    
    closepic = gtk_pixmap_new (btnClose_pm, btnClose_bm);
    gtk_container_add(GTK_CONTAINER(close),closepic);    
    gtk_box_pack_start (GTK_BOX (panel), close, FALSE, FALSE, 0);
    
    hbox=gtk_hbox_new(FALSE,0);
    
    gtk_box_pack_start (GTK_BOX (hbox), content, TRUE, TRUE, 0);    
    scroll = gtk_vscrollbar_new(GTK_TEXT (content)->vadj);
    gtk_box_pack_start (GTK_BOX (hbox),scroll, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (panel), hbox, TRUE, TRUE, 0);
    
    label = gtk_label_new(title);
//    tab = gtk_hbox_new();
//    button_pic = gtk_pixmap_new(btnClose_pm, btnClose_bm);
//    button = gtk_button_new();
    
    gtk_notebook_append_page(GTK_NOTEBOOK(book_notes),panel,label);
    gtk_widget_show(content);    
    gtk_widget_show(label);      
    gtk_widget_show(close);      
    gtk_widget_show(closepic);      
    gtk_widget_show(panel);
    gtk_widget_show(book_root);
    gtk_widget_show(hbox);
    gtk_widget_show(scroll);
    
    gtk_notebook_set_page(GTK_NOTEBOOK(book_notes),gtk_notebook_page_num(GTK_NOTEBOOK(book_notes),panel));
    gdk_window_raise (book_root->window);
    
    gtk_signal_connect_object (GTK_OBJECT (close), "clicked",
                   GTK_SIGNAL_FUNC(gtk_widget_destroy),
                   GTK_OBJECT (panel));    
       
}
void book_callback(int flag, int type, int subtype, char* message){
    LOG(LOG_DEBUG,"gtk::book_callback","got callback %d subtype %d\n",type,subtype);
    if (message!=NULL){
        char* title = message;
        while ( (*message!='\0') && (*message!='\n') )
            message++;
        if (*message!='\0'){
            *message='\0';
            message++;
        }
        if (*message=='\0'){
            message=title;
            title=NO_TITLE;
        }  
        add_book(title,message);
    }    
}
char* last_motd=NULL;
void motd_callback(int flag, int type, int subtype, char* message){
    
    if(last_motd)
        free(last_motd);
    last_motd = malloc(strlen(message)+1);
    if (last_motd==NULL)
        LOG(LOG_ERROR,"gtk::motd_callback","Outa memory, no save of motd");
    else
        strcpy(last_motd,message);    
}
char* getMOTD(){    
    return last_motd==NULL?"Please read motd written\nin [i]green[/i] inside main\nmessage window":last_motd;
}
void init_text_callbacks(){
    setTextManager(MSG_TYPE_BOOK,book_callback);
    setTextManager(MSG_TYPE_MOTD,motd_callback);
}
