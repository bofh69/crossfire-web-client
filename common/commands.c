/*
 * static char *rcsid_commands_c =
 *   "$Id$";
 */
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


/* Handles commands received by the server.  This does not necessarily
 * handle all commands - some might be in other files (like init.c)
 *
 * This file handles commans from the server->client.  See player.c
 * for client->server commands.
 *
 * this file contains most of the commands for the dispatch loop most of
 * the functions are self-explanatory, the pixmap/bitmap commands recieve
 * the picture, and display it.  The drawinfo command draws a string
 * in the info window, the stats command updates the local copy of the stats
 * and displays it. handle_query prompts the user for input.
 * send_reply sends off the reply for the input.
 * player command gets the player information.
 * MapScroll scrolls the map on the client by some amount
 * MapCmd displays the map either with layer packing or stack packing. 
 *   packing/unpacking is best understood by looking at the server code
 *   (server/ericserver.c)
 *   stack packing is easy, for every map entry that changed, we pack
 *   1 byte for the x/y location, 1 byte for the count, and 2 bytes per
 *   face in the stack.
 *   layer packing is harder, but I seem to remember more efficient:
 *   first we pack in a list of all map cells that changed and are now
 *   empty.  The end of this list is a 255, which is bigger that 121, the
 *   maximum packed map location.  
 *   For each changed location we also pack in a list of all the faces and
 *   X/Y coordinates by layer, where the layer is the depth in the map.
 *   This essentially takes slices through the map rather than stacks.
 *   Then for each layer, (max is MAXMAPCELLFACES, a bad name) we start
 *   packing the layer into the message.  First we pack in a face, then
 *   for each place on the layer with the same face, we pack in the x/y
 *   location.  We mark the last x/y location with the high bit on 
 *   (11*11 = 121 < 128).  We then continue on with the next face, which
 *   is why the code marks the faces as -1 if they are finished.  Finally
 *   we mark the last face in the layer again with the high bit, clearly
 *   limiting the total number of faces to 32767, the code comments it's
 *   16384, I'm not clear why, but the second bit may be used somewhere
 *   else as well.
 *   The unpacking routines basically perform the opposite operations.
 */
int mapupdatesent=0;
#include <client.h>
#include <external.h>

static void get_skill_info(char *data, int len)
{
    char *cp, *nl, *sn;
    int val;

    cp = data;
    do {
	nl = strchr(cp, '\n');
	if (nl) {
	    *nl=0;
	    nl++;
	}
	sn = strchr(cp, ':');
	if (!sn) {
	    fprintf(stderr,"get_skill_info: corrupt line: %s\n", cp);
	    return;
	}
	*sn=0;
	sn++;
	val = atoi(cp);
	val -= CS_STAT_SKILLINFO;
	if (val < 0 || val> CS_NUM_SKILLS) {
	    fprintf(stderr,"get_skill_info: invalid skill number %d\n", val);
	    return;
	}
	if (skill_names[val]) free(skill_names[val]);
	skill_names[val] = strdup_local(sn);
	cp = nl;
    } while (cp < (data + len));
}


/* handles the response from a 'requestinfo' command.  This function doesn't
 * do much itself other than dispatch to other functions.
 */

void ReplyInfoCmd(char *buf, int len)
{
    char *cp;
    int i;

    for (i=0; i<len; i++) {
	/* Either a space or newline represents a break */
	if (*(buf+i) == ' ' || *(buf+i) == '\n' ) break;
    }
    if (i>=len) {
	/* Don't print buf, as it may contain binary data */
	fprintf(stderr,"ReplyInfoCmd: Never found a space in the replyinfo\n");
	return;
    }
    /* Null out the space and put cp beyond it */
    cp = buf + i;
    *cp++ = '\0';
    if (!strcmp(buf,"image_info")) {
	get_image_info(cp, len - i - 1);   /* located in common/image.c */
    }
    else if (!strcmp(buf,"image_sums")) {
	get_image_sums(cp, len - i - 1);   /* located in common/image.c */
    }
    else if (!strcmp(buf,"skill_info")) {
	get_skill_info(cp, len - i - 1);   /* located in common/image.c */
    }
}
    

/* Received a response to a setup from the server.
 * This function is basically the same as the server side function - we
 * just do some different processing on the data.
 */

