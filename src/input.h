#ifndef INPUT_H
#define INPUT_H

#include <zephyr/kernel.h>

int get_bit(unsigned char byte, int n);
extern atomic_t inputs;

extern struct k_sem input_sem;

#endif