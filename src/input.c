#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include "input.h"
#include "paint.h"
#include "luazephyrlib.h"
#include "drivers/74HC165.h"
#include "imgdata.h"
#include <stdio.h>


#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0')

  
static void input_thread(void *a, void *b, void *c)
{
    k_msleep(2000);
    while (1)
    {
        int inputs = SR165Read();
        if (inputs < 0)
        {
            printk("SR165 read failed\n");
        }
        else
        {
            char bin_str[16];
            sprintf(bin_str, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(inputs));
            k_mutex_lock(&paint_mutex, K_FOREVER);
            paintText(IMAGE_DATA2, 1, 40, 30, bin_str);
            k_mutex_unlock(&paint_mutex);
        }
        k_msleep(30);
    }
}

K_THREAD_DEFINE(input_tid, 1024, input_thread, NULL, NULL, NULL, 10, 0, 0);

// SYS_INIT(input_init2, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);