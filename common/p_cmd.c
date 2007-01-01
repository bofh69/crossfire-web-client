/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2005,2006 Mark Wedel & Crossfire Development Team

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

/*
 * Contains a lot about the commands typed into the client.
 */

#ifndef CPROTO
/* use declartions from p_cmd.h instead of doing make proto on this file */
 
#ifdef WIN32
#include <windows.h>
#endif
#include <client.h>
#include <external.h>
#include <script.h>
#include <p_cmd.h>

/*
 *
 * Help commands.
 *
 */

/* TODO This should really be under /help commands or something... */

/* This dynamically generates a list from the ConsoleCommand list. */
#undef CLIENTHELP_LONG_LIST

/*
long-list:
category
name - description
name - description
...

not long list:
category
name name name ...
*/

#undef HELP_USE_COLOR
#ifdef HELP_USE_COLOR
#error Oops, need to put them back.
#else
#define H1(a) draw_info(a, NDI_BLACK)
#define H2(a) draw_info(a, NDI_BLACK)
#define LINE(a) draw_info(a, NDI_BLACK)
#endif

#define assumed_wrap get_info_width()


/* TODO Help topics other than commands? Refer to other documents? */
static void do_clienthelp_list(void) {
    ConsoleCommand ** commands_array;
    ConsoleCommand * commands_copy;
    int i;
    CommCat current_cat = COMM_CAT_MISC;
#ifndef CLIENTHELP_LONG_LIST
    char line_buf[MAX_BUF];
    size_t line_len = 0; 
  
    line_buf[0] = '\0'; 
#endif
    
    commands_array = get_cat_sorted_commands();

    /* Now we have a nice sorted list. */

    H1(" === Client Side Commands === ");
    
    for (i = 0; i < get_num_commands(); i++) {
        commands_copy = commands_array[i];

        /* Should be LOG_SPAM but I'm too lazy to tweak it. */
        /* LOG(LOG_INFO, "p_cmd::do_clienthelp_list", "%s Command %s", get_category_name(commands_copy->cat), commands_copy->name); */

        if (commands_copy->cat != current_cat) {
            char buf[MAX_BUF];

#ifndef CLIENTHELP_LONG_LIST
            if (line_len > 0) {
                LINE(line_buf);
                line_buf[0] = '\0';
                line_len = 0;
            }
#endif

#ifdef HELP_USE_COLOR
            snprintf(buf, MAX_BUF - 1, "%s Commands:", get_category_name(commands_copy->cat));
#else
            snprintf(buf, MAX_BUF - 1, " --- %s Commands --- ", get_category_name(commands_copy->cat));
#endif

            H2(buf);
            current_cat = commands_copy->cat; 
        }

#ifdef CLIENTHELP_LONG_LIST
        if (commands_copy->desc != NULL) {
            char buf[MAX_BUF];
            snprintf(buf, MAX_BUF - 1, "%s - %s", commands_copy->name, commands_copy->desc);
            LINE(buf);
        } else {
            LINE(commands_copy->name);
        }
#else
        {
            size_t name_len;

            name_len = strlen(commands_copy->name);

            if (strlen(commands_copy->name) > MAX_BUF) {
                LINE(commands_copy->name);
            } else if (name_len > assumed_wrap) {
                LINE(line_buf);
                LINE(commands_copy->name);
                line_len = 0;
            } else if (line_len + name_len > assumed_wrap) {
                LINE(line_buf);
                strncpy(line_buf, commands_copy->name, name_len + 1);
                line_len = name_len;
            } else {
                if (line_len > 0) {
                    strncat(line_buf, " ", 2);
                    line_len += 1;
                }
                strncat(line_buf, commands_copy->name, name_len + 1);
                line_len += name_len;
            }
        }
#endif
    }

#ifndef CLIENTHELP_LONG_LIST
    if (line_len) {
        LINE(line_buf);
    }
#endif
}


