#include <stdlib.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/shell/shell.h>
#include <stdio.h>
#include <string.h>
#include "console.h"

K_MUTEX_DEFINE(my_mutex);

const int32_t blink_max_ms = 2000;
const int32_t blink_min_ms = 100;
int32_t blink_sleep_ms = 500;

void input_thread_start(void *arg_1, void *arg_2, void *arg_3)
{
    int8_t inc = 0;
    k_mutex_lock(&my_mutex, K_FOREVER);
    blink_sleep_ms += (int32_t)inc * 100;
    if (blink_sleep_ms > blink_max_ms)
    {
        blink_sleep_ms = blink_max_ms;
    }
    else if (blink_sleep_ms < blink_min_ms)
    {
        blink_sleep_ms = blink_min_ms;
    }
    k_mutex_unlock(&my_mutex);
    printf("Updating blink sleep to: %d\n", blink_sleep_ms);
}

static int cmd_meow(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "mrrp~");
    // printk("mrieoiowww!!\n"); // printk also works but is a bit "slower", takes time to actually appear. i think it goes to another buffer theneverything in the bfufer is displayed at once
    return 0;
}


static int cmd_reboot(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "Rebooting now");
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}

static int cmd_dfu(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "Rebooting with DFU now");
    NRF_POWER->GPREGRET = 0x57;
    sys_reboot(SYS_REBOOT_WARM);
    return 0;
}

SHELL_CMD_REGISTER(meow, NULL, "Ping but kitty", cmd_meow);
SHELL_CMD_REGISTER(reboot, NULL, "Reboot device", cmd_reboot);
SHELL_CMD_REGISTER(dfu, NULL, "Enter DFU mode", cmd_dfu); // todo: conditionally compile this for different bootloaders