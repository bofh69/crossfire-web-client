#include <client.h>

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
    copy_name (cpl.ob->name, name);
    cpl.ob->weight = (float) weight / 1000;
    cpl.ob->face   = face;
}

void look_at(int x, int y)
{
    char buf[MAX_BUF];

    sprintf(buf,"lookat %d %d", x, y);
    cs_write_string(csocket.fd, buf, strlen(buf));
}

void client_send_apply (int tag)
{
    char buf[MAX_BUF];

    sprintf(buf,"apply %d", tag);
    cs_write_string(csocket.fd, buf, strlen(buf));
}

void client_send_examine (int tag)
{
    char buf[MAX_BUF];

    sprintf(buf,"examine %d", tag);
    cs_write_string(csocket.fd, buf, strlen(buf));

}

/* Requests nrof objects of tag get moved to loc. */
void client_send_move (int loc, int tag, int nrof)
{
    char buf[MAX_BUF];

    sprintf(buf,"move %d %d %d", loc, tag, nrof);
    cs_write_string(csocket.fd, buf, strlen(buf));
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
    dfire &= 0xff;
    if (dir!= dfire) {
	sprintf(buf,"fire %d", dir);
	send_command(buf, -1, SC_NORMAL);
	dfire=dir;
    }
}

void stop_run()
{
    send_command("run_stop", -1, SC_FIRERUN);
    drun |= 0x100;
}

void run_dir(int dir) {
    char buf[MAX_BUF];

    drun &= 0xff;
    if (dir!=drun) {
	sprintf(buf,"run %d", dir);
	send_command(buf, -1, SC_NORMAL);
	drun=dir;
    }
}


/* This should be used for all 'command' processing.  Other functions should
 * call this so that proper windowing will be done.
 * command is the text command, repeat is a count value, or -1 if none
 * is desired and we don't want to reset the current count.
 * must_send means we must send this command no matter what (ie, it is
 * an administrative type of command like fire_stop, and failure to send
 * it will cause definate problems
 */

void send_command(const char *command, int repeat, int must_send) {
    char buf[MAX_BUF];
    static char last_command[MAX_BUF]="";

    if (cpl.input_state==Reply_One) {
	fprintf(stderr,"Wont send command '%s' - since in reply mode!\n ",
		command);
	cpl.count=0;
	return;
    }

    /* Does the server understand 'ncom'? If so, special code */
    if (csocket.cs_version >= 1021) {
	int commdiff=csocket.command_sent - csocket.command_received;

	if (commdiff<0) commdiff +=256;

	/* if to many unanswer commands, not a must send, and command is
	 * the same, drop it
	 */
	if (commdiff>cpl.command_window && !must_send && !strcmp(command, last_command)) {
	    fprintf(stderr,"Wont send command %s - window oversized %d %d\n",
		    command, csocket.command_sent, csocket.command_received);
	}
	else {
	    SockList sl;
	    char buf[MAX_BUF];

	    /* Don't want to copy in administrative commands */
	    if (!must_send) strcpy(last_command, command);
	    csocket.command_sent++;
	    csocket.command_sent &= 0xff;   /* max out at 255 */

	    sl.buf = (unsigned char*)buf;
	    strcpy((char*)sl.buf,"ncom ");
	    sl.len=5;
	    SockList_AddShort(&sl, csocket.command_sent);
	    SockList_AddInt(&sl, repeat);
	    strcpy((char*)sl.buf + sl.len, command);
	    sl.len += strlen(command);
	    send_socklist(csocket.fd, sl);
	}
    } else {
	sprintf(buf,"command %d %s", repeat,command);
	cs_write_string(csocket.fd, buf, strlen(buf));
    }
    if (repeat!=-1) cpl.count=0;
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
void show_help() {
    draw_info("Client Side Commands", NDI_NAVY);
    draw_info("  bind        - bind a command to key", NDI_BLACK);
    draw_info("  unbind      - unbind a command, show bindings", NDI_BLACK);
    draw_info("  cwindow <val>  - set size of command window (if val is exceeded", NDI_BLACK);
    draw_info("     client won't send new commands to server", NDI_BLACK);
    draw_info("  showicon    - draw status icons in inventory window", NDI_BLACK);
    draw_info("  showweight  - show weight in inventory/look windows", NDI_BLACK);
    draw_info("  scroll      - toggle scroll/wrap mode in info window", NDI_BLACK);
    draw_info("  magicmap    - show last received magicmap", NDI_BLACK);
    draw_info("  savewinpos  - save window positions (split mode", NDI_BLACK);
    draw_info("  savedefaults- save various defaults into ~/.crossfire/defaults", NDI_BLACK);
    draw_info("  inv         - show clients inventory (debug)", NDI_BLACK);
    draw_info("  show        - determine what type of items to show in inventory", NDI_BLACK);
    draw_info("  foodbeep    - toggle audible low on food warning", NDI_BLACK);

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
        strncpy(command, ocommand, cpnext - ocommand);
	command[cpnext - ocommand] = '\0';
	cp = command;
    }


    if (!strcmp(cp, "bind"))
	bind_key(cpnext);
    else if (!strcmp(cp,"unbind"))
	unbind_key(cpnext);
    else if (!strcmp(cp,"showicon"))
	set_show_icon (cpnext);
    else if (!strcmp(cp,"showweight"))
	set_show_weight (cpnext);
    else if (!strcmp(cp,"scroll"))
	set_scroll(cpnext);
    else if (!strcmp(cp,"magicmap")) {
	cpl.showmagic=1;
	draw_magic_map();
    }
    else if (!strcmp(cp,"savewinpos")) {
	save_winpos();
    }
    else if (!strcmp(cp,"savedefaults")) {
	save_defaults();
    }
    else if (!strcmp(cp,"cwindow")) {
        /* check no arguments for cwindow */
        if( cpnext == NULL )
	    cpl.command_window=COMMAND_WINDOW;
	else
	    cpl.command_window = atoi(cpnext);
	if (cpl.command_window<1 || cpl.command_window>127)
	    cpl.command_window=COMMAND_WINDOW;
    }

    else if (!strcmp(cp,"help")) {
	char buf[MAX_BUF];

	if (cpnext) {
	    sprintf(buf,"help %s", cpnext);
	    /* maybe not a must send, but we probably don't want to drop it */
	    send_command(buf, -1, 1);
	} else {
	    show_help();
	    send_command("help", -1, 1);
	}

    }
    else if (!strcmp(cp,"show")) {
	command_show(cpnext);
    }
    else if (!strcmp(cp,"inv")) {/* inventory command is sended to server
				   for debugging purposes */
	print_inventory (cpl.ob);
    }
    else if (!strcmp(cp,"foodbeep")) {
	if (cpl.food_beep) {
	    cpl.food_beep=0;
	    draw_info("Warning bell when low on food disabled", NDI_BLACK);
	} else {
	    cpl.food_beep=1;
	    draw_info("Warning bell when low on food enabled", NDI_BLACK);
	}
    } else {
	/* just send the command(s)  (if `ocommand' is a compound command */
	/* then split it and send each part seperately */
        strcpy(command, ocommand);
	cp = strtok(command, ";");
	while ( cp ) {
	  while( *cp == ' ' ) cp++; /* throw out leading spaces; server
				       does not like them */
	  send_command(cp, cpl.count, 0);
	  cp = strtok(NULL, ";");
	}
    }
}
 