static void show_help(const ConsoleCommand * cc) {
    {
        char buf[MAX_BUF];
        snprintf(buf, MAX_BUF - 1, "%s Command help:", get_category_name(cc->cat));
        H1(buf);
    }

    if (cc->desc != NULL) {
        char buf[MAX_BUF];
        snprintf(buf, MAX_BUF - 1, "%s - %s", cc->name, cc->desc);
        H2(buf);
    } else {
        H2(cc->name);
    }

    if (cc->helpfunc != NULL) {
        const char * long_help = NULL;

        long_help = cc->helpfunc();

        if (long_help != NULL) {
            /* For a test, let's watch draw_info() choke on newlines. */
            /* TODO C line wrapping (get_info_width()), argh. Or move it to UI? */
            LINE(long_help);
        } else {
            LINE("This command's documentation is bugged!");
        }
    } else {
        LINE("This command has no extended documentation. :("); 
    }
}

static void do_clienthelp(const char * arg) {
    const ConsoleCommand * cc;

    if (!arg || !strlen(arg)) { 
        do_clienthelp_list();
        return;
    }

    cc = find_command(arg);

    if (cc == NULL) {
        char buf[MAX_BUF];
        snprintf(buf, MAX_BUF - 1, "clienthelp: Unknown command %s.", arg);
        draw_info(buf, NDI_BLACK);
        return;
    }
    
    show_help(cc);

}

static const char * help_clienthelp(void) {
    return
        "Syntax:\n"
        "\n"
        "    clienthelp\n"
        "    clienthelp <command>\n"
        "\n"
        "Without any arguments, displays a list of client-side "
        "commands.\n"
        "\n"
        "With arguments, displays the help for the client-side "
        "command <command>.\n"
        "\n"
        "See also: serverhelp, help.";
}

static void do_serverhelp(const char * arg) {

    if (arg) {
        char buf[MAX_BUF];
	sprintf(buf,"help %s", arg);
	/* maybe not a must send, but we probably don't want to drop it */
	send_command(buf, -1, 1);
    } else {
	send_command("help", -1, 1); /* TODO make install in server branch doesn't install def_help. */
    }
}

static const char * help_serverhelp(void) {
    return
        "Syntax:\n"
        "\n"
        "    serverhelp\n"
        "    serverhelp <command>\n"
        "\n"
        "Fetches help from the server.\n"
        "\n"
        "Note that currently nothing can be done (without a recompile) if a "
        "client command masks a server command.\n"
        "\n"
        "See also: clienthelp, help.";
}


static void command_help(const char *cpnext) {
    if (cpnext) {
        const ConsoleCommand * cc;    
        char buf[MAX_BUF];
        
        cc = find_command(cpnext);
        if (cc != NULL) {
            show_help(cc);
        } else  {        
	    sprintf(buf,"help %s", cpnext);
	    /* maybe not a must send, but we probably don't want to drop it */
	    send_command(buf, -1, 1);
	}
    } else {
	do_clienthelp_list();
        /* Now fetch (in theory) command list from the server.
	TODO Protocol command - feed it to the tab completer. 
	
	Nope! It effectivey fetches '/help commands for commands'.
	*/
	send_command("help", -1, 1); /* TODO make install in server branch doesn't install def_help. */
    }
}

static const char * help_help(void) {
    return
        "Syntax:\n"
        "\n"
        "    help\n"
        "    help <topic>\n"
        "\n"
        "Without any arguments, displays a list of client-side "
        "commands, and fetches the without-arguments help from "
        "the server.\n"
        "\n"
        "With arguments, first checks if there's a client command "
        "named <topic>. If there is, display it's help. If there " 
        "isn't, send the topic to the server.\n"
        "\n"
        "See also: clienthelp, serverhelp.";        
}


/*
 *
 * Other commands.
 *
 */

static void set_command_window(const char *cpnext)
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

static void command_foodbep(const char *cpnext)
{
   (void)cpnext; /* __UNUSED__ */
    if (want_config[CONFIG_FOODBEEP]) {
	want_config[CONFIG_FOODBEEP]=0;
	draw_info("Warning bell when low on food disabled", NDI_BLACK);
    } else {
	want_config[CONFIG_FOODBEEP]=1;
	draw_info("Warning bell when low on food enabled", NDI_BLACK);
    }
    use_config[CONFIG_FOODBEEP] = want_config[CONFIG_FOODBEEP];
}




