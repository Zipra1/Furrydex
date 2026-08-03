#ifndef LUAZEPHYRLIB_H
#define LUAZEPHYRLIB_H

#include <stddef.h>
#include <zephyr/kernel.h>

extern struct k_mutex paint_mutex;

typedef struct
{
    int width;
    int height;
    struct
    {
        size_t size;
        void *ptr;
    };
} canvas_t;

typedef struct
{
    int char_width;
    int char_height;
    struct
    {
        size_t size;
        void *ptr;
    };
} font_t;

#endif