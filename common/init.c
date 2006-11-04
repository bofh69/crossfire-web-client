const char *rcsid_common_init_c =
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

/* This handles the initialization of the client.  This includes making
 * the I_IMAGE and I_ARCH commands.
 */

#ifdef WIN32
#define srandom(x) srand(x)
#endif
#include <client.h>
#include "p_cmd.h" /* init_commands() */

/* XXX Does the x11 client *use* these? */

/* Makes the load/save code trivial - basically, the
 * entries here match the same numbers as the CONFIG_ values defined
 * in common/client.h - this means the load and save just does
 * something like a fprintf(outifle, "%s: %d", config_names[i],
 *			    want_config[i]);
 */
const char *const config_names[CONFIG_NUMS] = {
NULL, "download_all_images", "echo_bindings",
"fasttcpsend", "command_window", "cacheimages", "fog_of_war", "iconscale",
"mapscale", "popups", "displaymode", "showicon", "tooltips", "sound", "splitinfo",
"split", "show_grid", "lighting", "trim_info_window",
"map_width", "map_height", "foodbeep", "darkness", "port",
"grad_color_bars", "resistances", "smoothing", "nosplash",
"auto_apply_container", "mapscroll", "sign_popups", "message_timestamping"
};

sint16 want_config[CONFIG_NUMS], use_config[CONFIG_NUMS];

#define FREE_AND_CLEAR(xyz) { free(xyz); xyz=NULL; }

void VersionCmd(char *data, int len)
{
    char *cp;

    csocket.cs_version = atoi(data);
    /* set sc_version in case it is an old server supplying only one version */
    csocket.sc_version = csocket.cs_version;
    if (csocket.cs_version != VERSION_CS) {
        LOG(LOG_WARNING,"common::VersionCmd","Differing C->S version numbers (%d,%d)",
            VERSION_CS,csocket.cs_version);
/*	exit(1);*/
    }
    cp = strchr(data,' ');
    if (!cp) return;
    csocket.sc_version = atoi(cp);
    if (csocket.sc_version != VERSION_SC) {
    LOG(LOG_WARNING,"common::VersionCmd","Differing S->C version numbers (%d,%d)",
        VERSION_SC,csocket.sc_version);
    }
    cp = strchr(cp+1, ' ');
    if (cp)
	LOG(LOG_INFO,"common::VersionCmd","Playing on server type %s", cp);
}

void SendVersion(ClientSocket csock)
{
    cs_print_string(csock.fd,
		    "version %d %d %s", VERSION_CS, VERSION_SC, VERSION_INFO);
}


void SendAddMe(ClientSocket csock)
{
    cs_print_string(csock.fd, "addme");
}


void SendSetFaceMode(ClientSocket csock,int mode)
{
    cs_print_string(csock.fd, "setfacemode %d", mode);
}


