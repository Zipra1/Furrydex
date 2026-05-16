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
#include "lua_thread.h"
#include "luazephyrlib.h"
#include "imgdata.h"

atomic_t selected_page = ATOMIC_INIT(0);

void page_left()
{
    atomic_dec(&selected_page);
    if (atomic_get(&selected_page) < 0)
    {
        atomic_set(&selected_page, 0);
    }
    lua_thread_update_priorities(atomic_get(&selected_page));
}

void page_right()
{
    atomic_inc(&selected_page);
    if (atomic_get(&selected_page) > num_lua_threads)
    {
        atomic_set(&selected_page, num_lua_threads);
    }
    lua_thread_update_priorities(atomic_get(&selected_page));
}

void draw_ui()
{
    k_mutex_lock(&paint_mutex, K_FOREVER);
    paintPageBubbles(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, num_lua_threads + 1, atomic_get(&selected_page));
    k_mutex_unlock(&paint_mutex);
}

static void ui_thread(void *a, void *b, void *c)
{
    int prev_inputs = 0;

    while (1)
    {
        k_sem_take(&input_sem, K_FOREVER);

        int newly_pressed = atomic_get(&inputs) & ~prev_inputs;
        if (lua_slots[atomic_get(&selected_page)].capture_input != LUA_INPUT_CAPTURE)
        {
            if (get_bit(newly_pressed, CONFIG_FURRYDEX_INPUT_LEFT))
            {
                page_left();
            }
            if (get_bit(newly_pressed, CONFIG_FURRYDEX_INPUT_RIGHT))
            {
                page_right();
            }
        }

        prev_inputs = atomic_get(&inputs);
    }
}

K_THREAD_DEFINE(ui_tid, 1024, ui_thread, NULL, NULL, NULL, 10, 0, 0);
// SYS_INIT(input_init2, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);