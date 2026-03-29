local paint = require("paint")

-- Load the file into memory
-- BMP files must be 1 bit per pixel (bpp). In Photoshop, this is the "Bitmap" colour mode.
local image, width, height = paint.load_bmp("/SD:/blit/blit_test.bmp")

-- The blit function also accepts a mask for transparency, where a bit being 1 is transparent, and a 0 is opaque.
-- In this case, the mask is equal to the image, but it would more commonly be a silhouette of what you want to display.
-- The mask should always be the same dimensions as the image you are using it on!
local mask = image

-- Clear the lua buffer.
paint.clear()

-- Blit the image to the lua frame buffer
paint.blit(image, width, height, 10, 40, mask)
-- The mask is optional. If you don't have one, blit will treat the entire image as opaque by default.
-- paint.blit(image, width, height, 10, 40)

-- Update the display
paint.display()
-- More accurately, this copies the lua frame buffer(s) to the display output buffer.
-- The LCD display actually updates its display constantly, depending on the selected FPS.