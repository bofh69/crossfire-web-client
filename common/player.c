/*
 * static char *rcsid_player_c =
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

 
#include <client.h>
#include <external.h>

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

char *directions[9] = {"stay", "north", "northeast", "east", "southeast",
		"south","southwest", "west", "northwest"};


/*
 *  Initialiazes player item, information is received from server
 */
void new_player (long tag, char *name, long weight, long face)
{
    cpl.ob->tag    = tag;
    cpl.ob->nrof   = 1;
    copy_name (cpl.ob->d_name, name);
    cpl.ob->weight = (float) weight / 1000;
    cpl.ob->face   = face;
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

    if (cpl.input_state==Reply_One) {
	fprintf(stderr,"Wont send command '%s' - since in reply mode!\n ",
		command);
	cpl.count=0;
	return 0;
    }

    /* Does the server understand 'ncom'? If so, special code */
    if (csocket.cs_version >= 1021) {
	int commdiff=csocket.command_sent - csocket.command_received;

	if (commdiff<0) commdiff +=256;

	/* if to many unanswer commands, not a must send, and command is
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
	    char buf[MAX_BUF];

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
	fprintf(stderr,"comc - invalid length %d - ignoring\n", len);
    }
    csocket.command_received = GetShort_String(data);
    csocket.command_time = GetInt_String(data+2);
}


/* Show a basic help message */
static void command_help(char *cpnext) {
    char buf[MAX_BUF];

    if (cpnext) {
	sprintf(buf,"help %s", cpnext);
	/* maybe not a must send, but we probably don't want to drop it */
	send_command(buf, -1, 1);
    } else {
	draw_info("Client Side Commands", NDI_NAVY);
	draw_info(" bind        - bind a command to key", NDI_BLACK);
	draw_info(" unbind      - unbind a command, show", NDI_BLACK);
	draw_info("               bindings", NDI_BLACK);
	draw_info(" cwindow <val> set size of command", NDI_BLACK);
	draw_info("               window (if val is exceeded", NDI_BLACK);
	draw_info("               client won't send new", NDI_BLACK);
	draw_info("               commands to server", NDI_BLACK);
	draw_info(" foodbeep    - toggle audible low on food", NDI_BLACK);
	draw_info("               warning", NDI_BLACK);
	draw_info(" disconnect  - close connection to server", NDI_BLACK);
	draw_info(" magicmap    - show last received magicmap", NDI_BLACK);
	draw_info(" metaserver  - Get updated list of metaservers", NDI_BLACK);
	draw_info("               and show it.  Warning: This may", NDI_BLACK);
	draw_info("               freeze the client until it gets", NDI_BLACK);
	draw_info("               the update.", NDI_BLACK);
	draw_info(" showicon    - draw status icons in", NDI_BLACK);
	draw_info("               inventory window", NDI_BLACK);
	draw_info(" showweight  - show weight in inventory", NDI_BLACK);
	draw_info("               look windows", NDI_BLACK);
	draw_info(" scroll      - toggle scroll/wrap mode in",NDI_BLACK);
	draw_info("               info window", NDI_BLACK);
	draw_info(" savewinpos  - save window positions - ", NDI_BLACK);
	draw_info("               split windows mode only", NDI_BLACK);
	draw_info(" savedefaults  save various defaults into", NDI_BLACK);
	draw_info("               ~/.crossfire/defaults", NDI_BLACK);
	draw_info(" show        - determine what type of items", NDI_BLACK);
	draw_info("               to show in inventory", NDI_BLACK);
	draw_info(" autorepeat  - toggle autorepeat", NDI_BLACK);
	send_command("help", -1, 1);
    }
}

static void set_command_window(char *cpnext)
{
    if (!cpnext) {
	draw_info("cwindow command requires a number parameter", NDI_BLACK);
    } else {
	want_config[CONFIG_CWINDOW] = atoi(cpnext);
	if (want_config[CONFIG_CWINDOW]<1 || want_config[CONFIG_CWINDOW]>127)
	    want_config[CONFIG_CWINDOW]=COMMAND_WINDOW;
	else
	    use_config[CONFIG_CWINDOW] = want_config[CONFIG_CWINDOW];
    }
}

static void command_foodbep(char *cpnext)
{
    if (want_config[CONFIG_FOODBEEP]) {
	want_config[CONFIG_FOODBEEP]=0;
	draw_info("Warning bell when low on food disabled", NDI_BLACK);
    } else {
	want_config[CONFIG_FOODBEEP]=1;
	draw_info("Warning bell when low on food enabled", NDI_BLACK);
    }
    use_config[CONFIG_FOODBEEP] = want_config[CONFIG_FOODBEEP];
}

/* This does special processing on the 'take' command.  If the
 * player has a container open, we want to specifiy what object
 * to move from that since we've sorted it.  command is
 * the command as tped, cpnext is any optional params.
 */
void command_take (const char *command, char *cpnext)
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


/* This is an extended command (ie, 'who, 'whatever, etc).  In general,
 * we just send the command to the server, but there are a few that
 * we care about (bind, unbind)
 *
 * The command past to us can not be modified - if it is a keybinding,
 * we get passed the string that is that binding - modifying it effectively
 * changes the binding.
 */

void extended_command(const char *ocommand) {
    const char *cp = ocommand;
    char *cpnext, command[MAX_BUF];

    if ((cpnext = strchr(cp, ' '))!=NULL) {
	int len = cpnext - ocommand;
	if (len > (MAX_BUF -1 )) len = MAX_BUF-1;

	strncpy(command, ocommand, len);
	command[len] = '\0';
	cp = command;
	while (*cpnext == ' ')
	    cpnext++;
	if (*cpnext == 0)
	    cpnext = NULL;
    }

    /* cp now contains the command (everything before first space),
     * and cpnext contains everything after that first space.  cpnext
     * could be NULL.
     */

    /* I alphabetized the list of commands below to make it easier to
     * find/see what the extended commands are and what they do.
     */
    if (!strcmp(cp,"autorepeat"))	    set_autorepeat(cpnext);
    else if (!strcmp(cp, "bind"))	    bind_key(cpnext);
    else if (!strcmp(cp,"cwindow"))	    set_command_window(cpnext);
    else if (!strcmp(cp,"disconnect")) {
	close(csocket.fd);
	csocket.fd=-1;
	return;
    }
#ifdef HAVE_DMALLOC_H
#ifndef DMALLOC_VERIFY_NOERROR
  #define DMALLOC_VERIFY_NOERROR  1
#endif
    else if (!strcmp(cp,"dmalloc")) {
	if (dmalloc_verify(NULL)==DMALLOC_VERIFY_NOERROR)
	    draw_info("Heap checks out OK", NDI_BLACK);
	else 
	    draw_info("Heap corruption detected", NDI_RED);
    }
#endif
    else if (!strcmp(cp,"foodbeep")) command_foodbep(cpnext);
    else if (!strcmp(cp,"help"))		command_help(cpnext);
    else if (!strcmp(cp,"inv"))			print_inventory (cpl.ob);
    else if (!strcmp(cp,"magicmap")) {
	cpl.showmagic=1;
	draw_magic_map();
    }
    else if (!strcmp(cp,"mapredraw"))		cs_print_string(csocket.fd, "mapredraw");
    else if (!strcmp(cp,"metaserver")) {
	if (!metaserver_get_info(meta_server, meta_port))
	    metaserver_show(FALSE);
	else
	    draw_info("Unable to get metaserver information.", NDI_BLACK);
    }
    else if (!strcmp(cp,"resist")) {
	/* For debugging only */
	int i;
	char buf[256];
	for (i=0; i<NUM_RESISTS; i++) {
	    sprintf(buf,"%-20s %+4d",
		    resists_name[i], cpl.stats.resists[i]);
	    draw_info(buf, NDI_BLACK);
	}
    }
    else if (!strcmp(cp,"savedefaults"))	save_defaults();
    else if (!strcmp(cp,"savewinpos"))		save_winpos();
    else if (!strcmp(cp,"scroll"))		set_scroll(cpnext);
    else if (!strcmp(cp,"show"))		command_show(cpnext);
    else if (!strcmp(cp,"showicon"))		set_show_icon (cpnext);
    else if (!strcmp(cp,"showweight"))		set_show_weight (cpnext);
    else if (!strcmp(cp,"take"))		command_take(cp, cpnext);
    else if (!strcmp(cp,"unbind"))		unbind_key(cpnext);
    else {
	/* just send the command(s)  (if `ocommand' is a compound command */
	/* then split it and send each part seperately */
        strncpy(command, ocommand, MAX_BUF-1);
	command[MAX_BUF-1]=0;
	cp = strtok(command, ";");
	while ( cp ) {
	  while( *cp == ' ' ) cp++; /* throw out leading spaces; server
				       does not like them */
	  send_command(cp, cpl.count, 0);
	  cp = strtok(NULL, ";");
	}
    }
}

/* This list is used for the 'tab' completion, and nothing else.
 * Therefore, if it is out of date, it isn't that terrible, but
 * ideally it should stay somewhat up to date with regards to
 * the commands the server supports.
 */
 
static char *commands[] = {
"save", "sound", "party", "gsay", "apply", "brace",
"cast", "disarm", "disconnect", "drop", "dropall", "examine",
"get", "help", "hiscore", "inventory", "invoke",
"listen", "maps", "mapinfo", "mark", "motd",
"output-sync", "output-count", "peaceful",
"pickup", "players", "prepare", "quit",
"rotateshoottype", "rotatespells", "say",
"shout", "skills", "use_skill", "ready_skill",
"search", "search-items", "statistics", "take",
"tell", "throw", "usekeys", "version","wimpy",
"who", "stay"};
#define NUM_COMMANDS (sizeof(commands) / sizeof(char*))

/* Player has entered 'command' and hit tab to complete it.  
 * See if we can find a completion.  Returns matching
 * command.
 */

char * complete_command(char *command)
{
    int i, match=-1, len;
    char *cp;

    if (command[0] == '>') cp = command+1;
    else cp = command;
    len  = strlen(cp);

    if (len == 0) return cp;

    for (i=0; i<NUM_COMMANDS; i++) {
	if (!strncmp(cp, commands[i], len)) {
	    if (match != -1) return cp;
	    else match = i;
	}
    }
    if (match == -1) return cp;
    else return commands[match];
}