const char * get_category_name(CommCat cat) {
    const char * cat_name;

    /* HACK Need to keep this in sync. with player.h */
    switch(cat) {
        case COMM_CAT_MISC: cat_name = "Miscellaneous"; break;
        case COMM_CAT_HELP: cat_name = "Help"; break;
        case COMM_CAT_INFO: cat_name = "Informational"; break;
        case COMM_CAT_SETUP: cat_name = "Configuration"; break;
        case COMM_CAT_SCRIPT: cat_name = "Scripting"; break;
        case COMM_CAT_DEBUG: cat_name = "Debugging"; break;
        default: cat_name = "PROGRAMMER ERROR"; break; 
    }

    return cat_name;
}


/*
 * Command table.
 *
 * Implementation basically stolen verbatim from the server.
 */

/* "Typecasters" (and some forwards) */
static void do_script_list(const char * ignored) { script_list(); }
static void do_clearinfo(const char * ignored) { menu_clear(); }

static void do_disconnect(const char * ignored) {
#ifdef WIN32
        closesocket(csocket.fd);
#else
        close(csocket.fd);
#endif
        csocket.fd=-1;

	/* the gtk clients need to do some cleanup logic - otherwise,
	 * they start hogging CPU.
	 */
	cleanup_connection();
        return;
}

#ifdef HAVE_DMALLOC_H
#ifndef DMALLOC_VERIFY_NOERROR
  #define DMALLOC_VERIFY_NOERROR  1
#endif
static void do_dmalloc(const char * ignored) {
        if (dmalloc_verify(NULL)==DMALLOC_VERIFY_NOERROR)
            draw_info("Heap checks out OK", NDI_BLACK);
        else
            draw_info("Heap corruption detected", NDI_RED);
}
#endif

static void do_inv(const char * ignored) { print_inventory (cpl.ob); }

static void do_magicmap(const char * ignored) {
        cpl.showmagic=1;
        draw_magic_map();
}

static void do_mapredraw(const char * ignored) {
        /* TODO Okay, maybe we can't let this fall through. It still seems strange. */
        cs_print_string(csocket.fd, "mapredraw");
}

static void do_metaserver(const char * ignored) {
        if (!metaserver_get_info(meta_server, meta_port))
            metaserver_show(FALSE);
        else
            draw_info("Unable to get metaserver information.", NDI_BLACK);
}

static void do_resist(const char * ignored) {
        /* For debugging only */
        int i;
        char buf[256];
        for (i=0; i<NUM_RESISTS; i++) {
            sprintf(buf,"%-20s %+4d",
                    resists_name[i], cpl.stats.resists[i]);
            draw_info(buf, NDI_BLACK);
        }
}

static void do_savedefaults(const char * ignored) { save_defaults(); }

static void do_savewinpos(const char * ignored) { save_winpos(); }

static void do_take(const char * used) { command_take("take", used); /* I dunno why they want it. */ }

static void do_num_free_items(const char * ignored) {
    LOG(LOG_INFO,"common::extended_command","num_free_items=%d", num_free_items());
}

static void do_clienthelp(const char * arg); /* Forward. */

extern void do_filteritem(const char * params); /* filteritem.c - testing only. */

extern void do_filter(const char * params); /* filteritem.c */

extern void do_filters(const char * ignored); /* filteritem.c */

/* Help "typecasters". */
#include "../help/chelp.h"

static const char * help_bind(void) { return HELP_BIND_LONG; }

static const char * help_unbind(void) { return HELP_UNBIND_LONG; }

static const char * help_magicmap(void) { return HELP_MAGICMAP_LONG; }

static const char * help_inv(void) { return HELP_INV_LONG; }

static const char * help_cwindow(void) { 
    return 
        "Syntax:\n"
        "\n"
        "    cwindow <val>\n"
        "\n"
        "set size of command"
	"window (if val is exceeded"
	"client won't send new"
	"commands to server\n\n"
	"(What does this mean, 'put a lid on it'?) TODO";
}

