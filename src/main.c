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
#include "console.h"

#include <zephyr/storage/disk_access.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <ff.h>

#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 1000
#define BLINK_THREAD_STACK_SIZE 512 // 256 is the "bare minimum", 512 is small, most threads start at 1024
#define INPUT_THREAD_STACK_SIZE 512

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

LOG_MODULE_REGISTER(main);

static int lsdir(const char *path);

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};

/*
 *  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
 *  in ffconf.h
 */
static const char *disk_mount_pt = "/SD:";

static int lsdir(const char *path)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    fs_dir_t_init(&dirp);

    /* Verify fs_opendir() */
    res = fs_opendir(&dirp, path);
    if (res)
    {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    printk("\nListing dir %s ...\n", path);
    for (;;)
    {
        /* Verify fs_readdir() */
        res = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0)
        {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR)
        {
            printk("[DIR ] %s\n", entry.name);
        }
        else
        {
            printk("[FILE] %s (size = %zu)\n",
                   entry.name, entry.size);
        }
    }

    /* Verify fs_closedir() */
    fs_closedir(&dirp);

    return res;
}

static int mount_sd_card(void)
{
    /* raw disk i/o */
    static const char *disk_pdrv = "SD";
    uint64_t memory_size_mb;
    uint32_t block_count;
    uint32_t block_size;

    if (disk_access_init(disk_pdrv) != 0)
    {
        LOG_ERR("Storage init ERROR!");
        return -1;
    }

    if (disk_access_ioctl(disk_pdrv,
                          DISK_IOCTL_GET_SECTOR_COUNT, &block_count))
    {
        LOG_ERR("Unable to get sector count");
        return -1;
    }
    LOG_INF("Block count %u", block_count);

    if (disk_access_ioctl(disk_pdrv,
                          DISK_IOCTL_GET_SECTOR_SIZE, &block_size))
    {
        LOG_ERR("Unable to get sector size");
        return -1;
    }
    printk("Sector size %u\n", block_size);

    memory_size_mb = (uint64_t)block_count * block_size;
    printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));

    mp.mnt_point = disk_mount_pt;

    int res = fs_mount(&mp);

    if (res == FR_OK)
    {
        printk("Disk mounted.\n");
        lsdir(disk_mount_pt);
    }
    else
    {
        printk("Failed to mount disk - trying one more time\n");
        res = fs_mount(&mp);
        if (res != FR_OK)
        {
            printk("Error mounting disk.\n");
            return -1;
        }
    }

    return 0;
}

// Thread example settings

static struct k_thread blink_thread;
static struct k_thread input_thread;

K_THREAD_STACK_DEFINE(blink_stack, BLINK_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(input_stack, INPUT_THREAD_STACK_SIZE);

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

    if (mount_sd_card())
    {
        printf("Failed to mount SD card\n");
        return -1;
    }
    else
    {
        printf("Successfully mounted SD card\n");
    }

    char file_data_buffer[200];
    struct fs_file_t data_filp;
    fs_file_t_init(&data_filp);

    char file_ch;
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

    sprintf(file_data_buffer, "hello world!\n");
    ret = fs_write(&data_filp, file_data_buffer, strlen(file_data_buffer));
    fs_close(&data_filp);

    initDisplay();
    printf("Display initialized\n");
    int i = -64;
    // int i2 = 0;
    int64_t start_time = k_uptime_get();
    int64_t duration = k_uptime_get() - start_time;
    while (true)
    {
        k_msleep(44);
        start_time = k_uptime_get();
        paintText("meow! :3", IMAGE_DATA2, 1, 16 + i, 15);
        // paintCharacter('a', IMAGE_DATA2, i2, 1);
        paintTextWrap(IMAGE_DATA2, 1, 11, 30, 121, "test text");
        convertBuffer(IMAGE_DATA2, output_buffer);
        // invert(output_buffer,138,250,0,0,138,250);
        invert(output_buffer, CONFIG_FURRYDEX_EPD_MAX_BYTES);
        Display(25, 0, 36, 125, output_buffer);
        // paintRegion(IMAGE_DATA, 20, 20, 120, 120, 0);
        i++;
        // i2++;
        if (i > 122)
        {
            i = -64;
        }
        duration = k_uptime_get() - start_time;
        printf("Frame took %lld ms\n", duration);
    }
    return 0;
}
