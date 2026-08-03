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
#include "fonts/font8.h"

atomic_t selected_page = ATOMIC_INIT(0);

static int find_first_visible_lua_slot(void)
{
    for (int i = 0; i < CONFIG_LUA_MAX_THREADS; i++)
    {
        if (lua_slots[i].in_use && !lua_slots[i].in_tray)
        {
            return i;
        }
    }
    return -1;
}

static int find_last_visible_lua_slot(void)
{
    for (int i = CONFIG_LUA_MAX_THREADS - 1; i >= 0; i--)
    {
        if (lua_slots[i].in_use && !lua_slots[i].in_tray)
        {
            return i;
        }
    }
    return -1;
}

static int find_next_visible_lua_slot(int from)
{
    for (int i = from + 1; i < CONFIG_LUA_MAX_THREADS; i++)
    {
        if (lua_slots[i].in_use && !lua_slots[i].in_tray)
        {
            return i;
        }
    }
    return -1;
}

static int find_previous_visible_lua_slot(int from)
{
    for (int i = from - 1; i >= 0; i--)
    {
        if (lua_slots[i].in_use && !lua_slots[i].in_tray)
        {
            return i;
        }
    }
    return -1;
}

void page_left()
{
    int current = atomic_get(&selected_page);
    if (current < 0)
    {
        int last = find_last_visible_lua_slot();
        if (last >= 0)
        {
            atomic_set(&selected_page, last);
        }
    }
    else
    {
        int previous = find_previous_visible_lua_slot(current);
        if (previous >= 0)
        {
            atomic_set(&selected_page, previous);
        }
        else
        {
            int last = find_last_visible_lua_slot();
            if (last >= 0)
            {
                atomic_set(&selected_page, last);
            }
        }
    }

    lua_thread_update_priorities(atomic_get(&selected_page));
    update_visible_lua_slot_index();
}

void page_right()
{
    int current = atomic_get(&selected_page);
    if (current < 0)
    {
        int first = find_first_visible_lua_slot();
        if (first >= 0)
        {
            atomic_set(&selected_page, first);
        }
    }
    else
    {
        int next = find_next_visible_lua_slot(current);
        if (next >= 0)
        {
            atomic_set(&selected_page, next);
        }
        else
        {
            atomic_set(&selected_page, -1);
        }
    }

    lua_thread_update_priorities(atomic_get(&selected_page));
    update_visible_lua_slot_index();
}

void update_tray_icons()
{
    int length = sizeof(lua_slots) / sizeof(lua_slots[0]);
    int tray_index = 0;

    for (int i = 0; i < length; i++)
    {
        if (!lua_slots[i].in_tray)
        {
            continue;
        }

        int x = tray_index * 8;
        if (lua_slots[i].icon)
        {
            blit(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, lua_slots[i].icon, 8, 8, x, 1);
        }
        else
        {
            blit(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, lua_default_tray_icon, 8, 8, x, 1);
        }
        tray_index++;
    }
}

void draw_battery_indicator()
{
    int battery_width = 30;
    int battery_whitespace = 4;
    float battery_percent_divider = 100.0 / ((CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_whitespace - 1) - (CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width + 1) - 2);
    paintRegion(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 1, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_whitespace - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 9, 0);                                                               // battery body
    paintRegion(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, CONFIG_FURRYDEX_DISPLAY_WIDTH - 4 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 3, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_whitespace + 2 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 7, 0);                                                                       // battery bump
    paintRegion(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width + 1 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 2, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_whitespace - 1 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 8, 1);                                                       // battery white
    paintRegion(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width + 2 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 3, CONFIG_FURRYDEX_DISPLAY_WIDTH - battery_width + 2 + (atomic_get(&battery_percent) / battery_percent_divider) - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, 7, 0); // battery fill
}

void draw_ui()
{
    int selected = atomic_get(&selected_page);

    k_mutex_lock(&paint_mutex, K_FOREVER);

    if (selected < 0)
    {
        memset(main_buffer, 0xFF, CONFIG_FURRYDEX_FRAME_BYTES_BUFFER);
        update_tray_icons();
        draw_battery_indicator();
        paintPageBubbles(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, num_shown_lua_threads + 1, atomic_get(&visible_slot_index));
        paintText(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, 1, 4, 12, "Placeholder", font_8);
    }
    else
    {
        if (!lua_slots[selected].hide_top)
        {
            update_tray_icons();
            draw_battery_indicator();
        }
        if (!lua_slots[selected].hide_bottom)
        {
            paintPageBubbles(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, num_shown_lua_threads + 1, atomic_get(&visible_slot_index));
        }
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
        int selected = atomic_get(&selected_page);
        if (selected < 0 || lua_slots[selected].capture_input != LUA_INPUT_CAPTURE)
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