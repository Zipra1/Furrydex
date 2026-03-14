#include <stdlib.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/shell/shell.h>
#include <stdio.h>
#include <string.h>
#include "console.h"

K_MUTEX_DEFINE(blink_mutex);

const int32_t blink_max_ms = 2000;
const int32_t blink_min_ms = 100;
int32_t blink_sleep_ms = 500;

K_MUTEX_DEFINE(page_select_mutex);
int selected_page = 0;

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