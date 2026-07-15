#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/pinctrl.h>

#include <zephyr/drivers/pwm.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/fs_interface.h>
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include <zephyr/storage/disk_access.h>

#include <zephyr/kernel.h>

#include <stdlib.h>
#include <string.h>

#include "paint.h"
#include "input.h"
#include "drivers/ST7305.h" // todo: ifdef here for lcd/epaper
#include "imgdata.h"
#include "lua_thread.h"
#include "luazephyrlib.h"
#include "ui.h"
#include "battery.h"
#include "disk.h"

///////////////
///  PAINT  ///
///////////////

static int lua_canvas_gc(lua_State *L)
{
    canvas_t *canvas = luaL_checkudata(L, 1, "paint.canvas");
    free(canvas->ptr);
    canvas->ptr = NULL;
    return 0;
}

static int lua_canvas_index(lua_State *L)
{
    canvas_t *canvas = luaL_checkudata(L, 1, "paint.canvas");
    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "width") == 0)
        lua_pushinteger(L, canvas->width);
    else if (strcmp(key, "height") == 0)
        lua_pushinteger(L, canvas->height);
    else
        lua_pushnil(L);
    return 1;
}

static int lua_paint_new_canvas(lua_State *L)
{
    int w = luaL_optinteger(L, 1, CONFIG_FURRYDEX_DISPLAY_WIDTH);
    int h = luaL_optinteger(L, 2, CONFIG_FURRYDEX_DISPLAY_HEIGHT);

    size_t size = ((w + 7) / 8) * h;

    canvas_t *canvas = lua_newuserdata(L, sizeof(canvas_t));
    canvas->width = w;
    canvas->height = h;
    canvas->size = size;
    canvas->ptr = malloc(size);
    if (!canvas->ptr)
        return luaL_error(L, "out of memory");
    memset(canvas->ptr, 0xFF, size);
    luaL_getmetatable(L, "paint.canvas");
    lua_setmetatable(L, -2);
    return 1;
}

K_MUTEX_DEFINE(paint_mutex);
K_MUTEX_DEFINE(lua_paint_mutex);

static int should_display(void)
{
    if (get_current_lua_slot() == atomic_get(&selected_page))
    {
        return 1;
    }
    return 0;
}

static int lua_paint_display(lua_State *L)
{
    if (should_display())
    {
        k_mutex_lock(&paint_mutex, K_FOREVER);
        if (!lua_isnoneornil(L, 1))
        {
            const uint8_t *mask = NULL;
            canvas_t *canvas_mask = luaL_checkudata(L, 1, "paint.canvas");
            mask = (const uint8_t *)canvas_mask->ptr;
            blitMask(main_buffer,
                     CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                     lua_buffer,
                     CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                     mask, 0, 0);
        }
        else
        {
            memcpy(main_buffer, lua_buffer, CONFIG_FURRYDEX_FRAME_BYTES_BUFFER);
        }
        k_mutex_unlock(&paint_mutex);
    }
    return 0;
}

static int lua_paint_wait_for_display(lua_State *L)
{
    waitForTE();
    return 0;
}

static int lua_paint_clear(lua_State *L)
{
    int colour = luaL_optinteger(L, 1, 1);
    canvas_t *canvas = luaL_testudata(L, 2, "paint.canvas");
    if (canvas)
    {
        memset(canvas->ptr, colour ? 0xFF : 0x00, canvas->size);
    }
    else if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        memset(lua_buffer, colour ? 0xFF : 0x00, CONFIG_FURRYDEX_FRAME_BYTES_BUFFER);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0;
}

static int lua_paint_pixel(lua_State *L)
{
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int colour = luaL_checknumber(L, 3);
    canvas_t *canvas = luaL_testudata(L, 4, "paint.canvas");
    if (canvas)
    {
        paintPixel(canvas->ptr, canvas->width, canvas->height, x - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, y, colour);
    }
    else if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintPixel(lua_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, x, y, colour);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0;
}

static int lua_paint_rect(lua_State *L)
{
    int x1 = luaL_checknumber(L, 1);
    int y1 = luaL_checknumber(L, 2);
    int x2 = luaL_checknumber(L, 3);
    int y2 = luaL_checknumber(L, 4);
    int colour = luaL_checknumber(L, 5);
    canvas_t *canvas = luaL_testudata(L, 6, "paint.canvas");
    if (canvas)
    {
        paintRegion(canvas->ptr, canvas->width, canvas->height, x1 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, y1, x2 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, y2, colour);
    }
    else if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintRegion(lua_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, x1, y1, x2, y2, colour);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0;
}

static int lua_paint_circle(lua_State *L)
{
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int r = luaL_checknumber(L, 3);
    int colour = luaL_checknumber(L, 4);
    canvas_t *canvas = luaL_testudata(L, 5, "paint.canvas");
    if (canvas)
    {
        paintFilledCircle(canvas->ptr,
                          canvas->width,
                          canvas->height,
                          x - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, y, r, colour);
    }
    else if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintFilledCircle(lua_buffer,
                          CONFIG_FURRYDEX_DISPLAY_WIDTH,
                          CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                          x, y, r, colour);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0;
}

// static int lua_paint_character(lua_State *L)
// {
//     size_t len;
//     const char *character = luaL_checklstring(L, 1, &len);
//     if (len != 1)
//     {
//         return luaL_argerror(L, 1, "Expected single character, use text or text_wrap for a string");
//     }
//     int x = luaL_checknumber(L, 2);
//     int y = luaL_checknumber(L, 3);
//     canvas_t *canvas = luaL_testudata(L, 4, "paint.canvas");

//     if (should_display())
//     {
//         k_mutex_lock(&lua_paint_mutex, K_FOREVER);
//         paintCharacter(character[0], canvas ? canvas->width : CONFIG_FURRYDEX_DISPLAY_WIDTH, canvas ? canvas->height : CONFIG_FURRYDEX_DISPLAY_HEIGHT, canvas ? canvas->ptr : lua_buffer, x, y);
//         k_mutex_unlock(&lua_paint_mutex);
//     }
//     return 0;
// }

