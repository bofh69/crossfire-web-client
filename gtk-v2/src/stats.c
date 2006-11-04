char *rcsid_gtk2_stats_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005 Mark Wedel & Crossfire Development Team

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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "client.h"

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#include "main.h"


#define MAX_STAT_BARS	5
GtkWidget *stat_label[MAX_STAT_BARS], *stat_bar[MAX_STAT_BARS];

GdkColor gdk_green =    { 0, 0, 0xcfff, 0 };
GdkColor gdk_red =    { 0, 0xcfff, 0, 0 };
GdkColor ncolor = { 0, 0, 0, 0xffff };

/* The table for showing skill exp is an x & y grid.  Note
 * for proper formatting, SKILL_BOXES_X must be even.
 * Hmmm - perhaps these should instead be dynamically
 * generated?
 */
#define SKILL_BOXES_X	6
#define SKILL_BOXES_Y	6

#define PROTECTION_BOXES_X	6
#define PROTECTION_BOXES_Y	6

typedef struct {
    GtkWidget *playername;
    GtkWidget *Str;
    GtkWidget *Dex;
    GtkWidget *Con;
    GtkWidget *Int;
    GtkWidget *Wis;
    GtkWidget *Cha;
    GtkWidget *Pow;
    GtkWidget *wc;
    GtkWidget *dam;
    GtkWidget *ac;
    GtkWidget *armor;
    GtkWidget *speed;
    GtkWidget *weapon_speed;
    GtkWidget *range;
    GtkWidget *exp;
    GtkWidget *level;
    GtkWidget *table_skills_exp;
    GtkWidget *table_protections;
    GtkWidget *skill_exp[SKILL_BOXES_X * SKILL_BOXES_Y];
    GtkWidget *resists[PROTECTION_BOXES_X * PROTECTION_BOXES_Y];

} StatWindow;

static StatWindow statwindow;

/* The basic idea of this structure is there are some lists of
 * names we get from the server (like say skill names) -
 * internally, these are all referred to number, and the numbers
 * are not in any order.  The idea here is we can store away the
 * names, and then can display the items in the window in 
 * nice alphabetical order instead of the random order they
 * normally show up in.
 */
typedef struct {
    const char *name;
    int	    value;
} NameMapping;

NameMapping skill_mapping[MAX_SKILL], resist_mapping[NUM_RESISTS];

int need_mapping_update=1;


