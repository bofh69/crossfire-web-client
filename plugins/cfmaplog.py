#!/bin/python
#
license = '''
cfmaplog.py - Crossfire GTK Client plug-in to track per-character map visits.
Copyright (C) 2025, "Kevin R. Bulgrien" <kbulgrien@att.net>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <https://www.gnu.org/licenses/>.
'''

# os.path.expanduser()
import io

# os.path.expanduser()
import os

# regex
import re

# database
import sqlite3

# stderr,stdin,stdout
import sys

# time.sleep(), time.strftime()
import time

debug = False

# Functions ##################################################################

# Send something to the "server".  If a newline should go, and it usually
# should, pass one at the end of the text.  This script may build output
# incrementally, so do not add one here.
#
def client_send(text):
  sys.stdout.write(f"{text}{os.linesep}")
  sys.stdout.flush()
  return

# Send something to the "user" via stderr.  For these to be seen, the client
# is started in a console.
#
def console_send(text):
  sys.stderr.write(f"{text}{os.linesep}")
  sys.stderr.flush()
  return

# Send something to facilitate debugging via stderr.  For these to be seen,
# the client is started in a console.
#
def debug_send(text):
  if debug:
    console_send(f"{text}{os.linesep}")
  return

# Send something to the "player" by way of the Messages pane.  If a newline
# should go, and it usually should, pass one at the end of the text.  This
# script may build output incrementally, so do not add one here.
#
# IMPORTANT: It appears that the integer-based color is currently broken
#            in the client; text markup is required to set text attributes.
#
# See Also:  client/common/shared/newclient.h
#
NDI_BLACK      = 0
NDI_WHITE      = 1
NDI_NAVY       = 2
NDI_RED        = 3
NDI_ORANGE     = 4
NDI_BLUE       = 5       # Actually, it is Dodger Blue
NDI_DK_ORANGE  = 6       # DarkOrange2
NDI_GREEN      = 7       # SeaGreen
NDI_LT_GREEN   = 8       # DarkSeaGreen, which is actually paler
                         # than seagreen - also background color.
NDI_GREY       = 9
NDI_BROWN      = 10      # Sienna.
NDI_GOLD       = 11
NDI_TAN        = 12      # Khaki.
NDI_MAX_COLOR  = 12      # Last value in.

NDI_COLOR_MASK = 0xff    # Gives lots of room for expansion - we are
                         # using an int anyways, so we have the
                         # space to still do all the flags.

NDI_UNIQUE     = 0x100   # Print immediately, don't buffer.
NDI_ALL        = 0x200   # Inform all players of this message.
NDI_ALL_DMS    = 0x400   # Inform all logged in DMs. Used in case of

# See Also:  client/gtk-v2/src/info.c
#
# The following markup strings are supported:
#
# [b] ... [/b]   bold
# [i] ... [/i]   italic
# [ul] ... [/ul] underline
# [print]        font_style_names[0] ???
# [arcane]       font_style_names[1]
# [strange]      font_style_names[2]
# [fixed]        font_style_names[3]
# [hand]         font_style_names[4]
# [color=x] ... [/color] where x are the quoted color names below.
#
# font_style_names[] are determined by the selected theme.  The default
# installed theme files are in client/gtk-v2/themes and are "Standard"
# and Black.  Both themes use the same set of fonts by default.
#
#     font_style_names[0] =  <system/theme default> ???
#     font_style_names[1] =  "URW Chancery Lk"
#     font_style_names[2] =  "Sans Italic"
#     font_style_names[3] =  "Luxi Mono"
#     font_style_names[4] =  "Century Schoolbook L Italic"
#
# Color names set by the user in the gtkrc file. */
# static const char *const usercolorname[NUM_COLORS] = {
#   "black",                /* 0  */
#   "white",                /* 1  */
#   "darkblue",             /* 2  */
#   "red",                  /* 3  */
#   "orange",               /* 4  */
#   "lightblue",            /* 5  */
#   "darkorange",           /* 6  */
#   "green",                /* 7  */
#   "darkgreen",            /* 8  *//* Used for window background color */
#   "grey",                 /* 9  */
#   "brown",                /* 10 */
#   "yellow",               /* 11 */
#   "tan"                   /* 12 */
#
def player_send(text):
  lines = text.split(os.linesep)
  for line in lines:
    line = line.rstrip(os.linesep)
    client_send(f"draw 0 [color=darkorange]{codefile}: {line}")
    debug_send(f"{codefile}: {line}")
  return

