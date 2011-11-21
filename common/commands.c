const char * const rcsid_common_commands_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2001-2011 Mark Wedel & Crossfire Development Team

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

/**
 * @file common/commands.c
 * Handles server->client commands; See player.c for client->server commands.
 *
 * Not necessarily all commands are handled - some might be in other files
 * (like init.c)
 *
 * This file contains most of the commands for the dispatch loop. Most of the
 * functions are self-explanatory.
 *
 * pixmap/bitmap : receive the picture, and display it.
 * drawinfo      : draws a string in the info window.
 * stats         : updates the local copy of the stats and displays it.
 * handle_query  : prompts the user for input.
 * send_reply    : sends off the reply for the input.
 * player        : gets the player information.
 * MapScroll     : scrolls the map on the client by some amount.
 * MapCmd        : displays the map with layer packing or stack packing.
 *   packing/unpacking is best understood by looking at the server code
 *   (server/ericserver.c)
 *   stack packing: for every map entry that changed, we pack 1 byte for the
 *   x/y location, 1 byte for the count, and 2 bytes per face in the stack.
 *   layer packing is harder, but I seem to remember more efficient: first we
 *   pack in a list of all map cells that changed and are now empty.  The end
 *   of this list is a 255, which is bigger that 121, the maximum packed map
 *   location.
 *   For each changed location we also pack in a list of all the faces and X/Y
 *   coordinates by layer, where the layer is the depth in the map.  This
 *   essentially takes slices through the map rather than stacks.
 *   Then for each layer, (max is MAXMAPCELLFACES, a bad name) we start
 *   packing the layer into the message.  First we pack in a face, then for
 *   each place on the layer with the same face, we pack in the x/y location.
 *   We mark the last x/y location with the high bit on (11*11 = 121 < 128).
 *   We then continue on with the next face, which is why the code marks the
 *   faces as -1 if they are finished.  Finally we mark the last face in the
 *   layer again with the high bit, clearly limiting the total number of faces
 *   to 32767, the code comments it's 16384, I'm not clear why, but the second
 *   bit may be used somewhere else as well.
 *   The unpacking routines basically perform the opposite operations.
 */

int mapupdatesent = 0;

#include <client.h>
#include <external.h>
#include <assert.h>
#include <ctype.h>

#include "mapdata.h"

/* In general, the data from the server should not do bad
 * things like this, but checking for it makes it easier
 * to find bugs.  Often this is called within a loop
 * that of iterating over the length of the buffer, hence
 * the break.  Note that this may not prevent crashes,
 * but at least we generate a message.
 * Note that curpos & buflen may be string pointers or
 * may be integers - as long as both are the same
 * (both integers or both char *) it will work.
 */
#define ASSERT_LEN(function, curpos, buflen)     \
    if (curpos > buflen) { \
            LOG(LOG_WARNING, function, "Data goes beyond length of buffer (%d>%d)", curpos, buflen); \
            break; \
}

char *news=NULL, *motd=NULL, *rules=NULL;

int spellmon_level = 0;                 /**< Keeps track of what spellmon
                                         *   command is supported by the
                                         *   server. */
int num_races = 0;    /* Number of different races server has */
int used_races = 0;   /* How many races we have filled in */

int num_classes = 0;  /* Same as race data above, but for classes */
int used_classes = 0;

int stat_points = 0;    /* Number of stat points for new characters */
int stat_min = 0;       /* Minimum stat for new characters */
int stat_maximum = 0;   /* Maximum stat for new characters */
int starting_map_number = 0;   /* Number of starting maps */

Race_Class_Info *races=NULL, *classes=NULL;
Starting_Map_Info *starting_map_info = NULL;

/* Best I can tell, none of this stat information is stored anyplace
 * else in the server - MSW 2010-07-28
 */

#define NUM_STATS 7
/** Short name of stats. */
const char *const short_stat_name[NUM_STATS] = {
    "Str",    "Dex",    "Con",
    "Wis",    "Cha",    "Int",
    "Pow"
};

/* Note that the label_cs and label_rs will be in this same
 * order, eg, label_cs[0] will be strength, label_cs[1] will
 * be con.  However, this order can be changed, so it should
 * not be assumed that label_cs[1] will always be con.
 */
struct Stat_Mapping stat_mapping[NUM_NEW_CHAR_STATS] = {
    {"str", CS_STAT_STR, 0},
    {"con", CS_STAT_CON, 1},
    {"dex", CS_STAT_DEX, 2},
    {"int", CS_STAT_INT, 3},
    {"wis", CS_STAT_WIS, 4},
    {"pow", CS_STAT_POW, 5},
    {"cha", CS_STAT_CHA, 6}
};

/**
 * This function clears the data from the Race_Class_Info array.  Because the
 * structure itself contains data that is allocated, some work needs to be
 * done to clear that data.
 *
 */
void free_all_starting_map_info()
{
    int i;

    if (!starting_map_info) return;

    /* Because we are going free the array storage itself, there is no reason
     * to clear the data[i].. values.
     */
    for (i=0; i<starting_map_number; i++) {
        if (starting_map_info[i].arch_name) free(starting_map_info[i].arch_name);
        if (starting_map_info[i].public_name) free(starting_map_info[i].public_name);
        if (starting_map_info[i].description) free(starting_map_info[i].description);
    }

    free(starting_map_info);
    starting_map_info=NULL;
    starting_map_number = 0;
}

/**
 * This processes the replyinfo starting_map_info
 *
 * The data is a series of length prefixed lines.
 *
 * @param data
 * data returned from server.  Format is documented in protocol file.
 * @param len
 * length of data.
 */
static void get_starting_map_info(char *data, int len)
{
    int pos, type, length, map_entry=-1;
    char *cp;

    pos = 0;
    while (pos < len) {
        type = data[pos];
        pos++;

        /* Right now, all the data is length prefixed strings, so
         * the only real difference is where we store the data
         */

        length = GetShort_String(data + pos);
        pos += 2;

        if ((length+pos) > len) {
            LOG(LOG_WARNING, "common::get_starting_map_info", 
                "Length of data is greater than buffer (%d>%d)", length + pos, len);
            return;
        }

        cp = malloc(length+1);
        strncpy(cp, data+pos, length);
        cp[length] = 0;

        pos += length;

        /* If it is the arch name, it is a new entry, so we allocate
         * space and clear it.  This isn't most efficient, but at
         * the same time, I don't see there being many maps.
         * Note: If realloc is given a null pointer (which starting_map_info
         * will be after free or first load), realloc just acts as malloc.
         */
        if (type == INFO_MAP_ARCH_NAME) {
            map_entry++;
            starting_map_info = realloc(starting_map_info, 
                                        (map_entry + 1) * sizeof(Starting_Map_Info));
            memset(&starting_map_info[map_entry], 0, sizeof(Starting_Map_Info));
            starting_map_info[map_entry].arch_name = cp;
        } else if (type == INFO_MAP_NAME) {
            starting_map_info[map_entry].public_name = cp;
        } else if (type == INFO_MAP_DESCRIPTION) {
            starting_map_info[map_entry].description = cp;
        } else {
            /* Could be this is old client - but we can skip over
             * this bad data so long as the length byte is valid.
             */
            LOG(LOG_WARNING, "common::get_starting_map_info", 
                "Unknown type: %d\n", type);
        }
    }
    starting_map_number = map_entry;
    starting_map_update_info();
}

/**
 * This is process the newcharinfo requestinfo.
 * In some cases, it stores away the value, for others, it just
 * makes sure we understand them.
 *
 * The data is a series of length prefixed lines.
 *
 * @param data
 * data returned from server.  Format is documented in protocol file.
 * @param len
 * length of data.
 */