void init_client_vars()
{
    int i;

    /* I think environemental variables should be more important than
     * compiled in defaults, so these probably should be reversed. 
     */
    client_libdir=getenv("CFCLIENT_LIBDIR");
#ifdef CLIENT_LIBDIR
    if (client_libdir==NULL)
	client_libdir=CLIENT_LIBDIR;
#endif

    if (exp_table) {
	free(exp_table);
	exp_table=NULL;
    }
    exp_table_max=0;

    cpl.count_left = 0;
    cpl.container = NULL;
    memset(&cpl.stats,0, sizeof(Stats));
    cpl.stats.maxsp=1;	/* avoid div by 0 errors */
    cpl.stats.maxhp=1;	/* ditto */
    cpl.stats.maxgrace=1;	/* ditto */
    /* ditto - displayed weapon speed is weapon speed/speed */
    cpl.stats.speed=1;
    cpl.input_text[0]='\0';
    cpl.title[0] = '\0';
    cpl.range[0] = '\0';
    cpl.last_command[0] = '\0';

    for (i=0; i<range_size; i++)
	cpl.ranges[i]=NULL;

    for (i=0; i<MAX_SKILL; i++) {
	cpl.stats.skill_exp[i]=0;
	cpl.stats.skill_level[i] = 0;
	skill_names[i] = NULL;
    }

    cpl.ob = player_item();
    cpl.below = map_item();
    cpl.magicmap=NULL;
    cpl.showmagic=0;
    

    csocket.command_sent=0;
    csocket.command_received=0;
    csocket.command_time=0;

    face_info.faceset = 0;
    face_info.num_images = 0;
    face_info.bmaps_checksum = 0;
    face_info.old_bmaps_checksum = 0;
    face_info.want_faceset = NULL;
    face_info.cache_hits=0;
    face_info.cache_misses=0;
    face_info.have_faceset_info=0;
    for (i=0; i<MAX_FACE_SETS; i++) {
	face_info.facesets[i].prefix = NULL;
	face_info.facesets[i].fullname = NULL;
	face_info.facesets[i].fallback = 0;
	face_info.facesets[i].size = NULL;
	face_info.facesets[i].extension = NULL;
	face_info.facesets[i].comment = NULL;
    }
    /* Makes just as much sense to initialize the arrays
     * where they are declared, but I did this so I could 
     * keep track of everything as I was updating
     * the code.  Plus, the performance difference is virtually
     * nothing.
     */
    want_config[CONFIG_DOWNLOAD] = FALSE;
    want_config[CONFIG_ECHO] = FALSE;
    want_config[CONFIG_FASTTCP] = TRUE;
    want_config[CONFIG_CWINDOW] = COMMAND_WINDOW;
    want_config[CONFIG_CACHE] = FALSE;
    want_config[CONFIG_FOGWAR] = TRUE;
    want_config[CONFIG_ICONSCALE] = 100;
    want_config[CONFIG_MAPSCALE] = 100;
    want_config[CONFIG_POPUPS] = FALSE;
    want_config[CONFIG_DISPLAYMODE] = CFG_DM_PIXMAP;
    want_config[CONFIG_SHOWICON] = FALSE;
    want_config[CONFIG_TOOLTIPS] = TRUE;
    want_config[CONFIG_SOUND] = TRUE;
    want_config[CONFIG_SPLITINFO] = FALSE;
    want_config[CONFIG_SPLITWIN] = FALSE;
    want_config[CONFIG_SHOWGRID] = FALSE;
    want_config[CONFIG_LIGHTING] = CFG_LT_TILE;
    want_config[CONFIG_TRIMINFO] = FALSE;
    want_config[CONFIG_MAPWIDTH] = 11;
    want_config[CONFIG_MAPHEIGHT] = 11;
    want_config[CONFIG_FOODBEEP] = FALSE;
    want_config[CONFIG_DARKNESS] = TRUE;
    want_config[CONFIG_PORT] = EPORT;
    want_config[CONFIG_GRAD_COLOR] = FALSE;
    want_config[CONFIG_RESISTS] = 0;
    want_config[CONFIG_RESISTS] = 0;
    want_config[CONFIG_SMOOTH] = 0;
    want_config[CONFIG_SPLASH] = TRUE;
    want_config[CONFIG_APPLY_CONTAINER] = TRUE;	    /* Same behavior before this option was put in */
    want_config[CONFIG_MAPSCROLL] = TRUE;
    want_config[CONFIG_SIGNPOPUP] = TRUE;
    want_config[CONFIG_TIMESTAMP] = FALSE;
    for (i=0; i<CONFIG_NUMS; i++) 
	use_config[i] = want_config[i];

#ifdef WIN32
    /* If HOME is not set, let's set it to . to avoid things like (null)/.crossfire paths */
    if ( !getenv( "HOME" ) )
        {
        if ( getenv( "APPDATA" ) )
            {
            char env[ MAX_BUF ];
            _snprintf( env, MAX_BUF, "HOME=%s", getenv( "APPDATA" ) );
            LOG( LOG_INFO, "common::inic.c", "init_client_vars: HOME set to %APPDATA%.\n" );
            putenv( env );
            }
        else
            {
            LOG( LOG_INFO, "common::init.c", "init_client_vars: HOME not set, setting it to .\n" );
            putenv( "HOME=." );
            }
        }
#endif
    init_commands(); /* pcmd.c */

    /* Any reasonable seed really works */
    srandom(time(NULL));
}

/* This is basically called each time a new player logs
 * on - reset all the player data
 */
void reset_player_data()
{
    int i;

    for (i=0; i<MAX_SKILL; i++) {
	cpl.stats.skill_exp[i]=0;
	cpl.stats.skill_level[i] = 0;
    }
}

/* This is used to clear values between connections to different
 * servers.  This needs to be called after init_client_vars has
 * been called because it does not re-allocated some values.
 */

void reset_client_vars()
{
    int i;

    cpl.count_left = 0;
    cpl.container = NULL;
    memset(&cpl.stats,0, sizeof(Stats));
    cpl.stats.maxsp=1;	/* avoid div by 0 errors */
    cpl.stats.maxhp=1;	/* ditto */
    cpl.stats.maxgrace=1;	/* ditto */
    /* ditto - displayed weapon speed is weapon speed/speed */
    cpl.stats.speed=1;
    cpl.input_text[0]='\0';
    cpl.title[0] = '\0';
    cpl.range[0] = '\0';
    cpl.last_command[0] = '\0';

    for (i=0; i<range_size; i++)
	cpl.ranges[i]=NULL;

    cpl.magicmap=NULL;
    cpl.showmagic=0;

    csocket.command_sent=0;
    csocket.command_received=0;
    csocket.command_time=0;

    face_info.faceset = 0;
    face_info.num_images = 0;
    /* Preserve the old one - this can be used to see if the next
     * server has the same name -> number mapping so that we don't
     * need to rebuild all the images.
     */
    face_info.old_bmaps_checksum = face_info.bmaps_checksum;
    face_info.bmaps_checksum = 0;
    face_info.cache_hits=0;
    face_info.cache_misses=0;
    face_info.have_faceset_info=0;
    for (i=0; i<MAX_FACE_SETS; i++) {
	FREE_AND_CLEAR(face_info.facesets[i].prefix);
	FREE_AND_CLEAR(face_info.facesets[i].fullname);
	face_info.facesets[i].fallback = 0;
	FREE_AND_CLEAR(face_info.facesets[i].size);
	FREE_AND_CLEAR(face_info.facesets[i].extension);
	FREE_AND_CLEAR(face_info.facesets[i].comment);
    }
    reset_player_data();
    for (i=0; i<MAX_SKILL; i++)
        FREE_AND_CLEAR(skill_names[i]);

}