def player_send_bare(text):
  lines = text.split(os.linesep)
  for line in lines:
    line = line.rstrip(os.linesep)
    client_send(f"draw 0 {line}")
    debug_send(f"{line}")
  return

def check_completed():
  #
  # Does a visit log exist for this map and player?  This may fail if a player
  # hasn't visited the map before when the logger was active.  If not found,
  # a map cannot have been marked completed.
  #
  if map_id and player_id:
    cursor.execute('''
        SELECT COMPLETED, COMPLETED_DATE FROM visit
        WHERE  MAP_ID = ? AND PLAYER_ID = ?
    ''', ( map_id, player_id ))
    query = cursor.fetchone()
    if query != None:
      completed = query[0]
      completed_date = query[1]
      if completed:
        player_send(f"You marked this area completed {completed} times.")
        player_send(f"The most recent completion was: {completed_date}.")

# Begin ######################################################################

try:
  server_name = os.environ['CF_SERVER_NAME']
  player_name  = os.environ['CF_PLAYER_NAME']
except:
  console_send(f"{license}")
  console_send(f"This plug-in is not meant to run as a standalone program.")
  sys.exit(0)

# Sometimes its hard to pick out where the beginning of the run is located
# when stopping and starting the script during development so output this.
#
debug_send("" + \
  "-------------------------------------------------------------------------")

# sqlite3 initialization #####################################################
#
codefile = re.compile("[.][^.]+\Z").sub("", __file__)
datafile = codefile + ".db"
codefile = re.compile("^.+\/").sub("", codefile)
debug_send(f"{datafile}")
codepath = __file__

try:
  dbConn = sqlite3.connect(f"{datafile}")
except:
  console_send(f"{e}")
  player_send(f"{datafile}")
  player_send(f"[color=red]sqlite3.connect error[/color]")
  player_send(f"exiting...")
  sys.exit(0)

cursor = dbConn.cursor()

# server initialization ######################################################
#
servers     = 0
server_id   = 0

debug_send(f"CF_SERVER_NAME: {server_name}")

try:
  cursor.execute('''
      CREATE TABLE IF NOT EXISTS server (
        SERVER_ID         INTEGER PRIMARY KEY NOT NULL,
        SERVER_NAME       TEXT    UNIQUE NOT NULL
      )
  ''')
except:
  console_send(f"{e}")
  player_send(f"{datafile}")
  player_send(f"[color=red]CREATE TABLE server error[/color]")
  player_send(f"exiting...")
  sys.exit(0)

try:
  cursor.execute('''
      SELECT COUNT(*) FROM server
  ''')
except:
  console_send(f"{e}")
  player_send(f"{datafile}")
  player_send(f"[color=red]SELECT FROM server error[/color]")
  player_send(f"exiting...")
  sys.exit(0)

query = cursor.fetchone()
servers = query[0]

# Get or assign a server_id for the current server.
#
try:
  cursor.execute('''
    SELECT SERVER_ID FROM server
    WHERE  SERVER_NAME = ?
  ''', ( server_name, ))
except:
  console_send(f"{e}")
  player_send(f"{datafile}")
  player_send(f"[color=red]SELECT FROM server error[/color]")
  player_send(f"exiting...")

query = cursor.fetchone()
if query == None:
  servers = servers + 1
  server_id = servers
  try:
    cursor.execute('''
      INSERT INTO server
        ( SERVER_ID, SERVER_NAME )
          VALUES ( ?, ? )
    ''', (servers, server_name))
  except:
    console_send(f"{e}")
    player_send(f"{datafile}")
    player_send(f"[color=red]INSERT INTO server error[/color]")
    player_send(f"exiting...")
    sys.exit(0)

  dbConn.commit()
else:
  server_id = query[0]

debug_send(f"server_name (server_id): {server_name} ({server_id})")

# player initialization ######################################################
#
# request player
# request player 1481 Player: Brayagorn the human

players      = 0
player_code  = 0
player_title = ''
player_seen  = time.strftime("%Y/%m/%d %H:%M")

debug_send(f"CF_PLAYER_NAME: {player_name}")