static void get_new_char_info(char *data, int len)
{
    int olen=0, llen;

    /* We reset these values - if the user is switching between
     * servers before restarting the client, these may have
     * different values.
     */
    stat_points = 0;
    stat_min = 0;
    stat_maximum = 0;

    while (olen < len) {
        char datatype, *cp;

        /* Where this line ends in the total buffer */
        llen = olen + GetChar_String(data + olen);

        /* By protocol convention, this should already be NULL,
         * but we ensure it is.  If the server has not included the
         * null byte, we are overwriting some real data here, but
         * the client will probably get an error at that point -
         * if the server is not following the protocol, we really
         * can't trust any of the data we get from it.
         */
        data[llen] = 0;

        if (llen > len) {
            LOG(LOG_WARNING, "common::get_new_char_info", 
                "Length of line is greater than buffer (%d>%d)", llen, len);
            return;
        }
        olen++;
        datatype = GetChar_String(data+olen); /* Type value */
        olen++;
        /* First skip all the spaces */
        while (olen <= len) {
            if (!isspace(data[olen])) break;
            olen++;
        }
        if (olen > len) {
            LOG(LOG_WARNING, "common::get_new_char_info", 
                "Overran length of buffer (%d>%d)", olen, len);
            return;
        }

        cp = data + olen;
        /* Go until we find another space */
        while (olen <= len) {
            if (isspace(data[olen])) break;
            olen++;
        }
        data[olen] = 0;    /* Null terminate the string */
        olen++;
        if (olen > len) {
            LOG(LOG_WARNING, "common::get_new_char_info", 
                "Overran length of buffer (%d>%d)", olen, len);
            return;
        }
        /* At this point, cp points to the string portion (variable name)
         * of the line, with data+olen is the start of the next string
         * (variable value).  
         */
        if (!strcasecmp(cp,"points")) {
            stat_points = atoi(data+olen);
            olen = llen + 1;
            continue;
        } else if (!strcasecmp(cp,"statrange")) {
            if (sscanf(data + olen, "%d %d", &stat_min, &stat_maximum)!=2) {
                LOG(LOG_WARNING, "common::get_new_char_info", 
                    "Unable to process statrange line (%s)", data + olen);
            }
            /* Either way, we go onto the next line */
            olen = llen + 1;
            continue;
        } else if (!strcasecmp(cp,"statname")) {
            /* The checking we do here is somewhat basic:
             * 1) That we understand all the stat names that the server sends us
             * 2) That we get the correct number of stats.
             * Note that if the server sends us the same stat name twice, eg
             * Str Str Dex Con ..., that will screw up this logic, but to a
             * great extent, we have to trust that server is sending us correct
             * information - sending the same stat twice does not follow that.
             */
            int i, matches=0;

            while (olen < llen) {
                for (i=0; i < NUM_STATS; i++) {
                    if (!strncasecmp(data + olen, short_stat_name[i], strlen(short_stat_name[i]))) {
                        matches++;
                        olen += strlen(short_stat_name[i]) + 1;
                        break;
                    }
                }
                if (i == NUM_STATS) {
                    LOG(LOG_WARNING, "common::get_new_char_info", 
                        "Unable to find matching stat name (%s)", data + olen);
                    break;
                }
            }
            if (matches != NUM_STATS) {
                LOG(LOG_WARNING, "common::get_new_char_info", 
                    "Did not get correct number of stats (%d!=%d)", matches, NUM_STATS);
            }
            olen = llen + 1;
            continue;
        } else if (!strcasecmp(cp,"race") || !strcasecmp(cp,"class")) {
            if (strcasecmp(data+olen, "requestinfo")) {
                LOG(LOG_WARNING, "common::get_new_char_info", 
                    "Got unexpected value for %s: %s", cp, data+olen);
            }
            olen = llen + 1;
            continue;
        } else if (!strcasecmp(cp,"startingmap")) {
            if (strcasecmp(data+olen, "requestinfo")) {
                LOG(LOG_WARNING, "common::get_new_char_info", 
                    "Got unexpected value for %s: %s", cp, data+olen);
            } else {
                cs_print_string(csocket.fd, "requestinfo startingmap");
                free_all_starting_map_info();
            }
            olen = llen + 1;
            continue;
        } else {
            if (datatype == 'V' || datatype == 'R') {
                LOG(LOG_WARNING, "common::get_new_char_info", 
                    "Got unsupported string from server, type %c, value %s", datatype, cp);
                /* pop up error here */
            } else {
                /* pop up warning here */
            }
            olen = llen + 1;
        }
    }
    if (stat_min == 0 || stat_maximum == 0 || stat_points == 0) {
        /* this needs to be handled better, but I'm not sure how -
         * we could fall back to legacy character creation mode,
         * but that will go away at some point - in a sense, if the
         * server is not sending us values, that is a broken/non comformant
         * server - best we could perhaps do is throw up a window saying
         * this client is not compatible with the server.
         */
        LOG(LOG_ERROR, "common::get_new_char_info",
            "Processed all newcharinfo yet have 0 value: stat_min=%d, stat_maximum=%d, stat_points=%d",
            stat_min, stat_maximum, stat_points);
    } else {
        new_char_window_update_info();
    }
}


/**
 * Used for bsearch searching.
 */
static int rc_compar(const Race_Class_Info *a, const Race_Class_Info *b) {
    return strcasecmp(a->public_name, b->public_name);
}

/**
 * This function clears the data from the Race_Class_Info array.  Because the
 * structure itself contains data that is allocated, some work needs to be
 * done to clear that data.
 *
 * @param data
 * array to clear
 * @param num_entries
 * size of the array.
 */
void free_all_race_class_info(Race_Class_Info *data, int num_entries)
{
    int i;

    /* Because we are going free the array storage itself, there is no reason
     * to clear the data[i].. values.
     */
    for (i=0; i<num_entries; i++) {
        int j;

        if (data[i].arch_name) free(data[i].arch_name);
        if (data[i].public_name) free(data[i].public_name);
        if (data[i].description) free(data[i].description);

        for (j=0; j<data[i].num_rc_choice; j++) {
            int k;

            for (k=0; k<data[i].rc_choice[j].num_values; k++) {
                free(data[i].rc_choice[j].value_arch[k]);
                free(data[i].rc_choice[j].value_desc[k]);
            }
            free(data[i].rc_choice[j].value_arch);
            free(data[i].rc_choice[j].value_desc);
            free(data[i].rc_choice[j].choice_name);
            free(data[i].rc_choice[j].choice_desc);
        }
    }

    free(data);
    data=NULL;
}

/**
 * This extracts the data from a replyinfo race_info/class_info request.  We
 * only get this data if the client has made a requestinfo of this data.
 *
 * @param data
 * data returned from server.  Format is documented in protocol file.
 * @param len
 * length of data
 * @param rci
 * Where to store the data.
 */
static void process_race_class_info(char *data, int len, Race_Class_Info *rci)
{
    char *cp, *nl;

    cp = data;

    /* First thing is to process the remaining bit of the requestinfo line,
     * which is the archetype name for this race/class
     */
    nl = strchr(cp, '\n');
    if (nl) {
        *nl=0;
        rci->arch_name = strdup(cp);
        cp = nl+1;
    } else {
        LOG(LOG_WARNING, "common::process_race_class_info", "Did not find archetype name");
        return;
    }

    /* Now we process the rest of the data - we look for a word the describes
     * the data to follow.  cp is a pointer to the data we are processing.  nl
     * is used to store temporary values.
     */
    do {
        nl = strchr(cp, ' ');
        /* If we did not find a space, may just mean we have reached the end
         * of the data - could be a stray character, etc
         */
        if (!nl) break;

        if (nl) {
            *nl = 0;
            nl++;
        }
        if (!strcmp(cp, "name")) {
            /* We get a name.  The string is not NULL terminated, but the
             * length is transmitted.  So get the length, allocate a string
             * large enough for that + NULL terminator, and copy string in,
             * making sure to put terminator in place.  also make sure we
             * update cp beyond this block of data.
             */
            int namelen;

            namelen = GetChar_String(nl);
            ASSERT_LEN("common::process_race_class_info", nl + namelen, data + len);
            nl++;
            rci->public_name = malloc(namelen+1);
            strncpy(rci->public_name, nl, namelen);
            rci->public_name[namelen] = 0;
            cp = nl + namelen;
        } else if (!strcmp(cp, "stats")) {
            cp = nl;
            /* This loop goes through the stat values - *cp points to the stat
             * value - if 0, no more stats, hence the check here.
             */
            while (cp < data + len && *cp != 0) {
                int i;

                for (i=0; i < NUM_NEW_CHAR_STATS; i++) 
                    if (stat_mapping[i].cs_value == *cp) break;

                if (i == NUM_NEW_CHAR_STATS) {
                    /* Just return with what we have */
                    LOG(LOG_WARNING, "common::process_race_class_info",
                        "Unknown stat value: %d", cp);
                    return;
                }
                rci->stat_adj[stat_mapping[i].rc_offset] = GetShort_String(cp+1);
                cp += 3;
            }
            cp++;   /* Skip over 0 terminator */
        } else if (!strcmp(cp, "msg")) {
            /* This is really exactly same as name processing above, except
             * length is 2 bytes in this case.
             */
            int msglen;

            msglen = GetShort_String(nl);
            ASSERT_LEN("common::process_race_class_info", nl + msglen, data + len);
            nl+=2;
            rci->description = malloc(msglen+1);
            strncpy(rci->description, nl, msglen);
            rci->description[msglen] = 0;
            cp = nl + msglen;
        } else if (!strcmp(cp, "choice")) {
            int oc = rci->num_rc_choice, clen;

            rci->num_rc_choice++;
            /* rc_choice may be null, but realloc still works there */
            rci->rc_choice = realloc(rci->rc_choice, sizeof(struct RC_Choice) * rci->num_rc_choice);
            memset(&rci->rc_choice[oc], 0, sizeof(struct RC_Choice));

            cp = nl;

            /* First is the coice string we return */
            clen = GetChar_String(cp);
            cp++;
            ASSERT_LEN("common::process_race_class_info", cp + clen, data + len);
            rci->rc_choice[oc].choice_name = malloc(clen+1);
            strncpy(rci->rc_choice[oc].choice_name, cp, clen);
            rci->rc_choice[oc].choice_name[clen] = 0;
            cp += clen;

            /* Next is the description */
            clen = GetChar_String(cp);
            cp++;
            ASSERT_LEN("common::process_race_class_info", cp + clen, data + len);
            rci->rc_choice[oc].choice_desc = malloc(clen+1);
            strncpy(rci->rc_choice[oc].choice_desc, cp, clen);
            rci->rc_choice[oc].choice_desc[clen] = 0;
            cp += clen;

            /* Now is a series of archetype/description pairs */
            while (1) {
                int vn;

                clen = GetChar_String(cp);
                cp++;
                if (!clen) break;    /* 0 length is end of data */
                vn = rci->rc_choice[oc].num_values;
                rci->rc_choice[oc].num_values++;
                rci->rc_choice[oc].value_arch = realloc(rci->rc_choice[oc].value_arch,
                                                        sizeof(char*) * rci->rc_choice[oc].num_values);
                rci->rc_choice[oc].value_desc = realloc(rci->rc_choice[oc].value_desc,
                                                        sizeof(char*) * rci->rc_choice[oc].num_values);

                ASSERT_LEN("common::process_race_class_info", cp + clen, data + len);
                rci->rc_choice[oc].value_arch[vn] = malloc(clen+1);
                strncpy(rci->rc_choice[oc].value_arch[vn], cp, clen);
                rci->rc_choice[oc].value_arch[vn][clen] = 0;
                cp += clen;
                
                clen = GetChar_String(cp);
                cp++;
                ASSERT_LEN("common::process_race_class_info", cp + clen, data + len);
                rci->rc_choice[oc].value_desc[vn] = malloc(clen+1);
                strncpy(rci->rc_choice[oc].value_desc[vn], cp, clen);
                rci->rc_choice[oc].value_desc[vn][clen] = 0;
                cp += clen;
            }
        } else {
            /* Got some keyword we did not understand.  Because we do not know
             * about it, we do not know how to skip it over - the data could
             * very well contain spaces or other markers we look for.
             */
            LOG(LOG_WARNING, "common::process_race_class_info", "Got unknown keyword: %s", cp);
            break;
        }
    } while (cp < data+len);

    /* The display code expects all of these to have a description -
     * rather than add checks there for NULL values, simpler to
     * just set things to an empty value.
     */
    if (!rci->description)
        rci->description = strdup("");

}

