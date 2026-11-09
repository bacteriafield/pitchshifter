--[[
    MIT License

    Copyright (c) 2025 Evandro

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

--]]

--
-- The expression pedal.
--

local draw = require "view.draw"
local C = draw.C
local rect, line, text, inside, SANS_B = draw.rect, draw.line, draw.text, draw.inside, draw.SANS_B

local PEDAL = { 722, 60, 952, 670 }

local pedal = {}

function pedal.draw(panel)
    rect(700, 30, 968, 690, C.well)
    rect(PEDAL[1], PEDAL[2], PEDAL[3], PEDAL[4], C.pedal)
    for y = PEDAL[2] + 14, PEDAL[4] - 14, 10 do
        line(PEDAL[1] + 10, y, PEDAL[3] - 10, y, C.pedalLine, 1)
    end
    rect(PEDAL[1] - 10, 344, PEDAL[3] + 10, 390, C.body)
    text("PITCHSHIFTER", (PEDAL[1] + PEDAL[3]) / 2, 367, C.white, SANS_B, 13, "center")

    local py = PEDAL[4] - panel.state.pedal * (PEDAL[4] - PEDAL[2])
    rect(PEDAL[1] + 6, py - 3, PEDAL[3] - 6, py + 3, panel.whammyActive() and C.ledOn or C.pedalMark)
end

function pedal.hit(_, vx, vy)
    if inside(PEDAL, vx, vy, 0) then return "pedal" end
end

-- Pedal position (0 heel .. 1 toe) for a board y; not clamped.
function pedal.valueAt(vy)
    return 1 - (vy - PEDAL[2]) / (PEDAL[4] - PEDAL[2])
end

return pedal