regx_rqst_player      = "^request\splayer\s"
regc_rqst_player      = re.compile(regx_rqst_player)
regx_rqst_player_id   = "(\d+)\s+"
regc_rqst_player_id   = re.compile(regx_rqst_player_id)
regx_rqst_player_strt = regx_rqst_player + regx_rqst_player_id
regc_rqst_player_strt = re.compile(regx_rqst_player_strt)
regx_rqst_player_name = "Player:\s+(\w+)\s+the\s+(.+)"
regc_rqst_player_name = re.compile(regx_rqst_player_name)
regx_rqst_player_data = regx_rqst_player_id + regx_rqst_player_name
regc_rqst_player_data = re.compile(regx_rqst_player_data)

try:
  cursor.execute('''
      CREATE TABLE IF NOT EXISTS player (
        PLAYER_ID         INTEGER PRIMARY KEY NOT NULL,
        SERVER_ID         INTEGER NOT NULL,
        PLAYER_NAME       TEXT    NOT NULL,
        PLAYER_TITLE      TEXT    NOT NULL,
        PLAYER_SEEN       TEXT    NOT NULL
      )
  ''')
except:
  console_send(f"{e}")
  player_send(f"{datafile}")
  player_send(f"[color=red]CREATE TABLE player error[/color]")
  player_send(f"exiting...")
  sys.exit(0)

try:
  cursor.execute('''
      SELECT COUNT(*) from player
  ''')
except:
  console_send(f"{e}")
  player_send(f"{datafile}")
  player_send(f"[color=red]SELECT FROM player error[/color]")
  player_send(f"exiting...")

query = cursor.fetchone()
players = query[0]
try:
  cursor.execute('''
    SELECT PLAYER_ID, PLAYER_TITLE FROM player
    WHERE  PLAYER_NAME = ? AND SERVER_ID = ?
  ''', ( player_name, server_id ))
except:
  console_send(f"{e}")
  player_send(f"{datafile}")
  player_send(f"[color=red]SELECT FROM player error[/color]")
  player_send(f"exiting...")

query = cursor.fetchone()
if query == None:
  client_send("request player")

  for buffer in sys.stdin:
    buffer = buffer.rstrip(os.linesep)
    if regc_rqst_player_strt.match(buffer):
      buffer = regc_rqst_player.sub('', buffer)
      matches = regc_rqst_player_data.match(buffer)
      player_nmbr = matches.group(1)
      # player_name = matches.group(2)
      player_title = matches.group(3)
      players = players + 1
      try:
        cursor.execute('''
          INSERT INTO player
            ( PLAYER_ID, SERVER_ID, PLAYER_NAME, PLAYER_TITLE, PLAYER_SEEN )
              VALUES ( ?, ?, ?, ?, ? )
        ''', (players, server_id, player_name, player_title, player_seen))
      except:
        player_send(f"{datafile}")
        player_send(f"[color=red]INSERT INTO player error[/color]")
        player_send(f"exiting...")
        sys.exit(0)

      dbConn.commit()
      player_id = players
    else:
      continue
else:
  player_id = query[0]
  player_title = query[1]

debug_send(f"player_id: {player_id}")

try:
  cursor.execute('''
      UPDATE player
        SET   PLAYER_SEEN = ?
        WHERE PLAYER_ID = ?
  ''', (player_seen, player_id))
except:
  player_send(f"{datafile}")
  player_send(f"[color=red]UPDATE player error[/color]")
  player_send(f"exiting...")
  sys.exit(0)

dbConn.commit()

player_send(f"Hello, {player_name}")

