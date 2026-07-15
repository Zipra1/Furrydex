#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>

#include <inttypes.h>
#include <stdio.h>

#include "paint.h"
#include "battery.h"
#include "input.h"
#include "lua_thread.h"
#include "luazephyrlib.h"
#include "imgdata.h"

atomic_t selected_page = ATOMIC_INIT(0);

void page_left()
{
    int original_page = atomic_get(&selected_page);
    while (true)
    {
        atomic_dec(&selected_page);
        if (atomic_get(&selected_page) < 0)
        {
            atomic_set(&selected_page, original_page);
            break;
        }
        if (lua_slots[atomic_get(&selected_page)].in_tray == false)
        {
            break;
        }
    }
    lua_thread_update_priorities(atomic_get(&selected_page));
    update_visible_lua_slot_index();
}

void page_right()
{
    int original_page = atomic_get(&selected_page);
    while (true)
    {
        atomic_inc(&selected_page);
        if (atomic_get(&selected_page) > num_lua_threads)
        {
            atomic_set(&selected_page, original_page);
            break;
        }
        if (lua_slots[atomic_get(&selected_page)].in_tray == false)
        {
            break;
        }
    }
    lua_thread_update_priorities(atomic_get(&selected_page));
    update_visible_lua_slot_index();
}

void update_tray_icons()
{
    int length = sizeof(lua_slots) / sizeof(lua_slots[0]);
    int i;
    int i_trayed;
    for (i = 0; i < length; i++)
    {
        if (lua_slots[i].in_tray && lua_slots[i].icon)
        {
            blit(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, lua_slots[i].icon, 8, 8, i*8, 1);
            i_trayed++;
        }
    }
}

void draw_ui()
{
    int battery_width = 30;
    int battery_whitespace = 4;
    float battery_percent_divider = 100.0 / ((CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_whitespace - 1) - (CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width + 1) - 2);

    k_mutex_lock(&paint_mutex, K_FOREVER);

    if (!lua_slots[atomic_get(&selected_page)].hide_top)
    {
        update_tray_icons();
        // Battery indicator
        paintRegion(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 1, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_whitespace - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 9, 0);                                                               // battery body
        paintRegion(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, CONFIG_FURRYDEX_DISPLAY_WIDTH - 4 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 3, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_whitespace + 2 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 7, 0);                                                                       // battery bump
        paintRegion(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width + 1 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 2, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_whitespace - 1 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 8, 1);                                                       // battery white
        paintRegion(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width + 2 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 3, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width + 2 + (atomic_get(&battery_percent) / battery_percent_divider) - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 7, 0); // battery fill
    }
    if (!lua_slots[atomic_get(&selected_page)].hide_bottom)
    {
        paintPageBubbles(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, num_shown_lua_threads + 1, atomic_get(&visible_slot_index));
    }
    k_mutex_unlock(&paint_mutex);
}

static void ui_thread(void *a, void *b, void *c)
{
    int prev_inputs = 0;

    while (1)
    {
        k_sem_take(&input_sem, K_FOREVER); // wait for a button to be pressed

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