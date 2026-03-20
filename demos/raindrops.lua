local drops = {}
local MAX_DROPS = 5
local WIDTH = 132
local HEIGHT = 250

local function new_drop()
    return {
        x = math.random(20, WIDTH - 10),
        y = math.random(10, HEIGHT - 10),
        r = 0,
        max_r = math.random(10, 30)
    }
end

math.randomseed(42)
for i = 1, MAX_DROPS do
    local d = new_drop()
    d.r = math.random(0, d.max_r) -- stagger initial radii
    drops[i] = d
end

while true do
    -- clear screen
    paint.circle(71, 125, 140, 1)

    for i = 1, MAX_DROPS do
        local d = drops[i]

        -- draw expanding rings
        paint.circle(d.x, d.y, d.r, 0)
        if d.r > 2 then
            paint.circle(d.x, d.y, d.r - 2, 1)
        end

        d.r = d.r + 1
        if d.r > d.max_r then
            drops[i] = new_drop()
        end
    end
	paint.display()
	sleep_ms(50)
end