# map initialization #########################################################
#
# Scorn Alchemy Shop (/scorn/shops/potionshop) in The Kingdom of Scorn
# Created: 1996-05-02 bt (thomas@astro.psu.edu)
# Modified: 2023-11-27 Rick Tanner
#
# watch drawextinfo 0 10 0 Undead Church (/scorn/misc/church) in The Kingdom of Scorn
# watch drawextinfo 0 10 0 Created:  1993-10-15
# Modified: 2021-09-21 Nicolas Weeger
#
# (null) (/random/undead_quest0000) in The Kingdom of Scorn
# xsize -1
# ysize -1
# wallstyle dungeon2
# floorstyle lightdirt
# monsterstyle undead
# layoutstyle onion
# decorstyle creepy
# exitstyle sstair
# final_map /scorn/peterm/undead_quest
# symmetry 4
# difficulty_increase 0.000000
# dungeon_level 1
# dungeon_depth 5
# orientation 1
# origin_x 4
# origin_y 14
# random_seed 1746522242
#
regx_wtch_draw = "^watch drawextinfo (\d+\s){3}"
regc_wtch_draw = re.compile(regx_wtch_draw, 0)
regx_wtch_draw_strt = regx_wtch_draw + "([^\(]+|\(null\)\s*)\("
regc_wtch_draw_strt = re.compile(regx_wtch_draw_strt, 0)
regx_wtch_draw_name = regx_wtch_draw_strt + "[~/]"
regc_wtch_draw_name = re.compile(regx_wtch_draw_name, 0)
regx_wtch_draw_path = "^([^\(\)]+|\(null\)\s)\(([^\)]+)\)\s*(.*)"
regc_wtch_draw_path = re.compile(regx_wtch_draw_path, 0)
regx_wtch_draw_made = "Created:\s+(.*)"
regc_wtch_draw_made = re.compile(regx_wtch_draw_made, 0)
regx_wtch_draw_date = "Modified:\s+(.+)"
regc_wtch_draw_date = re.compile(regx_wtch_draw_date, 0)
regx_wtch_draw_xsiz = regx_wtch_draw + "xsize\s[-]?\d+"
regc_wtch_draw_xsiz = re.compile(regx_wtch_draw_xsiz, 0)
regx_wtch_draw_rnds = "random_seed\s\d+"
regc_wtch_draw_rnds = re.compile(regx_wtch_draw_rnds, 0)

cursor.execute('''
    CREATE TABLE IF NOT EXISTS map (
      MAP_ID   INTEGER PRIMARY KEY NOT NULL,
      MAP_PATH TEXT    UNIQUE NOT NULL,
      MAP_NAME TEXT    NOT NULL,
      MAP_MADE TEXT    NOT NULL,
      MAP_DATE TEXT    NOT NULL
    )
''')

cursor.execute('''
    SELECT COUNT(*) from map
''')
query = cursor.fetchone()
maps = query[0]

map_id = 0;
map_line = -1;
map_data = '';
map_name = '';
map_path = '';
map_made = '';
map_date = '';

# quiet list initialization ##################################################
#
cursor.execute('''
    CREATE TABLE  IF NOT EXISTS quiet (
      PLAYER_ID   INTEGER,
      SERVER_ID   INTEGER,
      MAP_PATTERN TEXT NOT NULL
    )
''')

quiets = 0

for loop in ( 'world_%_%', '%Apartment%', '%Inn %' ):
  quiets = quiets + 1
  cursor.execute('''
      SELECT
        MAP_PATTERN
      FROM
        quiet
      WHERE
        PLAYER_ID IS NULL AND SERVER_ID IS NULL AND MAP_PATTERN = ?
  ''', (loop, ))
  if cursor.fetchone() is None:
    cursor.execute('''
        INSERT INTO quiet
          ( PLAYER_ID, SERVER_ID, MAP_PATTERN )
        VALUES
          ( NULL, NULL, ? )
    ''', ( loop, ))

cursor.execute('''
    SELECT COUNT(*) from quiet
''')
query = cursor.fetchone()
quiets = query[0]

# visit log initialization ###################################################
#
cursor.execute('''
    CREATE TABLE  IF NOT EXISTS visit (
      MAP_ID      INTEGER NOT NULL,
      PLAYER_ID   INTEGER NOT NULL,
      SERVER_ID   INTEGER NOT NULL,
      VISIT_TOTAL INTEGER NOT NULL,
      VISIT_DATE  TEXT NOT NULL,
      COMPLETED   INTEGER DEFAULT 0,
      VISIT_DATE  TEXT NOT NULL DEFAULT ""
    )
''')

# Schema update if COMPLETED is missing.
#
cursor.execute('''
    PRAGMA table_info(visit)
''')
success = False
query = cursor.fetchall()
for row in query:
  # row field numbers are zero-based
  if row[1] == "COMPLETED":
    success = True
if not success:
  debug_send(f"ALTER TABLE visit ADD COLUMN COMPLETED INTEGER DEFAULT 0")
  cursor.execute('''
      ALTER TABLE visit ADD COLUMN COMPLETED INTEGER DEFAULT 0
  ''')

