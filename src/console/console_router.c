// Jank is all we have left. There is no room for fear here.
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <string.h>
#include <stdio.h>
#include "console_router.h"

K_MSGQ_DEFINE(lua_serial_msgq, LUA_SERIAL_MSG_SIZE, 8, 4);

static char router_line[LUA_SERIAL_MSG_SIZE];
static size_t router_len = 0;

static const char *const known_commands[] = {
    "page",
    "meow",
    "reboot",
    "dfu",
    "lua_lf",
    "lua_k",
    "lua",
    "paint",
    "display",
    "info",
    "ls",
    "clear",
    "history",
    "resize",
    "help",
    "kernel",
    "stats",
    "date",
    "device",
    "devmem",
    "rem",
    "retval",
    "shell",
    "lpm",
    "hpm",
    "fps",
    NULL,
};

static bool is_registered_command(const char *word)
{
    for (int i = 0; known_commands[i] != NULL; i++)
    {
        if (strcmp(known_commands[i], word) == 0)
        {
            return true;
        }
    }
    return false;
}

#define SHELL_CMD_MSGQ_LEN 4
K_MSGQ_DEFINE(shell_cmd_msgq, LUA_SERIAL_MSG_SIZE, SHELL_CMD_MSGQ_LEN, 4);

K_THREAD_STACK_DEFINE(console_cmd_stack, 4096);
static struct k_thread console_cmd_thread_data;

static void console_cmd_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    const struct shell *sh = shell_backend_uart_get_ptr();
    char line[LUA_SERIAL_MSG_SIZE];

    while (1)
    {
        if (k_msgq_get(&shell_cmd_msgq, line, K_FOREVER) == 0)
        {
            shell_execute_cmd(sh, line);
        }
    }
}

struct cmd_work_item
{
    struct k_work work;
    const struct shell *sh;
    char line[LUA_SERIAL_MSG_SIZE];
};
static struct cmd_work_item cmd_work;

static void run_queued_command(struct k_work *work)
{
    struct cmd_work_item *item = CONTAINER_OF(work, struct cmd_work_item, work);
    shell_execute_cmd(item->sh, item->line);
}

static void console_router_cb(const struct shell *sh, uint8_t *data, size_t len, void *user_data)
{
    for (size_t i = 0; i < len; i++)
    {
        uint8_t c = data[i];

        if (c == '\b' || c == 0x7F)
        {
            if (router_len > 0)
            {
                router_len--;
                shell_fprintf(sh, SHELL_NORMAL, "\b \b");
            }
            continue;
        }

        shell_fprintf(sh, SHELL_NORMAL, "%c", c);

        if (c == '\n' || c == '\r')
        {
            if (router_len == 0)
                continue;
            router_line[router_len] = '\0';

            char first_word[32] = {0};
            sscanf(router_line, "%31s", first_word);

            if (is_registered_command(first_word))
            {
                if (k_msgq_put(&shell_cmd_msgq, router_line, K_NO_WAIT) != 0)
                {
                    shell_fprintf(sh, SHELL_WARNING, "\r\n[command queue full, dropped]\r\n");
                }
            }
            else
            {
                k_msgq_put(&lua_serial_msgq, router_line, K_NO_WAIT);
            }
            router_len = 0;
        }
        else if (router_len < LUA_SERIAL_MSG_SIZE - 1)
        {
            router_line[router_len++] = c;
        }
    }
}

void console_router_init(void)
{
    const struct shell *sh = shell_backend_uart_get_ptr();

    if (sh == NULL)
    {
        printk("console_router_init: no shell UART backend found\n");
        return;
    }

    k_thread_create(&console_cmd_thread_data, console_cmd_stack,
                    K_THREAD_STACK_SIZEOF(console_cmd_stack),
                    console_cmd_thread, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(10), 0, K_NO_WAIT);

    shell_set_bypass(sh, console_router_cb, NULL);
}