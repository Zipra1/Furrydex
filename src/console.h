#ifndef CONSOLE_H
#define CONSOLE_H

#include <zephyr/kernel.h>

extern struct k_mutex my_mutex;
extern const int32_t blink_max_ms;
extern const int32_t blink_min_ms;
extern int32_t blink_sleep_ms;
void input_thread_start(void *arg_1, void *arg_2, void *arg_3);

#endif