# Schema update if COMPLETED_DATE is missing.
#
cursor.execute('''
    PRAGMA table_info(visit)
''')
success = False
query = cursor.fetchall()
for row in query:
  # row field numbers are zero-based
  if row[1] == "COMPLETED_DATE":
    success = True
if not success:
  debug_send(f"ALTER TABLE visit ADD COLUMN COMPLETED_DATE STRING NOT NULL" \
    + ": DEFAULT ''")
  cursor.execute('''
      ALTER TABLE visit ADD COLUMN COMPLETED_DATE STRING NOT NULL DEFAULT ""
  ''')

# Schema update if SERVER_ID is missing.  OH NO! MAY IT NEVER BE!  If the
# player ever played multiple servers, the old schema only tracked visits
# by MAP_ID and PLAYER_ID and omitted SERVER_ID.  There's really not any
# good way to fix the visit log.  The best thing to do is set SERVER_ID
# to the current server.  Unfortunately, if the assumption is wrong, the
# player's only option is to clear incorrect completion data, and then
# effectively lose visit data for the completion on a different server.
#
# The player hopefully performs this schema update on the server with the
# most log entries (since that minimizes server_id errors created here if
# they logged visits on more than one server).  Ouch!
#
cursor.execute('''
    PRAGMA table_info(visit)
''')
success = False
query = cursor.fetchall()
for row in query:
  # row field numbers are zero-based
  if row[1] == "SERVER_ID":
    success = True
if not success:
  player_send(f"[color=red]NOTE: visit database schema repair![color]")
  player_send(f"Visit data previously failed to track the server that a "  + \
              f"visit occurred on.  Unfortunately, if you logged visits "  + \
              f"on multiple servers, it is not possible to automatically " + \
              f"determine which server was in use at the time.  We are "   + \
              f"assuming (sorry) that the current server is the one that " + \
              f"should be used for all old visit data.  While playing, if" + \
              f" you notice incorrect completion data upon entering a map" + \
              f", use 'scripttell {codepath} incomplete' to erase it.  If" + \
              f" the map was completed on another server, logon to it and" + \
              f" re-visit the map and mark it complete (again) there."       \
             )
  debug_send(f"ALTER TABLE visit ADD COLUMN SERVER_ID INTEGER DEFAULT "    + \
             f"{server_id}")
  cursor.execute('''
      ALTER TABLE visit ADD COLUMN SERVER_ID INTEGER DEFAULT ''' + \
      f"{server_id}")

visits = 0
visit_date  = ''

cursor.execute('''
    SELECT COUNT(*) from visit
''')
query = cursor.fetchone()
visits = query[0]

# visit cache initialization #################################################
#
vcConn = sqlite3.connect(":memory:")
vcursor = vcConn.cursor()

vcursor.execute('''
    CREATE TABLE  IF NOT EXISTS vcache (
      MAP_SEQ     INTEGER NOT NULL,
      MAP_ID      INTEGER NOT NULL
    )
''')

v_head  = 1
v_tail  = 0

# cfmaplog service ###########################################################

regc_scripttell = re.compile('^scripttell\s+', 0)

# Notify when entering a map.
#
debug_send("watch newmap")
client_send("watch newmap")

