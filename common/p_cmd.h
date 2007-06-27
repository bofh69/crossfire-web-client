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

    The author can be reached via e-mail to crossfire-devel@real-time.com
*/


/*
 * Includes and prototypes for p_cmd.c.
 *
 * p_cmd.c has player-commands like '/magicmap'.
 */


/* Basically stolen piecemeal from the server branch. */

#ifndef PCMD_H
#define PCMD_H

/*
 * List of commands.
 */

typedef void (*CommFunc)(const char *params);

/* Cargo-cult from the above. Every entry in the table
 * complains about a type mismatch, too. :(
 */
typedef const char * (*CommHelpFunc)(void);

/* This is used for displaying lists of commands. */
typedef enum {
  COMM_CAT_MISC = 0,    /* Commands which can't be better sorted. */
  COMM_CAT_HELP = 1,
  COMM_CAT_INFO = 2,    /* A tad general. */
  COMM_CAT_SETUP = 3,   /* showicon, showweight, bind, commandkey... */
  COMM_CAT_SCRIPT = 4,  /* The four commands for the nifty-scripts. */
  COMM_CAT_DEBUG = 5,   /* Debugging commands - hide these? */
} CommCat;

/* Retrieves a Title Cased name for the above categories. */
const char * get_category_name(CommCat cat);


typedef struct {        /* global list's structure */
  const char * name;    /* Name of command - parsed against this. */
  CommCat cat;          /* What category the command is in. Used for sorting on display. */
  CommFunc dofunc;      /* If name is matched, this is called. */
  /* TODO Too specific? *sigh* Resolving *that* issue gives me a headache. */
  CommHelpFunc helpfunc;/* Returns a string documenting the command. - the *really* long desc. */
  const char * desc;    /* One-liner describing command. (Man page subtitle, anyone?) */
} ConsoleCommand;

extern const ConsoleCommand * find_command(const char * cmd);

/* Define this to let the toolkit give an array of toolkit-specific commands. */
#undef TOOLKIT_COMMANDS
#ifdef TOOLKIT_COMMANDS
extern ConsoleCommand ToolkitCommands[];
extern const int ToolkitCommandsSize;

/* Not defined in common, called at the very top of init_commands()
   so a toolkit can fill ToolkitCommands and ToolkitCommandsSize in.
*/
extern void init_toolkit_commands(void);
#endif

/**
 * Fills some internal arrays. Run this on startup, but not before filling in
 * ToolkitCommands and ToolkitCommandsSize.
 */
extern void init_commands(void);

extern int get_num_commands(void);
/**
 * Returns a pointer to the head of an array of ConsoleCommands
 * sorted by category, then by name.
 *
 * It's num_commands long.
 */
ConsoleCommand ** get_cat_sorted_commands(void);

/* Used only for searching the commands list for help, er. ... Oh, well. */
extern const ConsoleCommand * find_command(const char * cmd);

/* This searches ClientCommands; if there's nothing in there, it goes to the server.
 * With some exceptions. :(
 */
extern void extended_command(const char *ocommand);

extern const char * complete_command(const char * ocommand);

extern int handle_local_command(const char* cp, const char * cpnext);


#endif
