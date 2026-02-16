/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/console/console.h>
#include <stdbool.h>
#include <stdlib.h>
#include "lcd.h"
#include "imgdata.h"
#include "font8.h"
#include "paint.h"

#define SLEEP_TIME_MS 1000
#define BLINK_THREAD_STACK_SIZE 512 // 256 is the "bare minimum", 512 is small, most threads start at 1024
#define INPUT_THREAD_STACK_SIZE 512

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Thread example settings
static const int32_t blink_max_ms = 2000;
static const int32_t blink_min_ms = 0;

static struct k_thread blink_thread;
static struct k_thread input_thread;

K_MUTEX_DEFINE(my_mutex);

static int32_t blink_sleep_ms = 500;

K_THREAD_STACK_DEFINE(blink_stack, BLINK_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(input_stack, INPUT_THREAD_STACK_SIZE);

void input_thread_start(void *arg_1, void *arg_2, void *arg_3)
{
    int8_t inc;

    printk("Starting input thread\n");

    while (1)
    {
        const char *line = console_getline();

        if (line[0] == '+')
        {
            inc = 1;
        }
        else if (line[0] == '-')
        {
            inc = -1;
        }
        else
        {
            continue;
        }

        // Mutex is used to prevent other threads from accessing the variable
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
}

void blink_thread_start(void *arg_1, void *arg_2, void *arg_3)
{
    int ret;
    int state = 0;
    int32_t sleep_ms;

    printf("Starting blink thread\n");

    while (1)
    {

        k_mutex_lock(&my_mutex, K_FOREVER);
        sleep_ms = blink_sleep_ms; // Locking mutex while using the shared variable
        k_mutex_unlock(&my_mutex);

        state = !state;

        ret = gpio_pin_set_dt(&led, state);
        if (ret < 0)
        { // Printing to terminal within a thread is apparantly not good.
            printf("Could not toggle LED\n");
        }
        k_msleep(sleep_ms);
    }
}

unsigned char output_buffer[CONFIG_FURRYDEX_EPD_MAX_BYTES];

int main(void)
{
    int ret;
    k_tid_t input_tid;
    k_tid_t blink_tid; // this is a thread ID. Can be used to do things to the thread.

    if (!gpio_is_ready_dt(&led))
    {
        printf("Error! GPIO pin not ready\n");
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0)
    {
        printf("Error! Could not configure led GPIO pin\n");
        return 0;
    }

    console_getline_init(); // Initialize console

    input_tid = k_thread_create(&input_thread,
                                input_stack,
                                K_THREAD_STACK_SIZEOF(input_stack),
                                input_thread_start,
                                NULL,
                                NULL,
                                NULL,
                                7,
                                0,
                                K_NO_WAIT);

    blink_tid = k_thread_create(&blink_thread, // Thread struct
                                blink_stack,   // Stack
                                K_THREAD_STACK_SIZEOF(blink_stack),
                                blink_thread_start, // Entry point (function)
                                NULL,               // arg_1
                                NULL,               // arg_2
                                NULL,               // arg_3
                                8,                  // Priority. Lower = more important. There are also negatives, but see zephyr docs for when to use those. (For cooperative and preemptible threads). Main thread has priority 0.
                                0,                  // Options, see "Thread Options" in Zephyr. Can use multiple. K_ESSENTIAL treats the end as a fatal system error. K_FP_REGS can help with floating point math(?)
                                K_NO_WAIT);         // Tels kernel how long to wait before making thread

    initDisplay();
    printf("Display initialized\n");
    while (true)
    {
        printf("One\n");
        Display(25, 0, 36, 125, IMAGE_DATA);
        k_msleep(1000);
        printf("Two\n");
        paintRegion(IMAGE_DATA2, 136, 250, 10, 0, 60, 50, 0); // why 136? memory width is 138... Weird!!
        paintRegion(IMAGE_DATA2, 136, 250, 82, 200, 132, 250, 0);
        paintPixel(IMAGE_DATA2, 136, 250, 131, 249, 1);
        paintPixel(IMAGE_DATA2, 136, 250, 130, 248, 1);
        paintText("meow! :3", IMAGE_DATA2, 1, 15, 15);
        paintTextWrap("What the fuck did you just fucking say about me, you little bitch? I'll have you know I graduated top of my class in the Navy Seals, and I've been involved in numerous secret raids on Al-Quaeda, and I have over 300 confirmed kills. I am trained in gorilla warfare and I'm the top sniper in the entire US armed forces. You are nothing to me but just another target. I will wipe you the fuck out with precision the likes of which has never been seen before on this Earth, mark my fucking words.", IMAGE_DATA2, 1, 11, 1, 121);
        convertBuffer(IMAGE_DATA2, output_buffer);
        Display(25, 0, 36, 125, output_buffer);
        // paintRegion(IMAGE_DATA, 20, 20, 120, 120, 0);
        k_msleep(5000);
    }
    return 0;
}