/**
 * This is a little wrapper function that does some bounds checking and then
 * calls process_race_info() to do the bulk of the work.
 *
 * @param data
 * data returned from server.  Format is documented in protocol file.
 * @param len
 * length of data.
 */
static void get_race_info(char *data, int len) {

    /* This should not happen - the client is only requesting race info for
     * races it has received - and it knows how many of those it has.
     */
    if (used_races >= num_races) {
        LOG(LOG_ERROR, "common::get_race_info",
                "used races exceed num races, %d>=%d", used_races, num_races);
        return;
    }

    process_race_class_info(data, len, &races[used_races]);
    used_races++;

    if (used_races == num_races) {
        qsort(races, used_races, sizeof(Race_Class_Info),
              (int (*)(const void *, const void *))rc_compar);

        new_char_window_update_info();
    }
}

/**
 * This is a little wrapper function that does some bounds checking and then
 * calls process_race_info() to do the bulk of the work.  Pretty much
 * identical to get_race_info() except this is for classes.
 *
 * @param data
 * data returned from server.  Format is documented in protocol file.
 * @param len
 * length of data.
 */
static void get_class_info(char *data, int len) {

    /* This should not happen - the client is only requesting race info for
     * classes it has received - and it knows how many of those it has.
     */
    if (used_classes >= num_classes) {
        LOG(LOG_ERROR, "common::get_race_info",
                "used classes exceed num classes, %d>=%d", used_classes, num_classes);
        return;
    }

    process_race_class_info(data, len, &classes[used_classes]);
    used_classes++;

    if (used_classes == num_classes) {
        qsort(classes, used_classes, sizeof(Race_Class_Info),
              (int (*)(const void *, const void *))rc_compar);

        new_char_window_update_info();
    }
}

/**
 *
 * @param data
 * @param len
 */
static void get_exp_info(const unsigned char *data, int len) {
    int pos, level;

    if (len < 2) {
        LOG(LOG_ERROR, "common::get_exp_info", "no max level info from server provided");
        return;
    }

    exp_table_max = GetShort_String(data);
    pos = 2;
    exp_table = calloc(exp_table_max, sizeof(uint64));
    for (level = 1; level <= exp_table_max && pos < len; level++) {
        exp_table[level] = GetInt64_String(data+pos);
        pos += 8;
    }
    if (level != exp_table_max) {
        LOG(LOG_ERROR, "common::get_exp_info",
             "Incomplete table sent - got %d entries, wanted %d", level, exp_table_max);
    }
}

/**
 *
 * @param data
 * @param len
 */
static void get_skill_info(char *data, int len) {
    char *cp, *nl, *sn;
    int val;

    cp = data;
    do {
        nl = strchr(cp, '\n');
        if (nl) {
            *nl = 0;
            nl++;
        }
        sn = strchr(cp, ':');
        if (!sn) {
            LOG(LOG_WARNING, "common::get_skill_info", "corrupt line: /%s/", cp);
            return;
        }

        *sn = 0;
        sn++;
        val = atoi(cp);
        val -= CS_STAT_SKILLINFO;

        /* skill_names[MAX_SKILL] is the declaration, so check against that */
        if (val < 0 || val >= MAX_SKILL) {
            LOG(LOG_WARNING, "common::get_skill_info", "invalid skill number %d", val);
            return;
        }

        free(skill_names[val]);
        skill_names[val] = strdup_local(sn);
        cp = nl;
    } while (cp < data+len);
}

/**
 * Handles the response from a 'requestinfo' command.  This function doesn't
 * do much itself other than dispatch to other functions.
 *
 * @param buf
 * @param len
 */
void ReplyInfoCmd(uint8 *buf, int len) {
    uint8 *cp;
    int i;

    /* Covers a bug in the server in that it could send a replyinfo with no
     * parameters
     */
    if (!buf) {
        return;
    }

    for (i = 0; i < len; i++) {
        /* Either a space or newline represents a break */
        if (*(buf+i) == ' ' || *(buf+i) == '\n') {
            break;
        }
    }
    if (i >= len) {
        /* Don't print buf, as it may contain binary data */
        /* Downgrade this to DEBUG - if the client issued an unsupported
         * requestinfo info to the server, we'll end up here - this could be
         * normal behaviour
         */
        LOG(LOG_DEBUG, "common::ReplyInfoCmd", "Never found a space in the replyinfo");
        return;
    }

    /* Null out the space and put cp beyond it */
    cp = buf+i;
    *cp++ = '\0';
    if (!strcmp((char*)buf, "image_info")) {
        get_image_info(cp, len-i-1);        /* Located in common/image.c */
    } else if (!strcmp((char*)buf, "image_sums")) {
        get_image_sums((char*)cp, len-i-1); /* Located in common/image.c */
    } else if (!strcmp((char*)buf, "skill_info")) {
        get_skill_info((char*)cp, len-i-1); /* Located in common/commands.c */
    } else if (!strcmp((char*)buf, "exp_table")) {
        get_exp_info(cp, len-i-1);          /* Located in common/commands.c */
    } else if (!strcmp((char*)buf, "motd")) {
        if (motd) free((char*)motd);
        motd = strdup(cp);
        update_login_info(INFO_MOTD);
    } else if (!strcmp((char*)buf, "news")) {
        if (news) free((char*)news);
        news = strdup(cp);
        update_login_info(INFO_NEWS);
    } else if (!strcmp((char*)buf, "rules")) {
        if (rules) free((char*)rules);
        rules = strdup(cp);
        update_login_info(INFO_RULES);
    } else if (!strcmp((char*)buf, "race_list")) {
        char *cp1;
        for (cp1=cp; *cp !=0; cp++) {
            if (*cp == '|') {
                *cp++ = '\0';
                /* The first separator has no data, so only send request to
                 * server if this is not null.
                 */
                if (*cp1!='\0') {
                    cs_print_string(csocket.fd, "requestinfo race_info %s", cp1);
                    num_races++;
                }
                cp1 = cp;
            }
        }
        if (races) {
            free_all_race_class_info(races, num_races);
            num_races=0;
            used_races=0;
        }
        races = calloc(num_races, sizeof(Race_Class_Info));

    }  else if (!strcmp((char*)buf, "class_list")) {
        char *cp1;
        for (cp1=cp; *cp !=0; cp++) {
            if (*cp == '|') {
                *cp++ = '\0';
                /* The first separator has no data, so only send request to
                 * server if this is not null.
                 */
                if (*cp1!='\0') {
                    cs_print_string(csocket.fd, "requestinfo class_info %s", cp1);
                    num_classes++;
                }
                cp1 = cp;
            }
        }
        if (classes) {
            free_all_race_class_info(classes, num_classes);
            num_classes=0;
            used_classes=0;
        }
        classes = calloc(num_classes, sizeof(Race_Class_Info));
    }  else if (!strcmp((char*)buf, "race_info")) {
        get_race_info(cp, len -i -1);
    } else if (!strcmp((char*)buf, "class_info")) {
        get_class_info(cp, len -i -1);
    } else if (!strcmp((char*)buf, "newcharinfo")) {
        get_new_char_info(cp, len -i -1);
    } else if (!strcmp((char*)buf, "startingmap")) {
        get_starting_map_info(cp, len -i -1);
    }
}

