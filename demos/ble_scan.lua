local ble = require("ble")
local paint = require("paint")
local string = require("string")
local table = require("table")

local function hex_dump(s)
    local parts = {}
    for i = 1, #s do
        parts[#parts+1] = string.format("%02X", s:byte(i))
    end
    return table.concat(parts, " ")
end



while true do
    local receive, rssi, data = ble.scan()
    if receive then
        print("RSSI: " .. rssi)
        print("DATA: " .. hex_dump(data))
        paint.clear()
        paint.text(0,30,1,rssi)
        paint.text(0,38,1,hex_dump(data))
    end
    paint.wait_for_display()
    paint.display()
    paint.wait_for_display()
    paint.wait_for_display()
end