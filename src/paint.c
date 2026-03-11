/*unsigned char *layerFrames(size_t num_frames, const unsigned char *const frames[])
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
}*/
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
    for (int i = 0; i < strlen(string); i++)
    {
        paintCharacter(string[i], buf, translate_width + (i * (character_width + kerning)), translate_height);
    }
}

int paintTextWrap(unsigned char *buf, int kerning, int translate_width, int translate_height, int box_width, const char *string)
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