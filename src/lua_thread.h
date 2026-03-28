#ifndef LUA_THREAD_H
#define LUA_THREAD_H

#include <zephyr/kernel.h>
#include "lua/lua.h"

#define LUA_MAX_THREADS 7
#define LUA_THREAD_STACK_SIZE 4096

typedef struct {
    struct k_thread thread;
    char *script;
    const struct shell *shell;
    bool in_use;
    bool been_started;
    char name[16];
} lua_thread_slot_t;

extern lua_thread_slot_t lua_slots[LUA_MAX_THREADS];

int lua_thread_start(const struct shell *shell, char *script);

#endif