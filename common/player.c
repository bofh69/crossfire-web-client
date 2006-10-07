const char *rcsid_common_player_c =
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

 
#ifdef WIN32
#include <windows.h>
#endif
#include <client.h>
#include <external.h>
#include <script.h>

/* This file handles various player related functions.  This includes
 * both things that operate on the player item, cpl structure, or
 * various commands that the player issues.
 *
 *  This file does most of the handling of commands from the client to
 *  server (see commands.c for server->client)
 *
 *  does most of the work for sending messages to the server
 *   Again, most of these appear self explanatory.  Most send a bunch of
 *   commands like apply, examine, fire, run, etc.  This looks like it
 *   was done by Mark to remove the old keypress stupidity I used. 
 */

/* This translates the numeric direction id's into the actual direction
 * commands.  This lets us send the actual command (ie, 'north'), which
 * makes handling on the server side easier.
 */

const char *const directions[9] = {"stay", "north", "northeast", "east", "southeast",
		"south","southwest", "west", "northwest"};


/*
 *  Initialiazes player item, information is received from server
 */
void new_player (long tag, char *name, long weight, long face)
{
    Spell *spell, *spnext;

    cpl.ob->tag    = tag;
    cpl.ob->nrof   = 1;
    copy_name (cpl.ob->d_name, name);
    cpl.ob->weight = (float) weight / 1000;
    cpl.ob->face   = face;

    if (cpl.spelldata) {
	for (spell = cpl.spelldata; spell; spell = spnext) {
	    spnext = spell->next;
	    free(spell);
	}
	cpl.spelldata = NULL;
    }
	
}

void look_at(int x, int y)
{
    cs_print_string(csocket.fd, "lookat %d %d", x, y);
}

void client_send_apply (int tag)
{
    cs_print_string(csocket.fd, "apply %d", tag);
}

void client_send_examine (int tag)
{
    cs_print_string(csocket.fd, "examine %d", tag);

}

/* Requests nrof objects of tag get moved to loc. */
void client_send_move (int loc, int tag, int nrof)
{
    cs_print_string(csocket.fd, "move %d %d %d", loc, tag, nrof);
}



void move_player(int dir) {
    /* Should we perhaps use the real repeat count here? */
    send_command(directions[dir], -1, SC_NORMAL);
}


/* Fire & Run code.  The server handles repeating of these actions, so
 * we only need to send a run or fire command for a particular direction
 * once - we use the drun and dfire to keep track if we need to send
 * the full command.
 */
static int drun=-1, dfire=-1;

/* Fires in a specified direction.  Note that direction 0 is a valid
 * case - the fire is centered on the player.
 */

void stop_fire()
{
    if (cpl.input_state != Playing) return;
    dfire |= 0x100;
}

void clear_fire_run()
{
    if ((dfire!=-1) && (dfire & 0x100)) {
	send_command("fire_stop", -1, SC_FIRERUN);
	dfire=-1;
    }
    if ((drun!=-1) && (drun & 0x100)) {
	send_command("run_stop", -1, SC_FIRERUN);
	drun=-1;
    }
}

void clear_fire()
{
    if (dfire!=-1) {
	send_command("fire_stop", -1, SC_FIRERUN);
	dfire=-1;
    }
}

void clear_run()
{
    if (drun!=-1) {
	send_command("run_stop", -1, SC_FIRERUN);
	drun=-1;
    }
}


void fire_dir(int dir) {
    char buf[MAX_BUF];

    if (cpl.input_state != Playing) return;
    if (dir!= dfire) {
	sprintf(buf,"fire %d", dir);
	if (send_command(buf, cpl.count, SC_NORMAL)) {
	    dfire=dir;
	    cpl.count=0;
	}
    } else {
	dfire &= 0xff;	/* Mark it so that we need a stop_fire */
    }
}

void stop_run()
{
    send_command("run_stop", -1, SC_FIRERUN);
    drun |= 0x100;
}

void run_dir(int dir) {
    char buf[MAX_BUF];

    if (dir!=drun) {
	sprintf(buf,"run %d", dir);
	if (send_command(buf, -1, SC_NORMAL))
	    drun=dir;
    } else {
	drun &= 0xff;
    }
}


/* This should be used for all 'command' processing.  Other functions should
 * call this so that proper windowing will be done.
 * command is the text command, repeat is a count value, or -1 if none
 * is desired and we don't want to reset the current count.
 * must_send means we must send this command no matter what (ie, it is
 * an administrative type of command like fire_stop, and failure to send
 * it will cause definate problems
 * return 1 if command was sent, 0 if not sent.
 */

int send_command(const char *command, int repeat, int must_send) {
    static char last_command[MAX_BUF]="";

    script_monitor(command,repeat,must_send);
    if (cpl.input_state==Reply_One) {
	LOG(LOG_ERROR,"common::send_command","Wont send command '%s' - since in reply mode!",
		command);
	cpl.count=0;
	return 0;
    }

    /* Does the server understand 'ncom'? If so, special code */
    if (csocket.cs_version >= 1021) {
	int commdiff=csocket.command_sent - csocket.command_received;

	if (commdiff<0) commdiff +=256;

	/* if too many unanswered commands, not a must send, and command is
	 * the same, drop it
	 */
	if (commdiff>use_config[CONFIG_CWINDOW] && !must_send && !strcmp(command, last_command)) {
	    if (repeat!=-1) cpl.count=0;
	    return 0;
#if 0 /* Obnoxious warning message we don't need */
	    fprintf(stderr,"Wont send command %s - window oversized %d %d\n",
		    command, csocket.command_sent, csocket.command_received);
#endif
	}
	else {
	    SockList sl;
	    uint8 buf[MAX_BUF];

	    /* Don't want to copy in administrative commands */
	    if (!must_send) strcpy(last_command, command);
	    csocket.command_sent++;
	    csocket.command_sent &= 0xff;   /* max out at 255 */

	    SockList_Init(&sl, buf);
	    SockList_AddString(&sl, "ncom ");
	    SockList_AddShort(&sl, csocket.command_sent);
	    SockList_AddInt(&sl, repeat);
	    SockList_AddString(&sl, command);
	    SockList_Send(&sl, csocket.fd);
	}
    } else {
	cs_print_string(csocket.fd, "command %d %s", repeat,command);
    }
    if (repeat!=-1) cpl.count=0;
    return 1;
}

void CompleteCmd(unsigned char *data, int len)
{
    if (len !=6) {
	LOG(LOG_ERROR,"common::CompleteCmd","Invalid length %d - ignoring", len);
	return;
    }
    csocket.command_received = GetShort_String(data);
    csocket.command_time = GetInt_String(data+2);
    script_sync(csocket.command_sent - csocket.command_received);
}



/* This does special processing on the 'take' command.  If the
 * player has a container open, we want to specifiy what object
 * to move from that since we've sorted it.  command is
 * the command as tped, cpnext is any optional params.
 */
void command_take (const char *command, const char *cpnext)
{
    /* If the player has specified optional data, or the player
     * does not have a container open, just issue the command
     * as normal
     */
    if (cpnext || cpl.container == NULL)
	send_command(command, cpl.count, 0);
    else {
	if (cpl.container->inv == NULL) 
	    draw_info("There is nothing in the container to move", NDI_BLACK);
	else
	    cs_print_string(csocket.fd,"move %d %d %d", cpl.ob->tag, 
		cpl.container->inv->tag, cpl.count);
    }
}
