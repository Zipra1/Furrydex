#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/adc.h>
#include "input.h"
#include "luazephyrlib.h"
#include "AnalogIn.h"
#include "imgdata.h"

static void input_thread(void *a, void *b, void *c)
{
    k_msleep(2000);
    int ret = PinSetMode(7, true);
    printk("PinSetMode returned: %d\n", ret);

    while (1) {
        int16_t v = AnalogRead(7);
        if (v == BAD_ANALOG_READ) {
            printk("ADC read failed\n");
        } else {
            char mvstr[32];
            sprintf(mvstr, "%d     ", v);
            k_mutex_lock(&paint_mutex, K_FOREVER);
            paintText(IMAGE_DATA2, 1, 40, 30, mvstr);
            k_mutex_unlock(&paint_mutex);
        }
        k_msleep(30);
    }
}

K_THREAD_DEFINE(input_tid, 1024, input_thread, NULL, NULL, NULL, 10, 0, 0);

// SYS_INIT(input_init2, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);