static const char * help_script(void) {
    return
        "Syntax:\n"
        "\n"
        "    script <pathname>\n"
        "\n"        
	"Run the program at path <name>"
	"as a Crossfire client script." 
	"See Documentation/Script.html";
}

static const char * help_scripttell(void) {
    return
        "Syntax:\n"
        "\n"
        "    scripttell <yourname> <data>\n"
        "\n"        
        "?";
}

/* Toolkit-dependent. */
static const char * help_savewinpos(void) {
    return 
        "Syntax:\n"
        "\n"
        "    savewinpos\n"
        "\n"
        "save window positions - split windows mode only.";
}

static const char * help_metaserver(void) {
    /* TODO Add command_escape() where appropriate. On the other
    hand, that can lead to a meaningless syntax-display API.*/

    return
        "Syntax:\n"
        "\n"
        "    metaserver\n"
        "\n"
	"Get updated list of servers "
	"from the metaserver and show it."
	"This is the same information that the client "
	"uses to show a list of servers when it starts.\n"	
	"\n"
	"Warning: This command may freeze the client until it gets the list.";
}

static const char * help_scriptkill(void) {
    return
        "Syntax:\n"
        "\n"
        "    scriptkill <name>\n"
        "\n"
        "Stop scripts named <name>.\n"
	"(Not guaranteed to work?)";
}

static const char * help_showweight(void) {
    return
        "Syntax:\n"
        "\n"
        "    showweight\n"
        "    showweight inventory\n"
        "    showweight look\n"
        "\n"
        "(Or any prefix of the arguments.)"
        "Toggles if you see the weight of"
        "items in your inventory (also if"
        "no argument given) or your"
        "look-window.";
}
	
/*
*	draw_info("Information Commands", NDI_NAVY);*
	draw_info(" inv         - *recursively* print your", NDI_BLACK);
	draw_info("               inventory - includes containers.", NDI_BLACK);
	draw_info(" mapredraw, showinfo, take", NDI_BLACK);
	draw_info(" help        - show this message", NDI_BLACK);
	draw_info(" help <command> - get more information on a", NDI_BLACK);
	draw_info("                command (Server command only?)", NDI_BLACK);
	draw_info(" showicon    - draw status icons in", NDI_BLACK);
	draw_info("               inventory window", NDI_BLACK);
	draw_info(" showweight  - show weight in inventory", NDI_BLACK);
	draw_info("               look windows", NDI_BLACK);
	draw_info("Scripting Commands", NDI_NAVY);
	draw_info("Client Side Debugging Commands", NDI_NAVY);
#ifdef HAVE_DMALLOC_H
	draw_info(" dmalloc     - Check heap?", NDI_BLACK);
#endif
*/

/* Forward. */
static void do_clienthelp(const char * currently_ignored);

