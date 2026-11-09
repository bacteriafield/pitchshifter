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
-- What every view shares: the canvas, the colours, and drawing calls that take
-- coordinates on a 1000 x 720 board and scale them to the window.
--

-- The LCD overlays lit digits on ghost digits, so it needs a true monospace:
-- each system's own, which is always installed there.
local FONTS = { Win32 = { "Segoe UI", "Consolas" }, Darwin = { "Helvetica Neue", "Menlo" } }
local sans, mono = table.unpack(FONTS[iup.GetGlobal("DRIVER")] or FONTS[iup.GetGlobal("SYSTEM")]
                                or { "DejaVu Sans", "DejaVu Sans Mono" })

local draw = {
    W = 1000, H = 720,
    SANS_B = sans .. ", Bold",
    MONO_B = mono .. ", Bold",
    S = { k = 1, ox = 0, oy = 0 },
    cv = iup.canvas{ border = "NO" },
    C = {
        bg = "26 22 34", body = "116 66 164", bodyDark = "78 40 118", bodyLight = "142 94 190", well = "96 52 140",
        face = "26 24 30", rowSel = "70 46 104",
        lcdBg = "12 26 14", lcdGhost = "20 42 22", lcdOn = "110 240 120",
        numBg = "34 10 10", numGhost = "54 16 15", numOn = "250 70 55",
        white = "238 238 242", grey = "150 150 158", strip = "176 176 184", stripText = "30 30 36",
        cell = "210 210 216", cellSel = "250 236 150", cellEmpty = "64 60 72", cellText = "24 24 28",
        ledOn = "255 50 40", ledOff = "70 22 22",
        shadow = "60 30 90", knobRim = "18 18 20", knob = "46 46 52", pointer = "245 245 245", pointerDim = "110 110 118",
        button = "36 36 40", buttonHi = "90 90 98",
        switch = "30 30 33", switchTop = "54 54 60", switchPressed = "84 84 92",
        pedal = "30 30 32", pedalLine = "46 46 50", pedalMark = "170 130 230",
    },
}

local cv, S = draw.cv, draw.S

local function X(x) return math.floor(S.ox + x * S.k + 0.5) end
local function Y(y) return math.floor(S.oy + y * S.k + 0.5) end
draw.X, draw.Y = X, Y

-- Scale the board to fit a w x h canvas, centred.
function draw.fit(w, h)
    S.k = math.min(w / draw.W, h / draw.H)
    S.ox, S.oy = (w - draw.W * S.k) / 2, (h - draw.H * S.k) / 2
end

-- Canvas pixels back to board coordinates.
function draw.virtual(x, y) return (x - S.ox) / S.k, (y - S.oy) / S.k end

function draw.inside(r, x, y, pad)
    return x >= r[1] - pad and x <= r[3] + pad and y >= r[2] - pad and y <= r[4] + pad
end

function draw.rect(x1, y1, x2, y2, rgb, stroke)
    cv.drawcolor = rgb
    cv.drawstyle = stroke and "STROKE" or "FILL"
    cv.drawlinewidth = 1
    iup.DrawRectangle(cv, X(x1), Y(y1), X(x2), Y(y2))
end

function draw.circle(cx, cy, r, rgb)
    cv.drawcolor = rgb
    cv.drawstyle = "FILL"
    iup.DrawArc(cv, X(cx - r), Y(cy - r), X(cx + r), Y(cy + r), 0, 360)
end

-- A trapezoid with horizontal top and bottom edges, filled one pixel row at a
-- time. ponytail: iup.DrawPolygon smeared its fill across the panel on this
-- IUP 3.32 / GTK build however the points were passed, and the footswitches
-- are the only shape that needs this.
function draw.trapezoid(y1, top1, top2, y2, bot1, bot2, rgb)
    cv.drawcolor = rgb
    cv.drawstyle = "STROKE"
    cv.drawlinewidth = 1
    local sy1, sy2 = Y(y1), Y(y2)
    for sy = sy1, sy2 do
        local f = (sy2 > sy1) and (sy - sy1) / (sy2 - sy1) or 0
        iup.DrawLine(cv, X(top1 + (bot1 - top1) * f), sy, X(top2 + (bot2 - top2) * f), sy)
    end
end

function draw.line(x1, y1, x2, y2, rgb, width)
    cv.drawcolor = rgb
    cv.drawstyle = "STROKE"
    cv.drawlinewidth = math.max(1, math.floor(width * S.k + 0.5))
    iup.DrawLine(cv, X(x1), Y(y1), X(x2), Y(y2))
end

-- (x, y) is the anchor point; y is the vertical centre of the text.
function draw.text(s, x, y, rgb, font, size, align)
    cv.drawfont = font .. " " .. math.max(5, math.floor(size * S.k + 0.5))
    cv.drawcolor = rgb
    local tw, th = iup.DrawGetTextSize(cv, s)
    local px = X(x)
    if align == "center" then px = px - tw // 2 elseif align == "right" then px = px - tw end
    iup.DrawText(cv, s, px, Y(y) - th // 2)
end

return draw