# Interact with the client.
#
for buffer in sys.stdin:
  buffer = buffer.rstrip(os.linesep)

  debug_send(f"> '{buffer}'")

  if not len(buffer):
    time.sleep(0.25)
    continue

  if regc_scripttell.match(buffer):
    match regc_scripttell.sub('', buffer).split(os.linesep):

      # Player may halt the script with either 'scripttell' or 'scriptkill'
      #
      case ['quit']:
        break

      # player may toggle debug with 'scripttell'
      #
      case ['debug']:
        debug = not debug

      case ['completed']:
        try:
          cursor.execute('''
            SELECT v.COMPLETED_DATE, v.COMPLETED, m.MAP_NAME, m.MAP_PATH
            FROM visit v
            JOIN server s ON s.SERVER_ID = v.SERVER_ID
            JOIN player p ON p.PLAYER_ID = v.PLAYER_ID
            JOIN map m ON m.MAP_ID = v.MAP_ID
            WHERE v.PLAYER_ID = ? AND
                  v.SERVER_ID = ? AND
                  v.COMPLETED >= 1
            ORDER BY COMPLETED_DATE DESC
          ''', ( player_id, server_id ))
        except:
          console_send(f"{e}")

        player_send(f"You marked the following maps as completed:")
        player_send_bare(f"Last Completion : ### : Map Name : /Map/Path")
        query = cursor.fetchall()
        for row in query:
          player_send_bare(f"{row[0]} : {row[1]:03d} : {row[2]} : {row[3]}")

      case ['complete' | 'incomplete']:
        completed = 0
        completed_date = ""
        try:
          cursor.execute('''
            SELECT COMPLETED, COMPLETED_DATE
            FROM visit WHERE MAP_ID = ? and PLAYER_ID = ?
          ''', ( map_id, player_id))
        except:
          console_send(f"{e}")
          debug_send(f"map_id {map_id} player_id {player_id}")

        query = cursor.fetchone()
        if query == None:
          if buffer == 'scripttell complete':
            completed = 1
            completed_date = time.strftime("%Y/%m/%d %H:%M")
        else:
          if buffer == 'scripttell complete':
            completed = query[0] + 1
            player_send(f"You've marked this map complete {completed} times.")
            if completed > 1 and len(query[1]):
              player_send(f"The most recent previous time was:  {query[1]}.")
            completed_date = time.strftime("%Y/%m/%d %H:%M")
          else:
            if query[0] == 0:
              player_send(f"This map is already marked as not completed.")
            else:
              player_send(f"This map is now marked as not completed.")

        debug_send(f"map_id {map_id} player_id {player_id}")
        debug_send(f"completed {completed} completed_date {completed_date}")
        try:
          cursor.execute('''
            UPDATE visit
              SET   COMPLETED = ?, COMPLETED_DATE = ?
              WHERE MAP_ID = ? AND PLAYER_ID = ?
          ''', (completed, completed_date, map_id, player_id))
        except:
          console_send(f"{e}")

        dbConn.commit()

      case ['help', *arguments]:
        helptext = """
A Crossfire RPG client plug-in that keeps and reports statistics related to map
visits.  Statistics are kept separate for different characters, and are unique
per-server played.  Additionally, it serves as a utility to track which maps
have been 'completed'.  Statistics include the date and time of the most recent
visit and completion, if any.  A number of reporting options are supported to
allow the player to review the collected data.

The player interacts with the plug-in via 'scripttell' commands.  Supported
commands are:

* completed
  While in a map, mark it to show that you believe you have 'finished' all you
  want to do.  What you consider 'finished' is entirely up to you.  A tally is
  kept of the number of times the command is used on each map.  It is usually
  most helpful to mark the entrance map 'completed' since the number of times
  the map was 'completed' is shown upon entry after the tally is greater than
  zero.  Unfortunately, when the entrance map is a random map, this does not
  presently work since the entry map path may change from visit to visit.
  That said marking it anyway assures the completion is reported on entry to
  the map in the event the same map is shown first in the future.  For these
  maps, one could mark every level, but probably most importantly, the last
  non-random map of the dungeon.

* help
  This information.

* debug
  Enable console (not in-game) messages.  To see these messages, start the
  client in a console.  This is a toggle, so entering the command another
  time disables the messages.

* incomplete
  Clear the tally of a particular 'completed' map because one was erroneously
  marked as complete, or, due to discovering that something was missed on the
  objectives of the map.  Using this command means no 'completed' tally is
  shown on entry.

* quit
  Stop the plugin.  Basically, what 'scriptkill' does, except that the script
  initiates.  At present, the plugin doesn't know how to 'catch' a scriptkill
  (if that is even possible).

* visited
* visited <MaxToShow>
  List all, or up to <MaxToShow> entries, in date order, with recent first.
  <MaxToShow> is an optional integer.

* visited least
* visited least <MaxToShow>
  List all, or up to <MaxToShow> entries, with lowest number of visits as the
  primary sort key, and date order for ties, with the most recent first.

* visited most
* visited most <MaxToShow>
  List all, or up to <MaxToShow> entries, with highest number of visits as the
  primary sort key, and date order for ties, with the most recent first.
"""
        player_send_bare(helptext)

      # The bare 'visted' command MUST come last in the match cases.
      #
      case ['visited', *arguments]:
        sqlcmd = '''
          SELECT v.VISIT_DATE, v.VISIT_TOTAL, m.MAP_NAME, m.MAP_PATH
          FROM visit v
          JOIN server s ON s.SERVER_ID = v.SERVER_ID
          JOIN player p ON p.PLAYER_ID = v.PLAYER_ID
          JOIN map m ON m.MAP_ID = v.MAP_ID
          WHERE v.PLAYER_ID = ? AND
                v.SERVER_ID = ? AND
                v.VISIT_TOTAL >= 1'''

        match buffer.split():
          case ['scripttell', 'visited', 'least', *limit]:
            sqlcmd = sqlcmd + '''
            ORDER BY v.VISIT_TOTAL ASC, VISIT_DATE ASC'''
          case ['scripttell', 'visited', 'most', *limit]:
            sqlcmd = sqlcmd + '''
            ORDER BY v.VISIT_TOTAL DESC, VISIT_DATE DESC'''
          case ['scripttell', 'visited', *limit]:
            sqlcmd = sqlcmd + '''
            ORDER BY v.VISIT_DATE DESC'''

        match len(limit):
          case 0:
            pass
          case 1:
            if not limit[0].isnumeric():
              player_send(f"[color=red]LIMIT argument must be numeric." + \
                          f"[\color]")
              continue
            else:
              sqlcmd = sqlcmd + '''
              LIMIT ''' + f"{limit[0]}"
          case _:
            player_send(f"[color=red]Only one LIMIT argument wanted." + \
                        f"[\color]")
            continue

        player_send_bare(f'''{sqlcmd}, ({player_id}, {server_id})''')

        try:
          cursor.execute(sqlcmd, (player_id, server_id))
        except sqlite3.Error as e:
          console_send(f"{e}")

        player_send(f"All recorded visits:")
        player_send_bare(f"Most Recent Visit : ### : Map Name : /Map/Path")
        query = cursor.fetchall()
        for row in query:
          player_send_bare(f"{row[0]} : {row[1]:04d} : {row[2]} : {row[3]}")

      case _:
        player_send(f"'{buffer}'")
        player_send(f"[color=red]Not a recognized command.[\color]")

    buffer = ''
    continue

  # When a map is entered, ask the client to start forwarding drawextinfo
  # the server sends.  Instruct the client to issue a mapinfo command on
  # our behalf.  Then commence listening for mapinfo output the client
  # forwards to us.
  #
  if buffer == "watch newmap":
    buffer = ''
    map_id = 0;
    map_line = 0;
    map_data = '';
    map_name = '';
    map_path = '';
    map_made = '';
    map_date = '';
    client_send("watch drawextinfo")
    client_send("issue 1 1 mapinfo")
    continue

  if map_line >= 0:
    map_line = map_line + 1

    debug_send(f"{map_line}> '{buffer}'")

    if regc_wtch_draw_name.match(buffer):
      buffer = regc_wtch_draw.sub('', buffer)
      map_data = "map_name"
    elif regc_wtch_draw_made.search(buffer):
      buffer = regc_wtch_draw.sub('', buffer)
      map_data = "map_made"
    elif regc_wtch_draw_date.search(buffer):
      buffer = regc_wtch_draw.sub('', buffer)
      map_data = "map_date"
    elif regc_wtch_draw_xsiz.search(buffer):
      buffer = regc_wtch_draw.sub('', buffer)
      map_date = time.strftime("%Y-%m-%d")
      map_data = "map_xsize"
      map_made = 'random'
    elif regc_wtch_draw_rnds.match(buffer):
      map_data = "map_write"
    else:
      buffer = ''
      continue

    debug_send(f"map_data {map_data}")

    match map_data:
      case 'map_name':
        matches = regc_wtch_draw_path.match(buffer)
        debug_send(f"{map_line}_1: '" + matches.group(1) + "'")
        debug_send(f"{map_line}_2: '" + matches.group(2) + "'")
        debug_send(f"{map_line}_3: '" + matches.group(3) + "'")

        map_name = matches.group(1) + matches.group(3)
        map_path = matches.group(2)
        continue
      case 'map_made':
        matches = regc_wtch_draw_made.search(buffer)
        map_made = matches.group(1)
        buffer = ''
        continue
      case 'map_date':
        matches = regc_wtch_draw_date.search(buffer)
        map_date = matches.group(1)
        map_data = 'map_write'

    if map_data == 'map_write':

      debug_send(f"map_name (map_path) {map_name} ({map_path})")
      debug_send(f"map_made {map_made}")
      debug_send(f"map_date {map_date}")
      debug_send(f"unwatch: drawextinfo")

      client_send("unwatch drawextinfo")

      # Insert a "new" map into the map database.  Assume if the insertion
      # fails that it was already present.  TODO: Do not insert if unneeded.
      #
      try:
        maps = maps + 1
        cursor.execute('''
          INSERT INTO map
            ( MAP_ID, MAP_PATH, MAP_NAME, MAP_MADE, MAP_DATE )
              VALUES ( ?, ?, ?, ?, ? )
        ''', (maps, map_path, map_name, map_made, map_date))
        dbConn.commit()
      except sqlite3.IntegrityError:
        maps = maps - 1

      # Get the map_id of the entered map.  This SHOULD NEVER fail because the
      # map should always have been added by this point.
      #
      cursor.execute('''
        SELECT MAP_ID FROM map
        WHERE  MAP_PATH = ?
      ''', ( map_path, ))
      query = cursor.fetchone()
      if query == None:
        map_id = 0
        pass
      else:
        map_id = query[0]

      debug_send(f"map_id: {map_id}")

      # Is the map_id in the (recently) visit(ed) cache?  If so, do not make
      # an attempt to log the visit.  This should reduce spammy visits to
      # maps that may occur when passing through doors inside a store, or
      # perhaps even for events like dimention door.
      #
      vcursor.execute ('''
        SELECT MAP_SEQ FROM vcache
        WHERE  MAP_ID = ?
      ''', ( map_id, ) )
      query = vcursor.fetchone()
      if query != None:
        debug_send(f"SQUELCH visit message!")
        #
        # But completion notices are never squelched if for recent visits as
        # this may affect a player's choice as to whether or not to proceed.
        #
        check_completed()
      else:
        vcursor.execute('''
          INSERT INTO vcache
            ( MAP_SEQ, MAP_ID )
              VALUES ( ?, ? )
        ''', (v_head, map_id) )
        dbConn.commit()
        v_head = v_head + 1
        debug_send(f"vcache -> v_head: {v_head}")
        if v_head - v_tail >= 10:
          vcursor.execute('''
              DELETE FROM vcache
              WHERE MAP_SEQ = ?
          ''', (v_tail, ))
          dbConn.commit()
          v_tail = v_tail + 1
          debug_send(f"vcache -> v_tail: {v_tail}")

        # Is there already a visit log for this map and player?  This may fail
        # because a player hasn't visited the map before when the logger was
        # active.  If not found, add a new visit log, otherwise update the
        # existing log.
        #
        visit_date = time.strftime("%Y/%m/%d %H:%M")
        cursor.execute('''
          SELECT VISIT_TOTAL, COMPLETED, COMPLETED_DATE FROM visit
          WHERE  MAP_ID = ? AND PLAYER_ID = ?
        ''', ( map_id, player_id ))
        query = cursor.fetchone()
        if query == None:
          visit_total = 0
          cursor.execute('''
            INSERT INTO visit
              ( MAP_ID, PLAYER_ID, VISIT_TOTAL, VISIT_DATE )
                VALUES ( ?, ?, ?, ? )
          ''', (map_id, player_id, 1, visit_date))
          dbConn.commit()
          player_send(f"I don't think I remember this place!")
        else:
          completed_date = query[2]
          visit_total = query[0]
          completed = query[1]
          cursor.execute('''
            SELECT
              MAP_PATTERN
            FROM
              quiet
            WHERE PLAYER_ID IS NULL AND SERVER_ID IS NULL AND ? LIKE MAP_PATTERN
          ''', ( map_name, ))
          query = cursor.fetchone()
          if query:
            debug_send(f"QUIET!SHH!")
          else:
            player_send(f"You were here at least {visit_total} times prior.")
          #
          # Update the visit count
          #
          visit_total = visit_total + 1
          debug_send(f"visit_total {visit_total}")

          try:
            cursor.execute('''
              UPDATE visit
                SET   VISIT_TOTAL = ?, VISIT_DATE = ?
                WHERE MAP_ID = ? AND PLAYER_ID = ?
            ''', (visit_total, visit_date, map_id, player_id))
          except:
            console_send(f"{e}")
          dbConn.commit()
          #
          # Always check map completion status without regard to quiet status.
          #
          check_completed()

      map_line = -1;

  buffer = ''

player_send(f"Farewell, {player_name}")

vcConn.close()
dbConn.close()

# End ########################################################################

sys.exit(0)