/* TODO Wrap these? Um. */
static ConsoleCommand CommonCommands[] = {
    /* From player.h: 
        name, cat, 
        func, helpfunc,
        long_desc
    */
    
    {
        "autorepeat", COMM_CAT_MISC,
        set_autorepeat, NULL,
        "toggle autorepeat" /* XXX Eh? */
    },

    {
        "bind", COMM_CAT_SETUP,
        bind_key, help_bind,
        HELP_BIND_SHORT
    },

    {
        "script", COMM_CAT_SCRIPT,
        script_init, help_script,
        NULL
    },

    {
        "scripts", COMM_CAT_SCRIPT,
        do_script_list, NULL,
        "List the running scripts(?)"
    },

    {
        "scriptkill", COMM_CAT_SCRIPT,
        script_kill, help_scriptkill,
        NULL
    },

    {
        "scripttell", COMM_CAT_SCRIPT,
        script_tell, help_scripttell,
        NULL
    },

    {
        "clearinfo", COMM_CAT_MISC,
        do_clearinfo, NULL,
        "clear the info window"
    },

    {
        "cwindow", COMM_CAT_SETUP,
        set_command_window, help_cwindow,
        NULL
    },

    {
        "disconnect", COMM_CAT_MISC,
        do_disconnect, NULL,
        "close connection to server"
    },


#ifdef HAVE_DMALLOC_H
    {
        "dmalloc", COMM_CAT_DEBUG,
        do_dmalloc, NULL,
        NULL
    },
#endif

    {
        "foodbeep", COMM_CAT_SETUP,
        command_foodbep, NULL,
        "toggle audible low on food warning"

    },

    {
        "help", COMM_CAT_HELP,
        command_help, help_help,
        NULL
    },

    {
        "clienthelp", COMM_CAT_HELP,
        do_clienthelp, help_clienthelp,
        "Client-side command information"
    },

    {
        "serverhelp", COMM_CAT_HELP,
        do_serverhelp, help_serverhelp,
        "Server-side command information"
    },

    {
        "inv", COMM_CAT_DEBUG,
        do_inv, help_inv,
        HELP_INV_SHORT
    },

    {
        "magicmap", COMM_CAT_MISC,
        do_magicmap, help_magicmap,
        HELP_MAGICMAP_SHORT
    },

    {
        "mapredraw", COMM_CAT_INFO,
        do_mapredraw, NULL,
        NULL
    },

    {
        "metaserver", COMM_CAT_INFO,
        do_metaserver, help_metaserver,
        "Print 'metaserver information'. Warning - your client will pause."
    },

    {
        "resist", COMM_CAT_DEBUG,
        do_resist, NULL,
	"Print resistances"
    },

    {
        "savedefaults", COMM_CAT_SETUP,
        do_savedefaults, NULL,
        HELP_SAVEDEFAULTS_SHORT /* How do we make sure showicons stays on? */
    },

    {
        "savewinpos", COMM_CAT_SETUP,
        do_savewinpos, help_savewinpos,
        "Saves the position and sizes of windows." /* Panes? */
    },

    {
        "scroll", COMM_CAT_SETUP,
        set_scroll, NULL,
        "toggle scroll/wrap mode in info window"
    },

    {
        "showicon", COMM_CAT_SETUP,
        set_show_icon, NULL,
        "Toggles if you see the worn, locked, cursed etc state in the inventory pane."
    },

    {
        "showweight", COMM_CAT_SETUP,
        set_show_weight, help_showweight,
        "Toggles if you see item weights in inventory look windows."
    },

    {
        "take", COMM_CAT_MISC,
        do_take, NULL,
        NULL
    },

    {
        "unbind", COMM_CAT_SETUP,
        unbind_key, help_unbind,
        NULL
    },

    {
        "num_free_items", COMM_CAT_DEBUG,
        do_num_free_items, NULL,
	"log the number of free items?"
    },
    {
        "show", COMM_CAT_SETUP,
        command_show, NULL,
	"Change what items to show in inventory"
    },

};

const int CommonCommandsSize = sizeof(CommonCommands) / sizeof(ConsoleCommand);

#ifdef TOOLKIT_COMMANDS
extern ConsoleCommand ToolkitCommands[];

extern const int ToolkitCommandsSize;
#endif

/* ------------------------------------------------------------------ */

int num_commands;

int get_num_commands(void) { return num_commands; }

static ConsoleCommand ** name_sorted_commands;

static int sort_by_name(const void * a_, const void * b_)
{
    ConsoleCommand * a = *((ConsoleCommand **)a_);
    ConsoleCommand * b = *((ConsoleCommand **)b_);

    return strcmp(a->name, b->name);
}

static ConsoleCommand ** cat_sorted_commands;

/* Sort by category, then by name. */
static int sort_by_category(const void *a_, const void *b_)
{
    /* Typecasts, so it goes. */
    ConsoleCommand * a = *((ConsoleCommand **)a_);
    ConsoleCommand * b = *((ConsoleCommand **)b_);

    if (a->cat == b->cat) {
        return strcmp(a->name, b->name);
    }

    return a->cat - b->cat;
}

