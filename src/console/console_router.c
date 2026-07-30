// console_router.c
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/iterable_sections.h>
#include <string.h>
#include <stdio.h>

K_MSGQ_DEFINE(lua_serial_msgq, 128, 8, 4);

#define ROUTER_LINE_MAX 128
static char router_line[ROUTER_LINE_MAX];
static size_t router_len = 0;

struct cmd_work_item {
    struct k_work work;
    const struct shell *sh;
    char line[ROUTER_LINE_MAX];
};
static struct cmd_work_item cmd_work;

static void run_queued_command(struct k_work *work)
{
    struct cmd_work_item *item = CONTAINER_OF(work, struct cmd_work_item, work);
    shell_execute_cmd(item->sh, item->line);   // safe here: runs on the system workqueue,
                                                // after the bypass call has returned
}

static bool is_registered_command(const char *word)
{
    TYPE_SECTION_FOREACH(union shell_cmd_entry, shell_root_cmds, cmd) {
        const struct shell_static_entry *entry = cmd->entry;
        if (entry && entry->syntax && strcmp(entry->syntax, word) == 0) {
            return true;
        }
    }
    return false;
}

static void console_router_cb(const struct shell *sh, uint8_t *data, size_t len, void *user_data)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];

        if (c == '\b' || c == 0x7F) {
            if (router_len > 0) {
                router_len--;
                shell_fprintf(sh, SHELL_NORMAL, "\b \b");
            }
            continue;
        }

        shell_fprintf(sh, SHELL_NORMAL, "%c", c);   // echo typed char back

        if (c == '\n' || c == '\r') {
            if (router_len == 0) continue;
            router_line[router_len] = '\0';

            char first_word[32] = {0};
            sscanf(router_line, "%31s", first_word);

            if (is_registered_command(first_word)) {
                cmd_work.sh = sh;
                strncpy(cmd_work.line, router_line, sizeof(cmd_work.line) - 1);
                k_work_submit(&cmd_work.work);      // deferred, off the bypass callback
            } else {
                k_msgq_put(&lua_serial_msgq, router_line, K_NO_WAIT);
            }
            router_len = 0;
        } else if (router_len < ROUTER_LINE_MAX - 1) {
            router_line[router_len++] = c;
        }
    }
}

void console_router_init(void)
{
    const struct shell *sh = shell_backend_uart_get_ptr();  // the CDC-ACM shell instance

    while (!shell_ready(sh)) {
        k_msleep(10);
    }

    k_work_init(&cmd_work.work, run_queued_command);
    shell_set_bypass(sh, console_router_cb, NULL);
}