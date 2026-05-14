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

lua_State *L;

K_THREAD_STACK_ARRAY_DEFINE(lua_stacks, CONFIG_LUA_MAX_THREADS, CONFIG_LUA_THREAD_STACK_SIZE);

lua_thread_slot_t lua_slots[CONFIG_LUA_MAX_THREADS] = {0};

extern int lua_sleep_ms(lua_State *L);
extern int luaopen_paint(lua_State *L);
extern int luaopen_input(lua_State *L);

static uint8_t lua_alloc_pool[98304];
static struct k_heap lua_heap;
static bool lua_heap_initialized = false;

static void *lua_zephyr_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    struct k_heap *heap = (struct k_heap *)ud;
    if (nsize == 0)
    {
        if (ptr != NULL)
            k_heap_free(heap, ptr);
        return NULL;
    }
    else if (ptr == NULL)
    {
        return k_heap_alloc(heap, nsize, K_NO_WAIT);
    }
    else
    {
        void *new_ptr = k_heap_alloc(heap, nsize, K_NO_WAIT);
        if (new_ptr == NULL)
            return NULL;
        memcpy(new_ptr, ptr, osize < nsize ? osize : nsize);
        k_heap_free(heap, ptr);
        return new_ptr;
    }
}

static void lua_thread_cancel_hook(lua_State *L, lua_Debug *ar)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "lua_thread_slot");
    lua_thread_slot_t *slot = (lua_thread_slot_t *)lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (slot && slot->kill_requested)
    {
        luaL_error(L, "Lua thread canceled");
    }
}

static void lua_thread_entry(void *a, void *b, void *c)
{
    lua_thread_slot_t *slot = (lua_thread_slot_t *)a;

    slot->kill_requested = false;
    slot->state = NULL;

    if (!lua_heap_initialized)
    {
        k_heap_init(&lua_heap, lua_alloc_pool, sizeof(lua_alloc_pool));
        lua_heap_initialized = true;
    }
    lua_State *state = lua_newstate(lua_zephyr_alloc, &lua_heap);
    if (state == NULL)
    {
        shell_print(slot->shell, "Error: Could not allocate Lua state (out of memory)");
        slot->in_use = false;
        return;
    }

    slot->state = state;
    lua_pushlightuserdata(state, slot);
    lua_setfield(state, LUA_REGISTRYINDEX, "lua_thread_slot");
    lua_sethook(state, lua_thread_cancel_hook, LUA_MASKCOUNT, 256);

    luaL_openlibs(state);
    luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(state, luaopen_coroutine);
    lua_setfield(state, -2, "coroutine");
    lua_pushcfunction(state, luaopen_table);
    lua_setfield(state, -2, "table");
    lua_pushcfunction(state, luaopen_string);
    lua_setfield(state, -2, "string");
    lua_pushcfunction(state, luaopen_math);
    lua_setfield(state, -2, "math");
    lua_pushcfunction(state, luaopen_zephyr);
    lua_setfield(state, -2, "zephyr");
    lua_pushcfunction(state, luaopen_paint);
    lua_setfield(state, -2, "paint");
    lua_pushcfunction(state, luaopen_input);
    lua_setfield(state, -2, "input");
    // lua_pushcfunction(state, luaopen_zephyr);
    // lua_setfield(state, -2, "zephyr");
    lua_pop(state, 1);

    int err = luaL_loadstring(state, slot->script);
    if (err)
    {
        shell_print(slot->shell, "Lua compile error: %s", lua_tostring(state, -1));
        lua_pop(state, 1);
    }
    else
    {
        err = lua_pcall(state, 0, 0, 0);
        if (err)
        {
            shell_print(slot->shell, "Lua runtime error: %s", lua_tostring(state, -1));
            lua_pop(state, 1);
        }
        else
        {
            shell_print(slot->shell, "Lua script finished.");
        }
    }

    lua_close(state);
    slot->state = NULL;
    slot->kill_requested = false;
    free(slot->script);
    slot->script = NULL;
    slot->in_use = false;
    num_lua_threads = recount_lua_threads();
}
int num_lua_threads = 0;

int recount_lua_threads(void)
{
    int count = 0;
    for (int i = 0; i < CONFIG_LUA_MAX_THREADS; i++) {
        if (lua_slots[i].in_use) {
            count++;
        }
    }
    return count;
}

int lua_thread_start(const struct shell *shell, char *script)
{
    for (int i = 0; i < CONFIG_LUA_MAX_THREADS; i++)
    {
        if (!lua_slots[i].in_use)
        {
            if (lua_slots[i].been_started)
            {
                k_thread_join(&lua_slots[i].thread, K_FOREVER); // is this necessary?
            }
            lua_slots[i].script = script;
            lua_slots[i].shell = shell;
            lua_slots[i].in_use = true;
            lua_slots[i].been_started = true;
            lua_slots[i].kill_requested = false;
            lua_slots[i].state = NULL;
            k_thread_create(&lua_slots[i].thread,
                            lua_stacks[i],
                            K_THREAD_STACK_SIZEOF(lua_stacks[0]),
                            lua_thread_entry,
                            &lua_slots[i], NULL, NULL,
                            K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
            snprintf(lua_slots[i].name, sizeof(lua_slots[i].name), "Lua %d", i);
            k_thread_name_set(&lua_slots[i].thread, lua_slots[i].name);
            num_lua_threads = recount_lua_threads();
            return i;
        }
    }
    return -1; // no free slots
}

int lua_thread_kill(int slot)
{
    if (slot < 0 || slot >= CONFIG_LUA_MAX_THREADS)
    {
        return -EINVAL;
    }

    lua_thread_slot_t *selected = &lua_slots[slot];
    if (!selected->in_use)
    {
        return -EINVAL;
    }

    selected->kill_requested = true;
    return 0;
}

int get_current_lua_slot(void)
{
    struct k_thread *current = k_current_get();
    for (int i = 0; i < CONFIG_LUA_MAX_THREADS; i++)
    {
        if (&lua_slots[i].thread == current)
        {
            return i;
        }
    }
    return -1;
}