void init_commands(void) {
    int i;

#ifdef TOOLKIT_COMMANDS
    init_toolkit_commands();

    /* TODO I dunno ... go through the list and print commands without helps? */
    num_commands = CommonCommandsSize + ToolkitCommandsSize;
#else
    num_commands = CommonCommandsSize;
#endif    

    /* Make a list of (pointers to statically allocated!) all the commands.
       We have a list; the toolkit has a 
       ToolkitCommands and ToolkitCommandsSize, initialized before calling this.
    */

    /* XXX Leak! */
    name_sorted_commands = malloc(sizeof(ConsoleCommand *) * num_commands);

    for (i = 0; i < CommonCommandsSize; i++) {
        name_sorted_commands[i] = &CommonCommands[i];
    }

#ifdef TOOLKIT_COMMANDS    
    for(i = 0; i < ToolkitCommandsSize; i++) {
        name_sorted_commands[CommonCommandsSize + i] = &ToolkitCommands[i];
    }
#endif    

    /* Sort them. */
    qsort(name_sorted_commands, num_commands, sizeof(ConsoleCommand *), sort_by_name);

    /* Copy the list, then sort it by category. */
    cat_sorted_commands = malloc(sizeof(ConsoleCommand *) * num_commands);

    memcpy(cat_sorted_commands, name_sorted_commands, sizeof(ConsoleCommand *) * num_commands);

    qsort(cat_sorted_commands, num_commands, sizeof(ConsoleCommand *), sort_by_category);

    /* TODO Add to the list of tab-completion items. */
}

#ifndef tolower
#define tolower(C)      (((C) >= 'A' && (C) <= 'Z')? (C) - 'A' + 'a': (C))
#endif

const ConsoleCommand * find_command(const char * cmd) {
  ConsoleCommand ** asp_p = NULL, dummy;
  ConsoleCommand * dummy_p;
  ConsoleCommand * asp;
  char *cp, *cmd_cpy;
  cmd_cpy = strdup(cmd);

  for (cp=cmd_cpy; *cp; cp++) {
    *cp =tolower(*cp);
  }

  dummy.name = cmd_cpy;
  dummy_p = &dummy;
  asp_p = bsearch(
     (void *)&dummy_p, 
     (void *)name_sorted_commands, 
     num_commands,
     sizeof(ConsoleCommand *), 
     sort_by_name);

  if (asp_p == NULL)
  {
      free(cmd_cpy);
      return NULL;
  }

  asp = *asp_p;

  /* TODO The server's find_command() searches first the commands,
  then the emotes. We might have to do something similar someday, too. */
  /* if (asp == NULL) search something else? */

  free(cmd_cpy);

  return asp;
}


/**
 * Returns a pointer to the head of an array of ConsoleCommands
 * sorted by category, then by name.
 *
 * It's num_commands long.
 */
ConsoleCommand ** get_cat_sorted_commands(void) {
    return cat_sorted_commands;
}


/* Tries to handle command cp (with optional params in cpnext, which may be null)
 * as a local command. If this was a local command, returns true to indicate
 * command was handled.
 * This code was moved from extended_command so scripts ca issue local commands
 * to handle keybindings or anything else.
 */

int handle_local_command(const char* cp, const char * cpnext) {
    const ConsoleCommand * cc = NULL;
    
    cc = find_command(cp);

    if (cc == NULL) {
        return FALSE;
    }
    
    if (cc->dofunc == NULL) {
        char buf[MAX_BUF];

        snprintf(buf, MAX_BUF - 1, "Client command %s has no implementation!", cc->name); 
	draw_info(buf, NDI_RED);    
	
	return FALSE;
    }
    
    cc->dofunc(cpnext);
    
    return TRUE;
}