static int lua_paint_text(lua_State *L)
{
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int kerning = luaL_checknumber(L, 3);
    const char *text = luaL_checkstring(L, 4);
    canvas_t *canvas = luaL_testudata(L, 5, "paint.canvas");
    if (canvas || should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintText(canvas ? canvas->ptr : lua_buffer, canvas ? canvas->width : CONFIG_FURRYDEX_DISPLAY_WIDTH, canvas ? canvas->height : CONFIG_FURRYDEX_DISPLAY_HEIGHT, kerning, canvas ? x - CONFIG_FURRYDEX_DISPLAY_OFFSET_X : x, y, text);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0;
}

static int lua_paint_text_wrap(lua_State *L)
{
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int kerning = luaL_checknumber(L, 3);
    int width = luaL_checknumber(L, 4);
    const char *text = luaL_checkstring(L, 5);
    canvas_t *canvas = luaL_testudata(L, 6, "paint.canvas");

    int lines = 0;
    if (canvas || should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        lines = paintTextWrap(canvas ? canvas->ptr : lua_buffer, canvas ? canvas->width : CONFIG_FURRYDEX_DISPLAY_WIDTH, canvas ? canvas->height : CONFIG_FURRYDEX_DISPLAY_HEIGHT, kerning, canvas ? x - CONFIG_FURRYDEX_DISPLAY_OFFSET_X : x, y, width, text);
        k_mutex_unlock(&lua_paint_mutex);
    }
    lua_pushinteger(L, lines);
    return 1;
}

static int lua_paint_icon_set(lua_State *L)
{
    int src_w, src_h;
    const uint8_t *src = NULL;
    size_t icon_size = 8;

    canvas_t *src_canvas = luaL_checkudata(L, 1, "paint.canvas");
    src_w = src_canvas->width;
    src_h = src_canvas->height;
    src = (const uint8_t *)src_canvas->ptr;

    if (src_w != 8 || src_h != 8)
    {
        return luaL_error(L, "Incorrect dimensions. Icon must be 8x8");
    }

    lua_thread_slot_t *slot = &lua_slots[get_current_lua_slot()];
    lua_thread_free_icon(slot);
    slot->icon = malloc(icon_size);
    if (!slot->icon)
    {
        return luaL_error(L, "out of memory");
    }
    memcpy(slot->icon, src, icon_size);
    return 0;
}

static int lua_paint_blit(lua_State *L)
{
    const uint8_t *src = NULL;
    int src_w, src_h;

    canvas_t *src_canvas = luaL_checkudata(L, 1, "paint.canvas");
    src = (const uint8_t *)src_canvas->ptr;
    src_w = src_canvas->width;
    src_h = src_canvas->height;

    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    if (should_display())
    {
        const uint8_t *mask = NULL;
        if (!lua_isnoneornil(L, 4))
        {
            canvas_t *canvas_mask = luaL_checkudata(L, 4, "paint.canvas");
            mask = (const uint8_t *)canvas_mask->ptr;

            k_mutex_lock(&lua_paint_mutex, K_FOREVER);
            blitMask(lua_buffer,
                     CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                     src,
                     src_w, src_h,
                     mask, x, y);
            k_mutex_unlock(&lua_paint_mutex);
        }
        else
        {
            k_mutex_lock(&lua_paint_mutex, K_FOREVER);
            blit(lua_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, src, src_w, src_h, x, y);
            k_mutex_unlock(&lua_paint_mutex);
        }

        lua_gc(L, LUA_GCSTEP, 100);
    }
    return 0;
}

// ⚠ THIS FUNCTION WAS MADE IWTH GENERATIVE AI. CHECK IT THOROUGHLY!!
static int lua_paint_load_bmp(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);

    struct fs_file_t f;
    fs_file_t_init(&f);
    if (fs_open(&f, path, FS_O_READ) != 0)
    {
        return luaL_error(L, "could not open %s", path);
    }

    uint8_t header[54];
    fs_read(&f, header, 54);

    if (header[0] != 'B' || header[1] != 'M')
    {
        fs_close(&f);
        return luaL_error(L, "not a BMP file");
    }

    uint32_t pixel_offset = *(uint32_t *)(header + 10);
    int32_t bmp_w = *(int32_t *)(header + 18);
    int32_t bmp_h = *(int32_t *)(header + 22);
    uint16_t bpp = *(uint16_t *)(header + 28);
    bool flipped = bmp_h > 0;
    if (bmp_h < 0)
        bmp_h = -bmp_h;

    if (bpp != 1)
    {
        fs_close(&f);
        return luaL_error(L, "only 1-bit BMP supported (got %d bpp)", bpp);
    }

    // BMP rows padded to 4-byte boundaries
    int bmp_stride = ((bmp_w + 31) / 32) * 4;

    // Output buffer uses tightly packed stride
    int out_stride = (bmp_w + 7) / 8;
    size_t out_size = out_stride * bmp_h;

    uint8_t *bmp_buf = malloc(bmp_stride * bmp_h);
    canvas_t *canvas = lua_newuserdata(L, sizeof(canvas_t));
    canvas->width = bmp_w;
    canvas->height = bmp_h;
    canvas->size = out_size;
    canvas->ptr = malloc(out_size);

    if (bmp_buf == NULL || canvas->ptr == NULL)
    {
        free(bmp_buf);
        free(canvas->ptr);
        fs_close(&f);
        return luaL_error(L, "out of memory");
    }

    luaL_getmetatable(L, "paint.canvas");
    lua_setmetatable(L, -2);

    fs_seek(&f, pixel_offset, FS_SEEK_SET);
    fs_read(&f, bmp_buf, bmp_stride * bmp_h);
    fs_close(&f);

    // Convert from BMP layout (bottom-up, 4-byte padded rows) to tightly packed top-down layout
    memset(canvas->ptr, 0, out_size);
    for (int row = 0; row < bmp_h; row++)
    {
        int src_row = flipped ? (bmp_h - 1 - row) : row;
        uint8_t *src_line = bmp_buf + src_row * bmp_stride;

        for (int col = 0; col < bmp_w; col++)
        {
            int src_bit = 7 - (col % 8);
            int pixel = (src_line[col / 8] >> src_bit) & 1;

            int dst_bit = 7 - (col % 8);
            int dst_byte = row * out_stride + col / 8;
            if (pixel)
                ((uint8_t *)canvas->ptr)[dst_byte] &= ~(1 << dst_bit);
            else
                ((uint8_t *)canvas->ptr)[dst_byte] |= (1 << dst_bit);
        }
    }

    free(bmp_buf);

    return 1;
}

