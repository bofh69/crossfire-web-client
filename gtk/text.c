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
#include <config.h>
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
static char NO_TITLE[] = "[no title]";

#include "pixmaps/sign_flat.xpm"
#include "pixmaps/sign_west.xpm"
#include "pixmaps/sign_east.xpm"
#include "pixmaps/close.xpm"
GtkWidget* book_root = NULL;
GtkWidget* book_notes = NULL;
GdkBitmap* btnClose_bm = NULL;
GdkPixmap* btnClose_pm = NULL;

typedef struct picture_message_struct {
    const char *title;
    const char *const *xpm;
    int x;
    int y;
    int width;
    int height;
    int window_width;
    int window_height;
    GdkPixmap *picture;
    
} picture_message;

static picture_message sign_message[] = {
    {"sign",sign_flat_xpm,70,45,390,305,500,500,NULL},
    {"left sign",sign_west_xpm,95,85,615,190,750,400,NULL},
    {"right sign",sign_east_xpm,45,85,615,190,750,400,NULL},
    {"direction sign",sign_flat_xpm,70,45,390,305,500,500,NULL} };
static void init_pictures(GtkWidget *refWindow) {
    if (btnClose_pm==NULL)          
        btnClose_pm = gdk_pixmap_create_from_xpm_d(refWindow->window,&btnClose_bm,
                &gtk_widget_get_style(refWindow)->bg[GTK_STATE_NORMAL],
                (gchar**)close_xpm);
}
static void prepare_book_window(void) {
    if (!book_root){    
        book_root= gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(book_root),"books");
        book_notes = gtk_notebook_new();
        gtk_notebook_set_tab_pos(GTK_NOTEBOOK(book_notes),GTK_POS_LEFT);
        gtk_container_add(GTK_CONTAINER(book_root),book_notes);
        gtk_widget_show(GTK_WIDGET(book_notes));  
        gtk_widget_show(GTK_WIDGET(book_root));
        init_pictures (book_root);
        gtk_signal_connect (GTK_OBJECT (book_root), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &book_root);
        gtk_window_set_default_size(GTK_WINDOW(book_root),500,600);
        gtk_window_set_position(GTK_WINDOW(book_root),GTK_WIN_POS_CENTER);
    }
}
static GtkWidget *create_text_picture_window(picture_message *layout, char *message) {
    GtkWidget *window, *content, *fixed, *scroll, *close;    
    window = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_set_title(GTK_WINDOW(window),layout->title);
    gtk_widget_set_app_paintable(window,TRUE);
    gtk_widget_realize(window);
    init_pictures(window);
    if (layout->picture == NULL){
        layout->picture = gdk_pixmap_create_from_xpm_d(window->window,NULL,
                &gtk_widget_get_style(window)->bg[GTK_STATE_NORMAL],
                (gchar**)layout->xpm);        
    }
    gdk_window_set_back_pixmap(window->window,layout->picture,FALSE);
    content = gtk_text_new(NULL,NULL);
    gtk_text_set_editable(GTK_TEXT(content),FALSE);
    gtk_text_set_word_wrap(GTK_TEXT(content),TRUE);
    gtk_text_set_line_wrap(GTK_TEXT(content),TRUE);
    write_media(GTK_TEXT(content),message);
    gtk_window_set_default_size(GTK_WINDOW(window),layout->window_width,layout->window_height);
    gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);
    fixed=gtk_fixed_new();
    gtk_widget_set_app_paintable(fixed,TRUE);
    gtk_container_add(GTK_CONTAINER(window),fixed);
    gtk_widget_realize(fixed);
    gdk_window_set_back_pixmap(fixed->window,layout->picture,TRUE);
    
    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_show (scroll);
    gtk_widget_set_usize(scroll,layout->width,layout->height);
    gtk_fixed_put(GTK_FIXED(fixed),scroll,layout->x,layout->y);
    close=gtk_button_new();    
    gtk_widget_set_usize(close,0,0);
    gtk_fixed_put(GTK_FIXED(fixed),close,0,0);
    gtk_widget_show(fixed);
    gtk_widget_show(content);
    
    gtk_container_add(GTK_CONTAINER(scroll),content);
    gtk_text_set_adjustments(GTK_TEXT(content),
            NULL,
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll))
        );
	
    gtk_signal_connect_object (GTK_OBJECT (close), "clicked",
                   GTK_SIGNAL_FUNC(gtk_widget_destroy),
                   GTK_OBJECT (window));    
    gtk_widget_grab_focus (GTK_WIDGET(close));
    return window;
}
static void show_media_message(const char *title, const char *message) {
    GtkWidget *window, *scroll, *content;
    window = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_window_set_title(GTK_WINDOW(window),message);
    gtk_window_set_default_size(GTK_WINDOW(window),500,500);
    gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER);

    content = gtk_text_new(NULL,NULL);
    gtk_text_set_editable(GTK_TEXT(content),FALSE);
    gtk_text_set_word_wrap(GTK_TEXT(content),TRUE);
    gtk_text_set_line_wrap(GTK_TEXT(content),TRUE);
    write_media(GTK_TEXT(content),message);
    gtk_widget_show(content);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll),content);
    gtk_text_set_adjustments(GTK_TEXT(content),
            NULL,
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll))
        );

    gtk_widget_show(content);
    gtk_widget_show (scroll);    
    gtk_widget_show (window);
}
/**
 * Parse message, extract multimedia information, and push
 * as appropriate in the GtkText
 */
