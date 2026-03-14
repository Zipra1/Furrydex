#include <stdlib.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/shell/shell.h>
#include <stdio.h>
#include <string.h>
#include "console.h"

#include <zephyr/fs/fs.h>
#include <ff.h>
#include "lua/lauxlib.h"
#include "lua/lua.h"

K_MUTEX_DEFINE(blink_mutex);

const int32_t blink_max_ms = 2000;
const int32_t blink_min_ms = 100;
int32_t blink_sleep_ms = 500;

static int cmd_blink_inc(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    k_mutex_lock(&blink_mutex, K_FOREVER);
    blink_sleep_ms = MIN(blink_sleep_ms + 100, blink_max_ms);
    k_mutex_unlock(&blink_mutex);
    shell_print(sh, "Blink sleep: %d ms", blink_sleep_ms);
    return 0;
}

static int cmd_blink_dec(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    k_mutex_lock(&blink_mutex, K_FOREVER);
    blink_sleep_ms = MAX(blink_sleep_ms - 100, blink_min_ms);
    k_mutex_unlock(&blink_mutex);
    shell_print(sh, "Blink sleep: %d ms", blink_sleep_ms);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_blink,
                               SHELL_CMD(inc, NULL, "Increase blink speed by 100ms", cmd_blink_inc),
                               SHELL_CMD(dec, NULL, "Decrease blink speed by 100ms", cmd_blink_dec),
                               SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(blink, &sub_blink, "Blink speed controls", NULL);


K_MUTEX_DEFINE(page_select_mutex); // Why are you here. ＞︿＜
int selected_page = 0;

static int cmd_page_left(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    k_mutex_lock(&blink_mutex, K_FOREVER);
    selected_page--;
    if(selected_page<0){
        selected_page=0;
    }
    k_mutex_unlock(&blink_mutex);
    return 0;
}

static int cmd_page_right(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    k_mutex_lock(&blink_mutex, K_FOREVER);
    selected_page++;
    if(selected_page>2){
        selected_page=2;
    }
    k_mutex_unlock(&blink_mutex);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_page,
                               SHELL_CMD(left, NULL, "Navigate left of currently selected page", cmd_page_left),
                               SHELL_CMD(right, NULL, "Navigate right of currently selected page", cmd_page_right),
                               SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(page, &sub_page, "Page navigation", NULL);


static int cmd_meow(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "mrrp~");
    // printk("mrieoiowww!!\n"); // printk also works but is a bit "slower", takes time to actually appear. i think it goes to another buffer theneverything in the bfufer is displayed at once
    return 0;
}

SHELL_CMD_REGISTER(meow, NULL, "Ping but kitty", cmd_meow);

static int cmd_reboot(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "Rebooting now");
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}

SHELL_CMD_REGISTER(reboot, NULL, "Reboot device", cmd_reboot);

static int cmd_dfu(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "Rebooting with DFU now");
    NRF_POWER->GPREGRET = 0x57;
    sys_reboot(SYS_REBOOT_WARM);
    return 0;
}

SHELL_CMD_REGISTER(dfu, NULL, "Enter DFU mode", cmd_dfu); // todo: conditionally compile this for different bootloaders


extern lua_State* L;

void cmd_lua_loadfile(const struct shell * shell, size_t argc, char ** argv) {
    if (argc == 2) {
        char* path = argv[1];

        // Load init.lua
        struct fs_file_t init;
        fs_file_t_init(&init);
        if(0 == fs_open(&init, path, 1)) {
            fs_seek(&init, 0, FS_SEEK_END);
            size_t size = fs_tell(&init);
            char * str = malloc(size+1);
            str[size] = 0;
            fs_seek(&init, 0, FS_SEEK_SET);
            fs_read(&init, str, size);
            int err = luaL_dostring(L, str);
            if (err) {shell_print(shell, "Lua file executed with errors: %i", err);} else {
                shell_print(shell, "Lua file executed successfully.");
            }
            free(str);
        } else {
            shell_print(shell, "Could not open the file %s", path);
        }
        fs_close(&init);
    } else {
        shell_print(shell, "Invalid # of arguments %i", argc);
    }
}

void cmd_lua(const struct shell * shell, size_t argc, char ** argv) {
    if (argc == 2) {
        char* arg = argv[1];
        luaL_loadstring(L, arg);
        int err = lua_pcall(L, 0, 0, 0);
        if (err) {
            const char* str = luaL_checkstring(L, 1);
            if (str) {
                shell_print(shell, "<ERR %i: %s>", err, str);
                lua_pop(L, 1);
            } else {
                shell_print(shell, "<ERR %i>", err);
            }
        }
        fflush(stdout);
    } else if (argc == 1) {
        shell_print(shell, "Lua 5.4.4 %i", L);
    } else {
        shell_print(shell, "Invalid # of arguments %i", argc);
    }
}

SHELL_CMD_REGISTER(lua, NULL, "Execute lua code", cmd_lua);
SHELL_CMD_REGISTER(lua_lf, NULL, "Run the given lua file", cmd_lua_loadfile);