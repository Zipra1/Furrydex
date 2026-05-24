local paint = require("paint")

local image = paint.load_bmp("/SD:/blit/blit_test.bmp")

-----
-- full size canvas & mask example
-----

local canvas = paint.new_canvas()
-- A canvas can be made to use as a mask.
-- By default, the canvas' dimensions are the same as the screens dimensions.

paint.circle(55 + 16, 40 + 16, 10, 0, canvas)
-- By adding a canvas on the end of a paint function, it paints to that canvas
-- instead of the lua buffer.

paint.blit(image, 55, 40)

paint.display(canvas)
-- paint.display accepts a mask, but it must be the same dimensions as the screen.
-- Note that a mask takes priority over paint.clear(); if you clear() and then display with a mask,
-- only the area permitted by the mask is actually cleared.

paint.clear(nil, canvas)
-- paint.clear can clear a canvas if one is provided in the second argument.
-- Providing "nil" makes it use the default (white/1)
-- You can also provide 1 or 0 instead of nil.



-----
--small canvas & mask example
-----

local canvas_2 = paint.new_canvas(32, 32)
-- Ideally, you should keep the canvas small to save ram.

paint.circle(16, 16, 10, 0, canvas_2)
paint.circle(26, 5, 3, 0, canvas_2)
paint.circle(26-7, 5+7, 3, 1, canvas_2)

paint.clear()
paint.blit(image, 55, 40, canvas_2)
-- paint.blit accepts a mask

paint.display()

paint.blit(canvas_2, 55, 90)
-- Instead of using as a mask, this is just showing what's on canvas_2

paint.display()
