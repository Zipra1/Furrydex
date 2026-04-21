local paint = require("paint")
local input = require("input")
local zephyr = require("zephyr")
local math = require("math")

local ball_radius = 2
local ball_speed = 4
local paddle_width = 15

local paddle_pos = 71
local ball_angle = math.pi / 2.5
local ball_pos_x = 71
local ball_pos_y = 220

local block_cols = 8
local block_rows = 4
local block_w = 13
local block_h = 7
local block_gap = 2
local block_start_x = 12
local block_start_y = 85

local blocks = {}
local blocks_broken = 0

local function reset()
    paddle_pos = 71
    ball_pos_x = 71
    ball_pos_y = 220
    blocks_broken = 0
    ball_speed = 4
    ball_angle = math.pi / 2.5
    while input.get_input(3) == 0 do
        paint.clear()
        paint.text(20, 50, 1, "Press A to start")
        paint.text(14, 60, 1, "(UP/DOWN) # Rows: ")
        paint.text(115, 60, 1, block_rows)
        if input.get_input(4) == 1 then
            block_rows = block_rows + 1
            zephyr.sleep(100)
        end
        if input.get_input(7) == 1 then
            block_rows = block_rows - 1
            zephyr.sleep(100)
        end

        for r = 1, block_rows do
            blocks[r] = {}
            for c = 1, block_cols do
                blocks[r][c] = true
            end
        end

        paint.circle(ball_pos_x, ball_pos_y, ball_radius, 0)
        paint.rect(paddle_pos - paddle_width, 230, paddle_pos + paddle_width, 235, 0)

        for r = 1, block_rows do -- oh man
            for c = 1, block_cols do
                if blocks[r][c] then
                    local bx = block_start_x + (c - 1) * (block_w + block_gap)
                    local by = block_start_y + (r - 1) * (block_h + block_gap)

                    paint.rect(bx, by, bx + block_w, by + block_h, 0)

                    local cx = math.max(bx, math.min(ball_pos_x, bx + block_w))
                    local cy = math.max(by, math.min(ball_pos_y, by + block_h))
                    local dx = ball_pos_x - cx
                    local dy = ball_pos_y - cy

                    if (dx * dx + dy * dy) < (ball_radius * ball_radius) then
                        blocks[r][c] = false
                        if math.abs(dy) >= math.abs(dx) then
                            ball_angle = (math.pi * 2) - ball_angle
                        else
                            ball_angle = math.pi - ball_angle
                        end
                    end
                end
            end
        end
        paint.wait_for_display()
        paint.display()
    end
    paint.text(20, 50, 1, "Starting in 3")
    paint.wait_for_display()
    paint.display()
    zephyr.sleep(1000)
    paint.text(20, 50, 1, "Starting in 2")
    paint.wait_for_display()
    paint.display()
    zephyr.sleep(1000)
    paint.text(20, 50, 1, "Starting in 1")
    paint.wait_for_display()
    paint.display()
    zephyr.sleep(1000)
end

reset()

while true do
    paint.clear() -- Clear the screen
    --paint.text(30,30,1,"meow!")
    --paint.text(30,40,1,input.get_input(0)) -- NC
    --paint.text(30,50,1,input.get_input(1)) -- B
    --paint.text(30,60,1,input.get_input(2)) -- C
    --paint.text(30,70,1,input.get_input(3)) -- A
    --paint.text(30,80,1,input.get_input(4)) -- up
    --paint.text(30,90,1,input.get_input(5)) -- right
    --paint.text(30,100,1,input.get_input(6)) -- left
    --paint.text(30,110,1,input.get_input(7)) -- down
    --paint.text(30,120,1,paddle_pos)
    --paint.text(30,130,1,ball_pos_x)
    --paint.text(30,140,1,ball_pos_y)
    paint.text(30, 30, 1, blocks_broken)

    -------------------
    -- PADDLE INPUTS --
    -------------------
    if input.get_input(5) == 1 then -- right
        if paddle_pos < (132 - paddle_width) then
            paddle_pos = paddle_pos + 3
        end
    elseif input.get_input(6) == 1 then -- left
        if paddle_pos > (10 + paddle_width) then
            paddle_pos = paddle_pos - 3
        end
    end

    ---------------
    -- COLLISION --
    ---------------
    if (ball_pos_x - ball_radius) < 10 then -- left wall
        ball_pos_x = ball_radius + 10
        ball_angle = math.pi - ball_angle
    elseif (ball_pos_x + ball_radius) > 132 then -- right wall
        ball_pos_x = 132 - ball_radius
        ball_angle = math.pi - ball_angle
    end
    paint.rect(10, 78, 132, 80, 0)
    if (ball_pos_y < ball_radius + 80) then -- top wall
        ball_pos_y = ball_radius + 80
        ball_angle = (math.pi * 2) - ball_angle
    elseif (ball_pos_y + ball_radius) > 240 then -- bottom wall
        ball_pos_y = 240 - ball_radius
        ball_angle = (math.pi * 2) - ball_angle
        reset()
    end

    if (ball_pos_y + ball_radius) > 230 then -- paddle
        if ((paddle_pos - paddle_width) < ball_pos_x) and ((paddle_pos + paddle_width) > ball_pos_x) then
            ball_pos_y = 230
            local hit_pos = (ball_pos_x - paddle_pos) / paddle_width
            ball_angle = (math.pi * 2) - ball_angle
            ball_angle = ball_angle + (hit_pos * 0.25)
        end
    end

    ---------------
    -- RENDERING --
    ---------------
    ball_pos_x = ball_pos_x + math.cos(ball_angle) * ball_speed
    ball_pos_y = ball_pos_y + math.sin(ball_angle) * ball_speed
    paint.circle(ball_pos_x, ball_pos_y, ball_radius, 0)

    paint.rect(paddle_pos - paddle_width, 230, paddle_pos + paddle_width, 235, 0)

    ------------
    -- BLOCKS --
    ------------

    for r = 1, block_rows do
        for c = 1, block_cols do
            if blocks[r][c] then
                local bx = block_start_x + (c - 1) * (block_w + block_gap)
                local by = block_start_y + (r - 1) * (block_h + block_gap)

                paint.rect(bx, by, bx + block_w, by + block_h, 0)

                local cx = math.max(bx, math.min(ball_pos_x, bx + block_w))
                local cy = math.max(by, math.min(ball_pos_y, by + block_h))
                local dx = ball_pos_x - cx
                local dy = ball_pos_y - cy

                if (dx * dx + dy * dy) < (ball_radius * ball_radius) then
                    blocks[r][c] = false
                    blocks_broken = blocks_broken + 1
                    if blocks_broken >= 4 then
                        ball_speed = 5
                    end
                    if blocks_broken >= 12 then
                        ball_speed = 5.7
                    end
                    if blocks_broken >= (block_cols * block_rows) then
                        reset()
                    end
                    if math.abs(dy) >= math.abs(dx) then
                        ball_angle = (math.pi * 2) - ball_angle
                    else
                        ball_angle = math.pi - ball_angle
                    end
                end
            end
        end
    end


    paint.wait_for_display()
    paint.display()
end
