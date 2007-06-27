/* Client helpfile */

#ifndef CHELP_H
#define CHELP_H

#define HELP_BIND_SHORT "bind a command to a key"
#define HELP_BIND_LONG "Syntax:\n\
 bind [-nfreg] <command>\n\
\n\
Flags (default -nrf):\n\
 n - used in normal-mode\n\
 f - used in fire-mode\n\
 r - used in run-mode\n\
 e - leave command in line edit\n\
 g - global key (not recommended)\n\
\n\
Special 'commands':\n\
 bind commandkey - sets commandkey\n\
 bind firekey1 - sets first firekey\n\
 bind firekey2 - sets second firekey\n\
 bind runkey1 - sets first runkey\n\
 bind runkey2 - sets second runkey\n\
 bind prevkey - sets history-previous key\n\
 bind nextkey - sets history-next key\n\
 bind completekey - sets complete-command key\n\
\n\
Examples:\n\
bind -f cast paralyzed (F3)\n\
  will typically mean that Shift-F3\n\
  is used to select that spell (Shift\n\
  being the fire key)\n\
\n\
bind -e shout  (\")\n\
  will put the cursor in the command\n\
  box after writing 'shout' when you\n\
  press double-quote. So you can shout\n\
  to your friends easier. ;)\n"

#define HELP_UNBIND_SHORT "unbind a command, show bindings"
#define HELP_UNBIND_LONG "Syntax:\n\
 unbind [-g] [#]\n\
 unbind reset\n\
Without -g command uses user's bindind,\n\
with -g global binding.\n\
Without number it displays current bindings,\n\
with # it unbinds it.\n\
'reset' resets default bindings."

#define HELP_MAGICMAP_SHORT "show last received magic map"
#define HELP_MAGICMAP_LONG "Syntax:\n\
 magicmap\n\
Displays last shown magic map."

#define HELP_SAVEDEFAULTS_SHORT "save various defaults into ~/.crossfire/defaults"
#define HELP_SAVEDEFAULTS_LONG "Syntax:\n\
 savedefaults\n\
Saves configuration."

/* XXX *Which* configuration? */

#define HELP_INV_SHORT "show clients inventory (debug)"
#define HELP_INV_LONG "Syntax:\n\
 inv\n\
Debug info about inventory."


/* Used to be gchar, but it couldn't find it. */
/*
char * text = " === Client Side Commands: === \n\
\n\
bind    - " HELP_BIND_SHORT "\n\
unbind    - " HELP_UNBIND_SHORT  "\n\
magicmap    - " HELP_MAGICMAP_SHORT "\n\
savedefaults    - " HELP_SAVEDEFAULTS_SHORT "\n\
inv   - " HELP_INV_SHORT "\n\
\n\
\n\
bind\n\
\n\
" HELP_BIND_LONG "\n\
\n\
unbind\n\
\n\
" HELP_UNBIND_LONG "\n\
\n\
\n\
 magicmap\n\
\n\
" HELP_MAGICMAP_LONG "\n\
\n\
\n\
 savedefaults\n\
\n\
" HELP_SAVEDEFAULTS_LONG "\n\
\n\
\n\
 inv\n\
\n\
" HELP_INV_LONG "\n\
\n\
\n\
";
*/

#endif
