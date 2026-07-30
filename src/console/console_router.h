#ifndef CONSOLE_ROUTER_H
#define CONSOLE_ROUTER_H

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define LUA_SERIAL_MSG_SIZE 128

extern struct k_msgq lua_serial_msgq;
void console_router_init(void);

#endif