/* This is an extended command (ie, 'who, 'whatever, etc).  In general,
 * we just send the command to the server, but there are a few that
 * we care about (bind, unbind)
 *
 * The command passed to us can not be modified - if it is a keybinding,
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

    /* If this isn't a client-side command, send it to the server. */
    if (!handle_local_command(cp, cpnext)) {
	/* just send the command(s)  (if `ocommand' is a compound command */
	/* then split it and send each part seperately */
	/* TODO Remove this from the server; end of commands.c. */
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


/* ------------------------------------------------------------------ */

/* This list is used for the 'tab' completion, and nothing else.
 * Therefore, if it is out of date, it isn't that terrible, but
 * ideally it should stay somewhat up to date with regards to
 * the commands the server supports.
 */
 
/* TODO Dynamically generate. */
 
static const char *const commands[] = {
"accuse", "afk", "apply", "applymode", "archs", "beg", "bleed", "blush",
"body", "bounce", "bow", "bowmode", "brace", "build", "burp", "cackle", "cast",
"chat", "chuckle", "clap", "cointoss", "cough", "cringe", "cry", "dance",
"disarm", "dm", "dmhide", "drop", "dropall", "east", "examine", "explore",
"fire", "fire_stop", "fix_me", "flip", "frown", "gasp", "get", "giggle",
"glare", "grin", "groan", "growl", "gsay", "help", "hiccup", "hiscore", "hug",
"inventory", "invoke", "killpets", "kiss", "laugh", "lick", "listen", "logs",
"mapinfo", "maps", "mark", "me", "motd", "nod", "north", "northeast",
"northwest", "orcknuckle", "output-count", "output-sync", "party", "peaceful",
"petmode", "pickup", "players", "poke", "pout", "prepare", "printlos", "puke",
"quests", "quit", "ready_skill", "rename", "reply", "resistances",
"rotateshoottype", "run", "run_stop", "save", "say", "scream", "search",
"search-items", "shake", "shiver", "shout", "showpets", "shrug", "shutdown",
"sigh", "skills", "slap", "smile", "smirk", "snap", "sneeze", "snicker",
"sniff", "snore", "sound", "south", "southeast", "southwest", "spit",
"statistics", "stay", "strings", "strut", "sulk", "take", "tell", "thank",
"think", "throw", "time", "title", "twiddle", "use_skill", "usekeys",
"version", "wave", "weather", "west", "whereabouts", "whereami", "whistle",
"who", "wimpy", "wink", "yawn",
};
#define NUM_COMMANDS ((int)(sizeof(commands) / sizeof(char*)))

/* Player has entered 'command' and hit tab to complete it.  
 * See if we can find a completion.  Returns matching
 * command. Returns NULL if no command matches.
 */

const char * complete_command(const char *command)
{
    int i, len, display;
    const char *match;
    static char result[64];
    char list[500];

    len = strlen(command);

    if (len == 0)
        return NULL;

    display = 0;
    strcpy(list, "Matching commands:");

    /* TODO Partial match, e.g.:
         If the completion list was:
           wear
           wet #?

         If we type 'w' then hit tab, put in the e.

       Basically part of bash (readline?)'s behaviour.
    */

    match = NULL;

    /* check server side commands */
    for (i=0; i<NUM_COMMANDS; i++) {
        if (!strncmp(command, commands[i], len)) {
            if (display) {
                snprintf(list + strlen(list), 499 - strlen(list), " %s", commands[i]);
            } else if (match != NULL) {
                display = 1;
                snprintf(list + strlen(list), 499 - strlen(list), " %s %s", match, commands[i]);
                match = NULL;
            } else
                match = commands[i];
        }
    }

    /* check client side commands */
    for (i=0; i<CommonCommandsSize; i++) {
        if (!strncmp(command, CommonCommands[i].name, len)) {
            if (display) {
                snprintf(list + strlen(list), 499 - strlen(list), " %s", CommonCommands[i].name);
            } else if (match != NULL) {
                display = 1;
                snprintf(list + strlen(list), 499 - strlen(list), " %s %s", match, CommonCommands[i].name);
                match = NULL;
            } else
                match = CommonCommands[i].name;
        }
    }

    if (match == NULL) {
        if (display) {
            strncat(list, "\n", 499 - strlen(list));
            draw_info(list, NDI_BLACK);
        }
        else
            draw_info("No matching command.\n", NDI_BLACK);
        /* No match. */
        return NULL;
    }

    /*
     * Append a space to allow typing arguments. For commands without arguments
     * the excess space should be stripped off automatically.
     */ 
    snprintf(result, sizeof(result), "%s ", match);

    return result;
}

#endif /* CPROTO */
