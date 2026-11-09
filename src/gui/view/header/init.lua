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
-- The top strip: model name, the six-character LCD and the preset number.
--

local draw = require "view.draw"
local C, cv, X, Y, S = draw.C, draw.cv, draw.X, draw.Y, draw.S
local rect, text, SANS_B, MONO_B = draw.rect, draw.text, draw.SANS_B, draw.MONO_B

local header = {}

-- An LCD: the dim "8" ghosts and the lit characters start at the same x.
-- Centring each string on its own measured width put them half a character apart.
local function lcd(ghost, s, cx, cy, ghostRgb, onRgb)
    cv.drawfont = MONO_B .. " " .. math.max(5, math.floor(34 * S.k + 0.5))
    local gw, gh = iup.DrawGetTextSize(cv, ghost)
    local x, y = X(cx) - gw // 2, Y(cy) - gh // 2
    cv.drawcolor = ghostRgb
    iup.DrawText(cv, ghost, x, y)
    cv.drawcolor = onRgb
    iup.DrawText(cv, s, x, y)
end

local function lcdText(state)
    local s
    if state.msg then
        s = state.msg
    elseif state.bypassed then
        s = "BYPASS"
    else
        local p = state.presets[state.cur]
        s = p and p.name or "------"
    end
    s = s:upper()
    if #s > 6 then
        local loop = s .. "   "
        local off = state.scroll % #loop
        s = (loop .. loop):sub(off + 1, off + 6)
    end
    return s .. string.rep(" ", 6 - #s)
end

function header.draw(panel)
    local state = panel.state
    rect(50, 60, 680, 160, C.face)
    text("VOCAL", 108, 94, C.white, SANS_B, 19, "center")
    text("300", 108, 126, C.white, SANS_B, 19, "center")

    rect(170, 72, 450, 148, C.lcdBg)
    lcd("888888", lcdText(state), 310, 110, C.lcdGhost, C.lcdOn)

    rect(458, 72, 552, 148, C.numBg)
    lcd("88", string.format("%02d", state.cur % 100), 505, 110, C.numGhost, C.numOn)

    text("VOCAL", 568, 90, C.white, SANS_B, 11, "left")
    text("EFFECTS", 568, 110, C.white, SANS_B, 11, "left")
    text("PROCESSOR", 568, 130, C.white, SANS_B, 11, "left")
end

return header
