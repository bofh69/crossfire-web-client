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
  sys.stdout.write(text)
  sys.stdout.flush()
  return

# Send something to the "user" via stderr.  For these to be seen, the client
# is started in a console.  If a newline should go, and it usually should,
# pass "\n" at the end of the text.  This script may build output
# incrementally, so do not add one here.
#
def console_send(text):
  sys.stderr.write(text)
  sys.stderr.flush()
  return

# Send something to facilitate debugging via stderr.  For these to be seen,
# the client is started in a console. If a newline should go, and it usually
# should, pass "\n" at the end of the string.  This script may build output
# incrementally, so do not add one here.
#
def debug_send(text):
  if debug:
    console_send(text)
  return

# Send something to the "player" by way of the Messages pane.  If a newline
# should go, and it usually should, pass one at the end of the text.  This
# script may build output incrementally, so do not add one here.
#
def player_send(text):
  client_send(f"issue 1 1 tell {player_name} {codefile}: " + text)
  debug_send(f"{player_name} {codefile}: " + text)
  return

# Begin ######################################################################

try:
  server_name = os.environ['CF_SERVER_NAME']
  player_name  = os.environ['CF_PLAYER_NAME']
except:
  console_send(f"{license}\n")
  console_send(f"This plug-in is not meant to run as a standalone program.\n")
  sys.exit(0)

# Sometimes its hard to pick out where the beginning of the run is located
# when stopping and starting the script during development so output this.
#
debug_send(\
  "-----------------------------------------------------------------------\n")

# sqlite3 initialization #####################################################
#
codefile = re.compile("[.][^.]+\Z").sub("", __file__)
datafile = codefile + ".db"
codefile = re.compile("^.+\/").sub("", codefile)
debug_send(f"{datafile}\n")

dbConn = sqlite3.connect(f"{datafile}")
cursor = dbConn.cursor()

# server initialization ######################################################
#
servers     = 0
server_id   = 0

debug_send(f"CF_SERVER_NAME: {server_name}\n")

cursor.execute('''
    CREATE TABLE IF NOT EXISTS server (
      SERVER_ID         INTEGER PRIMARY KEY NOT NULL,
      SERVER_NAME       TEXT    UNIQUE NOT NULL
    )
''')

cursor.execute('''
    SELECT COUNT(*) from server
''')
query = cursor.fetchone()
servers = query[0]

# Get or assign a server_id for the current server.
#
cursor.execute('''
  SELECT SERVER_ID FROM server
  WHERE  SERVER_NAME = ?
''', ( server_name, ))
query = cursor.fetchone()
if query == None:
  try:
    servers = servers + 1
    cursor.execute('''
      INSERT INTO server
        ( SERVER_ID, SERVER_NAME )
          VALUES ( ?, ? )
    ''', (servers, server_name))
    dbConn.commit()
    server_id = servers
  except:
    console_send(f"{e}\n")
    servers = servers - 1
    server_id = 0
else:
  server_id = query[0]

debug_send(f"server_name (server_id): {server_name} ({server_id})\n")

# player initialization ######################################################
#
# request player
# request player 1481 Player: Brayagorn the human

players      = 0
player_code  = 0
player_title = ''
player_seen  = time.strftime("%Y/%m/%d %H:%M")

debug_send(f"CF_PLAYER_NAME: {player_name}\n")

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

cursor.execute('''
    CREATE TABLE IF NOT EXISTS player (
      PLAYER_ID         INTEGER PRIMARY KEY NOT NULL,
      SERVER_ID         INTEGER NOT NULL,
      PLAYER_NAME       TEXT    NOT NULL,
      PLAYER_TITLE      TEXT    NOT NULL,
      PLAYER_SEEN       TEXT    NOT NULL
    )
''')

cursor.execute('''
    SELECT COUNT(*) from player
''')
query = cursor.fetchone()
players = query[0]

cursor.execute('''
  SELECT PLAYER_ID, PLAYER_TITLE FROM player
  WHERE  PLAYER_NAME = ? AND SERVER_ID = ?
''', ( player_name, server_id ))
query = cursor.fetchone()

if query == None:
  client_send("request player\n")

  for buffer in sys.stdin:
    buffer = buffer.rstrip()
    if regc_rqst_player_strt.match(buffer):
      buffer = regc_rqst_player.sub('', buffer)
      matches = regc_rqst_player_data.match(buffer)
      player_nmbr  = matches.group(1)
      # player_name  = matches.group(2)
      player_title = matches.group(3)
      try:
        players = players + 1
        cursor.execute('''
          INSERT INTO player
            ( PLAYER_ID, SERVER_ID, PLAYER_NAME, PLAYER_TITLE, PLAYER_SEEN )
              VALUES ( ?, ?, ?, ?, ? )
        ''', (players, server_id, player_name, player_title, player_seen))
        dbConn.commit()
        player_id = players
      except:
        console_send(f"{e}\n")
        players = players - 1
        player_id = 0
      break
    else:
      continue
