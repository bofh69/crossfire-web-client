const char *rcsid_common_script_lua_c =
    "$Id$";
/*
    Crossfire client, a client program for the crossfire program.

    Copyright (C) 2006-2007 Mark Wedel & Crossfire Development Team
    This source file also Copyright (C) 2006 Nicolas Weeger

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

#ifndef WIN32
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#include <client.h>

#ifdef HAVE_LIBLUA
/* It seems easier to just comment everything out if we don't have
 * lua vs trying to play around with it in the makefiles.
 */

#include <external.h>
#include <script_lua.h>
#include <lua.h>
#include <lualib.h>

struct script_state
{
    lua_State* state;
    const char* filename;
};

#if 0
static void *l_alloc (void * /*ud*/, void *ptr, size_t /*osize*/, size_t nsize)
{
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    else
        return realloc(ptr, nsize);
}
#endif

static const char* l_readerfile(lua_State *L, void *data, size_t *size)
{
    static char buf[4096];
    FILE* file = (FILE*)data;
    *size = fread(buf, 1, 4096, file);
    if ( !*size && ferror(file) )
        return NULL;
    if ( !*size && feof(file))
        return NULL;
    return buf;
}

static struct script_state* scripts = NULL;
static int script_count = 0;

static void update_player(lua_State* lua)
{
    lua_pushstring(lua, "player");
    lua_gettable(lua, LUA_GLOBALSINDEX);
    if (!lua_istable(lua, -1))
    {
        lua_pop(lua, 1);
        return;
    }

    lua_pushstring(lua, "hp");
    lua_pushnumber(lua, cpl.stats.hp);
    lua_settable(lua, -3);
    lua_pushstring(lua, "gr");
    lua_pushnumber(lua, cpl.stats.grace);
    lua_settable(lua, -3);
    lua_pushstring(lua, "sp");
    lua_pushnumber(lua, cpl.stats.sp);
    lua_settable(lua, -3);
    lua_pushstring(lua, "food");
    lua_pushnumber(lua, cpl.stats.food);
    lua_settable(lua, -3);

    lua_pop(lua, 1);
}

static void do_item(lua_State* lua, item* it)
{
    lua_newtable(lua);
    lua_pushstring(lua, "s_name");
    lua_pushstring(lua, it->s_name);
    lua_settable(lua, -3);
    lua_pushstring(lua, "magical");
    lua_pushnumber(lua, it->magical);
    lua_settable(lua, -3);
    lua_pushstring(lua, "cursed");
    lua_pushnumber(lua, it->cursed);
    lua_settable(lua, -3);
    lua_pushstring(lua, "damned");
    lua_pushnumber(lua, it->damned);
    lua_settable(lua, -3);
    lua_pushstring(lua, "unpaid");
    lua_pushnumber(lua, it->unpaid);
    lua_settable(lua, -3);
    lua_pushstring(lua, "locked");
    lua_pushnumber(lua, it->locked);
    lua_settable(lua, -3);
    lua_pushstring(lua, "applied");
    lua_pushnumber(lua, it->applied);
    lua_settable(lua, -3);
    lua_pushstring(lua, "open");
    lua_pushnumber(lua, it->open);
    lua_settable(lua, -3);
}

static void update_inv(lua_State* lua)
{
    item* it;
    int index = 1;
    lua_pushstring(lua, "inv");
    lua_newtable(lua);
    lua_settable(lua, LUA_GLOBALSINDEX);
    lua_pushstring(lua, "inv");
    lua_gettable(lua, LUA_GLOBALSINDEX);

    for ( it = cpl.ob->inv; it; it = it->next )
    {
        lua_pushnumber(lua, index++);
        do_item(lua, it);
        lua_settable(lua, -3);
    }
    lua_pop(lua, 1);
}

static void update_ground(lua_State* lua)
{
    item* it;
    int index = 1;
    lua_pushstring(lua, "ground");
    lua_newtable(lua);
    lua_settable(lua, LUA_GLOBALSINDEX);
    lua_pushstring(lua, "ground");
    lua_gettable(lua, LUA_GLOBALSINDEX);

    for ( it = cpl.below->inv; it; it = it->next )
    {
        if ( it->tag == 0 || strlen(it->s_name) == 0 )
            continue;

        lua_pushnumber(lua, index++);

        do_item(lua, it);
        lua_settable(lua, -3);
    }
    lua_pop(lua, 1);
}


static int lua_draw(lua_State *L) {
    int n = lua_gettop(L);    /* number of arguments */
    const char* what;
    if ( n != 1 )
    {
        lua_pushstring(L, "draw what?");
        lua_error(L);
    }
    if ( !lua_isstring(L, 1) )
    {
        lua_pushstring(L, "expected a string");
        lua_error(L);
    }

    what = lua_tostring(L,1);
    draw_info(what, NDI_RED);

    return 0;
}

static int lua_issue(lua_State *L) {
    int n = lua_gettop(L);    /* number of arguments */
    const char* what;
    int repeat, must_send;
    if ( n != 3 )
    {
        lua_pushstring(L, "syntax is cfissue repeat must_send command");
        lua_error(L);
    }
    if ( !lua_isnumber(L, 1) )
    {
        lua_pushstring(L, "expected a number");
        lua_error(L);
    }

    if ( !lua_isnumber(L, 2) )
    {
        lua_pushstring(L, "expected a number");
        lua_error(L);
    }

    if ( !lua_isstring(L, 3) )
    {
        lua_pushstring(L, "expected a number");
        lua_error(L);
    }

    repeat = lua_tonumber(L, 1);
    must_send = lua_tonumber(L, 2);
    what = lua_tostring(L,3);
    send_command(what,repeat,must_send);

    return 0;
}

