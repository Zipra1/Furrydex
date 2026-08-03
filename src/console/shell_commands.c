#include <stdlib.h>
#include <errno.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <stdio.h>
#include <string.h>
#include "shell_commands.h"

#include <zephyr/fs/fs.h>
#include <ff.h>
#include "../lua/lauxlib.h"
#include "../lua/lua.h"
#include "../ui.h"
#include "../paint.h"
#include "../disk.h"
#include "../lua_thread.h"
#include "../drivers/ST7305.h"
#include "../lua_thread.h"
#include "../imgdata.h"

K_MUTEX_DEFINE(page_select_mutex);

// const int32_t blink_max_ms = 2000;
// const int32_t blink_min_ms = 100;
// int32_t blink_sleep_ms = 500;

// static int cmd_blink_inc(const struct shell *sh, size_t argc, char **argv)
// {
//     ARG_UNUSED(argc);
//     ARG_UNUSED(argv);
//     k_mutex_lock(&page_select_mutex, K_FOREVER);
//     blink_sleep_ms = MIN(blink_sleep_ms + 100, blink_max_ms);
//     k_mutex_unlock(&page_select_mutex);
//     shell_print(sh, "Blink sleep: %d ms", blink_sleep_ms);
//     return 0;
// }

// static int cmd_blink_dec(const struct shell *sh, size_t argc, char **argv)
// {
//     ARG_UNUSED(argc);
//     ARG_UNUSED(argv);
//     k_mutex_lock(&page_select_mutex, K_FOREVER);
//     blink_sleep_ms = MAX(blink_sleep_ms - 100, blink_min_ms);
//     k_mutex_unlock(&page_select_mutex);
//     shell_print(sh, "Blink sleep: %d ms", blink_sleep_ms);
//     return 0;
// }

// SHELL_STATIC_SUBCMD_SET_CREATE(sub_blink,
//                                SHELL_CMD(inc, NULL, "Increase blink speed by 100ms", cmd_blink_inc),
//                                SHELL_CMD(dec, NULL, "Decrease blink speed by 100ms", cmd_blink_dec),
//                                SHELL_SUBCMD_SET_END);
// SHELL_CMD_REGISTER(blink, &sub_blink, "Blink speed controls", NULL);

static int cmd_page_left(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    page_left();
    return 0;
}

static int cmd_page_right(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    page_right();
    return 0;
}

static int cmd_page_get(const struct shell *sh, size_t argc, char **argv)
{
    int selected = atomic_get(&selected_page);

    if (argc > 2)
    {
        shell_print(sh, "Usage: page get [slot]");
        return 0;
    }

    if (argc == 2)
    {
        selected = atoi(argv[1]);
    }

    if (selected < 0 || selected >= CONFIG_LUA_MAX_THREADS)
    {
        shell_print(sh, "selected page %d is out of range", selected);
        return 0;
    }

    lua_thread_slot_t *slot = &lua_slots[selected];
    shell_print(sh,
                "slot=%d\nin_use=%d\nin_tray=%d\nhide_top=%d\nhide_bottom=%d\ncapture_input=%d\nkill_requested=%d\nhas_icon=%d\npriority=%d\nname=%s\ncurrent visible slot=%d",
                selected,
                slot->in_use,
                slot->in_tray,
                slot->hide_top,
                slot->hide_bottom,
                slot->capture_input,
                slot->kill_requested,
                slot->icon != NULL,
                k_thread_priority_get(&slot->thread),
                slot->name,
                atomic_get(&visible_slot_index));
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_page,
                               SHELL_CMD(left, NULL, "Navigate left of currently selected page", cmd_page_left),
                               SHELL_CMD(right, NULL, "Navigate right of currently selected page", cmd_page_right),
                               SHELL_CMD(get, NULL, "Print currently selected page", cmd_page_get),
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

#ifdef CONFIG_BOARD_PROMICRO
static int cmd_dfu(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "Rebooting with DFU now");
    NRF_POWER->GPREGRET = 0x57;
    sys_reboot(SYS_REBOOT_WARM);
    return 0;
}

SHELL_CMD_REGISTER(dfu, NULL, "Enter DFU mode", cmd_dfu);
#endif

// LUA

extern lua_State *L;

void cmd_lua_loadfile(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "Invalid # of arguments %i", argc);
        return;
    }

    struct fs_file_t init;
    fs_file_t_init(&init);

    if (0 != fs_open(&init, argv[1], 1))
    {
        shell_print(shell, "Could not open file %s", argv[1]);
        return;
    }

    fs_seek(&init, 0, FS_SEEK_END);
    size_t size = fs_tell(&init);
    char *script = malloc(size + 1);
    script[size] = 0;
    fs_seek(&init, 0, FS_SEEK_SET);
    fs_read(&init, script, size);
    fs_close(&init);

    int slot = lua_thread_start(shell, script, argv[1]);
    if (slot < 0)
    {
        shell_print(shell, "Max concurrent Lua scripts (%d) already running.", CONFIG_LUA_MAX_THREADS);
        free(script);
    }
    else
    {
        shell_print(shell, "Lua script started in slot %d.", slot);
    }
}
SHELL_CMD_REGISTER(lua_lf, NULL, "Run the given lua file", cmd_lua_loadfile);

