/*
 * Crossfire -- cooperative multi-player graphical RPG and adventure game
 *
 * Copyright (c) 1999-2013 Mark Wedel and the Crossfire Development Team
 * Copyright (c) 1992 Frank Tore Johansen
 *
 * Crossfire is free software and comes with ABSOLUTELY NO WARRANTY. You are
 * welcome to redistribute it under certain conditions. For details, please
 * see COPYING and LICENSE.
 *
 * The authors can be reached via e-mail at <crossfire@metalforge.org>.
 */

/**
 * @file
 * Contains a lot about the commands typed into the client.
 */

#include <ctype.h>

#include "client.h"
#include "external.h"
#include "p_cmd.h"
#include "script.h"

/**
 * @defgroup PCmdHelpCommands Common client player commands.
 * @{
 */

#define H1(a) draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, a)
#define H2(a) draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, a)
#define LINE(a) draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, a)

/* TODO Help topics other than commands? Refer to other documents? */

static int get_num_commands(void);

static void do_clienthelp_list() {
    ConsoleCommand **sorted_cmds = get_cat_sorted_commands();
    CommCat category = COMM_CAT_MISC;
    GString *line = g_string_new(NULL);

    H1("Client commands:");
    for (int i = 0; i < get_num_commands(); i++) {
        ConsoleCommand *cmd = sorted_cmds[i];
        if (cmd->cat != category) {
            // If moving on to next category, dump line_buf and print header.
            char buf[MAX_BUF];
            snprintf(buf, sizeof(buf), "%s commands:",
                     get_category_name(cmd->cat));
            LINE(line->str);
            H2(buf);

            category = cmd->cat;
            g_string_free(line, true);
            line = g_string_new(NULL);
        }
        g_string_append_printf(line, "%s ", cmd->name);
    }

    LINE(line->str);
    g_string_free(line, true);
}

static void show_help(const ConsoleCommand *cc) {
    char buf[MAX_BUF];
    if (cc->desc != NULL) {
        snprintf(buf, MAX_BUF - 1, "%s - %s", cc->name, cc->desc);
    } else {
        snprintf(buf, MAX_BUF - 1, "Help for '%s':", cc->name);
    }
    H2(buf);

    if (cc->helpfunc != NULL) {
        const char *long_help = NULL;
        long_help = cc->helpfunc();

        if (long_help != NULL) {
            LINE(long_help);
        } else {
            LINE("Extended help for this command is broken.");
        }
    } else {
        LINE("No extended help is available for this command.");
    }
}

static void command_help(const char *cpnext) {
    if (cpnext) {
        const ConsoleCommand * cc;
        char buf[MAX_BUF];

        cc = find_command(cpnext);
        if (cc != NULL) {
            show_help(cc);
        } else  {
            snprintf(buf, sizeof(buf), "help %s", cpnext);
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
        "isn't, send the topic to the server.";
}

/**
 * @} */ /* EndOf PCmdHelpCommands
 */

static void set_command_window(const char *cpnext) {
    if (!cpnext) {
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                      "cwindow command requires a number parameter");
    } else {
        want_config[CONFIG_CWINDOW] = atoi(cpnext);
        if (want_config[CONFIG_CWINDOW]<1 || want_config[CONFIG_CWINDOW]>127) {
            want_config[CONFIG_CWINDOW]=COMMAND_WINDOW;
        } else {
            use_config[CONFIG_CWINDOW] = want_config[CONFIG_CWINDOW];
        }
    }
}

static void command_foodbeep() {
    if (want_config[CONFIG_FOODBEEP]) {
        want_config[CONFIG_FOODBEEP] = 0;
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                      "Warning bell when low on food disabled");
    } else {
        want_config[CONFIG_FOODBEEP] = 1;
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                      "Warning bell when low on food enabled");
    }
    use_config[CONFIG_FOODBEEP] = want_config[CONFIG_FOODBEEP];
}

