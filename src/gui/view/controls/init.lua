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
-- The control strip: SELECT, STORE, the four knobs and MASTER.
--

local draw = require "view.draw"
local C = draw.C
local rect, circle, line, text, inside, SANS_B = draw.rect, draw.circle, draw.line, draw.text, draw.inside, draw.SANS_B

local KNOB_X, MASTER_X = { 180, 280, 380, 480 }, 580
local KNOB_Y, KNOB_R = 520, 30
local SELECT_BTN = { 58, 442, 112, 460 }
local STORE_BTN = { 618, 442, 672, 460 }

local controls = {}

local function drawButton(r, pressed)
    rect(r[1], r[2], r[3], r[4], pressed and C.buttonHi or C.button)
    line(r[1], r[2], r[3], r[2], C.buttonHi, 1)
end

local function drawKnob(cx, cy, t)
    circle(cx, cy + 3, KNOB_R + 4, C.shadow)
    for s = 0, 10 do
        local a = math.rad(-135 + 27 * s)
        line(cx + math.sin(a) * (KNOB_R + 8), cy - math.cos(a) * (KNOB_R + 8),
             cx + math.sin(a) * (KNOB_R + 13), cy - math.cos(a) * (KNOB_R + 13), C.shadow, 2)
    end
    circle(cx, cy, KNOB_R + 2, C.knobRim)
    circle(cx, cy, KNOB_R - 3, C.knob)
    -- A knob whose value the preset doesn't define points at noon, dimmed.
    local a = math.rad(-135 + 270 * (t or 0.5))
    line(cx + math.sin(a) * 6, cy - math.cos(a) * 6,
         cx + math.sin(a) * (KNOB_R - 7), cy - math.cos(a) * (KNOB_R - 7),
         t and C.pointer or C.pointerDim, 4)
end

function controls.draw(panel)
    local state = panel.state
    rect(50, 408, 680, 432, C.strip)
    local ks = panel.rowKnobs(state.row)
    text("SELECT", 85, 420, C.stripText, SANS_B, 9, "center")
    for j, x in ipairs(KNOB_X) do
        if ks[j] then text(ks[j][1], x, 420, C.stripText, SANS_B, 9, "center") end
    end
    text("MASTER", MASTER_X, 420, C.stripText, SANS_B, 9, "center")
    text("STORE", 645, 420, C.stripText, SANS_B, 9, "center")

    drawButton(SELECT_BTN, state.pressed == "select")
    drawButton(STORE_BTN, state.pressed == "store")

    for j, x in ipairs(KNOB_X) do
        if ks[j] then drawKnob(x, KNOB_Y, panel.knobT(j)) end
    end
    drawKnob(MASTER_X, KNOB_Y, panel.knobT("master"))
end

function controls.hit(_, vx, vy)
    if inside(SELECT_BTN, vx, vy, 6) then return "select" end
    if inside(STORE_BTN, vx, vy, 6) then return "store" end
    for j, x in ipairs(KNOB_X) do
        if (vx - x) ^ 2 + (vy - KNOB_Y) ^ 2 <= (KNOB_R + 10) ^ 2 then return "knob", j end
    end
    if (vx - MASTER_X) ^ 2 + (vy - KNOB_Y) ^ 2 <= (KNOB_R + 10) ^ 2 then return "knob", "master" end
end

return controls
