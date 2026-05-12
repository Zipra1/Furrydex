#ifndef INPUT_H
#define INPUT_H

#include <zephyr/kernel.h>

extern struct k_mutex inputs_mutex;

int get_bit(unsigned char byte, int n);
extern int inputs;

extern struct k_sem input_sem;

#endif