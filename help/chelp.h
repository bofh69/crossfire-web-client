/* Client helpfile */
gchar *text="Client Side Commands:\n\
\n\
bind    - bind a command to key\n\
unbind    - unbind a command, show bindings\n\
magicmap    - show last received magicmap\n\
savedefaults    - save various defaults into ~/.crossfire/defaults\n\
inv   - show clients inventory (debug)\n\
\n\
\n\
bind\n\
\n\
Syntax:\n\
 bind [-nfreg] <command>\n\
\n\
Flags (default -nrf):\n\
 n - used in normal-mode\n\
 f - used in fire-mode\n\
 r - used in run-mode\n\
 e - leave command in line edit\n\
 g - global key (not recommended)\n\
\n\
Special bind's:\n\
 bind commandkey - sets commandkey\n\
 bind firekey1 - sets first firekey\n\
 bind firekey2 - sets second firekey\n\
 bind runkey1 - sets first runkey\n\
 bind runkey2 - sets second runkey\n\
\n\
Examples:\n\
bind -f cast paralyzed (F3)\n\
  will typically mean that Shift-F3\n\
  is used to select that spell (Shift\n\
  being the fire key)\n\
\n\
\n\
unbind\n\
\n\
Syntax:\n\
 unbind [-g] [#]\n\
 unbind reset\n\
Without -g command uses user's bindind,\n\
with -g global binding.\n\
Without number it displays current bindings,\n\
with # it unbinds it.\n\
'reset' resets default bindings.\n\
\n\
\n\
magicmap\n\
\n\
Syntax:\n\
 magicmap\n\
Displays last shown magic map.\n\
\n\
\n\
savedefaults\n\
\n\
Syntax:\n\
 savedefaults\n\
Saves configuration.\n\
\n\
\n\
inv\n\
\n\
Syntax:\n\
 inv\n\
Debug info about inventory.\n\
\n\
\n\
";
