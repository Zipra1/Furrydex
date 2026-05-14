/*unsigned char *layerFrames(size_t num_frames, const unsigned char *const frames[])
{
    for (int i = 0; i < CONFIG_FURRYDEX_FRAME_BYTES_BUFFER; i++)
    {
        unsigned char merged_byte = 0xFF; // This can probably be optimized further
        for (size_t j = 0; j < num_frames; j++)
        {
            merged_byte &= frames[j][i];
        }
        output_buffer[i] = merged_byte;
    }
    return output_buffer;
}*/
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "font8.h"

// Every function here should be display agnostic. That way, it will be possible to use different types of displays later on.

static inline uint8_t reverse_bits(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

void paintCharacter(char character, unsigned char *buf, int translate_width, int translate_height)
{
    int fontWidth = 1;
    int fontHeight = 8;
    int fontBytes = fontWidth * fontHeight;
    int charStart = ((int)character - 32) * fontBytes;
    int stride = 17;

    int byteOffset = translate_width / 8;
    if (byteOffset > 16 || byteOffset < 0) // Not display-agnostic! This needs to be fixed. Should cut off the character, instead of letting it bleed over.
    {
        return;
    }
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

void paintText(unsigned char *buf, int kerning, int translate_width, int translate_height, const char *string)
{
    int character_width = 5;
    int line_height = 8;
    int len = strlen(string);
    int current_x = 0;
    int current_y = 0;

    for (int i = 0; i < len; i++)
    {
        if (string[i] == '\n')
        {
            current_x = 0;
            current_y += line_height;
            continue;
        }
        paintCharacter(string[i], buf, translate_width + current_x, translate_height + current_y);
        current_x += (character_width + kerning);
    }
}

int paintTextWrap(unsigned char *buf, int kerning, int translate_width, int translate_height, int box_width, const char *string)
{
    int character_width = 5;
    int line_height = 8;

    int current_x = 0;
    int current_y = 0;
    int len = strlen(string);

    for (int i = 0; i < len; i++)
    {
        if (string[i] == '\n')
        {
            current_x = 0;
            current_y += line_height;
            continue;
        }

        if (current_x == 0 && string[i] == ' ')
        {
            continue;
        }

        int word_end = i;
        while (word_end < len && string[word_end] != ' ' && string[word_end] != '\n')
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

void invertRegion(unsigned char *buf, int buf_w, int buf_h, int start_x, int start_y, int end_x, int end_y) // should uint8_t arrays be used instead of unsigned char arrays?
{
    int stride = (buf_w + 7) / 8; // Number of bytes per row

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

void invert(uint8_t *buf, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        buf[i] = ~buf[i];
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

void paintRegion(uint8_t *buf, int buf_w, int buf_h, int start_x, int start_y, int end_x, int end_y, int colour)
{
    start_x = (start_x < 0)    ? 0    : (start_x > buf_w) ? buf_w : start_x;
    start_y = (start_y < 0)    ? 0    : (start_y > buf_h) ? buf_h : start_y;
    end_x   = (end_x   < 0)    ? 0    : (end_x   > buf_w) ? buf_w : end_x;
    end_y   = (end_y   < 0)    ? 0    : (end_y   > buf_h) ? buf_h : end_y;
    
    int stride = (buf_w + 7) / 8;

    for (int cur_y = start_y; cur_y < end_y; cur_y++)
    {
        for (int cur_x = start_x; cur_x < end_x; cur_x++)
        {
            int byte_idx = cur_y * stride + cur_x / 8;
            uint8_t bit_mask = (1 << (7 - (cur_x % 8)));

            if (colour)
                buf[byte_idx] |= bit_mask;
            else
                buf[byte_idx] &= ~bit_mask;
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
        uint16_t row0 = y * stride;
        uint16_t row1 = (y + 1) * stride;
        for (uint16_t x = 0; x < width; x += 4)
        {
            uint8_t mix = 0;
            for (uint8_t col = 0; col < 4; col++)
            {
                uint16_t currX = x + col;
                uint8_t byte_offset = currX >> 3;
                uint8_t bit = 7 - (currX & 0x07);

                if ((buffer[row0 + byte_offset] >> bit) & 0x01)
                    mix |= (1 << (7 - col * 2));
                if ((buffer[row1 + byte_offset] >> bit) & 0x01)
                    mix |= (1 << (7 - (col * 2 + 1)));
            }
            target_buffer[k++] = mix;
        }
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

void paintHorizontalLine(uint8_t *buf, int buf_w, int buf_h, int y, int x0, int x1, int colour)
{
    if (y < 0 || y >= buf_h)
    {
        // printk("paintHorizontalLine @ paint.c: Out of bounds A!!!!!!\n");
        return;
    }
    if (x1 < 0 || x0 >= buf_w)
    {
        // printk("paintHorizontalLine @ paint.c: Out of bounds B!!!!!!\n");
        return;
    }
    if (x0 < 0)
    {
        // printk("paintHorizontalLine @ paint.c: Out of bounds C!!!!!!\n");
        x0 = 0;
    }
    if (x1 >= buf_w)
    {
        // printk("paintHorizontalLine @ paint.c: Out of bounds D!!!!!!\n");
        x1 = buf_w - 1;
    }

    int stride = (buf_w + 7) / 8;

    if ((y * stride + x1 / 8) >= CONFIG_FURRYDEX_FRAME_BYTES_BUFFER)
    {
        // printk("out of bounds!");
        return;
    }

    int byte0 = x0 / 8, bit0 = x0 % 8;
    int byte1 = x1 / 8, bit1 = x1 % 8;
    int row = y * stride;

    uint8_t left_mask = 0xFF >> bit0;
    uint8_t right_mask = 0xFF << (7 - bit1);

    if (byte0 == byte1)
    {
        uint8_t mask = left_mask & right_mask;
        colour ? (buf[row + byte0] |= mask) : (buf[row + byte0] &= ~mask);
    }
    else
    {
        colour ? (buf[row + byte0] |= left_mask) : (buf[row + byte0] &= ~left_mask);
        for (int b = byte0 + 1; b < byte1; b++)
            buf[row + b] = colour ? 0xFF : 0x00;
        colour ? (buf[row + byte1] |= right_mask) : (buf[row + byte1] &= ~right_mask);
    }
}

void paintFilledCircle(uint8_t *buf, int buf_w, int buf_h, int center_x, int center_y, int radius, int colour)
{
    int x = 0;
    int y = radius;
    int d = 1 - radius;

    while (x <= y)
    {
        paintHorizontalLine(buf, buf_w, buf_h, center_y + y, center_x - x, center_x + x, colour);
        paintHorizontalLine(buf, buf_w, buf_h, center_y - y, center_x - x, center_x + x, colour);
        paintHorizontalLine(buf, buf_w, buf_h, center_y + x, center_x - y, center_x + y, colour);
        paintHorizontalLine(buf, buf_w, buf_h, center_y - x, center_x - y, center_x + y, colour);

        if (d < 0)
        {
            d += 2 * x + 3;
        }
        else
        {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void paintPageBubbles(uint8_t *buf, int buf_w, int buf_h, int num_bubble, int selected_bubble)
{
    int center = 71; // todo: calculate based off kconfig
    int i = 0;
    while (i <= num_bubble)
    {
        paintFilledCircle(buf, buf_w, buf_h, center + (i * 10) - (num_bubble * 5), 244, 4, 0);
        if (i == selected_bubble)
        {
            paintFilledCircle(buf, buf_w, buf_h, center + (i * 10) - (num_bubble * 5), 244, 2, 1);
        }
        i++;
    }
}

void blit(uint8_t *dst, int dst_w, int dst_h,
          const uint8_t *src, int src_w, int src_h,
          int x, int y)
{
    for (int row = 0; row < src_h; row++)
    {
        int dst_y = y + row;
        if (dst_y < 0 || dst_y >= dst_h)
            continue;

        for (int col = 0; col < src_w; col++)
        {
            int dst_x = x + col;
            if (dst_x < 0 || dst_x >= dst_w)
                continue;

            int src_byte = (row * ((src_w + 7) / 8)) + (col / 8);
            int src_bit = 7 - (col % 8);
            int pixel = (src[src_byte] >> src_bit) & 1;

            int dst_byte = (dst_y * ((dst_w + 7) / 8)) + (dst_x / 8);
            int dst_bit = 7 - (dst_x % 8);
            if (pixel)
                dst[dst_byte] |= (1 << dst_bit);
            else
                dst[dst_byte] &= ~(1 << dst_bit);
        }
    }
}

void blitMask(uint8_t *dst, int dst_w, int dst_h,
               const uint8_t *src, int src_w, int src_h,
               const uint8_t *mask,
               int x, int y)
{
    for (int row = 0; row < src_h; row++)
    {
        int dst_y = y + row;
        if (dst_y < 0 || dst_y >= dst_h)
            continue;
        for (int col = 0; col < src_w; col++)
        {
            int dst_x = x + col;
            if (dst_x < 0 || dst_x >= dst_w)
                continue;

            int src_byte = (row * ((src_w + 7) / 8)) + (col / 8);
            int src_bit = 7 - (col % 8);

            int mask_pixel = (mask[src_byte] >> src_bit) & 1;
            if (mask_pixel)
                continue;

            int pixel = (src[src_byte] >> src_bit) & 1;
            int dst_byte = (dst_y * ((dst_w + 7) / 8)) + (dst_x / 8);
            int dst_bit = 7 - (dst_x % 8);
            if (pixel)
                dst[dst_byte] |= (1 << dst_bit);
            else
                dst[dst_byte] &= ~(1 << dst_bit);
        }
    }
}