static const char *const arcane_medium_fontname[] = {
    "-*-cuneifontlight-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-linotext-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-blackforest-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-becker-*-*-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-arnoldboecklin-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-caligula-*-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static const char *const hand_medium_fontname[] = {
    "-*-dobkinscript-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-coronetscript-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-muriel-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-genoa-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-parkavenue-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-rechtmanscript-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
};
static const char *const strange_medium_fontname[] = {
    "-*-annstone-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-shalomstick-*-r-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    
};
static const char *const print_medium_fontname[] = {
    "-*-arial-medium-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-bookman-light-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-agate-normal-r-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static const char *const print_bold_fontname[] = {
    "-*-arial-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-bookman-demi-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-agate-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static const char *const print_italic_fontname[] = {
    "-*-arial-medium-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-bookman-light-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-agate-normal-i-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static const char *const print_italicbold_fontname[] = {
    "-*-arial-bold-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-bookman-demi-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-agate-bold-i-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static const char *const fixed_medium_fontname[] = {
    "-*-fixed-medium-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-courrier-medium-*-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-andale mono-medium-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static const char *const fixed_bold_fontname[] = {
    "-*-fixed-bold-r-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-courrier-bold-*-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-andale mono-medium-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static const char *const fixed_italic_fontname[] = {
    "-*-fixed-medium-o-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-courrier-medium-o-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-andale mono-medium-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static const char *const fixed_italicbold_fontname[] = {
    "-*-fixed-bold-i-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-courrier-bold-o-*-*-*-120-*-*-*-*-iso8859-*",
    "-*-andale mono-*-*-*-*-*-120-*-*-*-*-iso8859-*",
    NULL
    };
