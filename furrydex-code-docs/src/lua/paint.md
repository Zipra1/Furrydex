## This document is currently out of date, since the paint library is currently undergoing large changes.

# paint
The paint library is used to draw shapes to Lua's render buffer and push them to the main render buffer, which is then displayed on the screen.

```lua
-- Include this at the top of your script to use paint:
local paint = require("paint")
```


## wait_for_display()
Waits until display sends frame trigger signal. Should be put before `display()` in most cases.

```lua
-- Usage in Lua
paint.wait_for_display()
```

```c
// C
static int lua_paint_wait_for_display(lua_State *L)
{
    waitForTE();
    return 0;
}
```

## display()
If the current script should be displaying, copy the Lua render buffer to the main render buffer. The Lua render buffer is not visible until this is called.
Accepts a mask in the form of a string or canvas.

```lua
-- Usage in Lua
paint.display()
```
```c
// C
static int lua_paint_display(lua_State *L)
{
    if (should_display())
    {
        k_mutex_lock(&paint_mutex, K_FOREVER);
        if (!lua_isnoneornil(L, 1))
        {
            if (lua_isinteger(L, 1) && lua_tointeger(L, 1) == 1)
            {
                blitMask(main_buffer,
                         CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                         lua_buffer,
                         CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                         lua_buffer, 0, 0);
            }
            else
            {
                size_t mask_len;
                const uint8_t *mask = (const uint8_t *)luaL_checklstring(L, 1, &mask_len);
                blitMask(main_buffer,
                         CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                         lua_buffer,
                         CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                         mask, 0, 0);
            }
        }
        else
        {
            memcpy(main_buffer, lua_buffer, CONFIG_FURRYDEX_FRAME_BYTES_BUFFER);
        }
        k_mutex_unlock(&paint_mutex);
    }
    return 0;
}
```

## clear(`optinteger(colour)`)
Set the entire Lua render buffer to white (1). If provided with an integer of 0-1, then it will set the entire Lua render buffer to that colour.

```lua
-- Usage in Lua
paint.clear() -- Set entire buffer to white
paint.clear(1) -- Same as above
paint.clear(0) -- Set entire buffer to black
```
```c
// C
static int lua_paint_clear(lua_State *L)
{
    if (should_display())
    {
        int colour = luaL_optinteger(L, 1, 1);
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        memset(lua_buffer, colour ? 0xFF : 0x00, CONFIG_FURRYDEX_FRAME_BYTES_BUFFER);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0;
}
```

## pixel(`x`,`y`,`colour`)
Set a single pixel to colour. 1 = white, 0 = black.
```lua
-- Usage in Lua
paint.pixel(30,20,0)
```
```c
// C

static int lua_paint_pixel(lua_State *L)
{
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int colour = luaL_checknumber(L, 3);
    if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintPixel(lua_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, x, y, colour);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0;
}
```

## rect(`x1`,`y1`,`x2`,`y2`,`colour`)
Draw a rectangle from x1,y1 to x2,y2 with colour. 1 = white, 0 = black.
```lua
-- Usage in Lua
paint.rect(30,30,50,50,0) -- Draw 20x20 black rectangle
```
```c
// C
static int lua_paint_rect(lua_State *L)
{
    int x1 = luaL_checknumber(L, 1);
    int y1 = luaL_checknumber(L, 2);
    int x2 = luaL_checknumber(L, 3);
    int y2 = luaL_checknumber(L, 4);
    int colour = luaL_checknumber(L, 5);
    if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintRegion(lua_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, x1, y1, x2, y2, colour);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0; // no return values pushed to Lua
}
```

## circle(`x`,`y`,`radius`,`colour`)
Draw a circle at position `x`,`y` with radius and colour. 1 = white, 0 = black.
```lua
-- Usage in Lua
paint.circle(30,20,10,0) -- Draw a black circle
paint.circle(30,20,5,1) -- Draw a smaller white circle over top the previous
```
```c
static int lua_paint_circle(lua_State *L)
{
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int r = luaL_checknumber(L, 3);
    int colour = luaL_checknumber(L, 4);
    if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintFilledCircle(lua_buffer,
                          CONFIG_FURRYDEX_DISPLAY_WIDTH,
                          CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                          x, y, r, colour);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0; // no return values pushed to Lua
}
```

## character(`character`,`x`,`y`)
Draw a single character at position `x`,`y`

Expected to be deprecated in the near future, use `text`.

## text(`x`,`y`,`kerning`,`string`)
Draw text `string` at position `x`,`y` with spacing between letters of `kerning`
Use \n to make a newline.
```lua
-- Usage in Lua
paint.text(20,10,1,"Hello World")
```
```c
// C
static int lua_paint_text(lua_State *L)
{
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int kerning = luaL_checknumber(L, 3);
    const char *text = luaL_checkstring(L, 4);
    if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintText(lua_buffer, kerning, x, y, text);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0; // no return values pushed to Lua
}
```

