# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky_custom)

target_sources(app PRIVATE
    src/luazephyrlib.c
    src/lua/lapi.c
    src/lua/lauxlib.c
    src/lua/lbaselib.c
    src/lua/lcode.c
    src/lua/lcorolib.c
    src/lua/lctype.c
    src/lua/ldebug.c
    src/lua/ldo.c
    src/lua/ldump.c
    src/lua/lfunc.c
    src/lua/lgc.c
    src/lua/linit.c
    src/lua/liolib.c
    src/lua/llex.c
    src/lua/lmathlib.c
    src/lua/lmem.c
    src/lua/lobject.c
    src/lua/lopcodes.c
    src/lua/lparser.c
    src/lua/lstate.c
    src/lua/lstring.c
    src/lua/lstrlib.c
    src/lua/ltable.c
    src/lua/ltablib.c
    src/lua/ltm.c
    src/lua/lundump.c
    src/lua/lvm.c
    src/lua/lzio.c
    src/lua/loadlib.c

    src/main.c
    src/imgdata.c
    src/font8.c
    src/lcd.c
    src/paint.c
    src/console.c
    src/usb.c
    src/lua_thread.c
    src/disk.c
    src/input.c
    src/AnalogIn.c
    src/drivers/74HC165.c
)