else:
  player_id = query[0]
  player_title = query[1]

debug_send(f"player_id: {player_id}\n")

try:
  cursor.execute('''
    UPDATE player
      SET   PLAYER_SEEN = ?
      WHERE PLAYER_ID = ?
  ''', (player_seen, player_id))
  dbConn.commit()
except:
  console_send(f"{e}\n")

player_send(f"Welcome, {player_title}\n")

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
      VISIT_TOTAL INTEGER NOT NULL,
      VISIT_DATE  TEXT NOT NULL
    )
''')

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

# map logging service ########################################################

# Notify when enterings a map.
#
client_send("watch newmap\n")

# Interact with the client.
#
for buffer in sys.stdin:
  buffer = buffer.rstrip()

  debug_send(f"> '{buffer}'\n")

  match buffer:
    case '':
      time.sleep(0.25)
      continue

    # Player may halt the script with either 'scripttell' or 'scriptkill'
    #
    case 'scripttell quit':
      break

    # player may toggle debug with 'scripttell'
    #
    case 'scripttell debug':
      debug = not debug
      buffer = ''
      continue

    # When a map is entered, ask the client to start forwarding drawextinfo
    # the server sends.  Instruct the client to issue a mapinfo command on
    # our behalf.  Then commence listening for mapinfo output the client
    # forwards to us.
    #
    case 'watch newmap':
      map_line = 0;
      map_data = '';
      client_send("watch drawextinfo\n")
      client_send("issue 1 1 mapinfo\n")
      buffer = ''
      continue

  if map_line >= 0:
    map_line = map_line + 1

    debug_send(f"{map_line}> '{buffer}'\n")

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

    debug_send(f"map_data {map_data}\n")

    match map_data:
      case 'map_name':
        matches = regc_wtch_draw_path.match(buffer)
        debug_send(f"{map_line}_1: '" + matches.group(1) + "'\n")
        debug_send(f"{map_line}_2: '" + matches.group(2) + "'\n")
        debug_send(f"{map_line}_3: '" + matches.group(3) + "'\n")

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

      debug_send(f"map_name (map_path) {map_name} ({map_path})\n")
      debug_send(f"map_made {map_made}\n")
      debug_send(f"map_date {map_date}\n")
      debug_send(f"unwatch: drawextinfo\n")

      client_send("unwatch drawextinfo\n")

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

      debug_send(f"map_id: {map_id}\n")

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
        debug_send(f"SQUELCHED!\n")
      else:
        vcursor.execute('''
          INSERT INTO vcache
            ( MAP_SEQ, MAP_ID )
              VALUES ( ?, ? )
        ''', (v_head, map_id) )
        dbConn.commit()
        v_head = v_head + 1
        debug_send(f"vcache -> v_head: {v_head}\n")
        if v_head - v_tail >= 10:
          vcursor.execute('''
              DELETE FROM vcache
              WHERE MAP_SEQ = ?
          ''', (v_tail, ))
          dbConn.commit()
          v_tail = v_tail + 1
          debug_send(f"vcache -> v_tail: {v_tail}\n")

        # Is there already a visit log for this map and player?  This may fail
        # because a player hasn't visited the map before when the logger was
        # active.  If not found, add a new visit log, otherwise update the
        # existing log.
        #
        visit_date = time.strftime("%Y/%m/%d %H:%M")
        cursor.execute('''
          SELECT VISIT_TOTAL FROM visit
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
          player_send(f"I don't think I remember this place!\n")
        else:
          visit_total = query[0]
          cursor.execute('''
            SELECT
              MAP_PATTERN
            FROM
              quiet
            WHERE PLAYER_ID IS NULL AND SERVER_ID IS NULL AND ? LIKE MAP_PATTERN
          ''', ( map_name, ))
          query = cursor.fetchone()
          if query:
            debug_send(f"QUIET!SHH!\n")
          else:
            player_send(f"I've been here at least {visit_total} times before.\n")
          visit_total = visit_total + 1
          debug_send(f"visit_total {visit_total}\n")

          try:
            cursor.execute('''
              UPDATE visit
                SET   VISIT_TOTAL = ?, VISIT_DATE = ?
                WHERE MAP_ID = ? AND PLAYER_ID = ?
            ''', (visit_total, visit_date, map_id, player_id))
            dbConn.commit()
          except:
            console_send(f"{e}\n")

      map_id = 0;
      map_line = -1;
      map_data = '';
      map_name = '';
      map_path = '';
      map_made = '';
      map_date = '';

  buffer = ''

player_send(f"Farewell, {player_title}\n")

vcConn.close()
dbConn.close()

# End ########################################################################

sys.exit(0)

