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
-- The three footswitches: previous preset, next preset, BYPASS.
--

local draw = require "view.draw"
local C = draw.C
local rect, line, text, trapezoid, SANS_B = draw.rect, draw.line, draw.text, draw.trapezoid, draw.SANS_B

local SWITCH_X = { 145, 345, 545 }
local SWITCH_Y1, SWITCH_Y2 = 590, 680

local switches = {}

function switches.draw(panel)
    local state = panel.state
    local cx1, cx2, cx3 = SWITCH_X[1], SWITCH_X[2], SWITCH_X[3]
    line(cx1 - 22, 566, cx1 + 22, 566, C.white, 2)
    line(cx1 + 22, 566, cx1, 580, C.white, 2)
    line(cx1, 580, cx1 - 22, 566, C.white, 2)
    line(cx2 - 22, 580, cx2 + 22, 580, C.white, 2)
    line(cx2 + 22, 580, cx2, 566, C.white, 2)
    line(cx2, 566, cx2 - 22, 580, C.white, 2)
    text("BYPASS", cx3, 573, C.white, SANS_B, 12, "center")

    for j, cx in ipairs(SWITCH_X) do
        local top = state.pressed == ("switch" .. j) and C.switchPressed or C.switchTop
        trapezoid(SWITCH_Y1, cx - 72, cx + 72, SWITCH_Y1 + 55, cx - 85, cx + 85, top)
        rect(cx - 85, SWITCH_Y1 + 55, cx + 85, SWITCH_Y2, C.switch)
        text("PITCHSHIFTER", cx, SWITCH_Y1 + 30, C.grey, SANS_B, 9, "center")
    end
end

function switches.hit(_, vx, vy)
    for j, x in ipairs(SWITCH_X) do
        if vx >= x - 85 and vx <= x + 85 and vy >= SWITCH_Y1 and vy <= SWITCH_Y2 then return "switch", j end
    end
end

return switches