static int lua_paint_invert_region(lua_State *L)
{
    int x1 = luaL_checknumber(L, 1);
    int y1 = luaL_checknumber(L, 2);
    int x2 = luaL_checknumber(L, 3);
    int y2 = luaL_checknumber(L, 4);
    canvas_t *canvas = luaL_testudata(L, 5, "paint.canvas");
    if (canvas)
    {
        invertRegion(canvas->ptr, canvas->width, canvas->height, x1 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, y1, x2 - CONFIG_FURRYDEX_DISPLAY_OFFSET_X, y2);
    }
    else if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        invertRegion(lua_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, x1, y1, x2, y2);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0;
}

int lua_paint_hide_top(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    int input = lua_toboolean(L, 1);
    lua_slots[get_current_lua_slot()].hide_top = input;
    return 0;
}

int lua_paint_hide_bottom(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    int input = lua_toboolean(L, 1);
    lua_slots[get_current_lua_slot()].hide_bottom = input;
    return 0;
}

int lua_paint_tray(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    int input = lua_toboolean(L, 1);
    lua_slots[get_current_lua_slot()].in_tray = input;
    if ((input == true) && (get_current_lua_slot() == atomic_get(&selected_page)))
    {
        atomic_set(&selected_page, 0);
    }
    lua_thread_refresh_ui_state();
    return 0;
}

static const luaL_Reg paint_funcs[] = {
    {"wait_for_display", lua_paint_wait_for_display},
    {"display", lua_paint_display},
    {"clear", lua_paint_clear},
    {"pixel", lua_paint_pixel},
    {"rect", lua_paint_rect},
    {"circle", lua_paint_circle},
    //{"character", lua_paint_character},
    {"text", lua_paint_text},
    {"text_wrap", lua_paint_text_wrap},
    {"blit", lua_paint_blit},
    {"load_bmp", lua_paint_load_bmp},
    {"new_canvas", lua_paint_new_canvas},
    {"invert_region", lua_paint_invert_region},
    {"hide_top", lua_paint_hide_top},
    {"hide_bottom", lua_paint_hide_bottom},
    {"tray", lua_paint_tray},
    {"set_tray_icon", lua_paint_icon_set},
    {NULL, NULL},
};

LUAMOD_API int luaopen_paint(lua_State *L)
{
    luaL_newmetatable(L, "paint.canvas");
    lua_pushcfunction(L, lua_canvas_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, lua_canvas_index);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_newlib(L, paint_funcs);
    return 1;
}

/////////////
/// INPUT ///
/////////////

static const struct
{
    const char *name;
    int bit;
} input_map[] = {
    {"LEFT", CONFIG_FURRYDEX_INPUT_LEFT},
    {"RIGHT", CONFIG_FURRYDEX_INPUT_RIGHT},
    {"UP", CONFIG_FURRYDEX_INPUT_UP},
    {"DOWN", CONFIG_FURRYDEX_INPUT_DOWN},
    {"A", CONFIG_FURRYDEX_INPUT_A},
    {"B", CONFIG_FURRYDEX_INPUT_B},
    {"C", CONFIG_FURRYDEX_INPUT_C},
};
#define INPUT_MAP_SIZE ARRAY_SIZE(input_map)

int lua_input_get(lua_State *L)
{
    int current_lua_slot = get_current_lua_slot();
    if (lua_slots[current_lua_slot].capture_input == LUA_INPUT_NONE)
    {
        lua_pushboolean(L, false);
        return 1;
    }
    else if ((lua_slots[current_lua_slot].capture_input == LUA_INPUT_LISTEN_GATED) && (current_lua_slot != atomic_get(&selected_page)))
    {
        lua_pushboolean(L, false);
        return 1;
    }

    int input = luaL_checkinteger(L, 1);

    int output = atomic_get(&inputs) & input;
    // output = get_bit(atomic_get(&inputs), input);

    lua_pushboolean(L, output);
    return 1;
}

int lua_input_capture(lua_State *L)
{
    int input = (int)luaL_checknumber(L, 1);
    if (input < 0 || input > 3)
    {
        return luaL_error(L, "Invalid input. Usage:\n0 = No input\n1=Capture input\n2=Listen when selected\n3=Listen always\nDefault:2");
    }

    lua_slots[get_current_lua_slot()].capture_input = input;

    int output = true; // Eventually, should output true or false based on whether user allowed or denied input capture
    lua_pushboolean(L, output);
    return 1;
}

static const luaL_Reg input_funcs[] = {
    {"get_input", lua_input_get},
    {"capture", lua_input_capture},
    {NULL, NULL},
};

LUAMOD_API int luaopen_input(lua_State *L)
{
    luaL_newlib(L, input_funcs);

    for (int i = 0; i < (int)INPUT_MAP_SIZE; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "%s", input_map[i].name);

        // Correctly shift the hardware bit index into a byte mask
        lua_pushinteger(L, 1 << input_map[i].bit);
        lua_setfield(L, -2, key); // ← sets input.LEFT, input.RIGHT, etc.
    }

    return 1;
}

//////////////
/// SLEEPS ///
//////////////

int lua_sleep_ms(lua_State *L)
{
    int ms = (int)luaL_checknumber(L, 1);
    k_msleep(ms);
    return 0;
}

//////////////
/// BUFFER ///
//////////////

#define T_USERDATA_FAT 1
typedef struct
{
    int type;
    struct
    {
        size_t size;
        void *ptr;
    };
} ud_fat_t;

#define UD_GET_BUFFER(ix)                                                   \
    ud_fat_t *buf = lua_touserdata(L, ix);                                  \
    luaL_argcheck(L, buf->type == T_USERDATA_FAT, ix, "`buffer' expected"); \
    luaL_argcheck(L, buf->ptr != NULL, ix, "`buffer' does not exist");

static int lua_alloc_buffer(lua_State *L)
{
    int size = luaL_checkinteger(L, 1);
    ud_fat_t *ptr = lua_newuserdata(L, sizeof(ud_fat_t) + size);
    ptr->type = T_USERDATA_FAT;
    ptr->ptr = (void *)(ptr + 1);
    ptr->size = size;
    return 1;
}

static int lua_buffer_size(lua_State *L)
{
    UD_GET_BUFFER(1);
    lua_pushinteger(L, buf->size);
    return 1;
}