void SetupCmd(char *buf, int len)
{
    int s;
    char *cmd, *param;

    /* run through the cmds of setup
     * syntax is setup <cmdname1> <parameter> <cmdname2> <parameter> ...
     *
     * we send the status of the cmd back, or a FALSE is the cmd is the server unknown
     * The client then must sort this out
     */

    LOG(0,"Get SetupCmd:: %s\n", buf);
    for(s=0;;) {
	if(s>=len)	/* ugly, but for secure...*/
	    break;

	cmd = &buf[s];

	/* find the next space, and put a null there */
	for(;buf[s] && buf[s] != ' ';s++) ;
	buf[s++]=0;
	while (buf[s] == ' ') s++;

	if(s>=len)
	    break;

	param = &buf[s];

	for(;buf[s] && buf[s] != ' ';s++) ;
	buf[s++]=0;
	while (buf[s] == ' ') s++;
		
	/* what we do with the returned data depends on what the server
	 * returns to us.  In some cases, we may fall back to other
	 * methods, just report on error, or try another setup command.
	 */
	if (!strcmp(cmd,"sound")) {
	    if (!strcmp(param,"FALSE"))
		cs_print_string(csocket.fd, "setsound %d", want_config[CONFIG_SOUND]);
	} else if (!strcmp(cmd,"mapsize")) {
	    int x, y=0;
	    char *cp,tmpbuf[MAX_BUF];

	    if (!strcasecmp(param, "false")) {
		draw_info("Server only supports standard sized maps (11x11)", NDI_RED);
		/* Do this because we may have been playing on a big server before */
		use_config[CONFIG_MAPWIDTH]=11;
		use_config[CONFIG_MAPHEIGHT]=11;
		resize_map_window(use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
		continue;
	    }
	    x = atoi(param);
	    for (cp = param; *cp!=0; cp++)
		if (*cp == 'x' || *cp == 'X') {
		    y = atoi(cp+1);
		    break;
		}
	    /* we wanted a size larger than the server supports.  Reduce our
	     * size to server maximum, and re-sent the setup command.
	     * Update our want sizes, and also tell the player what we are doing
	     */
	    if (use_config[CONFIG_MAPWIDTH] > x || use_config[CONFIG_MAPHEIGHT] > y) {
		if (use_config[CONFIG_MAPWIDTH] > x) use_config[CONFIG_MAPWIDTH] = x;
		if (use_config[CONFIG_MAPHEIGHT] > y) use_config[CONFIG_MAPHEIGHT] = y;
		cs_print_string(csocket.fd,
				"setup mapsize %dx%d", use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
		sprintf(tmpbuf,"Server supports a max mapsize of %d x %d - requesting a %d x %d mapsize",
			x, y, use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
		draw_info(tmpbuf,NDI_RED);
	    }
	    else if (use_config[CONFIG_MAPWIDTH] == x && use_config[CONFIG_MAPHEIGHT] == y) {
		resize_map_window(use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
	    }
	    else {
		/* Our request was not bigger than what server supports, and 
		 * not the same size, so whats the problem?  Tell the user that
		 * something is wrong.
		 */
		sprintf(tmpbuf,"Unable to set mapsize on server - we wanted %d x %d, server returnd %d x %d",
			use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT], x, y);
		draw_info(tmpbuf,NDI_RED);
	    }
	} else if (!strcmp(cmd,"sexp") || !strcmp(cmd,"darkness") || 
		!strcmp(cmd,"newmapcmd") ) {
	        /* this really isn't an error or bug - in fact, it is expected if
		 * the user is playing on an older server.
		 */
		if (!strcmp(param,"FALSE")) {
		    fprintf(stderr,"Server returned FALSE on setup command %s\n",cmd);
#if 0
/* This should really be a callback to the gui if it needs to re-examine
 * the results here.
 */
		    if( !strcmp( cmd, "newmapcmd") && fog_of_war == TRUE) {
			fprintf( stderr, "**Warning: Fog of war is active but server does not support the newmap command\n");
		    }
#endif
		}
	} else if (!strcmp(cmd, "facecache")) {
	    if (!strcmp(param, "FALSE") && want_config[CONFIG_CACHE]) {
		SendSetFaceMode(csocket, CF_FACE_CACHE);
	    }
	    else {
		use_config[CONFIG_CACHE] = atoi(param);
	    }
	} else if (!strcmp(cmd,"faceset")) {
	    if (!strcmp(param, "FALSE")) {
		draw_info("Server does not support other image sets, will use default", NDI_RED);
		face_info.faceset=0;
	    }
	} else if (!strcmp(cmd,"map1acmd")) {
	    if (!strcmp(param,"FALSE")) {
		draw_info("Server does not support map1acmd, will try map1cmd", NDI_RED);
		cs_print_string(csocket.fd,"setup map1cmd 1");
	    }
	} else if (!strcmp(cmd,"map1cmd")) {
	    /* Not much we can do about someone playing on an ancient server. */
	    if (!strcmp(param,"FALSE")) {
		draw_info("Server does not support map1cmd - This server is too old to support this client!", NDI_RED);
		close(csocket.fd);
		csocket.fd = -1;
	    }
	} else if (!strcmp(cmd,"itemcmd")) {
	    /* don't really care - currently, the server will just
	     * fall back to the item1 transport mode.  If more item modes
	     * are used in the future, we should probably fall back one at
	     * a time or the like.
	     */
	} else if (!strcmp(cmd,"exp64")) {
	    /* If this server does not support the new skill code,
	     * fall back to the old which uses experience categories.
	     * for that, initialize the skill names appropriately.
	     * by doing so, the actual display code can be the same
	     * for both old and new.  If the server does support
	     * new exp system, send a request to get the mapping
	     * information.
	     */
	    if (!strcmp(param,"FALSE")) {
		int i;
		for (i=0; i<MAX_SKILL; i++) {
		    if (skill_names[i]) {
			free(skill_names[i]);
			skill_names[i] = NULL;
		    }
		}
		skill_names[0] = strdup_local("agility");
		skill_names[1] = strdup_local("personality");
		skill_names[2] = strdup_local("mental");
		skill_names[3] = strdup_local("physique");
		skill_names[4] = strdup_local("magic");
		skill_names[5] = strdup_local("wisdom");
	    }
	    else {
		cs_print_string(csocket.fd,"requestinfo skill_info");
	    }
	}
    else if (!strcmp(cmd,"extendedMapInfos")) {
        if (!strcmp(param,"FALSE")) {
            use_config[CONFIG_SMOOTH]=0;
        }else{
            /* Request all extended infos we want
             * Should regroup everything for easyness
             */
            if (use_config[CONFIG_SMOOTH]){
                cs_print_string(csocket.fd,"toggleextendedinfos smooth");
            }    
        }
    }
	else {
		fprintf(stderr,"Got setup for a command we don't understand: %s %s\n",
		    cmd, param);
	}
    }
}
void ExtendedInfoSetCmd (char *data, int len){
    /* Do nothing for now, perhaps later add some
     * support to check what server knows.
     */
   /* commented, no waranty string data is null terminated
   draw_info("ExtendedInfoSet returned from server: ",NDI_BLACK);
   draw_info(data,NDI_BLACK);
   */
}

/* Handles when the server says we can't be added.  In reality, we need to
 * close the connection and quit out, because the client is going to close
 * us down anyways.
 */
void AddMeFail(char *data, int len)
{
    LOG(0,"addme_failed received.\n");
    return;
}

/* This is really a throwaway command - there really isn't any reason to
 * send addme_success commands.
 */
void AddMeSuccess(char *data, int len)
{
    LOG(0,"addme_success received.\n");
    return;
}

void GoodbyeCmd(char *data, int len)
{
    /* This could probably be greatly improved - I am not sure if anything
     * needs to be saved here, but certainly it should be possible to
     * reconnect to the server or a different server without having to
     * rerun the client.
     */
    fprintf(stderr,"Received goodbye command from server - exiting\n");
    exit(0);
}

Animations animations[MAXANIM];

void AnimCmd(unsigned char *data, int len)
{
    short anum;
    int    i,j;

    anum=GetShort_String(data);
    if (anum<0 || anum > MAXANIM) {
	fprintf(stderr,"AnimCmd: animation number invalid: %d\n", anum);
	return;
    }
    animations[anum].flags=GetShort_String(data+2);
    animations[anum].num_animations = (len-4)/2;
    if (animations[anum].num_animations<1) {
	    fprintf(stderr,"AnimCmd: num animations invalid: %d\n",
		    animations[anum].num_animations);
	    return;
    }
    animations[anum].faces = malloc(sizeof(uint16)*animations[anum].num_animations);
    for (i=4,j=0; i<len; i+=2,j++)
	animations[anum].faces[j]=GetShort_String(data+i);

    if (j!=animations[anum].num_animations) 
	fprintf(stderr,"Calculated animations does not equal stored animations? (%d!=%d)\n",
		j, animations[anum].num_animations);

    LOG(0,"Received animation %d, %d faces\n", anum, animations[anum].num_animations);
}

Smooths smooths[MAXSMOOTH];
int smoothused=0;
void SmoothCmd(unsigned char *data, int len){
    uint16 faceid;
    uint16 smoothing;
    static int dx[8]={0,1,1,1,0,-1,-1,-1};
    static int dy[8]={-1,-1,0,1,1,1,0,-1};  
    int i,j,x,y,layer;
    int mx,my;
    display_map_startupdate();
    if (smoothused>=MAXSMOOTH) /*no place to put*/
        return;
    faceid=GetShort_String(data);
    smoothing=GetShort_String(data+2);
    for (i=0;i<smoothused;i++){
        if (smooths[i].smoothid==faceid)
            break;
    }
    if (i==smoothused)
        smoothused++;
    smooths[i].smoothid=faceid;
    smooths[i].received=1;
    smooths[i].face=smoothing;    
    /*update the map where needed*/
    for( x= 0; x<use_config[CONFIG_MAPWIDTH]; x++) {
    for( y= 0; y<use_config[CONFIG_MAPHEIGHT]; y++){
    for (layer=0;layer<MAXLAYERS;layer++){
        mx = x + pl_pos.x;
	    my = y + pl_pos.y;
        if ( (the_map.cells[mx][my].smooth[layer]==0) ||
             (the_map.cells[mx][my].heads[layer].face!=smooths[i].smoothid))
            continue; /*not concerned*/
        for (j=0;j<8;j++){
            mx=x + pl_pos.x+dx[j];
            my=y + pl_pos.y+dy[j];
            if ( (mx<0) || (my<0) || (the_map.x<=mx) || (the_map.y<=my))
                continue;
            the_map.cells[mx][my].need_resmooth=1;   
        }
    }
    }
    }
    display_map_doneupdate(FALSE);
    /*Should find some way to draw here, i suppose*/
}

void DrawInfoCmd(char *data, int len)
{
    int color=atoi(data);
    char *buf;

    buf = strchr(data, ' ');
    if (!buf) {
	fprintf(stderr,"DrawInfoCmd - got no data\n");
	buf="";
    }
    else buf++;
    if (color!=NDI_BLACK)
	draw_color_info(color, buf);
    else
	draw_info(buf,NDI_BLACK);

}

void StatsCmd(unsigned char *data, int len)
{
    int i=0, c, redraw=0;
    sint64 last_exp;

    while (i<len) {
	c=data[i++];
	if (c>=CS_STAT_RESIST_START && c<=CS_STAT_RESIST_END) {
	    cpl.stats.resists[c-CS_STAT_RESIST_START]=GetShort_String(data+i);
	    i+=2;
	    cpl.stats.resist_change=1;
	} else if (c >= CS_STAT_SKILLINFO && c < (CS_STAT_SKILLINFO+CS_NUM_SKILLS)) {
	    /* We track to see if the exp has gone from 0 to some total value -
	     * we do this because the draw logic currently only draws skills where
	     * the player has exp.  We need to communicate to the draw function
	     * that it should draw all the players skills.  Using redraw is
	     * a little overkill, because a lot of the data may not be changing.
	     * OTOH, such a transition should only happen rarely, not not be a very
	     * big deal.
	     */
	    cpl.stats.skill_level[c - CS_STAT_SKILLINFO] = data[i++];
	    last_exp = cpl.stats.skill_exp[c - CS_STAT_SKILLINFO];
	    cpl.stats.skill_exp[c - CS_STAT_SKILLINFO] = GetInt64_String(data+i);
	    if (last_exp == 0 && cpl.stats.skill_exp[c - CS_STAT_SKILLINFO])
		redraw=1;
	    i += 8;
	} else {
	    switch (c) {
		case CS_STAT_HP:	cpl.stats.hp=GetShort_String(data+i); i+=2; break;
		case CS_STAT_MAXHP:	cpl.stats.maxhp=GetShort_String(data+i); i+=2; break;
		case CS_STAT_SP:	cpl.stats.sp=GetShort_String(data+i); i+=2; break;
		case CS_STAT_MAXSP:	cpl.stats.maxsp=GetShort_String(data+i); i+=2; break;
		case CS_STAT_GRACE:	cpl.stats.grace=GetShort_String(data+i); i+=2; break;
		case CS_STAT_MAXGRACE:	cpl.stats.maxgrace=GetShort_String(data+i); i+=2; break;
		case CS_STAT_STR:	cpl.stats.Str=GetShort_String(data+i); i+=2; break;
		case CS_STAT_INT:	cpl.stats.Int=GetShort_String(data+i); i+=2; break;
		case CS_STAT_POW:	cpl.stats.Pow=GetShort_String(data+i); i+=2; break;
		case CS_STAT_WIS:	cpl.stats.Wis=GetShort_String(data+i); i+=2; break;
		case CS_STAT_DEX:	cpl.stats.Dex=GetShort_String(data+i); i+=2; break;
		case CS_STAT_CON:	cpl.stats.Con=GetShort_String(data+i); i+=2; break;
		case CS_STAT_CHA:	cpl.stats.Cha=GetShort_String(data+i); i+=2; break;
		case CS_STAT_EXP:	cpl.stats.exp=GetInt_String(data+i); i+=4; break;
		case CS_STAT_EXP64:	cpl.stats.exp=GetInt64_String(data+i); i+=8; break;
		case CS_STAT_LEVEL:	cpl.stats.level=GetShort_String(data+i); i+=2; break;
		case CS_STAT_WC:	cpl.stats.wc=GetShort_String(data+i); i+=2; break;
		case CS_STAT_AC:	cpl.stats.ac=GetShort_String(data+i); i+=2; break;
		case CS_STAT_DAM:	cpl.stats.dam=GetShort_String(data+i); i+=2; break;
		case CS_STAT_ARMOUR:    cpl.stats.resists[0]=GetShort_String(data+i); i+=2; break;
		case CS_STAT_SPEED:	cpl.stats.speed=GetInt_String(data+i); i+=4; break;
		case CS_STAT_FOOD:	cpl.stats.food=GetShort_String(data+i); i+=2; break;
		case CS_STAT_WEAP_SP:	cpl.stats.weapon_sp=GetInt_String(data+i); i+=4; break;
		case CS_STAT_FLAGS:	cpl.stats.flags=GetShort_String(data+i); i+=2; break;
		case CS_STAT_WEIGHT_LIM:set_weight_limit(GetInt_String(data+i)); i+=4; break;

		/* Skill experience handling */
		/* We make the assumption based on current bindings in the protocol
		 * that these skip 2 values and are otherwise in order.
		 */
		case CS_STAT_SKILLEXP_AGILITY:
		case CS_STAT_SKILLEXP_PERSONAL:
		case CS_STAT_SKILLEXP_MENTAL:
		case CS_STAT_SKILLEXP_PHYSIQUE:
		case CS_STAT_SKILLEXP_MAGIC:
		case CS_STAT_SKILLEXP_WISDOM:
		    cpl.stats.skill_exp[(c-CS_STAT_SKILLEXP_START)/2] = GetInt_String(data+i);
		    i+=4;
		    break;

		case CS_STAT_SKILLEXP_AGLEVEL:
		case CS_STAT_SKILLEXP_PELEVEL:
		case CS_STAT_SKILLEXP_MELEVEL:
		case CS_STAT_SKILLEXP_PHLEVEL:
		case CS_STAT_SKILLEXP_MALEVEL:
		case CS_STAT_SKILLEXP_WILEVEL:
		    cpl.stats.skill_level[(c-CS_STAT_SKILLEXP_START-1)/2] = GetShort_String(data+i);
		    i+=2;
		    break;

		case CS_STAT_RANGE: {
		    int rlen=data[i++];
		    strncpy(cpl.range,(const char*)data+i,rlen);
		    cpl.range[rlen]='\0';
		    i += rlen;
		    break;
		}
		case CS_STAT_TITLE: {
		    int rlen=data[i++];
		    strncpy(cpl.title,(const char*)data+i,rlen);
		    cpl.title[rlen]='\0';
		    i += rlen;
		    break;
		}
		default:
		    fprintf(stderr,"Unknown stat number %d\n",c);
/*		    abort();*/
	    }
	}
    }

    if (i>len) {
	fprintf(stderr,"got stats overflow, processed %d bytes out of %d\n",
		i, len);
    }
    draw_stats(redraw);
    draw_message_window(0);
}

void handle_query (char *data, int len)
{
    char *buf,*cp;
    uint8 flags = atoi(data);

    /* The actual text is optional */
    buf = strchr(data,' ');
    if (buf) buf++;

    /* If we just get passed an empty string, why draw this? */
    if (buf) {
	cp = buf;
	while ((buf=strchr(buf,'\n'))!=NULL) {
	    *buf++='\0';
	    draw_info(cp, NDI_BLACK);
	    cp = buf;
	}
	if (cp) draw_prompt(cp);
    }
    /* Yes/no - don't do anything with it now */
    if (flags & CS_QUERY_YESNO) {}

    /* one character response expected */
    if (flags & CS_QUERY_SINGLECHAR)
	cpl.input_state = Reply_One;
    else
	cpl.input_state = Reply_Many;

    if (flags & CS_QUERY_HIDEINPUT)		/* no echo */
	cpl.no_echo=1;
    else
	cpl.no_echo=0;

    /* Let the window system know this may have changed */
    x_set_echo();

      LOG(0,"Received query.  Input state now %d\n", cpl.input_state);
}

/* Sends a reply to the server.  text contains the null terminated
 * string of text to send.  This function basically just packs
 * the stuff up.
 */

void send_reply(char *text)
{
    cs_print_string(csocket.fd, "reply %s", text);
}



/* This function copies relevant data from the archetype to the
 * object.  Only copies data that was not set in the object
 * structure.
 *
 */

void PlayerCmd(unsigned char *data, int len)
{
    char name[MAX_BUF];
    int tag, weight, face,i=0,nlen;

    tag = GetInt_String(data);
    i+=4;
    weight = GetInt_String(data+i);
    i+=4;
    face= GetInt_String(data+i);
    i+=4;
    nlen=data[i++];
    memcpy(name, (const char*)data+i, nlen);
    name[nlen]='\0';
    i+= nlen;

    if (i!=len) {
	fprintf(stderr,"PlayerCmd: lengths do not match (%d!=%d)\n",
		len, i);
    }
    new_player(tag, name, weight, face);
}

void item_actions (item *op)
{
    if (!op) return;
    if (op->open) {
	open_container (op);
	cpl.container = op;
    } else if (op->was_open) {
	close_container (op);
	cpl.container=NULL;
    }
/*
    if (op->env == cpl.below) {
	check_auto_pickup (op);
    }
*/
}


/* common_item_cmd parses the data send to us from the server.
 * revision is what item command the data came from - newer
 * ones have addition fields.
 */
static void common_item_command(uint8 *data, int len, int revision)
{

    int weight, loc, tag, face, flags,pos=0,nlen,anim,nrof, type;
    uint8 animspeed;
    char name[MAX_BUF];

    loc = GetInt_String(data);
    pos+=4;

    if (pos == len) {
	fprintf(stderr,"ItemCmd: Got location with no other data\n");
	return;
    }
    else if (loc < 0) { /* delete following items */
	fprintf(stderr,"ItemCmd: got location with negative value (%d)\n", loc);
	return;
    } else {
	while (pos < len) {
	    tag=GetInt_String(data+pos); pos+=4;
	    flags = GetInt_String(data+pos); pos+=4;
	    weight = GetInt_String(data+pos); pos+=4;
	    face = GetInt_String(data+pos); pos+=4;
	    nlen = data[pos++];
	    memcpy(name, (char*)data+pos, nlen);
	    pos += nlen;
	    name[nlen]='\0';
	    anim = GetShort_String(data+pos); pos+=2;
	    animspeed = data[pos++];
	    nrof = GetInt_String(data+pos); pos+=4;
	    if (revision >= 2) {
		type = GetShort_String(data+pos); pos+=2;
	    } else
		type = NO_ITEM_TYPE;
	    update_item (tag, loc, name, weight, face, flags, anim,
			 animspeed, nrof, type);
	    item_actions (locate_item(tag));
	}
	if (pos>len) 
	    fprintf(stderr,"ItemCmd: Overread buffer: %d > %d\n", pos, len);
    }
}

/* parsing the item data is 95% the same - we just need to pass the
 * revision so that the function knows what values to extact.
 */
void Item1Cmd(unsigned char *data, int len)
{
    common_item_command(data, len, 1);
}

void Item2Cmd(unsigned char *data, int len)
{
    common_item_command(data, len, 2);
}


/* UpdateItemCmd updates some attributes of an item */
void UpdateItemCmd(unsigned char *data, int len)
{
    int weight, loc, tag, face, sendflags,flags,pos=0,nlen,anim,nrof;
    char name[MAX_BUF];
    item *ip,*env=NULL;
    uint8 animspeed;

    sendflags = data[0];
    pos+=1;
    tag = GetInt_String(data+pos);
    pos+=4;
    ip = locate_item(tag);
    if (!ip) {
/*
	fprintf(stderr,"Got update_item command for item we don't have (%d)\n", tag);
*/
	return;
    }
    /* Copy all of these so we can pass the values to update_item and
     * don't need to figure out which ones were modified by this function.
     */
    *name='\0';
    loc=ip->env?ip->env->tag:0;
    weight=ip->weight * 1000;
    face = ip->face;
    flags = ip->flagsval;
    anim = ip->animation_id;
    animspeed = ip->anim_speed;
    nrof = ip->nrof;

    if (sendflags & UPD_LOCATION) {
	loc=GetInt_String(data+pos);
	env=locate_item(loc);
	if (!env) fprintf(stderr,"UpdateItemCmd: got tag of unknown object (%d) for new location\n", loc);
	pos+=4;
    }
    if (sendflags & UPD_FLAGS) {
	    flags = GetInt_String(data+pos);
	    pos+=4;
    }
    if (sendflags & UPD_WEIGHT) {
	    weight = GetInt_String(data+pos);
	    pos+=4;
    }
    if (sendflags & UPD_FACE) {
	    face = GetInt_String(data+pos);
	    pos+=4;
    }
    if (sendflags & UPD_NAME) {
	    nlen = data[pos++];
	    memcpy(name, (char*)data+pos, nlen);
	    pos += nlen;
	    name[nlen]='\0';
    }
    if (pos>len)  {
	    fprintf(stderr,"UpdateItemCmd: Overread buffer: %d > %d\n", pos, len);
	    return; /* we have bad data, probably don't want to store it then */
    }
    if (sendflags & UPD_ANIM) {
	    anim = GetShort_String(data+pos);
	    pos+=2;
    }
    if (sendflags & UPD_ANIMSPEED) {
	    animspeed = data[pos++];
    }
    if (sendflags & UPD_NROF) {
	    nrof = GetInt_String(data+pos);
	    pos+=4;
    }
    /* update_item calls set_item_values which will then set the list
     * redraw flag, so we don't need to do an explicit redraw here.  Actually,
     * calling update_item is a little bit of overkill, since we
     * already determined some of the values in this function.
     */
    update_item (tag, loc, name, weight, face, flags,anim,animspeed,nrof, ip->type);
    item_actions (locate_item(tag));
}

void DeleteItem(unsigned char *data, int len)
{
    int pos=0,tag;

    while (pos<len) {
	tag=GetInt_String(data); pos+=4;
	delete_item(tag);
    }
    if (pos>len) 
	fprintf(stderr,"ItemCmd: Overread buffer: %d > %d\n", pos, len);
}

void DeleteInventory(unsigned char *data, int len)
{
    int tag;

    tag=atoi((const char*)data);
    if (tag<0) {
	fprintf(stderr,"DeleteInventory: Invalid tag: %d\n", tag);
	return;
    }
    remove_item_inventory(locate_item(tag));
}

/******************************************************************************
 * Start of map commands
 *****************************************************************************/

/* expand_face sets (or clears) big_face information.  IT basically starts
 * at hx,hy (in the_map spaces), and expands to dx,dy (in the negative direction.)
 * it looks in these spaces for values that matches oldface.  If it finds
 * such a value, it sets it with newface.
 * if oldface is zero and newface is nonzero, it then sets the size_x and size_y
 * values to positive values which point to where the base of the face is -
 * this is used for drawing.
 * If oldface is nonzero and newface is zero, then it only clears
 * those values in which the size_ values match.
 * layer is the preferred layer that a new face should be put on.
 */
static void expand_face(int hx, int hy, int dx, int dy, int oldface, int newface, int layer)
{
    int x, y, i, zx, zy, l;

/*
    fprintf(stderr,"expand_face: hx=%d, hy=%d, dx=%d, dy=%d, old=%d, new=%d, layer=%d\n",
	    hx, hy, dx, dy, oldface, newface,layer);
*/

    for (x=0; x<dx; x++) {
	zx = hx - x;
	if (zx < 0) zx+= the_map.x;
	for (y=0; y<dy; y++) {
	    if (x==0 && y==0) continue;	    /* Special - don't expand into ourselves */
	    zy = hy - y;
	    if (zy < 0) zy += the_map.y;

	    /* This loop here tries to find a tails slot to put this info
	     * into.  we use l as the offset to better try and match the layer
	     * to store this on.
	     */
	    for (i=0; i < MAXLAYERS; i++) {
		l = (i + layer) % MAXLAYERS;

		/* We try to match oldface - this can be non
		 * zero when we are removing a big image - in that case, we need
		 * to make sure the size values are the same.
		 */
		if (the_map.cells[zx][zy].tails[l].face == oldface) {
		    if (oldface == 0 || 
			(the_map.cells[zx][zy].tails[l].size_x == x && 
			 the_map.cells[zx][zy].tails[l].size_y == y))
			    break;
		}
		/* If we already have this face information, break out - the code below
		 * will fill in the same values, but that isn't any big deal.
		 */
		if (the_map.cells[zx][zy].tails[l].face == newface && 
		    the_map.cells[zx][zy].tails[l].size_x == x && 
		    the_map.cells[zx][zy].tails[l].size_y == y) break;

	    }
	    /* Not found - either no space for a new face, or the oldface was already cleared out */
	    if (i>= MAXLAYERS) {
		fprintf(stderr,"expand_face: Did not find empty slot\n");
		return;
	    }

	    the_map.cells[zx][zy].tails[l].face = newface;
	    /* only set the size_ values if there is an actual image */
	    if (newface) {
		the_map.cells[zx][zy].tails[l].size_x = x;
		the_map.cells[zx][zy].tails[l].size_y = y;
	    }
	    /* Else setting to empty face */
	    else {
		the_map.cells[zx][zy].tails[l].size_x = 0;
		the_map.cells[zx][zy].tails[l].size_y = 0;
	    }
	    the_map.cells[zx][zy].need_update = 1;
	} /* for y */
    } /* for x */
}

/* This does the task of actually clearing the data in a cell.
 * This is called if this space was cleared but we still wanted
 * to display the data for fog of war code.  This is called
 * when we have real data to over-write the contents of this
 * cell.
 */
void reset_cell_data(int x, int y)
{

    /* This cell has been cleared previously but now we are 
     * writing new data to do. So we have to clear it for real now 
     */
    int i= 0;

    the_map.cells[x][y].darkness= 0;
    the_map.cells[x][y].need_update= 1;
    the_map.cells[x][y].have_darkness= 0;
    the_map.cells[x][y].cleared= 0;
    /* only clear the even faces - those explicitly sent to us by the server */
    for (i=0; i<MAXLAYERS; i++) {
	/* If we are clearing out a 'big image', we need to clear the faces that
	 * this points into.
	 */
	if (the_map.cells[x][y].heads[i].face != 0 &&
	    (the_map.cells[x][y].heads[i].size_x>1 || the_map.cells[x][y].heads[i].size_y>1))
	    expand_face(x, y, the_map.cells[x][y].heads[i].size_x, the_map.cells[x][y].heads[i].size_y,
			the_map.cells[x][y].heads[i].face, 0, i);

	the_map.cells[x][y].heads[i].face= 0;  /* empty/blank face */
	the_map.cells[x][y].heads[i].size_x = 0;
	the_map.cells[x][y].heads[i].size_y = 0;
    }
}
/* Modified the logic of this.  Basically, we always act as if
 * we are using the fog mode in terms of clearing the data.  We'll
 * leave it to the display logic to determine if it should draw
 * the space black or draw it in fog fashion.
 * This makes the logic much easier when dealing with the big images,
 * as we sort of need to track that a portion of a multipart image is
 * in fact on that space, because when the space re-appears in view,
 * we may not know otherwise know that we need to update that space
 * again.
 */
void display_map_clearcell(int x,int y)
{

    int i,got_one=0;

    x+= pl_pos.x;
    y+= pl_pos.y;

    /* If the space is completly blank, don't mark this as a 
     * fog cell - that chews up extra cpu time.  Likewise,
     * if the space is completely dark, don't draw it either.
     * We do check the odd values (heads on other spaces),
     * because we are only checking to see if we should
     * in fact draw this space.
     */
    for (i=0; i<MAXLAYERS; i++) {
	if (the_map.cells[x][y].heads[i].face>0) got_one=1;
	if (the_map.cells[x][y].tails[i].face>0) got_one=1;
    }

    /* This spell is either already blank or completely dark */
#if 0
    if (!got_one || the_map.cells[x][y].darkness>200) {
#else
    if (!got_one) {
#endif
	reset_cell_data(x,y);
    }
    /* else this space has stuff we want to draw */
    else {
	the_map.cells[x][y].cleared= 1;
	the_map.cells[x][y].need_update= 1;
    }
    return;
}


/* sets the face at layer to some value.  We just can't
 * restact arbitrarily, as the server now sends faces only
 * for layers that change, and not the entire space.
 * offx and offy are typically zero - this is where the head
 * of the face is located.  This can be used by the client
 * to only blit the portions on the actual space.  This
 * is very important in the case where the head may be off the
 * viewable area, but a portion is visible.
 */
static void set_map_face(int x, int y, int layer, int face)
{
    x+= pl_pos.x;
    y+= pl_pos.y;

    /* this space is getting changed.  If the old face was a 'big face',
     * we need to clear the spaces that we were set to draw.
     * This loop basically just looks through the spaces that the face
     * we are clearing out extends to, and marks them as blank.
     * Set the new face to zero - the logic below will re-set this face
     * if this is a case of a face changing (eg, animated)
     */

    if (the_map.cells[x][y].heads[layer].face != 0 &&
       (the_map.cells[x][y].heads[layer].size_x>1 || the_map.cells[x][y].heads[layer].size_y>1))
	    expand_face(x, y, the_map.cells[x][y].heads[layer].size_x, the_map.cells[x][y].heads[layer].size_y,
		    the_map.cells[x][y].heads[layer].face, 0, layer);


    the_map.cells[x][y].heads[layer].face = face;
    get_map_image_size(face, 
		    &the_map.cells[x][y].heads[layer].size_x, 
		    &the_map.cells[x][y].heads[layer].size_y);

    the_map.cells[x][y].need_update = 1;
    the_map.cells[x][y].have_darkness = 1;
}


void NewmapCmd(unsigned char *data, int len)
{
    display_map_newmap();
}
/* This is the common processing block for the map1 and
 * map1a protocol commands.  The map1a mieks minor extensions
 * and are easy to deal with inline (in fact, this code
 * doesn't even care what rev is - just certain bits will
 * only bet set when using the map1a command.
 * rev is 0 for map1,
 * 1 for map1a.  It conceivable that there could be future
 * revisions.
 */
#define NUM_LAYERS 2
static void map1_common(unsigned char *data, int len, int rev)
{
    int mask, x, y, pos=0,face, layer;

    map1cmd=1;
    if (!mapupdatesent) /* see MapExtendedCmd */
        display_map_startupdate();
/*    fprintf(stderr,"map1 bytes %d\n", len);*/

    while (pos <len) {
	mask = GetShort_String(data+pos); pos+=2;
	x = (mask >>10) & 0x3f;
	y = (mask >>4) & 0x3f;


	/* If there are no low bits (face/darkness), this space is
	 * not visible.
	 */
	if ((mask & 0xf) == 0) {
	    display_map_clearcell(x,y);
	    continue;	/* if empty mask, none of the checks will be true. */
	}
	/* If this space was previously stored for fog of war, need to clear
	 * it now.  Needs to be done before anything else happens that we
	 * might want to store in it.
	 */

	if(the_map.cells[x+pl_pos.x][y+pl_pos.y].cleared == 1) {
	    reset_cell_data(x+pl_pos.x, y+pl_pos.y);
	}

	if (mask & 0x8) { /* darkness bit */
	    set_map_darkness(x,y, (uint8)(data[pos]));
	    pos++;
	}
	/* Reduce redundant by putting the get image
	 * and flags in a common block.  The layers
	 * are the inverse of the bit order unfortunately.
	 */
	for (layer=NUM_LAYERS; layer>=0; layer--) {
	    if (mask & (1 << layer)) {
		face = GetShort_String(data+pos); pos +=2;
        if ( (face<0) || (face>10000)){
            printf ("BUG: face %d for %dx%d at layer %d\n",face,x,y,NUM_LAYERS - layer);
            printf ("     mask was %2X\n",mask);
        }
		set_map_face(x, y, NUM_LAYERS - layer, face);
	    }
	}

	/* If this is a big space, need to tell spaces that this extends into about it.
	 * We do this this after filling in all the faces for the space.  This
	 * is necessary because expand_face collapses duplicates.  But we can get
	 * into cases were layer 2 adds a space and layer 1 removes it because it used
	 * to be on layer 1.  This results in it completely disappearing.  So
	 * we do this here, because layer 2 still has the right value.
	 */
	x+= pl_pos.x;
	y+= pl_pos.y;
	for (layer=0; layer<MAXLAYERS; layer++) {
	    if (the_map.cells[x][y].heads[layer].face &&
		(the_map.cells[x][y].heads[layer].size_x>1 || the_map.cells[x][y].heads[layer].size_y>1))
		expand_face(x, y, the_map.cells[x][y].heads[layer].size_x,
		    the_map.cells[x][y].heads[layer].size_y,
		    0, the_map.cells[x][y].heads[layer].face, layer);
	}
    }
    mapupdatesent=0;
    display_map_doneupdate(FALSE);

}

/* These wrapper functions actually are not needed since
 * the logic above doesn't care what the revision actually
 * is.
 */

void Map1Cmd(unsigned char *data, int len)
{
    map1_common(data, len, 0);
}
void Map1aCmd(unsigned char *data, int len)
{
    map1_common(data, len, 1);
}


void map_scrollCmd(char *data, int len)
{
    int dx,dy;
    char *buf;

    dx = atoi(data);
    buf = strchr(data,' ');
    if (!buf) {
	fprintf(stderr,"map_scrollCmd: Got short packet.\n");
	return;
    }
    buf++;
    dy = atoi(buf);
    display_mapscroll(dx,dy);
}

/* Extract smoothing infos from an extendedmapinfo packet part
 * data is located at the beginning of the smooth datas
 */
int ExtSmooth(unsigned char* data,int len,int x,int y,int layer){
    static int dx[8]={0,1,1,1,0,-1,-1,-1};
    static int dy[8]={-1,-1,0,1,1,1,0,-1};
    int i,rx,ry;
    if (len<1)
        return 0;
    x+= pl_pos.x;
	y+= pl_pos.y;
    for (i=0;i<8;i++){
        rx=x+dx[i];
        ry=y+dy[i];
        if ( (rx<0) || (ry<0) || (the_map.x<=rx) || (the_map.y<=ry))
            continue;
        the_map.cells[x][y].need_resmooth=1;            
    }
    the_map.cells[x][y].smooth[layer]=GetChar_String(data);
    return 1;/*Cause smooth infos only use 1 byte*/
}
/* Handle MapExtended command
 * Warning! if you add commands to extended, take
 * care that the 'layer' argument of main loop is
 * the opposite of the layer of the map.
 * so if you reference a layer, use NUM_LAYERS-layer
 */
void MapExtendedCmd(unsigned char *data, int len){
    int mask, x, y, pos=0, layer;
    int noredraw=0;
    int hassmooth=0;
    int entrysize;
    int startpackentry;
    map1cmd=1;
    if (!mapupdatesent)
        display_map_startupdate();
    mapupdatesent=1;
	mask = GetChar_String(data+pos); pos+=1;
    if (mask&EMI_NOREDRAW)
        noredraw=1;
    if (mask&EMI_SMOOTH){
        hassmooth=1;
    }
    while (mask&EMI_HASMOREBITS){
        /*There may be bits we ignore about*/
        mask = GetChar_String(data+pos); 
        pos+=1;
    }
    entrysize=GetChar_String(data+pos); 
    pos=pos+1;
    
    
    while (pos+entrysize+2 <=len) {
        mask = GetShort_String(data+pos); pos+=2;
        x = (mask >>10) & 0x3f;
        y = (mask >>4) & 0x3f;       
        for (layer=NUM_LAYERS; layer>=0; layer--) {
            if (mask & (1 << layer)) {
                /*handle an entry*/
                if (pos+entrysize>len)/*erroneous packet*/
                    break;
                startpackentry=pos;
                /* If you had extended infos to the server, this
                 * is where, in the client, you may add your code
                 */
                if (hassmooth)
                    pos=pos+ExtSmooth(data+pos,len-pos,x,y,NUM_LAYERS - layer);
                /* continue with other if you add new extended 
                 * infos to server
                 */
                 
                /* Now point to the next data */
                pos=startpackentry+entrysize; 
                
            }
        }
	}
    if (!noredraw){        
        display_map_doneupdate(FALSE);
        mapupdatesent=0;
    }
}


void MagicMapCmd(unsigned char *data, int len)
{
    unsigned char *cp;
    int i;

    /* First, extract the size/position information. */
    if (sscanf((const char*)data,"%hd %hd %hd %hd", &cpl.mmapx, &cpl.mmapy, 
	&cpl.pmapx, &cpl.pmapy)!=4) {
	fprintf(stderr,"Was not able to properly extract magic map size, pos\n");
	return;
    }
    /* Now we need to find the start of the actual data.  There are 4
     * space characters we need to skip over.
     */
    for (cp=data, i=0; i<4 && cp < (data+len); cp++)
	if (*cp==' ') i++;
    if (i!=4) {
	fprintf(stderr,"Was unable to find start of magic map data\n");
	return;
    }
    i = len - (cp - data); /* This should be the number of bytes left */
    if (i != cpl.mmapx * cpl.mmapy) {
	fprintf(stderr,"magic map size mismatch.  Have %d bytes, should have %d\n",
		i, cpl.mmapx * cpl.mmapy);
	return;
    }
    if (cpl.magicmap) free(cpl.magicmap);
    cpl.magicmap = malloc(cpl.mmapx * cpl.mmapy);
    /* Order the server puts it in should be just fine.  Note that
     * the only requirement that this works is that magicmap by 8 bits,
     * being that is the size specified in the protocol and what the
     * server sends us.
     */
    memcpy(cpl.magicmap, cp, cpl.mmapx * cpl.mmapy);
    cpl.showmagic=1;
    draw_magic_map();
}
