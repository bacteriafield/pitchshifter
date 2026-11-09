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
-- The matrix: one row per block of the signal path, with its on/off LED and
-- the names of the four knobs it maps.
--

local draw = require "view.draw"
local C, cv, X, Y = draw.C, draw.cv, draw.X, draw.Y
local rect, circle, text, SANS_B = draw.rect, draw.circle, draw.text, draw.SANS_B

local ROW_Y0, ROW_H = 180, 30
local LED_X = 165
local CELL_X0, CELL_W = 180, 100

local matrix = {}

function matrix.draw(panel)
    local state = panel.state
    rect(50, 170, 680, 398, C.face)
    for i, r in ipairs(panel.rows) do
        local y1 = ROW_Y0 + (i - 1) * ROW_H
        local yc = y1 + ROW_H / 2
        local selected = i == state.row
        if selected then rect(54, y1 + 2, 676, y1 + ROW_H - 2, C.rowSel) end

        text(r.name, 62, yc, C.white, SANS_B, 10, "left")
        circle(LED_X, yc, 6, (state.vals[r.on] or 0) >= 0.5 and C.ledOn or C.ledOff)

        local ks = panel.rowKnobs(i)
        for j = 1, 4 do
            local k = ks[j]
            local x1, x2 = CELL_X0 + (j - 1) * CELL_W + 4, CELL_X0 + j * CELL_W - 4
            rect(x1, y1 + 7, x2, y1 + ROW_H - 7, k and (selected and C.cellSel or C.cell) or C.cellEmpty)
            if k then
                local label = selected and (k[1] .. " " .. panel.fmt(k, state.vals[k[2]])) or k[1]
                iup.DrawSetClipRect(cv, X(x1), Y(y1 + 7), X(x2), Y(y1 + ROW_H - 7))
                text(label, (x1 + x2) / 2, yc, C.cellText, SANS_B, 8, "center")
                iup.DrawResetClip(cv)
            end
        end
    end
end

function matrix.hit(panel, vx, vy)
    for i = 1, #panel.rows do
        local y1 = ROW_Y0 + (i - 1) * ROW_H
        if vy >= y1 and vy < y1 + ROW_H and vx >= 54 and vx <= 676 then
            if math.abs(vx - LED_X) <= 12 then return "led", i end
            return "row", i
        end
    end
end

return matrix