/**
 * Received a response to a setup from the server.  This function is basically
 * the same as the server side function - we just do some different processing
 * on the data.
 *
 * @param buf
 * @param len
 */
void SetupCmd(char *buf, int len) {
    int s;
    char *cmd, *param;

    /* Process the setup commands.
     * Syntax is setup <cmdname1> <parameter> <cmdname2> <parameter> ...
     *
     * The server sends the status of the cmd back, or a FALSE if the cmd is
     * unknown.  The client then must sort this out.
     */

    LOG(LOG_DEBUG, "common::SetupCmd", "%s", buf);
    for (s = 0; ; ) {
        if (s >= len) { /* Ugly, but for secure...*/
            break;
        }

        cmd = &buf[s];

        /* Find the next space, and put a null there */
        for (; buf[s] && buf[s] != ' '; s++)
            ;
        buf[s++] = 0;
        while (buf[s] == ' ') {
            s++;
        }
        if (s >= len) {
            break;
        }

        param = &buf[s];

        for (; buf[s] && buf[s] != ' '; s++)
            ;
        buf[s++] = 0;
        while (s < len && buf[s] == ' ') {
            s++;
        }

        /* What is done with the returned data depends on what the server
         * returns.  In some cases the client may fall back to other methods,
         * report an error, or try another setup command.
         */
        if (!strcmp(cmd, "sound2")) {
            /* No parsing needed, but we don't want a warning about unknown
             * setup option below.
             */
        } else if (!strcmp(cmd, "sound")) {
            /* No, this should not be !strcmp()... */
        } else if (!strcmp(cmd, "mapsize")) {
            int x, y = 0;
            char *cp, tmpbuf[MAX_BUF];

            if (!strcasecmp(param, "false")) {
                draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SERVER,
                  "Server only supports standard sized maps (11x11)");
                /* Do this because we may have been playing on a big server
                 * before */
                use_config[CONFIG_MAPWIDTH] = 11;
                use_config[CONFIG_MAPHEIGHT] = 11;
                mapdata_set_size(use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
                resize_map_window(use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
                continue;
            }
            x = atoi(param);
            for (cp = param; *cp != 0; cp++) {
                if (*cp == 'x' || *cp == 'X') {
                    y = atoi(cp+1);
                    break;
                }
            }
            /* A size larger than what the server supports was requested.
             * Reduce the size to server maximum, and re-send the setup
             * command.  Update our want sizes, and tell the player what is
             * going on.
             */
            if (use_config[CONFIG_MAPWIDTH] > x || use_config[CONFIG_MAPHEIGHT] > y) {
                if (use_config[CONFIG_MAPWIDTH] > x) use_config[CONFIG_MAPWIDTH] = x;
                if (use_config[CONFIG_MAPHEIGHT] > y) use_config[CONFIG_MAPHEIGHT] = y;
                mapdata_set_size(use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
                cs_print_string(csocket.fd,
                                "setup mapsize %dx%d", use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
                snprintf(tmpbuf, sizeof(tmpbuf), "Server supports a max mapsize of %d x %d - requesting a %d x %d mapsize",
                    x, y, use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
                draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SERVER,
                    tmpbuf);
            } else if (use_config[CONFIG_MAPWIDTH] == x && use_config[CONFIG_MAPHEIGHT] == y) {
                mapdata_set_size(use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
                resize_map_window(use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT]);
            } else {
                /* The request was not bigger than what server supports, and
                 * not the same size, so what is the problem?  Tell the user
                 * that something is wrong.
                 */
                snprintf(tmpbuf, sizeof(tmpbuf), "Unable to set mapsize on server - we wanted %d x %d, server returned %d x %d",
                    use_config[CONFIG_MAPWIDTH], use_config[CONFIG_MAPHEIGHT], x, y);
                draw_ext_info(
                    NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SERVER, tmpbuf);
            }
        } else if (!strcmp(cmd, "darkness")) {
            /* Older servers might not support this setup command.
             */
            if (!strcmp(param, "FALSE")) {
                LOG(LOG_WARNING, "common::SetupCmd", "Server returned FALSE for setup command %s", cmd);
            }
        } else if (!strcmp(cmd, "spellmon")) {

            /* Older servers might not support this setup command or all of
             * the extensions.
             *
             * Spellmon 2 was added to the protocol in January 2010 to send an
             * additional spell information string with casting requirements
             * including required items, if the spell needs arguments passed
             * (like text for rune of marking), etc.
             *
             * To use the new feature, "setup spellmon 1 spellmon 2" is sent,
             * and if "spellmon 1 spellmon FALSE" is returned then the server
             * doesn't accept 2 - sending spellmon 2 to a server that does not
             * support it is not problematic, so the spellmon 1 command will
             * still be handled correctly by the server.  If the server sends
             * "spellmon 1 spellmon 2" then the extended mode is in effect.
             *
             * It is not particularly important for the player to know what
             * level of command is accepted by the server.  The extra features
             * will simply not be functionally available.
             */
            if (!strcmp(param, "FALSE")) {
                LOG(LOG_INFO, "common::SetupCmd", "Server returned FALSE for a %s setup command", cmd);
            } else {
                spellmon_level = atoi(param);
            }
        } else if (!strcmp(cmd, "facecache")) {
                use_config[CONFIG_CACHE] = atoi(param);
        } else if (!strcmp(cmd, "faceset")) {
            if (!strcmp(param, "FALSE")) {
                draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SERVER,
                    "Server does not support other image sets, will use default");
                face_info.faceset = 0;
            }
        } else if (!strcmp(cmd, "map2cmd")) {
            if (!strcmp(param, "FALSE")) {
                draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SERVER,
                    "Server does not support map2cmd!");
                draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SERVER,
                    "This server is too old to support this client!");
                close_server_connection();
            }
        } else if (!strcmp(cmd, "want_pickup")) {
            /* Nothing special to do as this is info pushed from server and
             * not having it isn't that bad.
             */
        } else if (!strcmp(cmd, "loginmethod")) {
            int method = atoi(param);

            /* If the server supports new login, start the process.  Pass what
             * version the server supports so client can do appropriate
             * work
             */
            if (method) {
                start_login(method);
            }
        } else {
            LOG(LOG_INFO, "common::SetupCmd", "Got setup for a command we don't understand: %s %s",
                cmd, param);
        }
    }
}

/**
 * Handles when the server says we can't be added.  In reality, we need to
 * close the connection and quit out, because the client is going to close us
 * down anyways.
 *
 * @param data
 * @param len
 */
void AddMeFail(char *data, int len) {
    (void)data; /* __UNUSED__ */
    (void)len; /* __UNUSED__ */

    LOG(LOG_INFO, "common::AddMeFail", "addme_failed received.");
    return;
}

/**
 * This is really a throwaway command - there really isn't any reason to send
 * addme_success commands.
 *
 * @param data
 * @param len
 */
void AddMeSuccess(char *data, int len) {
    (void)data; /* __UNUSED__ */
    (void)len; /* __UNUSED__ */

    hide_all_login_windows();
    LOG(LOG_INFO, "common::AddMeSuccess", "addme_success received.");
    return;
}

/**
 *
 * @param data
 * @param len
 */
void GoodbyeCmd(char *data, int len) {
    (void)data; /* __UNUSED__ */
    (void)len; /* __UNUSED__ */

    /* This could probably be greatly improved - I am not sure if anything
     * needs to be saved here, but it should be possible to reconnect to the
     * server or a different server without having to rerun the client.
     */
    LOG(LOG_WARNING, "common::GoodbyeCmd", "Received goodbye command from server - exiting");
    exit(0);
}

Animations animations[MAXANIM];

/**
 *
 * @param data
 * @param len
 */
void AnimCmd(unsigned char *data, int len) {
    short anum;
    int i, j;

    anum = GetShort_String(data);
    if (anum < 0 || anum > MAXANIM) {
        LOG(LOG_WARNING, "common::AnimCmd", "animation number invalid: %d", anum);
        return;
    }

    animations[anum].flags = GetShort_String(data+2);
    animations[anum].num_animations = (len-4)/2;
    if (animations[anum].num_animations < 1) {
        LOG(LOG_WARNING, "common::AnimCmd", "num animations invalid: %d",
            animations[anum].num_animations);
        return;
    }
    animations[anum].faces = malloc(sizeof(uint16)*animations[anum].num_animations);
    for (i = 4, j = 0; i < len; i += 2, j++) {
        animations[anum].faces[j] = GetShort_String(data+i);
    }

    if (j != animations[anum].num_animations) {
        LOG(LOG_WARNING, "common::AnimCmd",
            "Calculated animations does not equal stored animations? (%d!=%d)",
            j, animations[anum].num_animations);
    }

    animations[anum].speed = 0;
    animations[anum].speed_left = 0;
    animations[anum].phase = 0;

    LOG(LOG_DEBUG, "common::AnimCmd", "Received animation %d, %d faces", anum, animations[anum].num_animations);
}

/**
 * Receives the smooth mapping from the server.  Because this information is
 * reference a lot, the smoothing face is stored in the pixmap data - this
 * makes access much faster than searching an array of data for the face to
 * use.
 *
 * @param data
 * @param len
 */
