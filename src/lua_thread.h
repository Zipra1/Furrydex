#ifndef LUA_THREAD_H
#define LUA_THREAD_H

#include <zephyr/kernel.h>
#include "luazephyrlib.h"
#include "lua/lua.h"

#define LUA_THREAD_PRIO_FG (K_LOWEST_APPLICATION_THREAD_PRIO - 2)
#define LUA_THREAD_PRIO_BG  K_LOWEST_APPLICATION_THREAD_PRIO
#define LUA_THREAD_MAX_NAME_LEN 64

typedef struct {
    struct k_thread thread;
    char *script;
    const struct shell *shell;
    bool in_use;
    bool been_started;
    volatile bool kill_requested;
    lua_State *state;
    char name[LUA_THREAD_MAX_NAME_LEN];
    int capture_input;
    // 0 = no inputs
    // 1 = capture
    // 2 = listen (gated)
    // 3 = listen (always)
    bool hide_top;
    bool hide_bottom;
    bool in_tray;
    uint8_t *icon;
    atomic_t ble_fifo_depth;
    bool ble_enabled;
} lua_thread_slot_t;

#define LUA_INPUT_NONE 0
#define LUA_INPUT_CAPTURE 1
#define LUA_INPUT_LISTEN_GATED 2
#define LUA_INPUT_LISTEN_ALWAYS 3

extern lua_thread_slot_t lua_slots[CONFIG_LUA_MAX_THREADS];

extern atomic_t visible_slot_index;
extern int num_lua_threads;
extern int num_shown_lua_threads;

int lua_thread_start(const struct shell *shell, char *script, char *name);
int lua_thread_kill(int slot);
int get_current_lua_slot(void);
int lua_thread_update_priorities(int selected_slot);
int recount_shown_lua_threads(void);
int update_visible_lua_slot_index(void);
void lua_thread_refresh_ui_state(void);
void lua_thread_free_icon(lua_thread_slot_t *slot);

#endif