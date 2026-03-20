#include "lua_thread.h"
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
#include <zephyr/shell/shell.h>
#include <stdlib.h>

/*
⚠ This file was produced with generative ai.
⚠ It *MUST* be reviewed in the future.
⚠ REMOVE THIS COMMENT after review/rewrite. AI code should only be used for prototyping!!
*/

K_THREAD_STACK_ARRAY_DEFINE(lua_stacks, LUA_MAX_THREADS, LUA_THREAD_STACK_SIZE);

lua_thread_slot_t lua_slots[LUA_MAX_THREADS] = {0};

extern int lua_sleep_ms(lua_State *L);
extern int luaopen_paint(lua_State *L);

static void lua_thread_entry(void *a, void *b, void *c)
{
    lua_thread_slot_t *slot = (lua_thread_slot_t *)a;

    lua_State *state = luaL_newstate();
    if (state == NULL) {
        shell_print(slot->shell, "Lua error: failed to allocate state (out of memory)");
        free(slot->script);
        slot->script = NULL;
        slot->in_use = false;
        return;
    } // ^ Not sure that this is necessary
    luaL_openlibs(state);
    luaL_requiref(state, "math", luaopen_math, 1);
    lua_pop(state, 1);
    luaL_requiref(state, "paint", luaopen_paint, 1);
    lua_pop(state, 1);
    lua_pushcfunction(state, lua_sleep_ms);
    lua_setglobal(state, "sleep_ms");

    int err = luaL_loadstring(state, slot->script);
    if (err) {
        shell_print(slot->shell, "Lua compile error: %s", lua_tostring(state, -1));
        lua_pop(state, 1);
    } else {
        err = lua_pcall(state, 0, 0, 0);
        if (err) {
            shell_print(slot->shell, "Lua runtime error: %s", lua_tostring(state, -1));
            lua_pop(state, 1);
        } else {
            shell_print(slot->shell, "Lua script finished.");
        }
    }

    lua_close(state);
    free(slot->script);
    slot->script = NULL;
    slot->in_use = false;
}

int lua_thread_start(const struct shell *shell, char *script)
{
    for (int i = 0; i < LUA_MAX_THREADS; i++) {
        if (!lua_slots[i].in_use) {
            lua_slots[i].script = script;
            lua_slots[i].shell = shell;
            lua_slots[i].in_use = true;
            k_thread_create(&lua_slots[i].thread,
                            lua_stacks[i],
                            K_THREAD_STACK_SIZEOF(lua_stacks[0]),
                            lua_thread_entry,
                            &lua_slots[i], NULL, NULL,
                            15, 0, K_NO_WAIT);
            return i;
        }
    }
    return -1; // no free slots
}