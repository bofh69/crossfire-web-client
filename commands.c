/*
 * static char *rcsid_commands_c =
 *   "$Id$";
 */
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2000 Mark Wedel

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

    The author can be reached via e-mail to mwedel@scruz.net
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

#include <client.h>


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
	    if (!strcmp(param,"FALSE")) {
		if (nosound)
		    cs_write_string(csocket.fd,"setsound 0", 10);
		else
		    cs_write_string(csocket.fd,"setsound 1", 10);
	    }
	}
	else if (!strcmp(cmd,"sexp")) {
	    if (!strcmp(param,"FALSE")) {
		fprintf(stderr,"Server returned FALSE on setup sexp\n");
	    }
	} else if (!strcmp(cmd,"mapsize")) {
	    int x, y=0;
	    char *cp,tmpbuf[MAX_BUF];

	    if (!strcasecmp(param, "false")) {
		draw_info("Server only supports standard sized maps (11x11)", NDI_RED);
		/* Do this because we may have been playing on a big server before */
		mapx = 11;
		mapy = 11;
		resize_map_window(mapx,mapy);
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
	    if (want_mapx > x || want_mapy > y) {
		if (want_mapx > x) want_mapx = x;
		if (want_mapy > y) want_mapy = y;
		sprintf(tmpbuf," setup mapsize %dx%d", want_mapx, want_mapy);
		cs_write_string(csocket.fd, tmpbuf, strlen(tmpbuf));
		sprintf(tmpbuf,"Server supports a max mapsize of %d x %d - requesting a %d x %d mapsize",
			x, y, want_mapx, want_mapy);
		draw_info(tmpbuf,NDI_RED);
	    }
	    else if (want_mapx == x && want_mapy == y) {
		mapx = x;
		mapy = y;
		resize_map_window(x,y);
	    }
	    else {
		/* Our request was not bigger than what server supports, and 
		 * not the same size, so whats the problem?  Tell the user that
		 * something is wrong.
		 */
		sprintf(tmpbuf,"Unable to set mapsize on server - we wanted %d x %d, server returnd %d x %d",
			want_mapx, want_mapy, x, y);
		draw_info(tmpbuf,NDI_RED);
	    }
	} else {
	    fprintf(stderr,"Got setup for a command we don't understand: %s %s\n",
		    cmd, param);
	}

    }
}

/* Move these here - they don't contain any windows dependant
 * code.
 */

/* We only get here if the server believes we are caching images. */
/* We rely on the fact that the server will only send a face command for
 * a particular number once - at current time, we have no way of knowing
 * if we have already received a face for a particular number.
 */

void FaceCmd(unsigned char *data,  int len)
{
    int pnum;
    char *face;

    /* A quick sanity check, since if client isn't caching, all the data
     * structures may not be initialized.
     */
    if (!display_willcache()) {
	fprintf(stderr,"Received a 'face' command when we are not caching\n");
	return;
    }
    pnum = GetShort_String(data);
    face = (char*)data+2;
    data[len] = '\0';

    finish_face_cmd(pnum, 0, 0, face);

}

