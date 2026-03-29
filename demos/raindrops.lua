-- Only minimal libraries are loaded for every script.
-- You'll need to enable basic features like math. This is to save RAM.
local paint = require("paint")
local math = require("math")

local drops = {}
local MAX_DROPS = 10
local WIDTH = 132
local HEIGHT = 250

local function new_drop()
    return {
        x = math.random(30, WIDTH - 20),
        y = math.random(20, HEIGHT - 20),
        r = 0,
        max_r = math.random(2, 20)
    }
end

math.randomseed(42)
for i = 1, MAX_DROPS do
    local d = new_drop()
    d.r = math.random(0, d.max_r) -- stagger initial radii
    drops[i] = d
end

while true do
	-- You can pass 0 or 1 to paint.clear as an argument to specify
	-- whether to fill white or black. By default, it's white (1)
    paint.clear()

    for i = 1, MAX_DROPS do
        local d = drops[i]

        -- first draws a black circle
        paint.circle(d.x, d.y, d.r, 0)
        if d.r > 2 then
			-- then draws a white circle, 2px smaller than the black circle. so that it appears as a ring
            paint.circle(d.x, d.y, d.r - 2, 1)
        end

        d.r = d.r + 1
        if d.r > d.max_r then
            drops[i] = new_drop()
        end
    end
	
	paint.text(20,50,1,"Hello from Lua")
	
	paint.display()
	paint.wait_for_display()
end