void stats_init(GtkWidget *window_root)
{
    int i, x, y;

    stat_label[0] = lookup_widget(window_root,"label_stats_hp");
    stat_bar[0]  = lookup_widget(window_root,"progressbar_hp");
    stat_label[1]  = lookup_widget(window_root,"label_stats_sp");
    stat_bar[1] = lookup_widget(window_root,"progressbar_sp");
    stat_label[2]  = lookup_widget(window_root,"label_stats_grace");
    stat_bar[2] = lookup_widget(window_root,"progressbar_grace");
    stat_label[3]  = lookup_widget(window_root,"label_stats_food");
    stat_bar[3] = lookup_widget(window_root,"progressbar_food");
    stat_label[4]  = lookup_widget(window_root,"label_stats_exp");
    stat_bar[4] = lookup_widget(window_root,"progressbar_exp");

    statwindow.playername = lookup_widget(window_root,"label_playername");
    statwindow.Str = lookup_widget(window_root,"label_str");
    statwindow.Dex = lookup_widget(window_root,"label_dex");
    statwindow.Con = lookup_widget(window_root,"label_con");
    statwindow.Int = lookup_widget(window_root,"label_int");
    statwindow.Wis = lookup_widget(window_root,"label_wis");
    statwindow.Pow = lookup_widget(window_root,"label_pow");
    statwindow.Cha = lookup_widget(window_root,"label_cha");
    statwindow.wc = lookup_widget(window_root,"label_wc");
    statwindow.dam = lookup_widget(window_root,"label_dam");
    statwindow.ac = lookup_widget(window_root,"label_ac");
    statwindow.armor = lookup_widget(window_root,"label_armor");
    statwindow.speed = lookup_widget(window_root,"label_speed");
    statwindow.weapon_speed = lookup_widget(window_root,"label_weapon_speed");
    statwindow.range = lookup_widget(window_root,"label_range");
    statwindow.exp = lookup_widget(window_root,"label_exp");
    statwindow.level = lookup_widget(window_root,"label_level");

    /* Note that the order the labels are attached to the tables determines
     * the order of display.  The order as right now is left to right,
     * then top to bottom, which means that is the order if displaying
     * skills & protections.
     */

    statwindow.table_skills_exp = lookup_widget(window_root,"table_skills_exp");
    for (i=0, x=0, y=0; i < SKILL_BOXES_X * SKILL_BOXES_Y; i++) {
	statwindow.skill_exp[i] = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(statwindow.table_skills_exp), statwindow.skill_exp[i],
		  x, x+1, y, y+1, GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(statwindow.skill_exp[i]);
	x++;
	if (x == SKILL_BOXES_X) {
	    x=0;
	    y++;
	}
    }

    statwindow.table_protections = lookup_widget(window_root,"table_protections");
    for (i=0, x=0, y=0; i < PROTECTION_BOXES_X * PROTECTION_BOXES_Y; i++) {
	statwindow.resists[i] = gtk_label_new("");
	gtk_table_attach(GTK_TABLE(statwindow.table_protections), statwindow.resists[i],
		  x, x+1, y, y+1, GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(statwindow.resists[i]);
	x++;
	if (x == PROTECTION_BOXES_X) {
	    x=0;
	    y++;
	}
    }
    
}


static int lastval[MAX_STAT_BARS] = {-1, -1, -1, -1, -1},
    lastmax[MAX_STAT_BARS] = {-1, -1, -1, -1, -1},
    last_alert[MAX_STAT_BARS]= {0, 0, 0, 0, 0};

void update_stat(int stat_no, int max_stat, int current_stat, const char *name, int can_alert)
{
    float bar;
    int is_alert;
    char buf[256];

    /* If nothing changed, don't need to do anything */
    if (lastval[stat_no] == current_stat && lastmax[stat_no] == max_stat)
	return;

    if (max_stat > 0) bar = (float) current_stat / (float) max_stat;
    else bar = 0.0;

    /* Simple check to see if current stat is less than 25% */
    if (can_alert && current_stat * 4 < max_stat) is_alert=1;
    else is_alert = 0;

    if (use_config[CONFIG_GRAD_COLOR]) {
	/* In this mode, the color of the stat bar were go between red and green
         * in a gradual style.  This, at 50% of the value, the stat bar will be
         * drawn in yellow.  Pure fluff I know.
         */
        /* We are 'supercharged' - scale to max of 2.0 for pure blue */
        if (bar > 1.0) {
            if (bar>2.0) bar=2.0;   /* Doesn't affect display, just are calculations */
            ncolor.blue = 65535.0 * (bar - 1.0);
            ncolor.green = 53247.0 * (2.0 - bar);
            ncolor.red = 0;
            bar=1.0;
	} else {
            /* Use 0.5 as the adjustment - basically, if greater than 0.5,
             * we have pure green with lesser amounts of red.  If less than
             * 0.5, we have pure red with lesser amounts of green.
             */
            if (bar < 0.0) bar=0.0;  /* Like above, doesn't affect display */
            if (bar >= 0.5) ncolor.green = 0xcfff;
            else ncolor.green = 106494.0 * bar;
            if (bar <= 0.5) ncolor.red = 0xcfff;
            else ncolor.red = 106494.0 * (1.0 - bar);
            ncolor.blue = 0;
	}
	gtk_widget_modify_base(stat_bar[stat_no], GTK_STATE_SELECTED, &ncolor);

    } else if (last_alert[stat_no] != is_alert) {
	if (is_alert)
	    gtk_widget_modify_base(stat_bar[stat_no], GTK_STATE_SELECTED, &gdk_red);
	else 
	    gtk_widget_modify_base(stat_bar[stat_no], GTK_STATE_SELECTED, &gdk_green);

	last_alert[stat_no] = is_alert;
    }
    if (bar > 1.0) bar = 1.0;
    if (bar < 0.0) bar = 0.0;

    gtk_progress_set_percentage(GTK_PROGRESS(stat_bar[stat_no]), bar);
    sprintf(buf, "%s %d/%d", name, current_stat, max_stat);
    gtk_label_set(GTK_LABEL(stat_label[stat_no]), buf);

}

/* Updates the stats pain - hp, sp, etc labels */
void draw_message_window(int redraw) {
    static int lastbeep=0;

    update_stat(0, cpl.stats.maxhp, cpl.stats.hp, "HP:", TRUE);
    update_stat(1, cpl.stats.maxsp, cpl.stats.sp, "Spell Points:", TRUE);
    update_stat(2, cpl.stats.maxgrace, cpl.stats.grace, "Grace:", TRUE);
    update_stat(3, 999, cpl.stats.food, "Food:", TRUE);

    /* We may or may not have an exp table from the server.  If we don't, just
     * use current exp value so it will always appear maxed out.
     */
    update_stat(4, 
	(cpl.stats.level+1) < exp_table_max ? exp_table[cpl.stats.level+1]:cpl.stats.exp,
	cpl.stats.exp, "Exp:", FALSE);
    if (use_config[CONFIG_FOODBEEP] && (cpl.stats.food%4==3) && (cpl.stats.food < 200)) {
	gdk_beep( );
    } else if (use_config[CONFIG_FOODBEEP] && cpl.stats.food == 0 && ++lastbeep == 5) {
        lastbeep = 0;
	gdk_beep( );
    }
}

/* The mapping tables may not be completely full, so handle null
 * values.  Always treat null values as later in the sort order.
 */
static int mapping_sort(NameMapping *a, NameMapping *b)
{
    if (!a->name && !b->name) return 0;
    if (!a->name) return 1;
    if (!b->name) return -1;
    else return strcasecmp(a->name, b->name);
}

static void update_stat_mapping(void)
{
    int i;

    for (i=0; i < MAX_SKILL; i++) {
	skill_mapping[i].value=i;
	if (skill_names[i])
	    skill_mapping[i].name = skill_names[i];
	else
	    skill_mapping[i].name = NULL;
    }
    qsort(skill_mapping, MAX_SKILL, sizeof(NameMapping), 
	  (int (*)(const void*, const void*))mapping_sort);

    for (i=0; i < NUM_RESISTS; i++) {
	resist_mapping[i].value=i;
	if (resists_name[i])
	    resist_mapping[i].name = resists_name[i];
	else
	    resist_mapping[i].name = NULL;
    }
    qsort(resist_mapping, NUM_RESISTS, sizeof(NameMapping), 
	  (int (*)(const void*, const void*))mapping_sort);

    need_mapping_update=0;
}

/* This draws the stats window.  If redraw is true, it means
 * we need to redraw the entire thing, and not just do an
 * updated.
 */

void draw_stats(int redraw) {
    static Stats last_stats;
    static char last_name[MAX_BUF]="", last_range[MAX_BUF]="";
    static int init_before=0, max_drawn_skill=0, max_drawn_resists=0;

    float weap_sp;
    char buff[MAX_BUF];
    int i, on_skill, sk;

    if (!init_before) {
	init_before=1;
	memset(&last_stats, 0, sizeof(Stats));
    }

    /* skill_names gets set as part of the initialization with the
     * client - however, right now, there is no callback when
     * it is set, so instead, just track that wee need to update
     * and see if it changes.
     */
    if (need_mapping_update && skill_names[1] != NULL) {
	update_stat_mapping();
    }

    if (strcmp(cpl.title, last_name) || redraw) {
	strcpy(last_name,cpl.title);
	gtk_label_set (GTK_LABEL(statwindow.playername), cpl.title);
    }
    
    if(redraw || cpl.stats.exp!=last_stats.exp) {
	last_stats.exp = cpl.stats.exp;
	sprintf(buff,"Experience: %5" FMT64 ,cpl.stats.exp);
	gtk_label_set (GTK_LABEL(statwindow.exp), buff);
    }
    
    if(redraw || cpl.stats.level!=last_stats.level) {
	last_stats.level = cpl.stats.level;
	sprintf(buff,"Level: %d",cpl.stats.level);
	gtk_label_set (GTK_LABEL(statwindow.level), buff);
    }

    if(redraw || cpl.stats.Str!=last_stats.Str) {
	last_stats.Str=cpl.stats.Str;
	sprintf(buff,"%2d",cpl.stats.Str);
	gtk_label_set (GTK_LABEL(statwindow.Str), buff);
    }
    
    if(redraw || cpl.stats.Dex!=last_stats.Dex) {
	last_stats.Dex=cpl.stats.Dex;
	sprintf(buff,"%2d",cpl.stats.Dex);
	gtk_label_set (GTK_LABEL(statwindow.Dex), buff);
    }
    
    if(redraw || cpl.stats.Con!=last_stats.Con) {
	last_stats.Con=cpl.stats.Con;
	sprintf(buff,"%2d",cpl.stats.Con);
	gtk_label_set (GTK_LABEL(statwindow.Con), buff);
    }
    
    if(redraw || cpl.stats.Int!=last_stats.Int) {
	last_stats.Int=cpl.stats.Int;
	sprintf(buff,"%2d",cpl.stats.Int);
	gtk_label_set (GTK_LABEL(statwindow.Int), buff);
    }
    
    if(redraw || cpl.stats.Wis!=last_stats.Wis) {
	last_stats.Wis=cpl.stats.Wis;
	sprintf(buff,"%2d",cpl.stats.Wis);
	gtk_label_set (GTK_LABEL(statwindow.Wis), buff);
    }
    
    if(redraw || cpl.stats.Pow!=last_stats.Pow) {
	last_stats.Pow=cpl.stats.Pow;
	sprintf(buff,"%2d",cpl.stats.Pow);
	gtk_label_set (GTK_LABEL(statwindow.Pow), buff);
    }
    
    if(redraw || cpl.stats.Cha!=last_stats.Cha) {
	last_stats.Cha=cpl.stats.Cha;
	sprintf(buff,"%2d",cpl.stats.Cha);
	gtk_label_set (GTK_LABEL(statwindow.Cha), buff);
    }
    
    if(redraw || cpl.stats.wc!=last_stats.wc) {
	last_stats.wc=cpl.stats.wc;
	sprintf(buff,"%3d",cpl.stats.wc);
	gtk_label_set (GTK_LABEL(statwindow.wc), buff);
    }
    
    if(redraw || cpl.stats.dam!=last_stats.dam) {
	last_stats.dam=cpl.stats.dam;
	sprintf(buff,"%d",cpl.stats.dam);
	gtk_label_set (GTK_LABEL(statwindow.dam), buff);
    }
    
    if(redraw || cpl.stats.ac!=last_stats.ac) {
	last_stats.ac=cpl.stats.ac;
	sprintf(buff,"%d",cpl.stats.ac);
	gtk_label_set (GTK_LABEL(statwindow.ac), buff);
    }
    
    if(redraw || cpl.stats.resists[0]!=last_stats.resists[0]) {
	last_stats.resists[0]=cpl.stats.resists[0];
	sprintf(buff,"%d",cpl.stats.resists[0]);
	gtk_label_set (GTK_LABEL(statwindow.armor), buff);
    }
    
    if (redraw || cpl.stats.speed!=last_stats.speed) {
	last_stats.speed=cpl.stats.speed;
	sprintf(buff,"%3.2f",(float)cpl.stats.speed/FLOAT_MULTF);
	gtk_label_set (GTK_LABEL(statwindow.speed), buff);
    }

    weap_sp = (float) cpl.stats.speed/((float)cpl.stats.weapon_sp);
    if (redraw || weap_sp !=last_stats.weapon_sp) {
	last_stats.weapon_sp=weap_sp;
	sprintf(buff,"%3.2f",weap_sp);
	gtk_label_set (GTK_LABEL(statwindow.weapon_speed), buff);
    }

    if(redraw || strcmp(cpl.range, last_range)) {
	strcpy(last_range, cpl.range);
	sprintf(buff,"Range: %s",cpl.range);
	gtk_label_set (GTK_LABEL(statwindow.range), cpl.range);
    }

    on_skill=0;
    for (i=0; i<MAX_SKILL; i++) {
	/* Drawing a particular skill entry is tricky - only draw if
	 * different, and only draw if we have a name for the skill
	 * and the player has some exp in the skill - don't draw
	 * all 30 skills for no reason.
	 */
	sk = skill_mapping[i].value;

	if ((redraw || cpl.stats.skill_exp[sk] != last_stats.skill_exp[sk]) &&
	    skill_mapping[i].name && cpl.stats.skill_exp[sk]){
	    gtk_label_set(GTK_LABEL(statwindow.skill_exp[on_skill++]), skill_mapping[i].name);
	    sprintf(buff,"%" FMT64 " (%d)", cpl.stats.skill_exp[sk], cpl.stats.skill_level[sk]);
	    gtk_label_set(GTK_LABEL(statwindow.skill_exp[on_skill++]), buff);
	    last_stats.skill_level[sk] = cpl.stats.skill_level[sk];
	    last_stats.skill_exp[sk] = cpl.stats.skill_exp[sk];
	} else if (cpl.stats.skill_exp[sk]) {
	    /* don't need to draw the skill, but need to update the position
	     * of where to draw the next one.
	     */
	    on_skill+=2;
	}
    }

    /* Since the number of skills we draw come and go, basically we want
     * to erase any extra.  This shows up when switching characters, eg, character
     * #1 knows 10 skills, #2 knows 5 - need to erase those 5 extra.
     */
    if (on_skill < max_drawn_skill) {
	int k;

	for (k = on_skill; k <= max_drawn_skill; k++)
	    gtk_label_set(GTK_LABEL(statwindow.skill_exp[k]), "");
    }
    max_drawn_skill = on_skill;

    /* Now do the resistance table */
    if (redraw || cpl.stats.resist_change) {
	int i,j=0;

	cpl.stats.resist_change=0;
	for (i=0; i<NUM_RESISTS; i++) {
	    sk = resist_mapping[i].value;
	    if (cpl.stats.resists[sk]) {
		gtk_label_set(GTK_LABEL(statwindow.resists[j]), resist_mapping[i].name);
		j++;
		sprintf(buff,"%+4d", cpl.stats.resists[sk]);
		gtk_label_set(GTK_LABEL(statwindow.resists[j]), buff);
		j++;
		if (j >= PROTECTION_BOXES_X * PROTECTION_BOXES_Y) break;
	    }
	}
	/* Erase old/unused resistances */
	if (j < max_drawn_resists) {
	    for (i=j; i <= max_drawn_resists; i++)  {
		gtk_label_set(GTK_LABEL(statwindow.resists[i]), "");
	    }
	}
	max_drawn_resists = j;
    } /* if we draw the resists */


    /* Don't need to worry about hp, sp, grace, food - update_stat()
     * deals with that as part of the stat bar logic.
     */
    
}



void clear_stat_mapping(void)
{
    need_mapping_update=1;
}
