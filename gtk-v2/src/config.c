char *rcsid_gtk2_config_c =
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


/* This file is here to cover configuration issues.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <ctype.h>

#include "client.h"

#include "interface.h"
#include "support.h"

#include "main.h"
#include "image.h"
#include "gtk2proto.h"

void load_defaults()
{
    char path[MAX_BUF],inbuf[MAX_BUF],*cp;
    FILE *fp;
    int i, val;

    /* Copy over the want values to use values now */
    for (i=0; i<CONFIG_NUMS; i++) {
	use_config[i] = want_config[i];
    }

    sprintf(path,"%s/.crossfire/gdefaults2", getenv("HOME"));
    if ((fp=fopen(path,"r"))==NULL) return;
    while (fgets(inbuf, MAX_BUF-1, fp)) {
	inbuf[MAX_BUF-1]='\0';
	inbuf[strlen(inbuf)-1]='\0';	/* kill newline */

	if (inbuf[0]=='#') continue;
	/* IF no colon, then we certainly don't have a real value, so just skip */
	if (!(cp=strchr(inbuf,':'))) continue;
	*cp='\0';
	cp+=2;	    /* colon, space, then value */

	val = -1;
	if (isdigit(*cp)) val=atoi(cp);
	else if (!strcmp(cp,"True")) val = TRUE;
	else if (!strcmp(cp,"False")) val = FALSE;

	for (i=1; i<CONFIG_NUMS; i++) {
	    if (!strcmp(config_names[i], inbuf)) {
		if (val == -1) {
		    LOG(LOG_WARNING,"gtk::load_defaults","Invalid value/line: %s: %s", inbuf, cp);
		} else {
		    want_config[i] = val;
		}
		break;	/* Found a match - won't find another */
	    }
	}
	/* We found a match in the loop above, so no need to do anything more */
	if (i < CONFIG_NUMS) continue;

	/* Legacy - now use the map_width and map_height values
	 * Don't do sanity checking - that will be done below
	 */
	if (!strcmp(inbuf,"mapsize")) {
	    if (sscanf(cp,"%hdx%hd", &want_config[CONFIG_MAPWIDTH], &want_config[CONFIG_MAPHEIGHT])!=2) {
		LOG(LOG_WARNING,"gtk::load_defaults","Malformed mapsize option in gdefaults2.  Ignoring");
	    }
	}
	else if (!strcmp(inbuf, "server")) {
	    server = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	else if (!strcmp(inbuf, "nopopups")) {
	    /* Changed name from nopopups to popups, so inverse value */
	    want_config[CONFIG_POPUPS] = !val;
	    continue;
	}
	else if (!strcmp(inbuf, "nosplash")) {
	    want_config[CONFIG_SPLASH] = !val;
	    continue;
	}
	else if (!strcmp(inbuf, "splash")) {
	    want_config[CONFIG_SPLASH] = val;
	    continue;
	}
	else if (!strcmp(inbuf, "faceset")) {
	    face_info.want_faceset = strdup_local(cp);	/* memory leak ! */
	    continue;
	}
	/* legacy, as this is now just saved as 'lighting' */
	else if (!strcmp(inbuf, "per_tile_lighting")) {
	    if (val) want_config[CONFIG_LIGHTING] = CFG_LT_TILE;
	}
	else if (!strcmp(inbuf, "per_pixel_lighting")) {
	    if (val) want_config[CONFIG_LIGHTING] = CFG_LT_PIXEL;
	}
	else if (!strcmp(inbuf, "resists")) {
	    if (val) want_config[CONFIG_RESISTS] = val;
	}
	else LOG(LOG_WARNING,"gtk::load_defaults","Unknown line in gdefaults2: %s %s", inbuf, cp);
    }
    fclose(fp);
    /* Make sure some of the values entered are sane - since a user can
     * edit the defaults file directly, they could put bogus values
     * in
     */
    if (want_config[CONFIG_ICONSCALE]< 25 || want_config[CONFIG_ICONSCALE]>200) {
	LOG(LOG_WARNING,"gtk::load_defaults","Ignoring iconscale value read for gdefaults2 file.\n"
            "Invalid iconscale range (%d), valid range for -iconscale is 25 through 200",
            want_config[CONFIG_ICONSCALE]);
	want_config[CONFIG_ICONSCALE] = use_config[CONFIG_ICONSCALE];
    }
    if (want_config[CONFIG_MAPSCALE]< 25 || want_config[CONFIG_MAPSCALE]>200) {
	LOG(LOG_WARNING,"gtk::load_defaults","ignoring mapscale value read for gdefaults2 file.\n"
	        "Invalid mapscale range (%d), valid range for -iconscale is 25 through 200",
            want_config[CONFIG_MAPSCALE]);
	want_config[CONFIG_MAPSCALE] = use_config[CONFIG_MAPSCALE];
    }
    if (!want_config[CONFIG_LIGHTING]) {
	LOG(LOG_WARNING,"gtk::load_defaults","No lighting mechanism selected - will not use darkness code");
	want_config[CONFIG_DARKNESS] = FALSE;
    }
    if (want_config[CONFIG_RESISTS] > 2) {
	LOG(LOG_WARNING,"gtk::load_defaults","ignoring resists display value read for gdafaults file.\n"
            "Invalid value (%d), must be one value of 0,1 or 2.",
            want_config[CONFIG_RESISTS]);
	want_config[CONFIG_RESISTS] = 0;
    }
    
    /* Make sure the map size os OK */
    if (want_config[CONFIG_MAPWIDTH] < 9 || want_config[CONFIG_MAPWIDTH] > MAP_MAX_SIZE) {
	LOG(LOG_WARNING,"gtk::load_defaults",
            "Invalid map width (%d) option in gdefaults2. Valid range is 9 to %d",
            want_config[CONFIG_MAPWIDTH], MAP_MAX_SIZE);
	want_config[CONFIG_MAPWIDTH] = use_config[CONFIG_MAPWIDTH];
    }
    if (want_config[CONFIG_MAPHEIGHT] < 9 || want_config[CONFIG_MAPHEIGHT] > MAP_MAX_SIZE) {
	LOG(LOG_WARNING,"gtk::load_defaults",
            "Invalid map height (%d) option in gdefaults2. Valid range is 9 to %d",
            want_config[CONFIG_MAPHEIGHT], MAP_MAX_SIZE);
	want_config[CONFIG_MAPHEIGHT] = use_config[CONFIG_MAPHEIGHT];
    }

    /* Now copy over the values just loaded */
    for (i=0; i<CONFIG_NUMS; i++) {
	use_config[i] = want_config[i];
    }
    
    image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_ICONSCALE] / 100;
    map_image_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 100;
    map_image_half_size = DEFAULT_IMAGE_SIZE * use_config[CONFIG_MAPSCALE] / 200;
    /*inv_list.show_icon = use_config[CONFIG_SHOWICON];*/

}