## text_wrap(`x`,`y`,`kerning`,`width`,`string`)
Draw text `string` at position `x`,`y` with spacing between letters of `kerning` and a box width of `width`. If a word were to make the lines width exceed `width`, it will be moved to the next line.
```lua
-- Usage in Lua
paint.text_wrap(20,10,1,60,"This text is so long that it will wrap around. Look at me go! I'm wrapping all the way around, making a big box of text that doesn't spill off screen!")
```
```c
// C
static int lua_paint_text_wrap(lua_State *L)
{
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int kerning = luaL_checknumber(L, 3);
    int width = luaL_checknumber(L, 4);
    const char *text = luaL_checkstring(L, 5);
    if (should_display())
    {
        k_mutex_lock(&lua_paint_mutex, K_FOREVER);
        paintTextWrap(lua_buffer, kerning, x, y, width, text);
        k_mutex_unlock(&lua_paint_mutex);
    }
    return 0; // no return values pushed to Lua
}
```

## blit(`img`,`img_w`,`img_h`,`x`,`y`, `opt(mask)`)
Paint image `img` of width `img_w` and height `img_h` at coordinates `x`, `y` with optional mask `mask`

If no mask is provided, the entire image bounds will be treated as opaque. The mask should be the same dimensions as `img`
```lua
-- Usage in Lua
-- Images can be made manually, too. But loading a file is easy
local image, width, height = paint.load_bmp("/SD:/blit/blit_test.bmp")

-- In this case, the mask is equal to the image, but it would more commonly be a silhouette of what you want to display.
local mask = image
paint.blit(image, width, height, 10, 40)
```

```c
static int lua_paint_blit(lua_State *L)
{
    size_t src_len;
    const uint8_t *src = (const uint8_t *)luaL_checklstring(L, 1, &src_len);
    int src_w = luaL_checkinteger(L, 2);
    int src_h = luaL_checkinteger(L, 3);
    int x = luaL_checkinteger(L, 4);
    int y = luaL_checkinteger(L, 5);
    if (should_display())
    {
        const uint8_t *mask = NULL;
        if (!lua_isnoneornil(L, 6))
        {
            size_t mask_len;
            mask = (const uint8_t *)luaL_checklstring(L, 6, &mask_len);

            k_mutex_lock(&lua_paint_mutex, K_FOREVER);
            blitMask(lua_buffer,
                     CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT,
                     src,
                     src_w, src_h,
                     mask, x, y);
        }
        else
        {
            blit(lua_buffer, CONFIG_FURRYDEX_DISPLAY_WIDTH, CONFIG_FURRYDEX_DISPLAY_HEIGHT, src, src_w, src_h, x, y);
        }

        k_mutex_unlock(&lua_paint_mutex);
        lua_gc(L, LUA_GCSTEP, 100);
    }
    return 0;
}
```

## load_bmp(`directory`)
Takes string `directory` and returns `image`, `width`, `height` of the bitmap.
Must be a 1bpp (black and white) `.bmp` file. Invalid files will either cause an error, or not display properly.

```lua
-- Usage in Lua:
local image, width, height = paint.load_bmp("/SD:/blit/blit_test.bmp")
```
```c
// C
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
    uint8_t *out_buf = malloc(out_size);
    if (bmp_buf == NULL || out_buf == NULL)
    {
        free(bmp_buf);
        free(out_buf);
        fs_close(&f);
        return luaL_error(L, "out of memory");
    }

    fs_seek(&f, pixel_offset, FS_SEEK_SET);
    fs_read(&f, bmp_buf, bmp_stride * bmp_h);
    fs_close(&f);

    // Convert from BMP layout (bottom-up, 4-byte padded rows)
    // to tightly packed top-down layout
    memset(out_buf, 0, out_size);
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
                out_buf[dst_byte] |= (1 << dst_bit);
            else
                out_buf[dst_byte] &= ~(1 << dst_bit);
        }
    }

    free(bmp_buf);

    // Push as Lua string (binary data), width, height
    lua_pushlstring(L, (const char *)out_buf, out_size);
    lua_pushinteger(L, bmp_w);
    lua_pushinteger(L, bmp_h);

    free(out_buf);
    return 3;
}
```

### new_canvas(optint(width),optint(height))
Creates a canvas which can be painted on instead of painting directly to the Lua buffer.
Default dimensions are the same as the screens dimensions.
```lua
-- Usage in Lua
paint.new_canvas() -- Canvas of dimensions FURRYDEX_DISPLAY_WIDTH x FURRYDEX_DISPLAY_HEIGHT
paint.new_canvas(32,32) -- Canvas of dimensions 32 x 32
```
```c
// C
static int lua_paint_new_canvas(lua_State *L)
{
    int w = luaL_optinteger(L, 1, CONFIG_FURRYDEX_DISPLAY_WIDTH);
    int h = luaL_optinteger(L, 2, CONFIG_FURRYDEX_DISPLAY_HEIGHT);

    size_t size = ((w + 7) / 8) * h;

    canvas_t *canvas = lua_newuserdata(L, sizeof(canvas_t));
    canvas->type = T_CANVAS;
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
```