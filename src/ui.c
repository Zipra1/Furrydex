#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>

#include <inttypes.h>
#include <stdio.h>

#include "paint.h"
#include "input.h"

atomic_t selected_page = ATOMIC_INIT(0);

void page_left()
{
    atomic_dec(&selected_page);
    if (atomic_get(&selected_page) < 0)
    {
        atomic_set(&selected_page, 0);
    }
}

void page_right()
{
    atomic_inc(&selected_page);
    if (atomic_get(&selected_page) > 2)
    {
        atomic_set(&selected_page, 2);
    }
}

static void ui_thread(void *a, void *b, void *c)
{
    while (1)
    {
        k_sem_take(&input_sem, K_FOREVER);
        if (get_bit(inputs, CONFIG_FURRYDEX_INPUT_LEFT))
        {
            page_left();
        }
        if (get_bit(inputs, CONFIG_FURRYDEX_INPUT_RIGHT))
        {
            page_right();
        }
    }
}

K_THREAD_DEFINE(ui_tid, 2048, ui_thread, NULL, NULL, NULL, 10, 0, 0);
// SYS_INIT(input_init2, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);