void save_defaults()
{
    char path[MAX_BUF],buf[MAX_BUF];
    FILE *fp;
    int i;

    sprintf(path,"%s/.crossfire/gdefaults2", getenv("HOME"));
    if (make_path_to_file(path)==-1) {
	LOG(LOG_ERROR,"gtk::save_defaults","Could not create %s", path);
	return;
    }
    if ((fp=fopen(path,"w"))==NULL) {
	LOG(LOG_ERROR,"gtk::save_defaults","Could not open %s", path);
	return;
    }
    fprintf(fp,"# This file is generated automatically by gcfclient.\n");
    fprintf(fp,"# Manually editing is allowed, however gcfclient may be a bit finicky about\n");
    fprintf(fp,"# some of the matching it does.  all comparisons are case sensitive.\n");
    fprintf(fp,"# 'True' and 'False' are the proper cases for those two values\n");
    fprintf(fp,"# 'True' and 'False' have been replaced with 1 and 0 respectively\n");
    fprintf(fp,"server: %s\n", server);
    fprintf(fp,"faceset: %s\n", face_info.want_faceset);

    /* This isn't quite as good as before, as instead of saving things as 'True'
     * or 'False', it is just 1 or 0.  However, for the most part, the user isn't
     * going to be editing the file directly. 
     */
    for (i=1; i < CONFIG_NUMS; i++) {
	fprintf(fp,"%s: %d\n", config_names[i], want_config[i]);
    }

    fclose(fp);
    sprintf(buf,"Defaults saved to %s",path);
    draw_info(buf,NDI_BLUE);
}