void cmd_lua_kill(const struct shell *shell, size_t argc, char **argv)
{
    if (argc != 2)
    {
        shell_print(shell, "Incorrect number of arguments. Usage: lua_kill(slot)");
    }
    if (lua_thread_kill(atoi(argv[1])) == 0)
    {
        shell_print(shell, "Lua script in slot %s killed", argv[1]);
    }
    else
    {
        shell_print(shell, "Failed to kill lua script in slot %s", argv[1]);
    }
}
SHELL_CMD_REGISTER(lua_k, NULL, "Kill lua file in given slot", cmd_lua_kill);

void cmd_lua(const struct shell *shell, size_t argc, char **argv)
{
    if (argc == 2)
    {
        char *arg = argv[1];
        luaL_loadstring(L, arg);
        int err = lua_pcall(L, 0, 0, 0);
        if (err)
        {
            const char *str = lua_tostring(L, -1);
            if (str)
            {
                shell_print(shell, "<ERR %i: %s>", err, str);
                lua_pop(L, 1);
            }
            else
            {
                shell_print(shell, "<ERR %i>", err);
            }
        }
        fflush(stdout);
    }
    else if (argc == 1)
    {
        shell_print(shell, "Lua 5.4.4");
    }
    else
    {
        shell_print(shell, "Invalid # of arguments %i", argc);
    }
}

SHELL_CMD_REGISTER(lua, NULL, "Execute lua code", cmd_lua);

// PAINT

static int cmd_paint_circle(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 5)
    {
        shell_error(sh, "Usage: paint circle <x> <y> <r> <c>");
        return -EINVAL;
    }

    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    int r = atoi(argv[3]);
    int c = atoi(argv[4]);

    paintFilledCircle(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, x, y, r, c);

    return 0;
}

static int cmd_paint_bubbles(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 3)
    {
        shell_error(sh, "Usage: paint bubbles <num> <selected>");
        return -EINVAL;
    }

    int num_bubbles = atoi(argv[1]);
    int selected_bubble = atoi(argv[2]);

    paintPageBubbles(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, num_bubbles, selected_bubble);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(paint_cmds,
                               SHELL_CMD_ARG(circle, NULL, "Paint a filled circle <x> <y> <r> <c>", cmd_paint_circle, 5, 0),
                               SHELL_CMD_ARG(bubbles, NULL, "Paint page bubbles <num> <selected>", cmd_paint_bubbles, 3, 0),
                               SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(paint, &paint_cmds, "Manually paint to the display buffer", NULL);

static int cmd_display(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2)
    {
        shell_error(sh, "Usage: display <command> [data ...]");
        return -EINVAL;
    }

    char *end = NULL;
    errno = 0;
    long command = strtol(argv[1], &end, 0);
    if (errno != 0 || end == argv[1] || *end != '\0' || command < 0 || command > 0xFF)
    {
        shell_error(sh, "Invalid command value '%s'", argv[1]);
        return -EINVAL;
    }

    sendCommand((uint8_t)command);

    for (size_t i = 2; i < argc; i++)
    {
        errno = 0;
        long value = strtol(argv[i], &end, 0);
        if (errno != 0 || end == argv[i] || *end != '\0' || value < 0 || value > 0xFF)
        {
            shell_error(sh, "Invalid data value '%s'", argv[i]);
            return -EINVAL;
        }

        sendData((uint8_t)value);
    }

    shell_print(sh, "Sent display command 0x%02X with %zu data byte(s)",
                (unsigned int)(uint8_t)command, argc - 2);
    return 0;
}

SHELL_CMD_REGISTER(display, NULL, "Send raw display commands and data bytes", cmd_display);

// INFO

static int cmd_info(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Built %s %s", __DATE__, __TIME__);
    shell_print(sh, "Board: %s", CONFIG_BOARD);

#ifdef CONFIG_FURRYDEX_DISPLAY_TYPE_LCD
    shell_print(sh, "Display type: LCD");
#endif

#ifdef CONFIG_FURRYDEX_DISPLAY_TYPE_EPD
    shell_print(sh, "Display type: EPD");
#endif

    shell_print(sh, "Resolution: %d x %d", CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT);

    int index = 0;
    const char *name;
    while (fs_readmount(&index, &name) == 0)
    {
        shell_print(sh, "Mount point [%d]: %s", index, name);
        index++;
    }
    if (index == 0)
    {
        shell_error(sh, "No disks mounted!!!");
    }

    return 0;
}

SHELL_CMD_REGISTER(info, NULL, "Print furrydex info to console", cmd_info);

// LS

static int cmd_ls(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (argc != 2)
    {
        shell_error(sh, "Usage: ls <directory>\nExample: ls /SD:/folder/");
        return -EINVAL;
    }

    lsdir_result_t list;

    if (lsdir(argv[1], &list) == 0)
    {
        for (int i = 0; i < list.count; i++)
        {
            const lsdir_entry_t *e = &list.entries[i];
            if (e->is_dir)
            {
                printk("[DIR ] %s\n", e->name);
            }
            else
            {
                printk("[FILE] %s (size = %zu)\n", e->name, e->size);
            }
        }
        lsdir_free(&list);
    }

    return 0;
}

SHELL_CMD_REGISTER(ls, NULL, "List files and directories of a directory", cmd_ls);