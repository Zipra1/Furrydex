#ifndef UI_H
#define UI_H

#include <zephyr/sys/atomic.h>

#define FUROS_BANNER \
"                            .-'''-.\n"\
"                          '    _    \\\n"\
"                        /    /` '.   \\\n"\
"     _.._              .    |     \\  '\n"\
"   .' .._|             |    '      |  '\n"\
"   | '                  \\    \\     / /.-'''-.\n"\
" __| |__  _    _  .-,.--.`.   ` ..' /  _     \\\n"\
"|__   __|| '  / | |  .-. |  '-...-'`(`' )/`--'\n"\
"   | |  .' | .' | | |  '-          (_ o _).\n"\
"   | |  /  | /  | | |               (_,_). '.\n"\
"   | | |   `'.  | | |              .---.  \\  :\n"\
"   | | '   .'|  '/|_|              \\    `-'  |\n"\
"   └-┘  `-'  `--'                   \\       /\n"\
"                                     `-...-'\n"\



extern atomic_t selected_page;
void page_left();
void page_right();
void draw_ui();

#endif