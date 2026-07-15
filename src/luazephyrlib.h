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

#endif