static int lua_get_buffer_u8(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    luaL_argcheck(L, sizeof(uint8_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    lua_pushinteger(L, ((uint8_t *)buf->ptr)[ix]);
    return 1;
}

static int lua_get_buffer_s8(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    luaL_argcheck(L, sizeof(int8_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    lua_pushinteger(L, ((int8_t *)buf->ptr)[ix]);
    return 1;
}

static int lua_get_buffer_u16(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    luaL_argcheck(L, sizeof(uint16_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    lua_pushinteger(L, ((uint16_t *)buf->ptr)[ix]);
    return 1;
}

static int lua_get_buffer_s16(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    luaL_argcheck(L, sizeof(int16_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    lua_pushinteger(L, ((int16_t *)buf->ptr)[ix]);
    return 1;
}

static int lua_get_buffer_u32(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    luaL_argcheck(L, sizeof(uint32_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    lua_pushinteger(L, ((uint32_t *)buf->ptr)[ix]);
    return 1;
}

static int lua_get_buffer_s32(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    luaL_argcheck(L, sizeof(int32_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    lua_pushinteger(L, ((int32_t *)buf->ptr)[ix]);
    return 1;
}

static int lua_set_buffer_u8(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    unsigned value = luaL_checkinteger(L, 3);
    luaL_argcheck(L, sizeof(uint8_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    ((uint8_t *)buf->ptr)[ix] = value;
    return 0;
}

static int lua_set_buffer_s8(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    luaL_argcheck(L, sizeof(int8_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    ((int8_t *)buf->ptr)[ix] = value;
    return 0;
}

static int lua_set_buffer_u16(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    unsigned value = luaL_checkinteger(L, 3);
    luaL_argcheck(L, sizeof(uint16_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    ((uint16_t *)buf->ptr)[ix] = value;
    return 0;
}

static int lua_set_buffer_s16(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    luaL_argcheck(L, sizeof(int16_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    ((int16_t *)buf->ptr)[ix] = value;
    return 0;
}

static int lua_set_buffer_u32(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    unsigned value = luaL_checkinteger(L, 3);
    luaL_argcheck(L, sizeof(uint32_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    ((uint32_t *)buf->ptr)[ix] = value;
    return 0;
}

static int lua_set_buffer_s32(lua_State *L)
{
    UD_GET_BUFFER(1);
    unsigned ix = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    luaL_argcheck(L, sizeof(int32_t) * ix < buf->size, ix, "`index' is out of scope for the buffer.");
    ((int32_t *)buf->ptr)[ix] = value;
    return 0;
}

//////////////
/// DEVICE ///
//////////////

#define T_USERDATA_DEVICE 2
#define UD_GET_DEVICE(ix)                                                      \
    ud_device_t *dev = lua_touserdata(L, ix);                                  \
    luaL_argcheck(L, dev->type == T_USERDATA_DEVICE, ix, "`device' expected"); \
    luaL_argcheck(L, dev->device != NULL, ix, "`device' does not exist");
typedef struct
{
    int type;
    struct device *device;
} ud_device_t;

static int lua_device_get_binding(lua_State *L)
{
    char *name = luaL_checkstring(L, 1);
    ud_device_t *dev = lua_newuserdata(L, sizeof(ud_device_t));
    dev->type = T_USERDATA_DEVICE;
    dev->device = device_get_binding(name);
    return 1;
}

static int lua_device_is_ready(lua_State *L)
{
    UD_GET_DEVICE(1);
    lua_pushboolean(L, device_is_ready(dev->device));
    return 1;
}

///////////
/// ADC ///
///////////

static int lua_read_batt(lua_State *L)
{

    int battery_mv;

    int16_t battery_pptt = read_batt_mV(&battery_mv);

    lua_pushinteger(L, battery_mv);
    lua_pushinteger(L, battery_pptt);
    return 2;
}

//////////////
/// EEPROM ///
//////////////

/* static int lua_eeprom_read(lua_State * L) { */
/*     UD_GET_DEVICE(1); */
/*     UD_GET_BUFFER(2); */
/*     unsigned int offset = luaL_checkinteger(L, 3); */

/*     eeprom_read(dev->device, offset, buf->ptr, buf->size); */
/*     return 0; */
/* } */

/* static int lua_eeprom_write(lua_State * L) { */
/*     UD_GET_DEVICE(1); */
/*     UD_GET_BUFFER(2); */
/*     unsigned int offset = luaL_checkinteger(L, 3); */

/*     eeprom_write(dev->device, offset, buf->ptr, buf->size); */
/*     return 0; */
/* } */

/* static int lua_eeprom_get_size(lua_State * L) { */
/*     UD_GET_DEVICE(1); */
/*     lua_pushinteger(L, eeprom_get_size(dev->device)); */
/*     return 1; */
/* } */

/////////////
/// FLASH ///
/////////////

static int lua_flash_read(lua_State *L)
{
    UD_GET_DEVICE(1);
    UD_GET_BUFFER(2);
    unsigned int offset = luaL_checkinteger(L, 3);
    flash_read(dev->device, offset, buf->ptr, buf->size);
    return 0;
}

static int lua_flash_write(lua_State *L)
{
    UD_GET_DEVICE(1);
    UD_GET_BUFFER(2);
    unsigned int offset = luaL_checkinteger(L, 3);

    flash_write(dev->device, offset, buf->ptr, buf->size);
    return 0;
}

static int lua_flash_erase(lua_State *L)
{
    UD_GET_DEVICE(1);
    unsigned int offset = luaL_checkinteger(L, 2);
    unsigned int size = luaL_checkinteger(L, 3);
    flash_erase(dev->device, offset, size);
    return 0;
}

/* static int lua_flash_get_page_count(lua_State * L) { */
/*     UD_GET_DEVICE(1); */
/*     lua_pushinteger(L, flash_get_page_count(dev->device)); */
/*     return 1; */
/* } */

///////////////////
/// DISK ACCESS ///
///////////////////

// lua_disk_ls is written by generative AI. Requires review
static int lua_disk_ls(lua_State *L)
{
    const char *dir = luaL_checkstring(L, 1); /* const char*, not char* */
    lsdir_result_t result;

    int res = lsdir(dir, &result);
    if (res != 0)
    {
        lua_pushnil(L);
        lua_pushfstring(L, "Failed to list directory '%s' [%d]", dir, res);
        return 2; /* nil, errmsg */
    }

    /* Outer table — one entry per item found */
    lua_newtable(L);

    for (int i = 0; i < result.count; i++)
    {
        const lsdir_entry_t *e = &result.entries[i];

        /* Inner table: { name, is_dir, size? } */
        lua_newtable(L);

        lua_pushstring(L, e->name);
        lua_setfield(L, -2, "name");

        lua_pushboolean(L, e->is_dir);
        lua_setfield(L, -2, "is_dir");

        if (!e->is_dir)
        {
            lua_pushinteger(L, (lua_Integer)e->size);
            lua_setfield(L, -2, "size");
        }

        lua_rawseti(L, -2, i + 1); /* Lua tables are 1-indexed */
    }

    lsdir_free(&result);
    return 1; /* the outer table */
}

static int lua_disk_access_init(lua_State *L)
{
    char *name = luaL_checkstring(L, 1);
    lua_pushinteger(L, disk_access_init(name));
    return 1;
}

static int lua_disk_access_status(lua_State *L)
{
    char *name = luaL_checkstring(L, 1);
    lua_pushinteger(L, disk_access_status(name));
    return 1;
}

static int lua_disk_access_ioctl(lua_State *L)
{
    char *name = luaL_checkstring(L, 1);
    int cmd = luaL_checkinteger(L, 2);
    UD_GET_BUFFER(3);
    lua_pushinteger(L, disk_access_ioctl(name, cmd, buf->ptr));
    return 1;
}

///////////////////
/// FILE SYSTEM ///
///////////////////

#define T_USERDATA_FILE 5
#define UD_GET_FILE(ix)                    \
    ud_file_t *fp = lua_touserdata(L, ix); \
    luaL_argcheck(L, fp->type == T_USERDATA_FILE, ix, "`file' expected");
typedef struct
{
    int type;
    struct fs_file_t file;
} ud_file_t;

static int lua_fs_open(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    // Flags:
    /*
     | Bit | 7 | 6 |      5 |      4 | 3 | 2 |     1 |    0 |
     | Def |   |   | APPEND | CREATE |   |   | WRITE | READ |
    */
    int flags = luaL_optinteger(L, 2, 0);
    ud_file_t *ptr = lua_newuserdata(L, sizeof(ud_file_t));
    ptr->type = T_USERDATA_FILE;
    fs_file_t_init(&ptr->file);
    lua_pushinteger(L, fs_open(&ptr->file, name, flags));
    return 2;
}

static int lua_fs_close(lua_State *L)
{
    UD_GET_FILE(1);
    lua_pushinteger(L, fs_close(&fp->file));
    return 1;
}

static int lua_fs_unlink(lua_State *L)
{
    const char *file = luaL_checkstring(L, 1);
    lua_pushinteger(L, fs_unlink(file));
    return 1;
}

static int lua_fs_rename(lua_State *L)
{
    const char *file = luaL_checkstring(L, 1);
    const char *to = luaL_checkstring(L, 2);
    lua_pushinteger(L, fs_rename(file, to));
    return 1;
}

static int lua_fs_read(lua_State *L)
{
    UD_GET_FILE(1);
    UD_GET_BUFFER(2);
    size_t size;
    if (lua_isinteger(L, 3))
    {
        // Read a specific size
        size = luaL_checkinteger(L, 3);
    }
    else
    {
        size = buf->size;
    }

    lua_pushinteger(L, fs_read(&fp->file, buf->ptr, size));
    return 1;
}

static int lua_fs_write(lua_State *L)
{
    UD_GET_FILE(1);
    const void *ptr;
    size_t size;

    if (lua_isuserdata(L, 2))
    {
        UD_GET_BUFFER(2);
        ptr = buf->ptr;
        if (lua_isinteger(L, 3))
        {
            size = luaL_checkinteger(L, 3);
        }
        else
        {
            size = buf->size;
        }
    }
    else
    {
        const char *str = luaL_checkstring(L, 2);
        ptr = str;
        size = strlen(str);
    }

    lua_pushinteger(L, fs_write(&fp->file, ptr, size));
    return 1;
}

static int lua_fs_sync(lua_State *L)
{
    UD_GET_FILE(1);
    lua_pushinteger(L, fs_sync(&fp->file));
    return 1;
}

static int lua_fs_mkdir(lua_State *L)
{
    const char *file = luaL_checkstring(L, 1);
    lua_pushinteger(L, fs_mkdir(file));
    return 1;
}

///////////
/// I2C ///
///////////

static int lua_i2c_configure(lua_State *L)
{
    UD_GET_DEVICE(1);
    int flags = luaL_checkinteger(L, 2);
    int ret = i2c_configure(dev->device, flags);
    lua_pushinteger(L, ret);
    return 1;
}

// static int lua_i2c_get_config(lua_State * L) {
//     ud_device_t * dev = lua_touserdata(L,1);
//     int flags;
//     i2c_get_config(dev->device, &flags);
//     lua_pushinteger(L, flags);
//     return 1;
// }

// TODO i2c_write_read
// TODO i2c_read
// TODO i2c_write

///////////
/// PWM ///
///////////

static int lua_pwm_pin_set_cycles(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pwm = luaL_checkinteger(L, 2);
    int period = luaL_checkinteger(L, 3);
    int pulse = luaL_checkinteger(L, 4);
    pwm_flags_t flags = luaL_optinteger(L, 5, 0);
    pwm_set_cycles(dev->device, pwm, period, pulse, flags);
    return 0;
}

static int lua_pwm_pin_set_usec(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pwm = luaL_checkinteger(L, 2);
    int period = luaL_checkinteger(L, 3);
    int pulse = luaL_checkinteger(L, 4);
    pwm_flags_t flags = luaL_optinteger(L, 5, 0);
    pwm_set(dev->device, pwm, PWM_USEC(period), PWM_USEC(pulse), flags);
    return 0;
}

static int lua_pwm_pin_set_nsec(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pwm = luaL_checkinteger(L, 2);
    int period = luaL_checkinteger(L, 3);
    int pulse = luaL_checkinteger(L, 4);
    pwm_flags_t flags = luaL_optinteger(L, 5, 0);
    pwm_set(dev->device, pwm, period, pulse, flags);
    return 0;
}

////////////
/// GPIO ///
////////////

static int lua_gpio_pin_interrupt_configure(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    int flags = luaL_checkinteger(L, 3);
    int ret = gpio_pin_interrupt_configure(dev->device, pin, flags);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_gpio_pin_configure(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    int flags = luaL_checkinteger(L, 3);
    int ret = gpio_pin_configure(dev->device, pin, flags);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_gpio_port_get_raw(lua_State *L)
{
    UD_GET_DEVICE(1);
    int value;
    gpio_port_get_raw(dev->device, &value);
    lua_pushinteger(L, value);
    return 1;
}

static int lua_gpio_port_get(lua_State *L)
{
    UD_GET_DEVICE(1);
    int value;
    gpio_port_get(dev->device, &value);
    lua_pushinteger(L, value);
    return 1;
}

static int lua_gpio_port_set_masked_raw(lua_State *L)
{
    UD_GET_DEVICE(1);
    int mask = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    gpio_port_set_masked_raw(dev->device, mask, value);
    return 0;
}

static int lua_gpio_port_set_masked(lua_State *L)
{
    UD_GET_DEVICE(1);
    int mask = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    gpio_port_set_masked(dev->device, mask, value);
    return 0;
}

static int lua_gpio_port_set_bits_raw(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    gpio_port_set_bits_raw(dev->device, pin);
    return 0;
}

static int lua_gpio_port_set_bits(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    gpio_port_set_bits(dev->device, pin);
    return 0;
}

static int lua_gpio_port_clear_bits_raw(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    gpio_port_clear_bits_raw(dev->device, pin);
    return 0;
}

static int lua_gpio_port_clear_bits(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    gpio_port_clear_bits(dev->device, pin);
    return 0;
}

static int lua_gpio_port_toggle_bits(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    gpio_port_toggle_bits(dev->device, pin);
    return 0;
}

static int lua_gpio_port_set_clr_bits_raw(lua_State *L)
{
    UD_GET_DEVICE(1);
    int setpin = luaL_checkinteger(L, 2);
    int clrpin = luaL_checkinteger(L, 3);
    gpio_port_set_clr_bits_raw(dev->device, setpin, clrpin);
    return 0;
}

static int lua_gpio_port_set_clr_bits(lua_State *L)
{
    UD_GET_DEVICE(1);
    int setpin = luaL_checkinteger(L, 2);
    int clrpin = luaL_checkinteger(L, 3);
    gpio_port_set_clr_bits(dev->device, setpin, clrpin);
    return 0;
}

static int lua_gpio_pin_get_raw(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    int val = gpio_pin_get_raw(dev->device, pin);
    lua_pushinteger(L, val);
    return 1;
}

static int lua_gpio_pin_get(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    int val = gpio_pin_get(dev->device, pin);
    lua_pushinteger(L, val);
    return 1;
}

static int lua_gpio_pin_set_raw(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    gpio_pin_set_raw(dev->device, pin, value);
    return 0;
}

static int lua_gpio_pin_set(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    gpio_pin_set(dev->device, pin, value);
    return 0;
}

static int lua_gpio_pin_toggle(lua_State *L)
{
    UD_GET_DEVICE(1);
    int pin = luaL_checkinteger(L, 2);
    gpio_pin_toggle(dev->device, pin);
    return 0;
}

////////////////////////////
/// HARDWARE INFORMATION ///
////////////////////////////

// static int lua_hwinfo_get_reset_cause(lua_State * L) {
//     int cause;
//     hwinfo_get_reset_cause(&cause);
//     lua_pushinteger(L, cause);
//     return 1;
// }
//
// static int lua_hwinfo_get_supported_reset_cause(lua_State * L) {
//     int cause;
//     hwinfo_get_supported_reset_cause(&cause);
//     lua_pushinteger(L, cause);
//     return 1;
// }
//
// static int lua_hwinfo_clear_reset_cause() {
//     hwinfo_clear_reset_cause();
//     return 0;
// }

//////////////
/// PINMUX ///
//////////////

/* static inline int lua_pinmux_pin_set(lua_State * L) { */
/*     UD_GET_DEVICE(1); */
/*     unsigned int pin = luaL_checkinteger(L, 2); */
/*     unsigned int func = luaL_checkinteger(L, 3); */
/*     lua_pushinteger(L, pinmux_pin_set(dev->device, pin, func)); */
/*     return 1; */
/* } */

/* static inline int lua_pinmux_pin_get(lua_State * L) { */
/*     UD_GET_DEVICE(1); */
/*     unsigned int pin = luaL_checkinteger(L, 2); */
/*     unsigned int func; */
/*     pinmux_pin_get(dev->device, pin, &func); */
/*     lua_pushinteger(L, func); */
/*     return 1; */
/* } */

/* static inline int lua_pinmux_pin_pullup(lua_State * L) { */
/*     UD_GET_DEVICE(1); */
/*     unsigned int pin = luaL_checkinteger(L, 2); */
/*     unsigned int func = luaL_checkinteger(L, 3); */
/*     lua_pushinteger(L, pinmux_pin_pullup(dev->device, pin, func)); */
/*     return 1; */
/* } */

/* static inline int lua_pinmux_pin_input_enable(lua_State * L) { */
/*     UD_GET_DEVICE(1); */
/*     unsigned int pin = luaL_checkinteger(L, 2); */
/*     unsigned int func = luaL_checkinteger(L, 3); */
/*     lua_pushinteger(L, pinmux_pin_input_enable(dev->device, pin, func)); */
/*     return 1; */
/* } */

///////////
/// SPI ///
///////////

// static int lua_spi_transceive(lua_State * L) {}
// static int lua_spi_read(lua_State * L) {}
// static int lua_spi_write(lua_State * L) {}
// static int lua_spi_transceive_async(lua_State * L) {}
// static int lua_spi_read_async(lua_State * L) {}
// static int lua_spi_write_async(lua_State * L) {}

////////////
/// UART ///
////////////

static int lua_uart_err_check(lua_State *L)
{
    UD_GET_DEVICE(1);
    lua_pushinteger(L, uart_err_check(dev->device));
    return 1;
}

static int lua_uart_configure(lua_State *L)
{
    UD_GET_DEVICE(1);
    int baudrate = luaL_checkinteger(L, 2);
    struct uart_config cfg;
    cfg.baudrate = baudrate;
    cfg.parity = UART_CFG_PARITY_NONE;
    cfg.stop_bits = UART_CFG_STOP_BITS_1;
    cfg.data_bits = UART_CFG_DATA_BITS_8;
    cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
    lua_pushinteger(L, uart_configure(dev->device, &cfg));
    return 1;
}

static int lua_uart_config_get(lua_State *L)
{
    UD_GET_DEVICE(1);
    struct uart_config cfg;
    uart_config_get(dev->device, &cfg);
    lua_pushinteger(L, cfg.baudrate);
    return 1;
}

static int lua_uart_line_ctrl_set(lua_State *L)
{
    UD_GET_DEVICE(1);
    int ctrl = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    uart_line_ctrl_set(dev->device, ctrl, value);
    return 0;
}

static int lua_uart_line_ctrl_get(lua_State *L)
{
    UD_GET_DEVICE(1);
    int ctrl = luaL_checkinteger(L, 2);
    int value;
    uart_line_ctrl_get(dev->device, ctrl, &value);
    lua_pushinteger(L, value);
    return 1;
}

static int lua_uart_poll_in(lua_State *L)
{
    UD_GET_DEVICE(1);
    unsigned char c;
    int ret = uart_poll_in(dev->device, &c);
    lua_pushinteger(L, c);
    lua_pushinteger(L, ret);
    return 2;
}

// static int lua_uart_poll_in_u16(lua_State * L) {
// UD_GET_DEVICE(1);
// uint16_t c;
// int ret = uart_poll_in_u16(dev->device, &c);
// lua_pushinteger(L, c);
// lua_pushinteger(L, ret);
// return 2;
//}

static int lua_uart_poll_out(lua_State *L)
{
    UD_GET_DEVICE(1);
    unsigned char c = luaL_checkinteger(L, 2);
    uart_poll_out(dev->device, c);
    return 0;
}

// static int lua_uart_poll_out_u16(lua_State * L) {
// UD_GET_DEVICE(1);
// uint16_t c = luaL_checkinteger(L, 2);
// uart_poll_out_u16(dev->device, c);
// return 0;
//}

// static int lua_uart_fifo_fill(lua_State * L) {
//     UD_GET_DEVICE(1);
//     UD_GET_BUFFER(2);
//     lua_pushinteger(L, uart_fifo_fill(dev->device, buf->ptr, buf->size));
//     return 1;
// }

static int lua_uart_tx(lua_State *L)
{
    UD_GET_DEVICE(1);
    UD_GET_BUFFER(2);
    int32_t timeout = luaL_checkinteger(L, 3);
    lua_pushinteger(L, uart_tx(dev->device, buf->ptr, buf->size, timeout));
    return 1;
}

// static int lua_uart_tx_u16(lua_State * L) {
// UD_GET_DEVICE(1);
// UD_GET_BUFFER(2);
// int32_t timeout = luaL_checkinteger(L, 3);
// lua_pushinteger(L, uart_tx_u16(dev->device, buf->ptr, buf->size/2, timeout));
// return 1;
//}

static int lua_uart_tx_abort(lua_State *L)
{
    UD_GET_DEVICE(1);
    lua_pushinteger(L, uart_tx_abort(dev->device));
    return 1;
}

static int lua_uart_rx_enable(lua_State *L)
{
    UD_GET_DEVICE(1);
    UD_GET_BUFFER(2);
    int32_t timeout = luaL_checkinteger(L, 3);
    lua_pushinteger(L, uart_rx_enable(dev->device, buf->ptr, buf->size, timeout));
    return 1;
}

// static int lua_uart_rx_enable_u16(lua_State * L) {
// UD_GET_DEVICE(1);
// UD_GET_BUFFER(2);
// int32_t timeout = luaL_checkinteger(L, 3);
// lua_pushinteger(L, uart_rx_enable_u16(dev->device, buf->ptr, buf->size/2, timeout));
// return 1;
//}

static int lua_uart_rx_disable(lua_State *L)
{
    UD_GET_DEVICE(1);
    lua_pushinteger(L, uart_rx_disable(dev->device));
    return 1;
}

//////////////////
/// RAW ACCESS ///
//////////////////

static int lua_peek(lua_State *L)
{
    int addr = luaL_checkinteger(L, 1);
    int offset = luaL_optinteger(L, 2, 0);
    int size = luaL_optinteger(L, 3, 4);
    int ptr = addr + size * offset;
    int ret = 0;

    if (ptr & 0x3)
    {
        // Unaligned memory access
        memcpy(&ret, (void *)ptr, size);
    }
    else
    {
        switch (size)
        {
        case 1:
            ret = *(uint8_t *)ptr;
            break;
        case 2:
            ret = *(uint16_t *)ptr;
            break;
        case 4:
            ret = *(int *)ptr;
            break;
        }
    }
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_poke(lua_State *L)
{
    int addr = luaL_checkinteger(L, 1);
    int val = luaL_checkinteger(L, 2);
    int offset = luaL_optinteger(L, 3, 0);
    int size = luaL_optinteger(L, 4, 4);
    int ptr = addr + size * offset;
    if (ptr & 0x3)
    {
        // Unaligned memory access
        memcpy((void *)ptr, &val, size);
    }
    else
    {
        switch (size)
        {
        case 1:
            *(uint8_t *)ptr = val;
            break;
        case 2:
            *(uint16_t *)ptr = val;
            break;
        case 4:
            *(int *)ptr = val;
            break;
        }
    }

    return 0;
}

//////////////////////////
/// Library definition ///
//////////////////////////

static const luaL_Reg zephyr_funcs[] = {
    // buffer
    {"alloc_buffer", lua_alloc_buffer},
    {"buffer_size", lua_buffer_size},
    {"get_buffer_u8", lua_get_buffer_u8},
    {"get_buffer_s8", lua_get_buffer_s8},
    {"get_buffer_u16", lua_get_buffer_u16},
    {"get_buffer_s16", lua_get_buffer_s16},
    {"get_buffer_u32", lua_get_buffer_u32},
    {"get_buffer_s32", lua_get_buffer_s32},
    {"set_buffer_u8", lua_set_buffer_u8},
    {"set_buffer_s8", lua_set_buffer_s8},
    {"set_buffer_u16", lua_set_buffer_u16},
    {"set_buffer_s16", lua_set_buffer_s16},
    {"set_buffer_u32", lua_set_buffer_u32},
    {"set_buffer_s32", lua_set_buffer_s32},
    // device model
    {"device_get_binding", lua_device_get_binding},
    {"device_is_ready", lua_device_is_ready},
    // EEPROM
    /* {"eeprom_read", lua_eeprom_read}, */
    /* {"eeprom_write", lua_eeprom_write}, */
    /* {"eeprom_get_size", lua_eeprom_get_size}, */
    // FLASH
    {"flash_read", lua_flash_read},
    {"flash_write", lua_flash_write},
    {"flash_erase", lua_flash_erase},
    /* {"flash_get_page_count", flash_get_page_count}, */
    // Disk
    {"ls", lua_disk_ls},
    {"disk_access_init", lua_disk_access_init},
    {"disk_access_status", lua_disk_access_status},
    {"disk_access_ioctl", lua_disk_access_ioctl},
    // File System
    // TODO
    {"fs_open", lua_fs_open},
    {"fs_close", lua_fs_close},
    {"fs_unlink", lua_fs_unlink},
    {"fs_rename", lua_fs_rename},
    {"fs_read", lua_fs_read},
    {"fs_write", lua_fs_write},
    /* {"fs_seek", lua_fs_seek}, */
    /* {"fs_tell", lua_fs_tell}, */
    /* {"fs_truncate", lua_fs_truncate}, */
    {"fs_sync", lua_fs_sync},
    {"fs_mkdir", lua_fs_mkdir},
    /* {"fs_opendir", lua_fs_opendir}, */
    /* {"fs_readdir", lua_fs_readdir}, */
    /* {"fs_closedir", lua_fs_closedir}, */
    /* {"fs_mount", lua_fs_mount}, */
    /* {"fs_unmount", lua_fs_unmount}, */
    /* {"fs_readmount", lua_fs_readmount}, */
    /* {"fs_stat", lua_fs_stat}, */
    /* {"fs_statvfs", lua_fs_statvfs}, */
    /* {"fs_register", lua_fs_register}, */
    /* {"fs_unregister", lua_fs_unregister}, */
    // i2c
    /* {"i2c_get_config", lua_i2c_get_config}, */
    {"i2c_configure", lua_i2c_configure},
    // PWM
    {"pwm_pin_set_cycles", lua_pwm_pin_set_cycles},
    {"pwm_pin_set_usec", lua_pwm_pin_set_usec},
    {"pwm_pin_set_nsec", lua_pwm_pin_set_nsec},
    // gpio
    {"gpio_pin_interrupt_configure", lua_gpio_pin_interrupt_configure},
    {"gpio_pin_configure", lua_gpio_pin_configure},
    {"gpio_port_get", lua_gpio_port_get},
    {"gpio_port_get_raw", lua_gpio_port_get_raw},
    {"gpio_port_set_masked", lua_gpio_port_set_masked},
    {"gpio_port_set_masked_raw", lua_gpio_port_set_masked_raw},
    {"gpio_port_set_bits", lua_gpio_port_set_bits},
    {"gpio_port_set_bits_raw", lua_gpio_port_set_bits_raw},
    {"gpio_port_clear_bits", lua_gpio_port_clear_bits},
    {"gpio_port_clear_bits_raw", lua_gpio_port_clear_bits_raw},
    {"gpio_port_toggle_bits", lua_gpio_port_toggle_bits},
    {"gpio_port_set_clr_bits_raw", lua_gpio_port_set_clr_bits_raw},
    {"gpio_port_set_clr_bits", lua_gpio_port_set_clr_bits},
    {"gpio_pin_get", lua_gpio_pin_get},
    {"gpio_pin_get_raw", lua_gpio_pin_get_raw},
    {"gpio_pin_set", lua_gpio_pin_set},
    {"gpio_pin_set_raw", lua_gpio_pin_set_raw},
    {"gpio_pin_toggle", lua_gpio_pin_toggle},
    // hwinfo
    /* {"hwinfo_get_reset_cause", lua_hwinfo_get_reset_cause}, */
    /* {"hwinfo_get_supported_reset_cause", lua_hwinfo_get_supported_reset_cause}, */
    /* {"hwinfo_clear_reset_cause", lua_hwinfo_clear_reset_cause}, */
    // pinmux
    /* {"pinmux_pin_set", lua_pinmux_pin_set}, */
    /* {"pinmux_pin_get", lua_pinmux_pin_set}, */
    /* {"pinmux_pin_pullup", lua_pinmux_pin_pullup}, */
    /* {"pinmux_pin_input_enable", lua_pinmux_pin_input_enable}, */
    // uart
    {"uart_err_check", lua_uart_err_check},
    {"uart_configure", lua_uart_configure},
    {"uart_config_get", lua_uart_config_get},
    {"uart_line_ctrl_set", lua_uart_line_ctrl_set},
    {"uart_line_ctrl_get", lua_uart_line_ctrl_get},
    {"uart_poll_in", lua_uart_poll_in},
    /* {"uart_poll_in_u16", lua_uart_poll_in_u16}, */
    {"uart_poll_out", lua_uart_poll_out},
    /* {"uart_poll_out_u16", lua_uart_poll_out_u16}, */
    {"uart_tx", lua_uart_tx},
    /* {"uart_tx_u16", lua_uart_tx_u16}, */
    {"uart_tx_abort", lua_uart_tx_abort},
    {"uart_rx_enable", lua_uart_rx_enable},
    /* {"uart_rx_enable_u16", lua_uart_rx_enable_u16}, */
    {"uart_rx_disable", lua_uart_rx_disable},
    // TODO statistics
    // TODO colorimetry
    // TODO probability
    // Raw memory access
    {"peek", lua_peek},
    {"poke", lua_poke},
    {"sleep", lua_sleep_ms},
    {"read_batt_mv", lua_read_batt},
    {NULL, NULL},
};

LUAMOD_API int luaopen_zephyr(lua_State *L)
{
    luaL_newlib(L, zephyr_funcs);
    return 1;
}

//////////////////
/// TEENSY LIB ///
/////////////////

static int lua_pwm_set(lua_State *L)
{
    uint8_t *ptr = (uint8_t *)luaL_checkinteger(L, 1);
    int pwm = luaL_checkinteger(L, 2);
    int half = luaL_checkinteger(L, 3);
    int full = luaL_checkinteger(L, 4);
    int prescaler = luaL_optinteger(L, 5, 0);

    uint16_t *pwm_ptr = (uint16_t *)(ptr + (0x60 * pwm));
    uint16_t *status_reg = (uint16_t *)(ptr + 0x188);

    pwm_ptr[11] = half;
    pwm_ptr[7] = full;
    // Control register
    pwm_ptr[3] = 0xC04 | (prescaler & 0xF) << 4;

    while (*status_reg & 0xf)
        ;
    /* lua_pushinteger(L, *status_reg); */
    *status_reg = 0x101;

    return 0;
}

static const luaL_Reg teensy_funcs[] = {
    {"pwm_set", lua_pwm_set},
    {NULL, NULL},
};

LUAMOD_API int luaopen_teensy(lua_State *L)
{
    luaL_newlib(L, teensy_funcs);
    return 1;
}