const char * get_category_name(CommCat cat) {
    const char * cat_name;

    /* HACK Need to keep this in sync. with player.h */
    switch(cat) {
    case COMM_CAT_MISC:
        cat_name = "Miscellaneous";
        break;
    case COMM_CAT_INFO:
        cat_name = "Informational";
        break;
    case COMM_CAT_SETUP:
        cat_name = "Configuration";
        break;
    case COMM_CAT_SCRIPT:
        cat_name = "Scripting";
        break;
    case COMM_CAT_DEBUG:
        cat_name = "Debugging";
        break;
    default:
        cat_name = "PROGRAMMER ERROR";
        break;
    }

    return cat_name;
}

/*
 * Command table.
 *
 * Implementation basically stolen verbatim from the server.
 */

static void do_script_list() { script_list(); }

static void do_clearinfo() { menu_clear(); }

static void do_disconnect() { client_disconnect(); }

#ifdef HAVE_DMALLOC_H
#ifndef DMALLOC_VERIFY_NOERROR
#define DMALLOC_VERIFY_NOERROR  1
#endif
static void do_dmalloc(const char *ignored) {
    if (dmalloc_verify(NULL)==DMALLOC_VERIFY_NOERROR)
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                      "Heap checks out OK");
    else
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_ERROR,
                      "Heap corruption detected");
}
#endif

static void do_inv() { print_inventory(cpl.ob); }

static void do_magicmap() {
    cpl.showmagic = 1;
    draw_magic_map();
}

static void do_savedefaults() { save_defaults(); }

static void do_savewinpos() { save_winpos(); }

static void do_take(const char *used) {
    command_take("take", used); /* I dunno why they want it. */
}

static void do_num_free_items() {
    LOG(LOG_INFO, "common::extended_command", "num_free_items=%d",
        num_free_items());
}

/* Help "typecasters". */
#include "chelp.h"

static const char * help_bind(void) {
    return HELP_BIND_LONG;
}

static const char * help_unbind(void) {
    return HELP_UNBIND_LONG;
}

static const char * help_magicmap(void) {
    return HELP_MAGICMAP_LONG;
}

static const char * help_inv(void) {
    return HELP_INV_LONG;
}

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
        "Syntax: script <path>\n\n"
        "Start an executable client script located at <path>. For details on "
        "client-side scripting, please see the Crossfire Wiki.";
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

static const char * help_scriptkill(void) {
    return
        "Syntax:\n"
        "\n"
        "    scriptkill <name>\n"
        "\n"
        "Stop scripts named <name>.\n"
        "(Not guaranteed to work?)";
}

static void cmd_raw(const char *cmd) {
    cs_print_string(csocket.fd, "%s", cmd);
}

static ConsoleCommand CommonCommands[] = {
    {"cmd", COMM_CAT_DEBUG, cmd_raw, NULL, "Send a raw command to the server"},

    {"bind", COMM_CAT_SETUP, bind_key, help_bind, HELP_BIND_SHORT},

    {"script", COMM_CAT_SCRIPT, script_init, help_script, NULL},
#ifdef HAVE_LUA
    {"lua_load", COMM_CAT_SCRIPT, script_lua_load, NULL, NULL},

    {"lua_list", COMM_CAT_SCRIPT, script_lua_list, NULL, NULL},

    {"lua_kill", COMM_CAT_SCRIPT, script_lua_kill, NULL, NULL},
#endif
    {"scripts", COMM_CAT_SCRIPT, do_script_list, NULL, "List running scripts"},

    {"scriptkill", COMM_CAT_SCRIPT, script_kill, help_scriptkill, NULL},

    {"scripttell", COMM_CAT_SCRIPT, script_tell, help_scripttell, NULL},

    {"clearinfo", COMM_CAT_MISC, do_clearinfo, NULL, "Clear message window"},

    {"cwindow", COMM_CAT_SETUP, set_command_window, help_cwindow, NULL},

    {"disconnect", COMM_CAT_MISC, do_disconnect, NULL, NULL},

#ifdef HAVE_DMALLOC_H
    {"dmalloc", COMM_CAT_DEBUG, do_dmalloc, NULL, NULL},
#endif

    {"foodbeep", COMM_CAT_SETUP, command_foodbeep, NULL,
     "toggle audible low on food warning"},

    {"help", COMM_CAT_MISC, command_help, help_help, NULL},

    {"inv", COMM_CAT_DEBUG, do_inv, help_inv, HELP_INV_SHORT},

    {"magicmap", COMM_CAT_MISC, do_magicmap, help_magicmap,
     HELP_MAGICMAP_SHORT},

    {"savedefaults", COMM_CAT_SETUP, do_savedefaults, NULL,
     HELP_SAVEDEFAULTS_SHORT},

    {
     "savewinpos", COMM_CAT_SETUP, do_savewinpos, help_savewinpos,
     "Saves the position and sizes of windows." /* Panes? */
    },

    {"take", COMM_CAT_MISC, do_take, NULL, NULL},

    {"unbind", COMM_CAT_SETUP, unbind_key, help_unbind, NULL},

    {"num_free_items", COMM_CAT_DEBUG, do_num_free_items, NULL,
     "log the number of free items?"},
    {"show", COMM_CAT_SETUP, command_show, NULL,
     "Change what items to show in inventory"},
};

