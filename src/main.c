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
#include <stdbool.h>
#include <stdlib.h>
#include "lcd.h"
#include "imgdata.h"
#include "font8.h"
#include "paint.h"
#include "console.h"
#include "usb.h"
#include "luazephyrlib.h"
#include "disk.h"

#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>
#include <ff.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>


#define DISK_NAME "SD"

USBD_DEFINE_MSC_LUN(sd_lun, DISK_NAME, "Zephyr", "SD_Card", "1.00");

const struct device *const uart_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

#define SLEEP_TIME_MS 1000
#define BLINK_THREAD_STACK_SIZE 512 // 256 is the "bare minimum", 512 is small, most threads start at 1024

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

LOG_MODULE_REGISTER(main);

// static int lsdir(const char *path);


/*
 *  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
 *  in ffconf.h
 */


// usb start 2
static inline void print_baudrate(const struct device *dev)
{
    uint32_t baudrate;
    int ret;

    ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baudrate);
    if (ret)
    {
        LOG_WRN("Failed to get baudrate, ret code %d", ret);
    }
    else
    {
        LOG_INF("Baudrate %u", baudrate);
    }
}

static struct usbd_context *sample_usbd;
K_SEM_DEFINE(dtr_sem, 0, 1);

static void sample_msg_cb(struct usbd_context *const ctx, const struct usbd_msg *msg)
{
    LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

    if (usbd_can_detect_vbus(ctx))
    {
        if (msg->type == USBD_MSG_VBUS_READY)
        {
            if (usbd_enable(ctx))
            {
                LOG_ERR("Failed to enable device support");
            }
        }

        if (msg->type == USBD_MSG_VBUS_REMOVED)
        {
            if (usbd_disable(ctx))
            {
                LOG_ERR("Failed to disable device support");
            }
        }
    }

    if (msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE)
    {
        uint32_t dtr = 0U;

        uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_DTR, &dtr);
        if (dtr)
        {
            k_sem_give(&dtr_sem);
        }
    }

    if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING)
    {
        print_baudrate(msg->dev);
    }
}

static int enable_usb_device_next(void)
{
    int err;
    // k_msleep(500);
    // give PC time to connect. This lets us see all logs. I think i'm doing something wrong and this shouldn't be needed, but that's something for me to figure out when I'm more acquainted with Zephyr.
    sample_usbd = sample_usbd_init_device(sample_msg_cb);
    if (sample_usbd == NULL)
    {
        LOG_ERR("Failed to initialize USB device");
        return -ENODEV;
    }

    if (!usbd_can_detect_vbus(sample_usbd))
    {
        err = usbd_enable(sample_usbd);
        if (err)
        {
            LOG_ERR("Failed to enable device support");
            return err;
        }
    }

    LOG_INF("USB device support enabled");

    return 0;
}
// usb end 2

static struct k_thread blink_thread;

K_THREAD_STACK_DEFINE(blink_stack, BLINK_THREAD_STACK_SIZE);

