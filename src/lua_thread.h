#ifndef LUA_THREAD_H
#define LUA_THREAD_H

#include <zephyr/kernel.h>
#include "lua/lua.h"

#define LUA_THREAD_PRIO_FG (K_LOWEST_APPLICATION_THREAD_PRIO - 2)
#define LUA_THREAD_PRIO_BG  K_LOWEST_APPLICATION_THREAD_PRIO

typedef struct {
    struct k_thread thread;
    char *script;
    const struct shell *shell;
    bool in_use;
    bool been_started;
    volatile bool kill_requested;
    lua_State *state;
    char name[64];
    int capture_input;
    // 0 = no inputs
    // 1 = capture
    // 2 = listen (gated)
    // 3 = listen (always)
} lua_thread_slot_t;

#define LUA_INPUT_NONE 0
#define LUA_INPUT_CAPTURE 1
#define LUA_INPUT_LISTEN_GATED 2
#define LUA_INPUT_LISTEN_ALWAYS 3

extern lua_thread_slot_t lua_slots[CONFIG_LUA_MAX_THREADS];

extern int num_lua_threads;

int lua_thread_start(const struct shell *shell, char *script, char *name);
int lua_thread_kill(int slot);
int get_current_lua_slot(void);

#endif