void script_lua_load(const char* name)
{
    lua_State* lua;
    FILE* file;
    int load;
    int index = script_count;

    file = fopen(name,"r");
    if ( !file )
    {
        draw_info("Invalid file",NDI_RED);
        return;
    }

    lua = lua_open();
    if ( !lua )
    {
        draw_info("Memory allocation error.",NDI_RED);
        fclose(file);
        return;
    }
    luaopen_base(lua);
    lua_pop(lua,1);
    luaopen_table(lua);
    lua_pop(lua,1);

    if (( load = lua_load(lua, l_readerfile, (void*)file, name)))
    {
        draw_info("Load error!",NDI_RED);
        if ( load == LUA_ERRSYNTAX )
            draw_info("Syntax error!",NDI_RED);
        fclose(file);
        lua_close(lua);
        return;
    }
    fclose(file);

    lua_register(lua, "cfdraw", lua_draw);
    lua_register(lua, "cfissue", lua_issue);

    lua_pushstring(lua, "player");
    lua_newtable(lua);
    lua_settable(lua, LUA_GLOBALSINDEX);
    update_player(lua);
    update_inv(lua);
    update_ground(lua);

    /* Load functions, init script */
    if (lua_pcall(lua, 0, 0, 0))
    {
        draw_info("Init error!", NDI_RED);
        fclose(file);
        lua_close(lua);
        return;
    }

    scripts = realloc(scripts,sizeof(scripts[0])*(script_count+1));
    script_count++;
    scripts[index].filename = strdup_local(name);
    scripts[index].state = lua;

    /*
    printf("lua_gettop = %d, lua_type => %s\n", lua_gettop(lua), lua_typename( lua, lua_type(lua, lua_gettop(lua))));
    printf("lua_gettop = %d, lua_type => %s\n", lua_gettop(lua), lua_typename( lua, lua_type(lua, lua_gettop(lua))));
    lua_pushstring(lua, "init");
    printf("lua_gettop = %d, lua_type => %s\n", lua_gettop(lua), lua_typename( lua, lua_type(lua, lua_gettop(lua))));
    lua_gettable(lua, LUA_GLOBALSINDEX);
    printf("lua_gettop = %d, lua_type => %s\n", lua_gettop(lua), lua_typename( lua, lua_type(lua, lua_gettop(lua))));
    if (lua_isfunction(lua, lua_gettop(lua)))
        lua_call(lua, 0, 0);
    lua_pop(lua, 1);
    */
}

void script_lua_list(const char* param)
{
    if ( script_count == 0 )
    {
        draw_info("No LUA scripts are currently running",NDI_BLACK);
    }
    else
    {
        int i;
        char buf[1024];

        sprintf(buf,"%d LUA scripts currently running:",script_count);
        draw_info(buf,NDI_BLACK);
        for ( i=0;i<script_count;++i)
        {
            sprintf(buf,"%d %s",i+1,scripts[i].filename);
            draw_info(buf,NDI_BLACK);
        }
    }
}

void script_lua_kill(const char* param)
{
    int i;
    i = atoi(param) - 1;
    if ( i < 0 || i >= script_count )
    {
        draw_info("Invalid script index!", NDI_BLACK);
        return;
    }
    lua_close(scripts[i].state);

    if ( i < (script_count-1) )
    {
        memmove(&scripts[i],&scripts[i+1],sizeof(scripts[i])*(script_count-i-1));
    }

    --script_count;
}

void script_lua_stats()
{
    int script;
    lua_State* lua;
    for ( script = 0; script < script_count; script++ )
    {
        lua = scripts[script].state;
        lua_pushstring(lua, "event_stats");
        lua_gettable(lua, LUA_GLOBALSINDEX);
        if (lua_isfunction(lua, lua_gettop(lua)))
        {
            int luaerror;
            update_player(lua);
            update_inv(lua);
            update_ground(lua);
            if ( ( luaerror = lua_pcall(lua, 0, 0, 0) ) )
            {
                const char* what = lua_tostring(lua, lua_gettop(lua));
                draw_info(what,NDI_RED);
                lua_pop(lua,1);
            }
        }
        else
            lua_pop(lua, 1);
    }
}

int script_lua_command(const char* command, const char* param)
{
    int script;
    lua_State* lua;
    int ret = 0;
    for ( script = 0; script < script_count; script++ )
    {
        lua = scripts[script].state;
        lua_pushstring(lua, "event_command");
        lua_gettable(lua, LUA_GLOBALSINDEX);
        if (lua_isfunction(lua, lua_gettop(lua)))
        {
            int luaerror;
            update_player(lua);
            update_inv(lua);
            update_ground(lua);
            lua_pushstring(lua, command);
            lua_pushstring(lua, param ? param : "");
            if ( ( luaerror = lua_pcall(lua, 2, 1, 0) ) ) {
                const char* what = lua_tostring(lua, lua_gettop(lua));
                draw_info(what,NDI_RED);
                lua_pop(lua,1);
            }
            else {
                ret = lua_tonumber(lua, 1);
                lua_pop(lua, 1);
            }
        }
        else
            lua_pop(lua, 1);
    }
    return ret;
}

#endif /* HAVE_LIB_LUA */