void blink_thread_start(void *arg_1, void *arg_2, void *arg_3)
{
    int ret;
    int state = 0;
    int32_t sleep_ms;

    printk("Starting blink thread\n");

    while (1)
    {
        k_mutex_lock(&blink_mutex, K_FOREVER);
        sleep_ms = blink_sleep_ms; // Locking mutex while using the shared variable
        k_mutex_unlock(&blink_mutex);

        state = !state;

        ret = gpio_pin_set_dt(&led, state);
        if (ret < 0)
        { // Printing to terminal within a thread is apparantly not good.
            printk("Could not toggle LED\n");
        }
        k_msleep(sleep_ms);
    }
}
unsigned char output_buffer[CONFIG_FURRYDEX_FRAME_BYTES_BUFFER];
int main(void)
{
    int ret;

    initDisplay();
    printk("Display initialized\n");



    if (!device_is_ready(uart_dev))
    {
        LOG_ERR("CDC ACM device not ready");
        return 0;
    }

    ret = enable_usb_device_next();
    if (ret != 0)
    {
        LOG_ERR("Failed to enable USB device support");
        return 0;
    }

    k_msleep(100); // is this necessary

    // if (!gpio_is_ready_dt(&led))
    // {
    //     printk("Error! GPIO pin not ready\n");
    //     return 0;
    // }

    // ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    // if (ret < 0)
    // {
    //     printk("Error! Could not configure led GPIO pin\n");
    //     return 0;
    // }
    
    // k_tid_t blink_tid; // this is a thread ID. Can be used to do things to the thread.
    // blink_tid = k_thread_create(&blink_thread, // Thread struct
    //                             blink_stack,   // Stack
    //                             K_THREAD_STACK_SIZEOF(blink_stack),
    //                             blink_thread_start, // Entry point (function)
    //                             NULL,               // arg_1
    //                             NULL,               // arg_2
    //                             NULL,               // arg_3
    //                             9,                  // Priority. Lower = more important. There are also negatives, but see zephyr docs for when to use those. (For cooperative and preemptible threads). Main thread has priority 0.
    //                             0,                  // Options, see "Thread Options" in Zephyr. Can use multiple. K_ESSENTIAL treats the end as a fatal system error. K_FP_REGS can help with floating point math(?)
    //                             K_NO_WAIT);         // Tels kernel how long to wait before making thread

    if (mount_sd_card())
    {
        LOG_ERR("Failed to mount SD card");
        //return -1;
    }
    else
    {
        printk("Successfully mounted SD card\n");

        struct fs_file_t data_filp;
        fs_file_t_init(&data_filp);

        ret = fs_unlink("/SD:/test_data.txt");

        ret = fs_open(&data_filp, "/SD:/test_data.txt", FS_O_WRITE | FS_O_CREATE);
        if (ret)
        {
            printk("%s -- failed to create file (err = %d)\n", __func__, ret);
            return -2;
        }
        else
        {
            printk("%s - successfully created file\n", __func__);
        }

        char file_data_buffer[200];
        sprintf(file_data_buffer, "hello world!\n");
        ret = fs_write(&data_filp, file_data_buffer, strlen(file_data_buffer));
        fs_close(&data_filp);

        // bool force = true;
        // disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_DEINIT, &force);
        msc_enabled = false;
    }

    int i = -64;
    // int64_t start_time = k_uptime_get();
    // int64_t duration = k_uptime_get() - start_time;

    int selected_page_local = 0;

    //blit(IMAGE_DATA2, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, blit_test, 32, 32, 50, 100);
    //blitMask(IMAGE_DATA2, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, blit_test, 32, 32, blit_test, 50, 50);

    while (true)
    {
        k_mutex_lock(&blink_mutex, K_FOREVER);
        selected_page_local = selected_page;
        k_mutex_unlock(&blink_mutex);
        k_mutex_lock(&paint_mutex, K_FOREVER);
        // paintPageBubbles(IMAGE_DATA2, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, 2, selected_page_local);
        // start_time = k_uptime_get();
        if (msc_enabled)
        {
            paintTextWrap(IMAGE_DATA2, 1, 11, 0, 121, "SD Passthrough Enabled ");
        }
        else
        {
            paintTextWrap(IMAGE_DATA2, 1, 11, 0, 121, "SD Passthrough Disabled");
        }
        paintText(IMAGE_DATA2, 1, 16 + i, 15, "meow! :3");
        convertBuffer(IMAGE_DATA2, output_buffer);
        k_mutex_unlock(&paint_mutex);
        invert(output_buffer, CONFIG_FURRYDEX_FRAME_BYTES_DISPLAY);
        waitForTE();
        Display(25, 0, 36, 125, output_buffer);
        // paintRegion(IMAGE_DATA, 20, 20, 120, 120, 0);
        i++;
        if (i > 122)
        {
            i = -64;
        }
        // duration = k_uptime_get() - start_time;
        //  printk("Frame took %lld ms\n", duration);
    }
    return 0;
}