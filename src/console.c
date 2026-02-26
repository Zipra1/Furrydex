#include <stdlib.h>
#include <zephyr/sys/reboot.h>
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