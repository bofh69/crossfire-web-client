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
 * @file common/script_lua.c
 *
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

#ifdef HAVE_LUA
/* It seems easier to just comment everything out if we don't have
 * lua vs trying to play around with it in the makefiles.
 */

#include <external.h>
#include <script_lua.h>
#include <lua.h>
#include <lualib.h>

#if LUA_VERSION_NUM >= 501
#include <lauxlib.h>
#endif

struct script_state {
    lua_State* state;
    const char* filename;
};

#if 0
static void *l_alloc (void * /*ud*/, void *ptr, size_t /*osize*/, size_t nsize)
{
    if (nsize == 0) {
        free(ptr);
        return NULL;
    } else {
        return realloc(ptr, nsize);
    }
}
#endif

static const char* l_readerfile(lua_State *L, void *data, size_t *size)
{
    static char buf[4096];
    FILE* file = (FILE*)data;
    *size = fread(buf, 1, 4096, file);
    if ( !*size && ferror(file) ) {
        return NULL;
    }
    if ( !*size && feof(file)) {
        return NULL;
    }
    return buf;
}

static struct script_state* scripts = NULL;
static int script_count = 0;

static void update_player(lua_State* lua)
{
    lua_pushstring(lua, "player");
    lua_gettable(lua, LUA_GLOBALSINDEX);
    if (!lua_istable(lua, -1)) {
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

    for ( it = cpl.ob->inv; it; it = it->next ) {
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

    for ( it = cpl.below->inv; it; it = it->next ) {
        if ( it->tag == 0 || strlen(it->s_name) == 0 ) {
            continue;
        }

        lua_pushnumber(lua, index++);

        do_item(lua, it);
        lua_settable(lua, -3);
    }
    lua_pop(lua, 1);
}


static int lua_draw(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */
    const char* what;
    if ( n != 1 ) {
        lua_pushstring(L, "draw what?");
        lua_error(L);
    }
    if ( !lua_isstring(L, 1) ) {
        lua_pushstring(L, "expected a string");
        lua_error(L);
    }

    what = lua_tostring(L,1);
    draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT, what);

    return 0;
}

static int lua_issue(lua_State *L)
{
    int n = lua_gettop(L);    /* number of arguments */
    const char* what;
    int repeat, must_send;
    if ( n != 3 ) {
        lua_pushstring(L, "syntax is cfissue repeat must_send command");
        lua_error(L);
    }
    if ( !lua_isnumber(L, 1) ) {
        lua_pushstring(L, "expected a number");
        lua_error(L);
    }

    if ( !lua_isnumber(L, 2) ) {
        lua_pushstring(L, "expected a number");
        lua_error(L);
    }

    if ( !lua_isstring(L, 3) ) {
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
    if ( !file ) {
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT,
                      "Invalid file");
        return;
    }

    lua = lua_open();
    if ( !lua ) {
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT,
                      "Memory allocation error.");
        fclose(file);
        return;
    }
    luaopen_base(lua);
    lua_pop(lua,1);
    luaopen_table(lua);
    lua_pop(lua,1);

    if (( load = lua_load(lua, l_readerfile, (void*)file, name))) {
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT,
                      "Load error!");
        if ( load == LUA_ERRSYNTAX )
            draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT,
                          "Syntax error!");
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
    if (lua_pcall(lua, 0, 0, 0)) {
        draw_ext_info(NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT,
                      "Init error!");
        lua_close(lua);
        return;
    }

    scripts = realloc(scripts,sizeof(scripts[0])*(script_count+1));

    if (scripts == NULL) {
        LOG(LOG_ERROR, "script_lua_load",
                "Could not allocate memory: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

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
    if ( script_count == 0 ) {
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT,
                      "No LUA scripts are currently running");
    } else {
        int i;
        char buf[1024];

        snprintf(buf, sizeof(buf), "%d LUA scripts currently running:",script_count);
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT, buf);
        for ( i=0; i<script_count; ++i) {
            snprintf(buf, sizeof(buf), "%d %s",i+1,scripts[i].filename);
            draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT, buf);
        }
    }
}

void script_lua_kill(const char* param)
{
    int i;
    i = atoi(param) - 1;
    if ( i < 0 || i >= script_count ) {
        draw_ext_info(NDI_BLACK, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT,
                      "Invalid script index!");
        return;
    }
    lua_close(scripts[i].state);

    if ( i < (script_count-1) ) {
        memmove(&scripts[i],&scripts[i+1],sizeof(scripts[i])*(script_count-i-1));
    }

    --script_count;
}

void script_lua_stats()
{
    int script;
    lua_State* lua;
    for ( script = 0; script < script_count; script++ ) {
        lua = scripts[script].state;
        lua_pushstring(lua, "event_stats");
        lua_gettable(lua, LUA_GLOBALSINDEX);
        if (lua_isfunction(lua, lua_gettop(lua))) {
            int luaerror;
            update_player(lua);
            update_inv(lua);
            update_ground(lua);
            if ( ( luaerror = lua_pcall(lua, 0, 0, 0) ) ) {
                const char* what = lua_tostring(lua, lua_gettop(lua));
                draw_ext_info(
                    NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT, what);
                lua_pop(lua,1);
            }
        } else {
            lua_pop(lua, 1);
        }
    }
}

int script_lua_command(const char* command, const char* param)
{
    int script;
    lua_State* lua;
    int ret = 0;
    for ( script = 0; script < script_count; script++ ) {
        lua = scripts[script].state;
        lua_pushstring(lua, "event_command");
        lua_gettable(lua, LUA_GLOBALSINDEX);
        if (lua_isfunction(lua, lua_gettop(lua))) {
            int luaerror;
            update_player(lua);
            update_inv(lua);
            update_ground(lua);
            lua_pushstring(lua, command);
            lua_pushstring(lua, param ? param : "");
            if ( ( luaerror = lua_pcall(lua, 2, 1, 0) ) ) {
                const char* what = lua_tostring(lua, lua_gettop(lua));
                draw_ext_info(
                    NDI_RED, MSG_TYPE_CLIENT, MSG_TYPE_CLIENT_SCRIPT, what);
                lua_pop(lua,1);
            } else {
                ret = lua_tonumber(lua, 1);
                lua_pop(lua, 1);
            }
        } else {
            lua_pop(lua, 1);
        }
    }
    return ret;
}

#endif /* HAVE_LIB_LUA */