static GdkFont *load_a_font(const char *const font_list[]) {
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
static GdkFont *style_fixed[4];
static GdkFont *style_arcane[4];
static GdkFont *style_hand[4];
static GdkFont *style_strange[4];
static GdkFont *style_print[4];
#define STYLE_BOLD 1
#define STYLE_ITALIC 2
static int style_inited = 0;

static void init_fonts(void) {
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
media_state write_media(GtkText* textarea, const char* message){
    media_state simple_state;
    simple_state.style=style_print;
    simple_state.has_color=0;
    simple_state.flavor=0;
    return write_media_with_state(textarea,message, simple_state);
}
media_state write_media_with_state(GtkText* textarea, const char* message, media_state current_state){   
     
    char *current, *marker, *original;
    if (message==NULL)
        return current_state;
    init_fonts();
    current=malloc(strlen(message)+1);
    if (current==NULL){
        LOG(LOG_ERROR,"gtk::write_media","couldn't alloc memory for string manipualtion. Dropping media\n");
        return current_state;
    }
    strcpy(current,message);
    original=current;
    while( (marker=strchr(current,'['))!=NULL){
        *marker='\0';
        gtk_text_insert(textarea,current_state.style[current_state.flavor],current_state.has_color?&current_state.fore:NULL,NULL,current,marker-current);
        current = marker+1;
        if ( (marker = strchr(current,']')) ==NULL)
            return current_state;  
        *marker='\0';
        if (!strcmp(current,"b"))
            current_state.flavor |=STYLE_BOLD;
        else if (!strcmp(current,"i"))
            current_state.flavor |=STYLE_ITALIC;
        else if (!strcmp(current,"/b"))
            current_state.flavor &=!STYLE_BOLD;
        else if (!strcmp(current,"/i"))
            current_state.flavor &=!STYLE_ITALIC;
        else if (!strcmp(current,"/color"))
            current_state.has_color = 0;
        else if (!strncmp(current,"color=",6))
            current_state.has_color = gdk_color_parse(current+6,&current_state.fore);
        else if (!strcmp(current,"fixed"))
            current_state.style = style_fixed;
        else if (!strcmp(current,"arcane"))
            current_state.style = style_arcane;
        else if (!strcmp(current,"hand"))
            current_state.style = style_hand;
        else if (!strcmp(current,"strange"))
            current_state.style = style_strange;
        else if (!strcmp(current,"print"))
            current_state.style = style_print;
        else
            LOG(LOG_INFO,"gtk::write_media_with_state","unidentified message: %s",current);
        current=marker+1;
    }
    gtk_text_insert(textarea,current_state.style[current_state.flavor],current_state.has_color?&current_state.fore:NULL,NULL,current,marker-current);
    free(original);
    return current_state;
}
static void add_book(char *title, char *message) {
    GtkWidget *content,*label,*hbox, *scroll, *panel, *close, *closepic;
    prepare_book_window();
    
    content = gtk_text_new(NULL,NULL);
    gtk_text_set_editable(GTK_TEXT(content),FALSE);
    gtk_text_set_word_wrap(GTK_TEXT(content),FALSE);
    gtk_text_set_line_wrap(GTK_TEXT(content),TRUE);
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
/*    tab = gtk_hbox_new();*/
/*    button_pic = gtk_pixmap_new(btnClose_pm, btnClose_bm);*/
/*    button = gtk_button_new();*/
    
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
    gtk_widget_grab_focus (GTK_WIDGET(close));
    gtk_signal_connect_object (GTK_OBJECT (close), "clicked",
                   GTK_SIGNAL_FUNC(gtk_widget_destroy),
                   GTK_OBJECT (panel));    
       
}
/* we need access to those when a sign is auto applied. 
 * We don't want to show player a new window while his character
 * keeps running in background
 */
extern GtkWidget* gtkwin_info_text;
extern GtkWidget* gtkwin_info_text2;
static void book_callback(int flag, int type, int subtype, char *message) {
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
        if (!want_config[CONFIG_POPUPS]) /*autoapply*/{
            if (use_config[CONFIG_SPLITINFO])
                write_media(GTK_TEXT(gtkwin_info_text2),message);
            else
                write_media(GTK_TEXT(gtkwin_info_text),message);
	} else
            add_book(title,message);
	
    }    
}
static char *last_motd = NULL;
static void motd_callback(int flag, int type, int subtype, char *message) {
    
    free(last_motd);
    last_motd = malloc(strlen(message)+1);
    if (last_motd==NULL)
        LOG(LOG_ERROR,"gtk::motd_callback","Outa memory, no save of motd");
    else
        strcpy(last_motd,message);    

    if (!want_config[CONFIG_POPUPS] || last_motd == NULL) {
        write_media(GTK_TEXT(gtkwin_info_text), message);
    }
}
static void void_callback(int flag, int type, int subtype, char *message) {
    
    LOG(LOG_INFO,"gtk::void_callback","got message --\n%s\n",message);
    
}
static void sign_callback(int color, int type, int subtype, char *message) {
    GtkWidget *window;

    if ( (subtype>4) || (subtype <1))    
        subtype=1;
    if (message==NULL)
        return;

    if ((!want_config[CONFIG_POPUPS]) || (!want_config[CONFIG_SIGNPOPUP])) /*autoapply*/{
        if (use_config[CONFIG_SPLITINFO])
            write_media(GTK_TEXT(gtkwin_info_text2),message);
        else
            write_media(GTK_TEXT(gtkwin_info_text),message);
    }else{
        window=create_text_picture_window(&(sign_message[subtype-1]), message);
        gtk_window_set_transient_for(GTK_WINDOW(window),GTK_WINDOW(gtkwin_root));
        gtk_widget_show(window);
    }
    
}
const char *getMOTD(void) {
    return last_motd==NULL?"Please read motd written\nin [i]green[/i] inside main\nmessage window":last_motd;
}
static char *rules = NULL;
news_entry* first_news = NULL;
const char *get_rules(void) {
    return rules;    
}
news_entry *get_news(void) {
    return first_news;    
}
static void admin_callback(int flag, int type, int subtype, char *message) {
    char* str1;
    news_entry* new;
    switch (subtype){
        case MSG_TYPE_ADMIN_NEWS:
            str1 = strstr(message,"\n");
            if (str1){
                *str1= '\0';
                str1+=strlen("\n");
                new = malloc(sizeof(news_entry));
                if (new){
                    new->title= malloc(strlen(message)+1);
                    new->content=malloc(strlen(str1)+1);
                    if ( (!new->title) || (!new->content)){
                        free(new->title);
                        free(new->content);
                        LOG(LOG_ERROR,"gtk::admin_callback","Outa memory, no save of news");
                        free(new);
                        return;
                    }
                    strcpy(new->title,message);
                    strcpy(new->content,str1);
                    new->next=first_news;
                    first_news=new;
                } else {
                    LOG(LOG_ERROR,"gtk::admin_callback","Outa memory, no save of news");
                }
                return;
            }
            break;

        case MSG_TYPE_ADMIN_RULES:
            free(rules);
            rules = malloc(strlen(message)+1);
            if (rules){
                strcpy(rules,message);
            }
            else
                LOG(LOG_ERROR,"gtk::admin_callback","Outa memory, no save of rules");
            return;

    }
    draw_info(message, flag);
}
void init_text_callbacks(void) {
    setTextManager(MSG_TYPE_BOOK,book_callback);
    setTextManager(MSG_TYPE_MOTD,motd_callback);
    setTextManager(MSG_TYPE_MONUMENT,void_callback);
    setTextManager(MSG_TYPE_SIGN,sign_callback);
    setTextManager(MSG_TYPE_ADMIN,admin_callback);
}
void cleanup_textmanagers(void) {
    news_entry* last_entry;
    free(last_motd);
    last_motd=NULL;
    free(rules);
    rules = NULL;
    last_entry= first_news;
    while (last_entry){
        first_news=last_entry->next;
        free(last_entry->content);
        free(last_entry->title);
        free(last_entry);
        last_entry=first_news;
    }
    
}
