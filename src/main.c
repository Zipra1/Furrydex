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
#include "fonts/font8.h"
#include "paint.h"
#include "console.h"
#include "usb.h"
#include "luazephyrlib.h"
#include "disk.h"
#include "input.h"
#include "ui.h"
#include "lua_thread.h"
#include "ui.h"

#include <zephyr/storage/disk_access.h>
#include <zephyr/fs/fs.h>
#include <ff.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>

#define DISK_NAME "SD"

USBD_DEFINE_MSC_LUN(sd_lun, DISK_NAME, "Macroplastics", "Furrydex", "1.00");

const struct device *const uart_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

/* The devicetree node identifier for the "led0" alias. */
// #define LED0_NODE DT_ALIAS(led0)
// static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

LOG_MODULE_REGISTER(main);

// static int lsdir(const char *path);

/*
 *  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
 *  in ffconf.h
 */

// usb start
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
// usb end

unsigned char output_buffer[CONFIG_FURRYDEX_FRAME_BYTES_BUFFER];

int main(void)
{
    int ret;
    initDisplay();
    invert(main_buffer, CONFIG_FURRYDEX_FRAME_BYTES_BUFFER);
    printk("Display initialized\n");
    paintText(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, 1, 10, 20, "Display initialized");
    convertBuffer(main_buffer, output_buffer);
    Display(25, 0, 36, 125, output_buffer);

    if (!device_is_ready(uart_dev))
    {
        LOG_ERR("CDC ACM device not ready");
        return 0;
    }

    paintText(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, 1, 10, 20, "Display initialized\nCDC ACM ready");
    convertBuffer(main_buffer, output_buffer);
    Display(25, 0, 36, 125, output_buffer);

    ret = enable_usb_device_next();
    if (ret != 0)
    {
        LOG_ERR("Failed to enable USB device support");
        return 0;
    }

    paintText(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, 1, 10, 20, "Display initialized\nCDC ACM ready\nUSB device started");
    convertBuffer(main_buffer, output_buffer);
    Display(25, 0, 36, 125, output_buffer);

    k_msleep(100); // is this necessary

    if (mount_sd_card())
    {
        LOG_ERR("Failed to mount SD card");
        // return -1;
    }
    else
    {
        printk("Successfully mounted SD card\n");

        paintText(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, 1, 10, 20, "Display initialized\nCDC ACM ready\nUSB device started\nSD card mounted");
        convertBuffer(main_buffer, output_buffer);
        Display(25, 0, 36, 125, output_buffer);

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

        paintText(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, 1, 10, 20, "Display initialized\nCDC ACM ready\nUSB device started\nSD card mounted\nTest file created");
        convertBuffer(main_buffer, output_buffer);
        Display(25, 0, 36, 125, output_buffer);

        // bool force = true;
        // disk_access_ioctl(DISK_NAME, DISK_IOCTL_CTRL_DEINIT, &force);
    }

    blit(main_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, blit_test, 32, 32, 55, 77);

    while (true)
    {
        draw_ui();
        k_mutex_lock(&paint_mutex, K_FOREVER);
        convertBuffer(main_buffer, output_buffer);
        k_mutex_unlock(&paint_mutex);
        waitForTE();
        Display(25, 0, 36, 125, output_buffer);
    }
    return 0;
}