const size_t num_commands = sizeof(CommonCommands) / sizeof(ConsoleCommand);
static int get_num_commands() {
    return num_commands;
}

static ConsoleCommand ** name_sorted_commands;

static int sort_by_name(const void * a_, const void * b_) {
    ConsoleCommand * a = *((ConsoleCommand **)a_);
    ConsoleCommand * b = *((ConsoleCommand **)b_);
    return strcmp(a->name, b->name);
}

static ConsoleCommand ** cat_sorted_commands;

/* Sort by category, then by name. */

static int sort_by_category(const void *a_, const void *b_) {
    /* Typecasts, so it goes. */
    ConsoleCommand * a = *((ConsoleCommand **)a_);
    ConsoleCommand * b = *((ConsoleCommand **)b_);

    if (a->cat == b->cat) {
        return strcmp(a->name, b->name);
    }

    return a->cat - b->cat;
}

void init_commands() {
    /* XXX Leak! */
    name_sorted_commands = g_malloc(sizeof(ConsoleCommand *) * num_commands);

    for (size_t i = 0; i < num_commands; i++) {
        name_sorted_commands[i] = &CommonCommands[i];
    }

    /* Sort them. */
    qsort(name_sorted_commands, num_commands, sizeof(ConsoleCommand *), sort_by_name);

    /* Copy the list, then sort it by category. */
    cat_sorted_commands = g_malloc(sizeof(ConsoleCommand *) * num_commands);

    memcpy(cat_sorted_commands, name_sorted_commands, sizeof(ConsoleCommand *) * num_commands);

    qsort(cat_sorted_commands, num_commands, sizeof(ConsoleCommand *), sort_by_category);

    /* TODO Add to the list of tab-completion items. */
}

