#ifndef UI_H
#define UI_H

#include <zephyr/sys/atomic.h>

extern atomic_t selected_page;
void page_left();
void page_right();
void draw_ui();

#endif