void SmoothCmd(unsigned char *data, int len) {
    uint16 faceid;
    uint16 smoothing;

    /* len is unused.  We should check that we don't have an invalid short
     * command.  Hence, the compiler warning is valid.
     */

    faceid = GetShort_String(data);
    smoothing = GetShort_String(data+2);
    addsmooth(faceid, smoothing);
}

/**
 * Draws a string in the info window.
 *
 * @param data
 * @param len
 */
void DrawInfoCmd(char *data, int len) {
    int color = atoi(data);
    char *buf;

    (void)len; /* __UNUSED__ */

    buf = strchr(data, ' ');
    if (!buf) {
        LOG(LOG_WARNING, "common::DrawInfoCmd", "got no data");
        buf = "";
    } else {
        buf++;
    }
    draw_ext_info(color, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_COMMAND, buf);
}

TextManager *firstTextManager = NULL;

/**
 *
 * @param type
 * @param callback
 */
void setTextManager(int type, ExtTextManager callback) {
    TextManager *current = firstTextManager;

    while (current != NULL) {
        if (current->type == type) {
            current->callback = callback;
            return;
        }
        current = current->next;
    }
    current = malloc(sizeof(TextManager));
    current->type = type;
    current->callback = callback;
    current->next = firstTextManager;
    firstTextManager = current;
}

/**
 *
 * @param type
 */
