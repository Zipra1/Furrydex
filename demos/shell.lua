local shell = require("shell")
local table = require("table")
local paint = require("paint")

local meow
local msgq = {}

while true do
    paint.clear()
    meow = shell.receive(5)
    if meow ~= nil then
        table.insert(msgq, 1, meow)
        if #msgq > 15 then
            table.remove(msgq, 15)
        end
    end
    paint.text(10, 20, 1, table.concat(msgq, "\n"))
    paint.display()
end