const ConsoleCommand * find_command(const char *cmd) {
    ConsoleCommand ** asp_p = NULL, dummy;
    ConsoleCommand * dummy_p;
    ConsoleCommand * asp;
    char *cp, *cmd_cpy;
    cmd_cpy = g_strdup(cmd);

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

    if (asp_p == NULL) {
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
 * Returns a pointer to the head of an array of ConsoleCommands sorted by
 * category, then by name.  It's num_commands long.
 */
ConsoleCommand ** get_cat_sorted_commands(void) {
    return cat_sorted_commands;
}

/**
 * Tries to handle command cp (with optional params in cpnext, which may be
 * null) as a local command. If this was a local command, returns true to
 * indicate command was handled.  This code was moved from extended_command so
 * scripts ca issue local commands to handle keybindings or anything else.
 *
 * @param cp
 * @param cpnext
 */
int handle_local_command(const char* cp, const char *cpnext) {
    const ConsoleCommand * cc = NULL;

    cc = find_command(cp);

    if (cc == NULL) {
        return FALSE;
    }

    if (cc->dofunc == NULL) {
        char buf[MAX_BUF];

        snprintf(buf, MAX_BUF - 1, "Client command %s has no implementation!", cc->name);
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, buf);

        return FALSE;
    }

    cc->dofunc(cpnext);

    return TRUE;
}

/**
 * This is an extended command (ie, 'who, 'whatever, etc).  In general, we
 * just send the command to the server, but there are a few that we care about
 * (bind, unbind)
 *
 * The command passed to us can not be modified - if it is a keybinding, we
 * get passed the string that is that binding - modifying it effectively
 * changes the binding.
 *
 * @param ocommand
 */
void extended_command(const char *ocommand) {
    const char *cp = ocommand;
    char *cpnext, command[MAX_BUF];

    if ((cpnext = strchr(cp, ' '))!=NULL) {
        int len = cpnext - ocommand;
        if (len > (MAX_BUF -1 )) {
            len = MAX_BUF-1;
        }

        strncpy(command, ocommand, len);
        command[len] = '\0';
        cp = command;
        while (*cpnext == ' ') {
            cpnext++;
        }
        if (*cpnext == 0) {
            cpnext = NULL;
        }
    }
    /*
     * Try to prevent potential client hang by trying to delete a
     * character when there is no character to delete.
     * Thus, only send quit command if there is a player to delete.
     */
    if (cpl.title[0] == '\0' && strcmp(cp, "quit") == 0){
        // Bail here, there isn't anything this should be doing.
        return;
    }

    /* cp now contains the command (everything before first space),
     * and cpnext contains everything after that first space.  cpnext
     * could be NULL.
     */
#ifdef HAVE_LUA
    if ( script_lua_command(cp, cpnext) ) {
        return;
    }
#endif

    /* If this isn't a client-side command, send it to the server. */
    if (!handle_local_command(cp, cpnext)) {
        /* just send the command(s)  (if `ocommand' is a compound command */
        /* then split it and send each part seperately */
        /* TODO Remove this from the server; end of commands.c. */
        strncpy(command, ocommand, MAX_BUF-1);
        command[MAX_BUF-1]=0;
        cp = strtok(command, ";");
        while ( cp ) {
            while( *cp == ' ' ) {
                cp++;
            } /* throw out leading spaces; server
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
const size_t num_server_commands = sizeof(commands) / sizeof(char *);

/**
 * Player has entered 'command' and hit tab to complete it.  See if we can
 * find a completion.  Returns matching command. Returns NULL if no command
 * matches.
 *
 * @param command
 */
const char * complete_command(const char *command) {
    int len, display = 0;
    const char *match;
    static char result[64];
    char list[500];

    len = strlen(command);

    if (len == 0) {
        return NULL;
    }

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
    for (size_t i = 0; i < num_server_commands; i++) {
        if (!strncmp(command, commands[i], len)) {
            if (display) {
                snprintf(list + strlen(list), 499 - strlen(list), " %s", commands[i]);
            } else if (match != NULL) {
                display = 1;
                snprintf(list + strlen(list), 499 - strlen(list), " %s %s", match, commands[i]);
                match = NULL;
            } else {
                match = commands[i];
            }
        }
    }

    /* check client side commands */
    for (size_t i = 0; i < num_commands; i++) {
        if (!strncmp(command, CommonCommands[i].name, len)) {
            if (display) {
                snprintf(list + strlen(list), 499 - strlen(list), " %s", CommonCommands[i].name);
            } else if (match != NULL) {
                display = 1;
                snprintf(list + strlen(list), 499 - strlen(list), " %s %s", match, CommonCommands[i].name);
                match = NULL;
            } else {
                match = CommonCommands[i].name;
            }
        }
    }

    if (match == NULL) {
        if (display) {
            strncat(list, "\n", 499 - strlen(list));
            draw_ext_info(
                NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE, list);
        } else
            draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_NOTICE,
                          "No matching command.\n");
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