static ExtTextManager getTextManager(int type) {
    TextManager *current = firstTextManager;
    while (current != NULL) {
        if (current->type == type) {
            return current->callback;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * We must extract color, type, subtype and dispatch to callback
 *
 * @param data
 * @param len
 */
void DrawExtInfoCmd(char *data, int len) {
    int color;
    int type, subtype;
    char *buf = data;
    int wordCount = 3;
    ExtTextManager fnct;

    while (wordCount > 0) {
        while (buf[0] == ' ') {
            buf++;
        }
        wordCount--;
        while (buf[0] != ' ') {
            if (buf[0] == '\0') {
                LOG(LOG_WARNING,
                    "common::DrawExtInfoCmd", "Data is missing %d parameters %s",
                    wordCount,
                    data);
                return;
            } else {
                buf++;
            }
        }
        if (buf[0] == ' ') {
            buf++; /*remove trailing space to send clean data to callback */
        }
    }
    wordCount = sscanf(data, "%d %d %d", &color, &type, &subtype);
    if (wordCount != 3) {
        LOG(LOG_WARNING,
            "common::DrawExtInfoCmd", "Wrong parameters received. Could only parse %d out of 3 int in %s",
            wordCount,
            data);
        return;
    }
    fnct = getTextManager(type);
    if (fnct == NULL) {
        LOG(LOG_WARNING,
            "common::DrawExtInfoCmd", "Server send us a type %d but i can't find any callback for it",
            type);
        return;
    }
    fnct(color, type, subtype, buf);
}

/**
 * Maintain the last_used_skills LRU list for displaying the recently used
 * skills first.
 *
 * @param skill_id
 */
void use_skill(int skill_id)
{
   int i = 0;
   int next;
   int prev = last_used_skills[0];

   if(last_used_skills[0] == skill_id) return;

   do
   {
       next = last_used_skills[i+1];
       last_used_skills[i+1] = prev;
       prev = next;
       ++i;
   } while(next != skill_id && next >= 0);
   last_used_skills[0] = skill_id;
}

/**
 * Updates the local copy of the stats and displays it.
 *
 * @param data
 * @param len
 */
void StatsCmd(unsigned char *data, int len) {
    int i = 0, c, redraw = 0;
    sint64 last_exp;

    while (i < len) {
        c = data[i++];
        if (c >= CS_STAT_RESIST_START && c <= CS_STAT_RESIST_END) {
            cpl.stats.resists[c-CS_STAT_RESIST_START] = GetShort_String(data+i);
            i += 2;
            cpl.stats.resist_change = 1;
        } else if (c >= CS_STAT_SKILLINFO && c < (CS_STAT_SKILLINFO+CS_NUM_SKILLS)) {
            /* We track to see if the exp has gone from 0 to some total value
             * - we do this because the draw logic currently only draws skills
             * where the player has exp.  We need to communicate to the draw
             * function that it should draw all the players skills.  Using
             * redraw is a little overkill, because a lot of the data may not
             * be changing.  OTOH, such a transition should only happen
             * rarely, not not be a very big deal.
             */
            cpl.stats.skill_level[c-CS_STAT_SKILLINFO] = data[i++];
            last_exp = cpl.stats.skill_exp[c-CS_STAT_SKILLINFO];
            cpl.stats.skill_exp[c-CS_STAT_SKILLINFO] = GetInt64_String(data+i);
            use_skill(c-CS_STAT_SKILLINFO);
            if (last_exp == 0 && cpl.stats.skill_exp[c-CS_STAT_SKILLINFO]) {
                redraw = 1;
            }
            i += 8;
        } else {
            switch (c) {
            case CS_STAT_HP:      cpl.stats.hp = GetShort_String(data+i); i += 2; break;
            case CS_STAT_MAXHP:   cpl.stats.maxhp = GetShort_String(data+i); i += 2; break;
            case CS_STAT_SP:      cpl.stats.sp = GetShort_String(data+i); i += 2; break;
            case CS_STAT_MAXSP:   cpl.stats.maxsp = GetShort_String(data+i); i += 2; break;
            case CS_STAT_GRACE:   cpl.stats.grace = GetShort_String(data+i); i += 2; break;
            case CS_STAT_MAXGRACE:cpl.stats.maxgrace = GetShort_String(data+i); i += 2; break;
            case CS_STAT_STR:     cpl.stats.Str = GetShort_String(data+i); i += 2; break;
            case CS_STAT_INT:     cpl.stats.Int = GetShort_String(data+i); i += 2; break;
            case CS_STAT_POW:     cpl.stats.Pow = GetShort_String(data+i); i += 2; break;
            case CS_STAT_WIS:     cpl.stats.Wis = GetShort_String(data+i); i += 2; break;
            case CS_STAT_DEX:     cpl.stats.Dex = GetShort_String(data+i); i += 2; break;
            case CS_STAT_CON:     cpl.stats.Con = GetShort_String(data+i); i += 2; break;
            case CS_STAT_CHA:     cpl.stats.Cha = GetShort_String(data+i); i += 2; break;
            case CS_STAT_EXP:     cpl.stats.exp = GetInt_String(data+i); i += 4; break;
            case CS_STAT_EXP64:   cpl.stats.exp = GetInt64_String(data+i); i += 8; break;
            case CS_STAT_LEVEL:   cpl.stats.level = GetShort_String(data+i); i += 2; break;
            case CS_STAT_WC:      cpl.stats.wc = GetShort_String(data+i); i += 2; break;
            case CS_STAT_AC:      cpl.stats.ac = GetShort_String(data+i); i += 2; break;
            case CS_STAT_DAM:     cpl.stats.dam = GetShort_String(data+i); i += 2; break;
            case CS_STAT_ARMOUR:  cpl.stats.resists[0] = GetShort_String(data+i); i += 2; break;
            case CS_STAT_SPEED:   cpl.stats.speed = GetInt_String(data+i); i += 4; break;
            case CS_STAT_FOOD:    cpl.stats.food = GetShort_String(data+i); i += 2; break;
            case CS_STAT_WEAP_SP: cpl.stats.weapon_sp = GetInt_String(data+i); i += 4; break;
            case CS_STAT_SPELL_ATTUNE:cpl.stats.attuned = GetInt_String(data+i); i += 4; cpl.spells_updated = 1; break;
            case CS_STAT_SPELL_REPEL:cpl.stats.repelled = GetInt_String(data+i); i += 4; cpl.spells_updated = 1; break;
            case CS_STAT_SPELL_DENY:cpl.stats.denied = GetInt_String(data+i); i += 4; cpl.spells_updated = 1; break;

            case CS_STAT_FLAGS: cpl.stats.flags = GetShort_String(data+i); i += 2; break;
            case CS_STAT_WEIGHT_LIM:set_weight_limit(cpl.stats.weight_limit = GetInt_String(data+i)); i += 4; break;

            case CS_STAT_RANGE: {
                int rlen = data[i++];
                strncpy(cpl.range, (const char*)data+i, rlen);
                cpl.range[rlen] = '\0';
                i += rlen;
                break;
            }

            case CS_STAT_TITLE: {
                int rlen = data[i++];
                strncpy(cpl.title, (const char*)data+i, rlen);
                cpl.title[rlen] = '\0';
                i += rlen;
                break;
            }

            default:
                LOG(LOG_WARNING, "common::StatsCmd", "Unknown stat number %d", c);
                break;
            }
        }
    }

    if (i > len) {
        LOG(LOG_WARNING, "common::StatsCmd", "got stats overflow, processed %d bytes out of %d", i, len);
    }
    draw_stats(redraw);
    draw_message_window(0);
#ifdef HAVE_LUA
    script_lua_stats();
#endif
}

/**
 * Prompts the user for input.
 *
 * @param data
 * @param len
 */
void handle_query(char *data, int len) {
    char *buf, *cp;
    uint8 flags = atoi(data);

    (void)len; /* __UNUSED__ */

    if (flags&CS_QUERY_HIDEINPUT) { /* No echo */
        cpl.no_echo = 1;
    } else {
        cpl.no_echo = 0;
    }

    /* Let the window system know this may have changed */
    x_set_echo();

    /* The actual text is optional */
    buf = strchr(data, ' ');
    if (buf) {
        buf++;
    }

    /* If we just get passed an empty string, why draw this? */
    if (buf) {
        cp = buf;
        while ((buf = strchr(buf, '\n')) != NULL) {
            *buf++ = '\0';
            draw_ext_info(
                NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_QUERY, cp);
            cp = buf;
        }
        /* Yes/no - don't do anything with it now */
        if (flags&CS_QUERY_YESNO) {
        }

        /* One character response expected */
        if (flags&CS_QUERY_SINGLECHAR) {
            cpl.input_state = Reply_One;
        } else {
            cpl.input_state = Reply_Many;
        }

        if (cp) {
            draw_prompt(cp);
        }
    }

    LOG(LOG_DEBUG, "common::handle_query", "Received query.  Input state now %d", cpl.input_state);
}

/**
 * Sends a reply to the server.  This function basically just packs the stuff
 * up.
 *
 * @param text contains the null terminated string of text to send.
 */
void send_reply(const char *text) {
    cs_print_string(csocket.fd, "reply %s", text);

    /* Let the window system know that the (possibly hidden) query is over. */
    cpl.no_echo = 0;
    x_set_echo();
}

/**
 * Gets the player information.  This function copies relevant data from the
 * archetype to the object.  Only copies data that was not set in the object
 * structure.
 *
 * @param data
 * @param len
 */
void PlayerCmd(unsigned char *data, int len) {
    char name[MAX_BUF];
    int tag, weight, face, i = 0, nlen;

    reset_player_data();
    tag = GetInt_String(data); i += 4;
    weight = GetInt_String(data+i); i += 4;
    face = GetInt_String(data+i); i += 4;
    nlen = data[i++];
    memcpy(name, (const char*)data+i, nlen);
    name[nlen] = '\0';
    i += nlen;

    if (i != len) {
        LOG(LOG_WARNING, "common::PlayerCmd", "lengths do not match (%d!=%d)", len, i);
    }
    new_player(tag, name, weight, face);
}

/**
 *
 * @param op
 */
void item_actions(item *op) {
    if (!op) {
        return;
    }

    if (op->open) {
        open_container(op);
        cpl.container = op;
    } else if (op->was_open) {
        close_container(op);
        cpl.container = NULL;
    }
}

/**
 * Parses the data sent to us from the server.  revision is what item command
 * the data came from - newer ones have addition fields.
 *
 * @param data
 * @param len
 */
static void common_item_command(uint8 *data, int len) {

    int weight, loc, tag, face, flags, pos = 0, nlen, anim, nrof, type;
    uint8 animspeed;
    char name[MAX_BUF];

    loc = GetInt_String(data);
    pos += 4;

    if (pos == len) {
        LOG(LOG_WARNING, "common::common_item_command", "Got location with no other data");
        return;
    } else if (loc < 0) { /* Delete following items */
        LOG(LOG_WARNING, "common::common_item_command", "Got location with negative value (%d)", loc);
        return;
    } else {
        while (pos < len) {
            tag = GetInt_String(data+pos); pos += 4;
            flags = GetInt_String(data+pos); pos += 4;
            weight = GetInt_String(data+pos); pos += 4;
            face = GetInt_String(data+pos); pos += 4;
            nlen = data[pos++];
            memcpy(name, (char*)data+pos, nlen);
            pos += nlen;
            name[nlen] = '\0';
            anim = GetShort_String(data+pos); pos += 2;
            animspeed = data[pos++];
            nrof = GetInt_String(data+pos); pos += 4;
            type = GetShort_String(data+pos); pos += 2;
            update_item(tag, loc, name, weight, face, flags, anim, animspeed, nrof, type);
            item_actions(locate_item(tag));
        }
        if (pos > len) {
            LOG(LOG_WARNING, "common::common_item_cmd", "Overread buffer: %d > %d", pos, len);
        }
    }
}

/**
 *
 * @param data
 * @param len
 */
void Item2Cmd(unsigned char *data, int len) {
    common_item_command(data, len);
}

/**
 * Updates some attributes of an item
 *
 * @param data
 * @param len
 */
void UpdateItemCmd(unsigned char *data, int len) {
    int weight, loc, tag, face, sendflags, flags, pos = 0, nlen, anim;
    uint32 nrof;
    char name[MAX_BUF];
    item *ip, *env = NULL;
    uint8 animspeed;

    sendflags = data[0];
    pos += 1;
    tag = GetInt_String(data+pos);
    pos += 4;
    ip = locate_item(tag);
    if (!ip) {
/*
        fprintf(stderr, "Got update_item command for item we don't have (%d)\n", tag);
*/
        return;
    }

    /* Copy all of these so we can pass the values to update_item and don't
     * need to figure out which ones were modified by this function.
     */
    *name = '\0';
    loc = ip->env ? ip->env->tag : 0;
    weight = ip->weight*1000;
    face = ip->face;
    flags = ip->flagsval;
    anim = ip->animation_id;
    animspeed = ip->anim_speed;
    nrof = ip->nrof;

    if (sendflags&UPD_LOCATION) {
        loc = GetInt_String(data+pos);
        env = locate_item(loc);
        LOG(LOG_WARNING, "common::UpdateItemCmd", "Got tag of unknown object (%d) for new location", loc);
        pos += 4;
    }
    if (sendflags&UPD_FLAGS) {
        flags = GetInt_String(data+pos);
        pos += 4;
    }
    if (sendflags&UPD_WEIGHT) {
        weight = GetInt_String(data+pos);
        pos += 4;
    }
    if (sendflags&UPD_FACE) {
        face = GetInt_String(data+pos);
        pos += 4;
    }
    if (sendflags&UPD_NAME) {
        nlen = data[pos++];
        memcpy(name, (char*)data+pos, nlen);
        pos += nlen;
        name[nlen] = '\0';
    }
    if (pos > len) {
        LOG(LOG_WARNING, "common::UpdateItemCmd", "Overread buffer: %d > %d", pos, len);
        return; /* We have bad data, probably don't want to store it then */
    }
    if (sendflags&UPD_ANIM) {
        anim = GetShort_String(data+pos);
        pos += 2;
    }
    if (sendflags&UPD_ANIMSPEED) {
        animspeed = data[pos++];
    }
    if (sendflags&UPD_NROF) {
        nrof = (uint32)GetInt_String(data+pos);
        pos += 4;
    }
    /* update_item calls set_item_values which will then set the list redraw
     * flag, so we don't need to do an explicit redraw here.  Actually,
     * calling update_item is a little bit of overkill, since we already
     * determined some of the values in this function.
     */
    update_item(tag, loc, name, weight, face, flags, anim, animspeed, nrof, ip->type);
    item_actions(locate_item(tag));
}

/**
 *
 * @param data
 * @param len
 */
void DeleteItem(unsigned char *data, int len) {
    int pos = 0, tag;

    while (pos < len) {
        item *op;

        tag = GetInt_String(data+pos); pos += 4;
        op = locate_item(tag);
        if (op != NULL) {
            remove_item(op);
        } else {
            LOG(LOG_WARNING, "common::DeleteItem", "Cannot find tag %d", tag);
        }
    }
    if (pos > len) {
        LOG(LOG_WARNING, "common::DeleteItem", "Overread buffer: %d > %d", pos, len);
    }
}

/**
 *
 * @param data
 * @param len
 */
void DeleteInventory(unsigned char *data, int len) {
    int tag;
    item *op;

    (void)len; /* __UNUSED__ */

    tag = atoi((const char*)data);
    op = locate_item(tag);
    if (op != NULL) {
        remove_item_inventory(op);
    } else {
        LOG(LOG_WARNING, "common::DeleteInventory", "Invalid tag: %d", tag);
    }
}

/****************************************************************************/

/**
 * @defgroup SCSpellCommands Server->Client spell command functions.
 * @{
 */

/**
 *
 * @param data
 * @param len
 */
void AddspellCmd(unsigned char *data, int len) {
    uint8 nlen;
    uint16 mlen, pos = 0;
    Spell *newspell, *tmp;

    while (pos < len) {
        newspell = calloc(1, sizeof(Spell));

        /* Get standard spell information (spellmon 1)
         */
        newspell->tag = GetInt_String(data+pos); pos += 4;
        newspell->level = GetShort_String(data+pos); pos += 2;
        newspell->time = GetShort_String(data+pos); pos += 2;
        newspell->sp = GetShort_String(data+pos); pos += 2;
        newspell->grace = GetShort_String(data+pos); pos += 2;
        newspell->dam = GetShort_String(data+pos); pos += 2;
        newspell->skill_number = GetChar_String(data+pos); pos += 1;
        newspell->path = GetInt_String(data+pos); pos += 4;
        newspell->face = GetInt_String(data+pos); pos += 4;
        nlen = GetChar_String(data+pos); pos += 1;
        strncpy(newspell->name, (char*)data+pos, nlen); pos += nlen;
        newspell->name[nlen] = '\0'; /* To ensure we are null terminated */
        mlen = GetShort_String(data+pos); pos += 2;
        strncpy(newspell->message, (char*)data+pos, mlen); pos += mlen;
        newspell->message[mlen] = '\0'; /* To ensure we are null terminated */

        if (spellmon_level < 2) {

          /* The server is not sending spellmon 2 extended information, so
           * initialize the spell data fields as unused/empty.
           */
          newspell->usage = 0;
          newspell->requirements[0] = '\0';

        } else if (pos < len) {

          /* The server is sending extended spell information (spellmon 2) so
           * process it.
           */
          newspell->usage = GetChar_String(data+pos); pos += 1;
          nlen = GetChar_String(data+pos); pos += 1;
          strncpy(newspell->requirements, (char*) data+pos, nlen); pos += nlen;
          newspell->requirements[nlen] = '\0'; /* Ensure null-termination */
        }

        /* Compute the derived spell information.
         */
        newspell->skill = skill_names[newspell->skill_number-CS_STAT_SKILLINFO];

        /* Add the spell to the player struct.
         */
        if (!cpl.spelldata) {
            cpl.spelldata = newspell;
        } else {
            for (tmp = cpl.spelldata; tmp->next; tmp = tmp->next)
                ;
            tmp->next = newspell;
        }
        /* Check to see if there are more spells to add.
         */
    }
    if (pos > len) {
        LOG(LOG_WARNING, "common::AddspellCmd", "Overread buffer: %d > %d", pos, len);
    }
    cpl.spells_updated = 1;
}

/**
 *
 * @param data
 * @param len
 */
void UpdspellCmd(unsigned char *data, int len) {
    int flags, tag, pos = 0;
    Spell *tmp;

    if (!cpl.spelldata) {
        LOG(LOG_WARNING, "common::UpdspellCmd", "I know no spells to update");
        return;
    }

    flags = GetChar_String(data+pos); pos += 1;
    tag = GetInt_String(data+pos); pos += 4;
    for (tmp = cpl.spelldata; tmp && tmp->tag != tag; tmp = tmp->next)
        ;
    if (!tmp) {
        LOG(LOG_WARNING, "common::UpdspellCmd", "Invalid tag: %d", tag);
        return;
    }
    if (flags&UPD_SP_MANA) {
        tmp->sp = GetShort_String(data+pos); pos += 2;
    }
    if (flags&UPD_SP_GRACE) {
        tmp->grace = GetShort_String(data+pos); pos += 2;
    }
    if (flags&UPD_SP_DAMAGE) {
        tmp->dam = GetShort_String(data+pos); pos += 2;
    }
    if (pos > len) {
        LOG(LOG_WARNING, "common::UpdspellCmd", "Overread buffer: %d > %d", pos, len);
    }
    cpl.spells_updated = 1;
}

/**
 *
 * @param data
 * @param len
 */
void DeleteSpell(unsigned char *data, int len) {
    int tag;
    Spell *tmp, *target;

    if (!cpl.spelldata) {
        LOG(LOG_WARNING, "common::DeleteSpell", "I know no spells to delete");
        return;
    }

    tag = GetInt_String(data);
    /* Special case: the first spell is the one removed */
    if (cpl.spelldata->tag == tag) {
        target = cpl.spelldata;
        if (target->next) {
            cpl.spelldata = target->next;
        } else {
            cpl.spelldata = NULL;
        }
        free(target);
        return;
    }

    for (tmp = cpl.spelldata; tmp->next && tmp->next->tag != tag; tmp = tmp->next)
        ;
    if (!tmp->next) {
        LOG(LOG_WARNING, "common::DeleteSpell", "Invalid tag: %d", tag);
        return;
    }
    target = tmp->next;
    if (target->next) {
        tmp->next = target->next;
    } else {
        tmp->next = NULL;
    }
    free(target);
    cpl.spells_updated = 1;
}

/****************************************************************************/

/**
 * @} */ /* EndOf SCSpellCommands
 */

/**
 * @defgroup SCMapCommands Server->Client map command functions.
 * @{
 */

/**
 *
 * @param data
 * @param len
 */
void NewmapCmd(unsigned char *data, int len) {
    (void)data; /* __UNUSED__ */
    (void)len; /* __UNUSED__ */

    mapdata_newmap();
}

/* This is the common processing block for the map1 and map1a protocol
 * commands.  The map1a mieks minor extensions and are easy to deal with
 * inline (in fact, this code doesn't even care what rev is - just certain
 * bits will only bet set when using the map1a command.  rev is 0 for map1, 1
 * for map1a.  It conceivable that there could be future revisions.
 */

/* NUM_LAYERS should only be used for the map1{a} which only has a few layers.
 * Map2 has 10 layers.  However, some of the map1 logic requires this to be
 * set right.
 */
#define NUM_LAYERS (MAP1_LAYERS-1)

/**
 *
 * @param data
 * @param len
 */
void Map2Cmd(unsigned char *data, int len) {
    int mask, x, y, pos = 0, space_len, value;
    uint8 type;

    display_map_startupdate();
    /* Not really using map1 protocol, but some draw logic differs from the
     * original draw logic, and map2 is closest.
     */
    while (pos < len) {
        mask = GetShort_String(data+pos); pos += 2;
        x = ((mask>>10)&0x3f)-MAP2_COORD_OFFSET;
        y = ((mask>>4)&0x3f)-MAP2_COORD_OFFSET;

        /* This is a scroll then.  Go back and fetch another coordinate */
        if (mask&0x1) {
            mapdata_scroll(x, y);
            continue;
        }

        if (x<0) {
            LOG(LOG_WARNING, "commands.c::Map2Cmd", "got negative x!");
            x = 0;
        } else if (x >= MAX_VIEW) {
            LOG(LOG_WARNING, "commands.c::Map2Cmd", "got x >= MAX_VIEW!");
            x = MAX_VIEW - 1;
        }

        if (y<0) {
            LOG(LOG_WARNING, "commands.c::Map2Cmd", "got negative y!");
            y = 0;
        } else if (y >= MAX_VIEW) {
            LOG(LOG_WARNING, "commands.c::Map2Cmd", "got y >= MAX_VIEW!");
            y = MAX_VIEW - 1;
        }

        assert(0 <= x && x < MAX_VIEW);
        assert(0 <= y && y < MAX_VIEW);
        /* Clearing old cell data as needed (was in mapdata_set_face_layer()
         * before however that caused darkness to only work if sent after the
         * layers).
         */
        mapdata_clear_old(x, y);

        /* Inner loop is for the data on the space itself */
        while (pos < len) {
            type = data[pos++];
            /* type == 255 means nothing more for this space */
            if (type == 255) {
                mapdata_set_check_space(x, y);
                break;
            }
            space_len = type>>5;
            type &= 0x1f;
            /* Clear the space */
            if (type == MAP2_TYPE_CLEAR) {
                mapdata_clear_space(x, y);
                continue;
            } else if (type == MAP2_TYPE_DARKNESS) {
                value = data[pos++];
                mapdata_set_darkness(x, y, value);
                continue;
            } else if (type >= MAP2_LAYER_START && type < MAP2_LAYER_START+MAXLAYERS) {
                int layer, opt;

                /* This is face information for a layer. */
                layer = type&0xf;

                if (layer < 0) {
                    LOG(LOG_WARNING, "commands.c::Map2Cmd", "got negative layer!");
                    layer = 0;
                } else if (layer >= MAXLAYERS) {
                    LOG(LOG_WARNING, "commands.c::Map2Cmd", "got layer >= MAXLAYERS!");
                    layer = MAXLAYERS - 1;
                }
                assert(0 <= layer && layer < MAXLAYERS);

                /* This is the face */
                value = GetShort_String(data+pos); pos += 2;
                if (!(value&FACE_IS_ANIM)) {
                    mapdata_set_face_layer(x, y, value, layer);
                }

                if (space_len > 2) {
                    opt = data[pos++];
                    if (value&FACE_IS_ANIM) {
                        /* Animation speed */
                        mapdata_set_anim_layer(x, y, value, opt, layer);
                    } else {
                        /* Smooth info */
                        mapdata_set_smooth(x, y, opt, layer);
                    }
                }
                /* Currently, if 4 bytes, must be a smooth byte */
                if (space_len > 3) {
                    opt = data[pos++];
                    mapdata_set_smooth(x, y, opt, layer);
                }
                continue;
            } /* if image layer */
        } /* while pos<len inner loop for space */
    } /* While pos<len outer loop */
    mapupdatesent = 0;
    display_map_doneupdate(FALSE, FALSE);
}

/**
 * Scrolls the map on the client by some amount.
 *
 * @param data
 * @param len
 */
void map_scrollCmd(char *data, int len) {
    int dx, dy;
    char *buf;

    (void)len; /* __UNUSED__ */

    dx = atoi(data);
    buf = strchr(data, ' ');
    if (!buf) {
        LOG(LOG_WARNING, "common::map_scrollCmd", "Got short packet.");
        return;
    }
    buf++;
    dy = atoi(buf);

    display_map_startupdate();
    mapdata_scroll(dx, dy);
    display_map_doneupdate(FALSE, TRUE);
}

/**
 * Extract smoothing infos from an extendedmapinfo packet part data is located
 * at the beginning of the smooth datas
 *
 * @param data
 * @param len
 * @param x
 * @param y
 * @param layer
 */
int ExtSmooth(unsigned char *data, int len, int x, int y, int layer) {
    static int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1, };
    static int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1, };
    int i, rx, ry;
    int newsm;

    if (len < 1) {
        return 0;
    }

    x += pl_pos.x;
    y += pl_pos.y;
    newsm = GetChar_String(data);

    if (the_map.cells[x][y].smooth[layer] != newsm) {
        for (i = 0; i < 8; i++) {
            rx = x+dx[i];
            ry = y+dy[i];
            if (rx < 0 || ry < 0 || the_map.x <= rx || the_map.y <= ry) {
                continue;
            }
            the_map.cells[x][y].need_resmooth = 1;
        }
    }
    the_map.cells[x][y].smooth[layer] = newsm;
    return 1;/*Cause smooth infos only use 1 byte*/
}

/**
 * Handle MapExtended command
 * Warning! if you add commands to extended, take care that the 'layer'
 * argument of main loop is the opposite of the layer of the map so if you
 * reference a layer, use NUM_LAYERS-layer.
 *
 * @param data
 * @param len
 */
void MapExtendedCmd(unsigned char *data, int len) {
    int mask, x, y, pos = 0, layer;
    int noredraw = 0;
    int hassmooth = 0;
    int entrysize;
    int startpackentry;

    if (!mapupdatesent) {
        display_map_startupdate();
    }
    mapupdatesent = 1;
    mask = GetChar_String(data+pos); pos += 1;
    if (mask&EMI_NOREDRAW) {
        noredraw = 1;
    }
    if (mask&EMI_SMOOTH) {
        hassmooth = 1;
    }
    while (mask&EMI_HASMOREBITS) {
        /*There may be bits we ignore about*/
        mask = GetChar_String(data+pos);
        pos += 1;
    }
    entrysize = GetChar_String(data+pos);
    pos = pos+1;

    while (pos+entrysize+2 <= len) {
        mask = GetShort_String(data+pos); pos += 2;
        x = (mask>>10)&0x3f;
        y = (mask>>4)&0x3f;
        for (layer = NUM_LAYERS; layer >= 0; layer--) {
            if (mask&(1<<layer)) {
                /*handle an entry*/
                if (pos+entrysize > len) { /*erroneous packet*/
                    break;
                }
                startpackentry = pos;
                /* If you had extended infos to the server, this is where, in
                 * the client, you may add your code
                 */
                if (hassmooth) {
                    pos = pos+ExtSmooth(data+pos, len-pos, x, y, NUM_LAYERS-layer);
                }
                /* Continue with other if you add new extended infos to server
                 *
                 * Now point to the next data
                 */
                pos = startpackentry+entrysize;
            }
        }
    }
    if (!noredraw) {
        display_map_doneupdate(FALSE, FALSE);
        mapupdatesent = 0;
    }
}

/**
 *
 * @param data
 * @param len
 */
void MagicMapCmd(unsigned char *data, int len) {
    unsigned char *cp;
    int i;

    /* First, extract the size/position information. */
    if (sscanf((const char*)data, "%hd %hd %hd %hd", &cpl.mmapx, &cpl.mmapy, &cpl.pmapx, &cpl.pmapy) != 4) {
        LOG(LOG_WARNING, "common::MagicMapCmd", "Was not able to properly extract magic map size, pos");
        return;
    }

    if (cpl.mmapx == 0 || cpl.mmapy == 0) {
        LOG(LOG_WARNING, "common::MagicMapCmd", "empty map");
        return;
    }

    /* Now we need to find the start of the actual data.  There are 4 space
     * characters we need to skip over.
     */
    for (cp = data, i = 0; i < 4 && cp < data+len; cp++) {
        if (*cp == ' ') {
            i++;
        }
    }
    if (i != 4) {
        LOG(LOG_WARNING, "common::MagicMapCmd", "Was unable to find start of magic map data");
        return;
    }
    i = len-(cp-data); /* This should be the number of bytes left */
    if (i != cpl.mmapx*cpl.mmapy) {
        LOG(LOG_WARNING, "common::MagicMapCmd", "Magic map size mismatch.  Have %d bytes, should have %d",
            i, cpl.mmapx*cpl.mmapy);
        return;
    }
    free(cpl.magicmap);
    cpl.magicmap = malloc(cpl.mmapx*cpl.mmapy);
    /* Order the server puts it in should be just fine.  Note that the only
     * requirement that this works is that magicmap by 8 bits, being that is
     * the size specified in the protocol and what the server sends us.
     */
    memcpy(cpl.magicmap, cp, cpl.mmapx*cpl.mmapy);
    cpl.showmagic = 1;
    draw_magic_map();
}

/**
 * @} */ /* EndOf SCMapCommands
 */

/**
 *
 * @param data
 * @param len
 */
void SinkCmd(unsigned char *data, int len) {
}

/**
 * Got a tick from the server.  We currently don't care what tick number it
 * is, but just have the code in case at some time we do.
 *
 * @param data
 * @param len
 */
void TickCmd(uint8 *data, int len) {

    tick = GetInt_String(data);

    /* Up to the specific client to decide what to do */
    client_tick(tick);
}

/**
 * Server gives us current player's pickup.
 *
 * @param data
 * buffer sent by server.
 * @param len
 * length of data.
 */
void PickupCmd(uint8 *data, int len) {
    uint32 pickup = GetInt_String(data);
    client_pickup(pickup);
}

/**
 * Handles a failure return from the server.
 *
 * @param buf
 * buffer sent by server.
 * @param len
 * length of data.
 */
void FailureCmd(char *buf, int len) {
    char *cp;

    /* The format of the buffer is 'command error message'.  We need to
     * extract the failed command, and then pass in the error message to the
     * appropriate handler.  So find the space, set it to null.  in that way,
     * buf is now just the failure command, and cp is the message.
     */
    cp = strchr(buf,' ');
    if (!cp) return;

    *cp = 0;
    cp++;

    if (!strcmp(buf,"accountlogin")) {
        account_login_failure(cp);
    }
    else if (!strcmp(buf,"accountnew")) {
        account_creation_failure(cp);
    }
    else if (!strcmp(buf,"accountaddplayer")) {
        account_add_character_failure(cp);
    }
    else if (!strcmp(buf,"createplayer")) {
        create_new_character_failure(cp);
    }
    else if (!strcmp(buf, "accountpw")) {
        account_change_password_failure(cp);
    }
    else
        /* This really is an error - if this happens it menas the server
         * failed to process a request that the client made - the client
         * should be able to handle failures for all request types it makes.
         * But this is also a problem in that it means that the server is
         * waiting for a correct response, and if we do not display anything,
         * the player is unlikely to know this.
         */
        LOG(LOG_ERROR, "common::FailureCmd", "Got a failure response we can not handle: %s:%s",
            buf, cp);
}

/**
 * This handles the accountplayers command
 */
void AccountPlayersCmd(char *buf, int len) {

    int num_characters, level, pos, flen, faceno;
    char name[MAX_BUF], class[MAX_BUF], race[MAX_BUF],
        face[MAX_BUF], party[MAX_BUF], map[MAX_BUF];

    /* This is called first so it can clear out the existing data store.
     */
    choose_character_init();

    level=0;
    name[0]=0;
    class[0]=0;
    race[0]=0;
    face[0]=0;
    party[0]=0;
    map[0]=0;
    faceno=0;

    /* We don't do anything with this right now */
    num_characters=buf[0];
    pos=1;
    while (pos < len) {
        flen = buf[pos];
        /* flen == 0 is to note that we got end of character data */
        if (flen == 0) {
            update_character_choose(name, class, race, face, party, map, level, faceno);
            /* Blank all the values - it is no sure thing that the next
             * character will fill all these in.
             */
            level=0;
            name[0]=0;
            class[0]=0;
            race[0]=0;
            face[0]=0;
            party[0]=0;
            map[0]=0;
            faceno=0;
            pos++;
            continue;
        }
        pos++;
        if ((pos +flen) > len || flen>=MAX_BUF) {
            LOG(LOG_ERROR,"commands.c:AccountPlayerCmd", "data overran buffer");
            return;
        }
        switch (buf[pos]) {
        case ACL_NAME:
            strncpy(name, buf + pos +1, flen-1);
            name[flen-1] = 0;
            break;

        case ACL_CLASS:
            strncpy(class, buf + pos +1, flen-1);
            class[flen-1] = 0;
            break;

        case ACL_RACE:
            strncpy(race, buf + pos +1, flen-1);
            race[flen-1] = 0;
            break;
       
        case ACL_FACE:
            strncpy(face, buf + pos +1, flen-1);
            face[flen-1] = 0;
            break;

        case ACL_PARTY:
            strncpy(party, buf + pos +1, flen-1);
            party[flen-1] = 0;
            break;

        case ACL_MAP:
            strncpy(map, buf + pos +1, flen-1);
            map[flen-1] = 0;
            break;

        case ACL_LEVEL:
            level = GetShort_String(buf+pos+1);
            break;
        case ACL_FACE_NUM:
            faceno = GetShort_String(buf+pos+1);
            break;
        }
        pos += flen;
    }
}

