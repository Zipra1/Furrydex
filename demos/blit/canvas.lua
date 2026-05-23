local paint = require("paint")

local image, width, height = paint.load_bmp("/SD:/blit/blit_test.bmp")

-----
-- full size canvas & mask example
-----

local canvas = paint.new_canvas()
-- A canvas can be made to use as a mask.
-- By default, the canvas' dimensions are the same as the screens dimensions.

paint.circle(55 + 16, 40 + 16, 10, 0, canvas)
-- By adding a canvas on the end of a paint function, it paints to that canvas
-- instead of the lua buffer.

paint.blit(image, width, height, 55, 40)

paint.display(canvas)
-- paint.display accepts a mask, but it must be the same dimensions as the screen.
-- Note that a mask takes priority over paint.clear(); if you clear() and then display with a mask,
-- only the area permitted by the mask is actually cleared.

paint.clear(nil, canvas)
-- paint.clear can clear a canvas. Providing "nil" just makes it use the default colour
-- You can also provide 1 or 0 instead of nil.



-----
--small canvas & mask example
-----

local canvas_2 = paint.new_canvas(32, 32)
-- Ideally, you should keep the canvas small to save ram.

paint.circle(16, 16, 10, 0, canvas_2)

paint.clear()
paint.blit(image, width, height, 55, 40, canvas_2)
-- paint.blit accepts a mask

paint.display()