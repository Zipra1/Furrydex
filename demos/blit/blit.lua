local paint = require("paint")

-- Load the file into memory
-- BMP files must be 1bpp (bit per pixel). In Photoshop, this is the "Bitmap" colour mode.
-- Images load in as a canvas, which can later be modified, displayed, and masked.
local image = paint.load_bmp("/SD:/blit/blit_test.bmp")

-- Clear the lua buffer.
paint.clear()

-- Blit the image to the lua frame buffer
paint.blit(image, 0, 40)
-- The mask is optional. If you don't have one, blit will treat the entire image as opaque by default.
-- paint.blit(image, x, y)

-- You can get the width and height from a canvas like so:
print(image.width)
print(image.height)

-- Update the display
paint.display()
-- More accurately, this copies the lua frame buffer(s) to the display output buffer.
-- The LCD display actually updates its display constantly, depending on the selected FPS.