void Face1Cmd(unsigned char *data,  int len)
{
    int pnum;
    uint32  checksum;
    char *face;

    /* A quick sanity check, since if client isn't caching, all the data
     * structures may not be initialized.
     */
    if (!display_willcache()) {
	fprintf(stderr,"Received a 'face' command when we are not caching\n");
	return;
    }
    pnum = GetShort_String(data);
    checksum = GetInt_String(data+2);
    face = (char*)data+6;
    data[len] = '\0';

    finish_face_cmd(pnum, checksum, 1, face);
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

void PixMapCmd(unsigned char *data,  int len)
{  
    int pnum,plen;

    pnum = GetInt_String(data);
    plen = GetInt_String(data+4);
/*    fprintf(stderr,"Recieved pixmap %d, len %d", pnum, plen);*/
    if (len<8 || (len-8)!=plen) {
	fprintf(stderr,"PixMapCmd: Lengths don't compare (%d,%d)\n",
		(len-8),plen);
	return;
    }
    display_newpixmap(pnum,(char*)data+8,plen);
}

void ImageCmd(unsigned char *data,  int len)
{  
    int pnum,plen;

    pnum = GetInt_String(data);
    plen = GetInt_String(data+4);
/*    fprintf(stderr,"Recieved pixmap %d, len %d", pnum, plen);*/
    if (len<8 || (len-8)!=plen) {
	fprintf(stderr,"PixMapCmd: Lengths don't compare (%d,%d)\n",
		(len-8),plen);
	return;
    }
    /* Currently, the image command should only really be used for 
     * for png, but it could just as easily get used for xpm with no
     * significant changes.
     */
    if (display_usexpm()) {
	display_newpixmap(pnum,(char*)data+8,plen);
    } else if (display_usepng()) {
	display_newpng(pnum,(char*)data+8,plen);
    }
    else {
	fprintf(stderr,"Image command called with unknown image type.\n");
    }
}

void BitMapCmd(unsigned char *data, int len)
{
    int pnum = GetInt_String(data);

    /* We take off 6 bytes for image number, fg, bg */
    if ((len-6) != (24*3)) {
	fprintf(stderr,"Incorrect length on bitmap buffer should be %d was %d\n",
		24*3,len-6);
	return;
    }
    display_newbitmap(pnum,data[4],data[5],(char*)data+6);
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
  int i=0;
  int c;

    while (i<len) {
	c=data[i++];
	if (c>=CS_STAT_RESIST_START && c<=CS_STAT_RESIST_END) {
	    cpl.stats.resists[c-CS_STAT_RESIST_START]=GetShort_String(data+i);
	    i+=2;
	    cpl.stats.resist_change=1;
	} else {
	    switch (c) {
		case CS_STAT_HP:	cpl.stats.hp=GetShort_String(data+i); i+=2; break;
		case CS_STAT_MAXHP:cpl.stats.maxhp=GetShort_String(data+i); i+=2; break;
		case CS_STAT_SP:	cpl.stats.sp=GetShort_String(data+i); i+=2; break;
		case CS_STAT_MAXSP:cpl.stats.maxsp=GetShort_String(data+i); i+=2; break;
		case CS_STAT_GRACE:cpl.stats.grace=GetShort_String(data+i); i+=2; break;
		case CS_STAT_MAXGRACE:cpl.stats.maxgrace=GetShort_String(data+i); i+=2; break;
		case CS_STAT_STR:	cpl.stats.Str=GetShort_String(data+i); i+=2; break;
		case CS_STAT_INT:	cpl.stats.Int=GetShort_String(data+i); i+=2; break;
		case CS_STAT_POW:	cpl.stats.Pow=GetShort_String(data+i); i+=2; break;
		case CS_STAT_WIS:	cpl.stats.Wis=GetShort_String(data+i); i+=2; break;
		case CS_STAT_DEX:	cpl.stats.Dex=GetShort_String(data+i); i+=2; break;
		case CS_STAT_CON:	cpl.stats.Con=GetShort_String(data+i); i+=2; break;
		case CS_STAT_CHA:  cpl.stats.Cha=GetShort_String(data+i); i+=2; break;
		case CS_STAT_EXP:  cpl.stats.exp=GetInt_String(data+i); i+=4; break;
		case CS_STAT_LEVEL:cpl.stats.level=GetShort_String(data+i); i+=2; break;
		case CS_STAT_WC:   cpl.stats.wc=GetShort_String(data+i); i+=2; break;
		case CS_STAT_AC:   cpl.stats.ac=GetShort_String(data+i); i+=2; break;
		case CS_STAT_DAM:  cpl.stats.dam=GetShort_String(data+i); i+=2; break;
		case CS_STAT_ARMOUR:cpl.stats.resists[0]=GetShort_String(data+i); i+=2; break;
		case CS_STAT_SPEED: cpl.stats.speed=GetInt_String(data+i); i+=4; break;
		case CS_STAT_FOOD:  cpl.stats.food=GetShort_String(data+i); i+=2; break;
		case CS_STAT_WEAP_SP:cpl.stats.weapon_sp=GetInt_String(data+i); i+=4; break;
		case CS_STAT_FLAGS:cpl.stats.flags=GetShort_String(data+i); i+=2; break;
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
    draw_stats(0);
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
    char buf[MAXSOCKBUF];

    sprintf(buf,"reply %s", text);

    cs_write_string(csocket.fd, buf, strlen(buf));
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

/* ItemCmd grabs and display information for items in the inventory */
void ItemCmd(unsigned char *data, int len)
{
    int weight, loc, tag, face, flags,pos=0,nlen;
    char name[MAX_BUF];

    loc = GetInt_String(data);
    pos+=4;

    if (pos == len) {
	/* An empty item command can be used to delete the whole
	 * inventory of item 'loc'. Esspecially if loc is 0, then
	 * there is no floor under the player.
	 */
/*	delete_item_inventory(loc);*/
	fprintf(stderr,"ItemCmd: Got location with no other data\n");
    }
    else if (loc < 0) { /* delele following items */
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
	    update_item (tag, loc, name, weight, face, flags,0,0,-1);
	    item_actions (locate_item(tag));
	}
	if (pos>len) 
	    fprintf(stderr,"ItemCmd: Overread buffer: %d > %d\n", pos, len);
    }
}

/* ItemCmd grabs and display information for items in the inventory */
void Item1Cmd(unsigned char *data, int len)
{
    int weight, loc, tag, face, flags,pos=0,nlen,anim,nrof;
    uint8 animspeed;
    char name[MAX_BUF];

    loc = GetInt_String(data);
    pos+=4;

    if (pos == len) {
	/* An empty item command can be used to delete the whole
	 * inventory of item 'loc'. Esspecially if loc is 0, then
	 * there is no floor under the player.
	 */
/*	delete_item_inventory(loc);*/
	fprintf(stderr,"ItemCmd: Got location with no other data\n");
    }
    else if (loc < 0) { /* delele following items */
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
	    update_item (tag, loc, name, weight, face, flags, anim,
			 animspeed, nrof);
	    item_actions (locate_item(tag));
	}
	if (pos>len) 
	    fprintf(stderr,"ItemCmd: Overread buffer: %d > %d\n", pos, len);
    }
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
    update_item (tag, loc, name, weight, face, flags,anim,animspeed,nrof);
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

void Map_unpacklayer(unsigned char *cur,unsigned char *end)
{
  long x,y,face;
  int xy;
  int clear = 1;
  unsigned char *fcur;
  
  while (cur < end && *cur != 255) {
    xy = *cur;
    cur++;
    x = xy / mapy;
    y = xy % mapy;
    display_map_clearcell(x,y);
  }
  cur++;
  while(cur < end) {
    fcur = cur;
    face = *cur & 0x7f;
    cur++;
    face = (face << 8) | *cur;
    cur++;

    /* Now process all the spaces with this face */
    while(1) {
      xy = *cur & 0x7f;
      x = xy / mapy;
      y = xy % mapy;
      if (clear) {
	display_map_clearcell(x,y);
      }
      display_map_addbelow(x,y,face);
      xy = *cur;
      cur++;
      if (xy & 128) {
	break;
      }
      if (cur==end) {
	fprintf(stderr,"Incorrectly packed map.\n");
	abort();
      }
    }
    /* Got end of layer */
    if (*fcur & 128) 
      clear = 0; /* end of first layer (or some layer after the first) */
  }
}

void MapCmd(unsigned char *data, int len)
{
    unsigned char *end;

#if 0
    fprintf(stderr,"Got MapCmd - %d bytes\n", len);
#endif
    map1cmd=0;
    end = data + len;

    display_map_startupdate();
    Map_unpacklayer(data,end);
    display_map_doneupdate(FALSE);
}

void Map1Cmd(unsigned char *data, int len)
{
    int mask, x, y, pos=0,face;

    map1cmd=1;
#if 0
    fprintf(stderr,"Got Map1Cmd - %d bytes\n", len);
#endif
    display_map_startupdate();

    while (pos <len) {
	mask = GetShort_String(data+pos); pos+=2;
	x = (mask >>10) & 0x3f;
	y = (mask >>4) & 0x3f;
	/* If there are no low bits (face/darkness), this space is
	 * not visible.
	 */
	if ((mask & 0xf) == 0) {
	    display_map_clearcell(x,y);
	}
	if (mask & 0x8) { /* darkness bit */
	    set_map_darkness(x,y, (uint8)(data[pos]));
	    pos++;
	}
	if (mask & 0x4) { /* floor bit */
	    face = GetShort_String(data+pos); pos +=2;
	    set_map_face(x,y,0, face);
	}
	if (mask & 0x2) { /* middle face */
	    face = GetShort_String(data+pos); pos +=2;
	    set_map_face(x,y,1, face);
	}
	if (mask & 0x1) { /* top face */
	    face = GetShort_String(data+pos); pos +=2;
	    set_map_face(x,y,2, face);
	}
    }
    display_map_doneupdate(FALSE);
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
