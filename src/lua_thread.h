#ifndef LUA_THREAD_H
#define LUA_THREAD_H

#include <zephyr/kernel.h>
#include "lua/lua.h"

typedef struct {
    struct k_thread thread;
    char *script;
    const struct shell *shell;
    bool in_use;
    bool been_started;
    volatile bool kill_requested;
    lua_State *state;
    char name[16];
} lua_thread_slot_t;

extern lua_thread_slot_t lua_slots[CONFIG_LUA_MAX_THREADS];

extern int num_lua_threads;

int lua_thread_start(const struct shell *shell, char *script);
int lua_thread_kill(int slot);
int get_current_lua_slot(void);

#endif