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

#define SLEEP_TIME_MS 1000
#define BLINK_THREAD_STACK_SIZE 512 // 256 is the "bare minimum", 512 is small, most threads start at 1024
#define INPUT_THREAD_STACK_SIZE 512

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static inline uint8_t reverse_bits(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

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

void FlipBuffer(unsigned char *buf, int physical_width, int height, bool flip_h, bool flip_v)
{
    int w_bytes = (physical_width + 7) / 8; // can this just be set to the width since width is already 128? not sure how that will interact with other displays, tho.
    int padding_bits = (w_bytes * 8) - physical_width;
    unsigned char temp_row[w_bytes];

    if (flip_v)
    {
        for (int y = 0; y < height / 2; y++)
        {
            unsigned char *top_row = &buf[y * w_bytes];
            unsigned char *bot_row = &buf[(height - 1 - y) * w_bytes];
            memcpy(temp_row, top_row, w_bytes);
            memcpy(top_row, bot_row, w_bytes);
            memcpy(bot_row, temp_row, w_bytes);
        }
    }

    if (flip_h)
    {
        for (int y = 0; y < height; y++)
        {
            unsigned char *row = &buf[y * w_bytes];

            for (int x = 0; x < (w_bytes + 1) / 2; x++)
            {
                int left = x;
                int right = w_bytes - 1 - x;
                if (left == right)
                {
                    row[left] = reverse_bits(row[left]);
                }
                else
                {
                    unsigned char t = reverse_bits(row[left]);
                    row[left] = reverse_bits(row[right]);
                    row[right] = t;
                }
            }

            if (padding_bits > 0)
            {
                for (int i = 0; i < w_bytes; i++)
                {
                    // shift current byte left, pull in bits from the next byte
                    unsigned char next_val = (i < w_bytes - 1) ? row[i + 1] : 0;
                    row[i] = (row[i] << padding_bits) | (next_val >> (8 - padding_bits));
                }
            }
        }
    }
}

unsigned char output_buffer[CONFIG_FURRYDEX_EPD_MAX_BYTES];

unsigned char *layerFrames(size_t num_frames, const unsigned char *const frames[])
{
    for (int i = 0; i < CONFIG_FURRYDEX_EPD_MAX_BYTES; i++)
    {
        unsigned char merged_byte = 0xFF; // This can probably be optimized further
        for (size_t j = 0; j < num_frames; j++)
        {
            merged_byte &= frames[j][i];
        }
        output_buffer[i] = merged_byte;
    }
    return output_buffer;
}

void paintCharacter(char character, unsigned char *buf, int translate_width, int translate_height)
{
    int fontWidth = 1;
    int fontHeight = 8;
    int fontBytes = fontWidth * fontHeight;
    int charStart = ((int)character - 32) * fontBytes;
    int stride = 17;

    int byteOffset = translate_width / 8;
    int bitShift = translate_width % 8;

    for (int i = 0; i < fontHeight; i++)
    {
        unsigned char fontByte = ~font_8[charStart + i];
        int rowStart = (i + translate_height) * stride;
        int targetIdx = rowStart + byteOffset;

        if (bitShift == 0)
        {
            buf[targetIdx] = fontByte;
        }
        else
        {
            buf[targetIdx] &= ~(0xFF >> bitShift);
            buf[targetIdx] |= (fontByte >> bitShift);

            if (byteOffset + 1 < stride)
            {
                buf[targetIdx + 1] &= ~(0xFF << (8 - bitShift));
                buf[targetIdx + 1] |= (fontByte << (8 - bitShift));
            }
        }
    }
}

void paintText(const char *string, unsigned char *buf, int kerning, int translate_width, int translate_height)
{
    int character_width = 5;
    for (int i = 0; i < strlen(string); i++)
    {
        paintCharacter(string[i], buf, translate_width + (i * (character_width + kerning)), translate_height);
    }
}

int paintTextWrap(const char *string, unsigned char *buf, int kerning, int translate_width, int translate_height, int box_width)
// This function can overflow into adjacent memory. Add a check that it's not beyond the buffer limits
{
    int character_width = 5;
    int line_height = 8;

    int current_x = 0;
    int current_y = 0;
    int len = strlen(string);

    for (int i = 0; i < len; i++)
    {
        if (current_x == 0 && string[i] == ' ')
        {
            continue;
        }

        int word_end = i;
        while (word_end < len && string[word_end] != ' ')
        {
            word_end++;
        }

        int word_len = word_end - i;
        int word_pixel_width = word_len * (character_width + kerning);

        if (current_x + word_pixel_width > box_width && current_x > 0)
        {
            current_x = 0;
            current_y += line_height;
        }

        for (int j = 0; j < word_len; j++)
        {
            paintCharacter(string[i], buf, translate_width + current_x, translate_height + current_y);
            current_x += (character_width + kerning);
            i++;
        }

        if (i < len && string[i] == ' ')
        {
            current_x += (character_width + kerning);
        }
    }

    return current_y + line_height;
}

void invert(unsigned char *buf, int buf_w, int buf_h, int start_x, int start_y, int end_x, int end_y) // should uint8_t arrays be used instead of unsigned char arrays?
{
    int stride = buf_w / 8; // Number of bytes per row

    for (int cur_y = start_y; cur_y < end_y; cur_y++)
    {
        for (int cur_x = start_x; cur_x < end_x; cur_x++)
        {
            int byte_idx = (cur_y * stride) + (cur_x / 8);
            int bit_pos = 7 - (cur_x % 8);
            buf[byte_idx] ^= (1 << bit_pos);
        }
    }
}

void paintPixel(uint8_t *buf, int buf_w, int buf_h, int x, int y, int colour)
{
    int byte_idx = (y * (buf_w / 8)) + (x / 8);
    uint8_t bit_mask = 1 << (7 - (x % 8));

    if (colour)
    {
        buf[byte_idx] |= bit_mask;
    }
    else
    {
        buf[byte_idx] &= ~bit_mask;
    }
}

void paintLine(unsigned char *buf, int buf_w, int buf_h, int x0, int y0, int x1, int y1, int colour)
{

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    for (;;)
    {
        paintPixel(buf, buf_w, buf_h, x0, y0, colour);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }
}

struct menuBox
{
    int x_start;
    int y_start;
    int x_end;
    int y_end;
    int num_items;
    char **items;
    int selected;
};

void paintRegion(uint8_t *buf, int buf_w, int buf_h, int start_x, int start_y, int end_x, int end_y, int colour)
{
    for (int cur_y = start_y; cur_y < end_y; cur_y++)
    {
        for (int cur_x = start_x; cur_x < end_x; cur_x++)
        {
            uint32_t total_bit_offset = (uint32_t)cur_y * buf_w + cur_x;

            int byte_idx = total_bit_offset / 8;
            uint8_t bit_mask = (1 << (7 - (total_bit_offset % 8)));

            if (colour)
            {
                buf[byte_idx] |= bit_mask;
            }
            else
            {
                buf[byte_idx] &= ~bit_mask;
            }
        }
    }
}

void convertBuffer(uint8_t *buffer, uint8_t *target_buffer)
{
    uint16_t k = 0;
    const uint16_t width = 132;
    const uint16_t height = 250;
    const uint16_t stride = (width + 7) / 8; // Bytes per row (32 for 250px)

    // theres two buffers because the conversion methods destroy data bc it needs to overwrite. could what beings overwritten be stored in a smaller buffer, and used later? would save like 4kb of ram
    // this should probably be done the way that other repo does it, im not sure how I feel about this.

    for (uint16_t y = 0; y < height; y += 2)
    {
        for (uint16_t x = 0; x < width; x += 4)
        {
            uint8_t mix = 0;

            for (uint8_t col = 0; col < 4; col++)
            {
                for (uint8_t row = 0; row < 2; row++)
                {
                    uint16_t currX = x + col;
                    uint16_t currY = y + row;
                    if (currX < width && currY < height)
                    {
                        uint8_t pixel = (buffer[currY * stride + (currX >> 3)] >> (7 - (currX & 0x07))) & 0x01;

                        if (pixel)
                        {
                            mix |= (1 << (7 - (col * 2 + row)));
                        }
                    }
                }
            }
            target_buffer[k++] = mix;
        }
    }
}

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
