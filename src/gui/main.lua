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
-- PitchShifter front panel, laid out after the DigiTech Vocal 300
-- (docs/static/digitech-vocal300.png). Drawn with IupDraw on one canvas so it
-- can look like the hardware instead of a stock GTK form.
--
--   cmake --build build --target pcore
--   lua src/gui/main.lua
--
-- Controls, as on the unit:
--   click a matrix row, or SELECT   choose the block the four knobs edit
--   click a row's LED               switch that block on or off
--   drag a knob up/down             turn it (hold shift for fine, wheel works too)
--   the two left footswitches       previous / next preset
--   BYPASS                          everything off until pressed again
--   STORE                           save the current sound as a user preset
--   expression pedal (drag)         volume, or pitch when the effect is Whammy
--

local here = (arg and arg[0] or ""):match("^(.*)[/\\]") or "."
package.cpath = here .. "/../../build/lib/?.so;" .. package.cpath

require "iuplua"
iup.SetGlobal("UTF8MODE", "YES")

local loaded, pcore = pcall(require, "pcore")
if not loaded then
    iup.Message("PitchShifter",
        "The audio engine module (pcore) did not load:\n\n" .. tostring(pcore) ..
        "\n\nBuild it with:\n    cmake --build build --target pcore")
    return
end

local audioOk, audioErr, presetErr = pcore.open{ rate = 48000, block = 512, inputs = 1, outputs = 2 }

local VOICES, EFFECTS, DELAYS = pcore.voices(), pcore.effects(), pcore.delays()
local WHAMMY = -1
for i, name in ipairs(EFFECTS) do
    if name == "Whammy" then WHAMMY = i - 1 end
end

------------------------------------------------------------------------------
-- The matrix: one row per block of the Vocal 300 signal path, up to four
-- knobs each.  { label, parameter, min, max, format [, step=n] [, curve="log"] }
-- Format is a string.format pattern, "pct", or the name of a type list.
------------------------------------------------------------------------------

local FX_TYPE = { "TYPE", "fx.type", 0, #EFFECTS - 1, "fx", step = 1 }

-- Knobs 2-4 of the EFFECTS row depend on which effect is selected.
-- Indexed by effect type, in VocalChain::FxType order.
local FX_KNOBS = {
    [0]  = { { "RATE", "fx.lfo_rate_hz", 0.05, 5, "%.2fHz" }, { "DEPTH", "fx.depth_ms", 1, 25, "%.0fms" }, { "MIX", "fx.mix", 0, 1, "pct" } },        -- Chorus
    [1]  = { { "RATE", "fx.rate", 0, 10, "%.2fHz" }, { "DEPTH", "fx.depth", 0, 1, "pct" }, { "FDBK", "fx.feedback", 0, 1, "pct" } },                  -- Flanger
    [2]  = { { "RATE", "fx.rate", 0.5, 10, "%.2fHz" }, { "DEPTH", "fx.octaves", 0, 2, "%.2foct" }, { "FDBK", "fx.feedback", 0, 0.75, "pct" } },       -- Phaser
    [3]  = { { "RATE", "fx.rate", 0.5, 15, "%.1fHz" }, { "DEPTH", "fx.depth", 0, 1, "pct" } },                                                        -- Tremolo
    [4]  = { { "RATE", "fx.lfo_rate_hz", 0.05, 5, "%.2fHz" }, { "DEPTH", "fx.depth_ms", 1, 25, "%.0fms" } },                                          -- Vibrato
    [5]  = { { "RATE", "fx.rate", 0.5, 15, "%.1fHz" }, { "DEPTH", "fx.depth", 0, 1, "pct" } },                                                        -- Strobe
    [6]  = { { "DELAY", "fx.base_delay_ms", 1, 50, "%.0fms" }, { "DEPTH", "fx.depth_ms", 1, 25, "%.0fms" }, { "MIX", "fx.mix", 0, 1, "pct" } },       -- Doubler
    [7]  = { { "SENS", "fx.sensitivity", 0.5, 40, "%.1f" }, { "Q", "fx.q", 0.5, 8, "%.1f" }, { "MIX", "fx.mix", 0, 1, "pct" } },                      -- Envelope
    [8]  = { { "BITS", "fx.bits", 1, 16, "%.0f", step = 1 }, { "RATE", "fx.downsample", 1, 64, "1/%.0f", step = 1 }, { "MIX", "fx.mix", 0, 1, "pct" } }, -- Pixelator
    [9]  = { { "SHIFT", "fx.semitones", -1, 1, "%+.2fst" }, { "MIX", "fx.mix", 0, 1, "pct" } },                                                       -- Detune
    [10] = { { "SHIFT", "fx.semitones", -24, 24, "%+.0fst", step = 1 }, { "MIX", "fx.mix", 0, 1, "pct" } },                                           -- Pitch Shifter
    [11] = { { "PEDAL", "fx.pedal", 0, 1, "pct" } },                                                                                                  -- Whammy
}

local ROWS = {
    { name = "COMP", on = "comp.on", knobs = {
        { "GAIN", "in.level", -24, 24, "%+.0fdB" },
        { "THRESH", "comp.threshold", -40, 0, "%.0fdB" },
        { "RATIO", "comp.ratio", 1, 20, "%.1f:1" },
        { "MAKEUP", "comp.makeup", 0, 24, "%+.0fdB" } } },
    { name = "VOICE", on = "voice.on", knobs = {
        { "TYPE", "voice.type", 0, #VOICES - 1, "voice", step = 1 },
        { "DRIVE", "voice.drive", 0, 40, "%.0fdB" },
        { "TONE", "voice.tone", 1000, 16000, "%.0fHz", curve = "log" },
        { "MIX", "voice.mix", 0, 1, "pct" } } },
    { name = "EQ", on = "eq.on", knobs = {
        { "LOW", "eq.band0", -20, 20, "%+.0fdB" },
        { "MID", "eq.band5", -20, 20, "%+.0fdB" },
        { "PRES", "eq.band7", -20, 20, "%+.0fdB" },
        { "HIGH", "eq.band9", -20, 20, "%+.0fdB" } } },
    { name = "GATE", on = "gate.on", knobs = {
        { "THRESH", "gate.threshold", -90, 0, "%.0fdB" },
        { "ATTACK", "gate.attack", 0.1, 50, "%.1fms" },
        { "RELEASE", "gate.release", 5, 1000, "%.0fms", curve = "log" } } },
    { name = "EFFECTS", on = "fx.on", knobs = "fx" },
    { name = "DELAY", on = "delay.on", knobs = {
        { "TYPE", "delay.type", 0, #DELAYS - 1, "delay", step = 1 },
        { "TIME", "delay.time_ms", 0, 1000, "%.0fms" },
        { "FDBK", "delay.feedback", 0, 1, "pct" },
        { "MIX", "delay.mix", 0, 1, "pct" } } },
    { name = "REVERB", on = "reverb.on", knobs = {
        { "TIME", "reverb.time_s", 0.05, 1.5, "%.2fs" },
        { "DECAY", "reverb.decay", 0, 0.99, "pct" },
        { "MIX", "reverb.mix", 0, 1, "pct" } } },
}

local MASTER = { "MASTER", "out.level", -24, 12, "%+.0fdB" }

------------------------------------------------------------------------------
-- State and parameter plumbing
------------------------------------------------------------------------------

local state = {
    presets = {}, cur = 1,
    vals = {},
    row = 1,
    bypassed = false, bypassSaved = nil,
    master = 0,       -- MASTER knob, dB
    pedal = 1,        -- expression pedal, 0 heel .. 1 toe
    msg = nil, msgTicks = 0,
    scroll = 0, tick = 0,
    relayouts = 0,
    drag = nil,       -- { knob = key, y =, t = }  or  { pedal = true }
    pressed = nil,    -- "select", "store", "switch1".."switch3" while held
}

local function clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end

local function rowKnobs(i)
    local r = ROWS[i]
    if r.knobs ~= "fx" then return r.knobs end
    local list = { FX_TYPE }
    for _, k in ipairs(FX_KNOBS[math.floor((state.vals["fx.type"] or 0) + 0.5)] or {}) do
        list[#list + 1] = k
    end
    return list
end

local function fmt(k, v)
    if v == nil then return "--" end
    local f = k[5]
    local i = math.floor(v + 0.5) + 1
    if f == "voice" then return VOICES[i] or "?" end
    if f == "fx" then return EFFECTS[i] or "?" end
    if f == "delay" then return DELAYS[i] or "?" end
    if f == "pct" then return string.format("%.0f%%", v * 100) end
    return string.format(f, v)
end

local function toValue(k, t)
    local lo, hi = k[3], k[4]
    local v = (k.curve == "log") and lo * (hi / lo) ^ t or lo + t * (hi - lo)
    if k.step then v = math.floor(v / k.step + 0.5) * k.step end
    return v
end

local function toT(k, v)
    if v == nil then return nil end
    local lo, hi = k[3], k[4]
    local t
    if k.curve == "log" then
        t = (v > 0) and math.log(v / lo) / math.log(hi / lo) or 0
    else
        t = (hi > lo) and (v - lo) / (hi - lo) or 0
    end
    return clamp(t, 0, 1)
end

local function say(text, ticks)
    state.msg, state.msgTicks = text, ticks or 14
end

local function setParam(path, v)
    if pcore.set(path, v) then state.vals[path] = v end
end

local function whammyActive()
    return (state.vals["fx.on"] or 0) >= 0.5 and math.floor((state.vals["fx.type"] or 0) + 0.5) == WHAMMY
end

-- The pedal is a volume pedal in front of MASTER, unless the effect is Whammy.
local function applyOutput()
    local db = state.master
    if not whammyActive() then db = db + (state.pedal - 1) * 30 end
    setParam("out.level", clamp(db, -24, 24))
end

local function loadPreset(i)
    local n = #state.presets
    if n == 0 then return end
    state.cur = (i - 1) % n + 1
    state.bypassed, state.bypassSaved = false, nil
    pcore.load(state.cur)
    state.vals = pcore.values()
    state.master = state.vals["out.level"] or 0
    state.pedal = 1
    state.msg, state.scroll = nil, 0
end

local function toggleBypass()
    if not state.bypassed then
        state.bypassSaved = {}
        for _, r in ipairs(ROWS) do
            state.bypassSaved[r.on] = state.vals[r.on] or 0
            setParam(r.on, 0)
        end
        state.bypassed = true
    else
        for path, v in pairs(state.bypassSaved) do setParam(path, v) end
        state.bypassed, state.bypassSaved = false, nil
    end
    state.msg = nil
end

local function toggleBlock(i)
    local path = ROWS[i].on
    if state.bypassed then state.bypassed, state.bypassSaved = false, nil end
    local on = (state.vals[path] or 0) >= 0.5
    setParam(path, on and 0 or 1)
    if path == "fx.on" then applyOutput() end
    say(ROWS[i].name:sub(1, 2) .. (on and " OFF" or " ON"))
end

local function knobSpec(key)
    if key == "master" then return MASTER end
    return rowKnobs(state.row)[key]
end

local function knobT(key)
    if key == "master" then return toT(MASTER, state.master) end
    local k = knobSpec(key)
    return k and toT(k, state.vals[k[2]])
end

local function turnKnob(key, t)
    if key == "master" then
        state.master = toValue(MASTER, t)
        applyOutput()
        say(fmt(MASTER, state.master))
        return
    end
    local k = knobSpec(key)
    if not k then return end
    local v = toValue(k, t)
    if state.vals[k[2]] ~= v then
        setParam(k[2], v)
        if k[2] == "fx.type" or k[2] == "voice.type" then
            state.vals = pcore.values()   -- a new type loads its own settings
            applyOutput()
        end
    end
    say(fmt(k, v))
end

local function setPedal(p)
    state.pedal = clamp(p, 0, 1)
    if whammyActive() then
        setParam("fx.pedal", state.pedal)
        say(string.format("WHAM%02d", math.floor(state.pedal * 99 + 0.5)))
    else
        applyOutput()
        say(string.format("VOL%3d", math.floor(state.pedal * 100 + 0.5)))
    end
end

local function store()
    local cur = state.presets[state.cur]
    local name = iup.GetText("Store preset", cur and cur.name or "My Preset")
    if not name or name == "" then return end

    -- Store the sound, not the moment: no pedal volume and no bypass in the preset.
    pcore.set("out.level", state.master)
    if state.bypassed then
        for path, v in pairs(state.bypassSaved) do pcore.set(path, v) end
    end
    local ok, err, index = pcore.save(name)
    if state.bypassed then
        for path in pairs(state.bypassSaved) do pcore.set(path, 0) end
    end
    applyOutput()

    if not ok then
        iup.Message("Store preset", tostring(err))
        return
    end
    state.presets = pcore.presets()
    state.cur = index
    say("STORED")
end

------------------------------------------------------------------------------
-- Drawing. Everything is laid out on a 1000 x 720 board and scaled to fit.
------------------------------------------------------------------------------

local W, H = 1000, 720
local ROW_Y0, ROW_H = 180, 30
local LED_X = 165
local CELL_X0, CELL_W = 180, 100
local KNOB_X, MASTER_X = { 180, 280, 380, 480 }, 580
local KNOB_Y, KNOB_R = 520, 30
local SELECT_BTN = { 58, 442, 112, 460 }
local STORE_BTN = { 618, 442, 672, 460 }
local SWITCH_X = { 145, 345, 545 }
local SWITCH_Y1, SWITCH_Y2 = 590, 680
local PEDAL = { 722, 60, 952, 670 }

local SANS, SANS_B, MONO_B = "DejaVu Sans,", "DejaVu Sans, Bold", "DejaVu Sans Mono, Bold"

local C = {
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
}

local cv = iup.canvas{ border = "NO" }
local S = { k = 1, ox = 0, oy = 0 }

local function X(x) return math.floor(S.ox + x * S.k + 0.5) end
local function Y(y) return math.floor(S.oy + y * S.k + 0.5) end

local function rect(x1, y1, x2, y2, rgb, stroke)
    cv.drawcolor = rgb
    cv.drawstyle = stroke and "STROKE" or "FILL"
    cv.drawlinewidth = 1
    iup.DrawRectangle(cv, X(x1), Y(y1), X(x2), Y(y2))
end

local function circle(cx, cy, r, rgb)
    cv.drawcolor = rgb
    cv.drawstyle = "FILL"
    iup.DrawArc(cv, X(cx - r), Y(cy - r), X(cx + r), Y(cy + r), 0, 360)
end

-- A trapezoid with horizontal top and bottom edges, filled one pixel row at a
-- time. ponytail: iup.DrawPolygon smeared its fill across the panel on this
-- IUP 3.32 / GTK build however the points were passed, and the footswitches
-- are the only shape that needs this.
local function trapezoid(y1, top1, top2, y2, bot1, bot2, rgb)
    cv.drawcolor = rgb
    cv.drawstyle = "STROKE"
    cv.drawlinewidth = 1
    local sy1, sy2 = Y(y1), Y(y2)
    for sy = sy1, sy2 do
        local f = (sy2 > sy1) and (sy - sy1) / (sy2 - sy1) or 0
        iup.DrawLine(cv, X(top1 + (bot1 - top1) * f), sy, X(top2 + (bot2 - top2) * f), sy)
    end
end

local function line(x1, y1, x2, y2, rgb, width)
    cv.drawcolor = rgb
    cv.drawstyle = "STROKE"
    cv.drawlinewidth = math.max(1, math.floor(width * S.k + 0.5))
    iup.DrawLine(cv, X(x1), Y(y1), X(x2), Y(y2))
end

-- (x, y) is the anchor point; y is the vertical centre of the text.
local function text(s, x, y, rgb, font, size, align)
    cv.drawfont = font .. " " .. math.max(5, math.floor(size * S.k + 0.5))
    cv.drawcolor = rgb
    local tw, th = iup.DrawGetTextSize(cv, s)
    local px = X(x)
    if align == "center" then px = px - tw // 2 elseif align == "right" then px = px - tw end
    iup.DrawText(cv, s, px, Y(y) - th // 2)
end

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

local function lcdText()
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

local function drawHeader()
    rect(50, 60, 680, 160, C.face)
    text("VOCAL", 108, 94, C.white, SANS_B, 19, "center")
    text("300", 108, 126, C.white, SANS_B, 19, "center")

    rect(170, 72, 450, 148, C.lcdBg)
    lcd("888888", lcdText(), 310, 110, C.lcdGhost, C.lcdOn)

    rect(458, 72, 552, 148, C.numBg)
    lcd("88", string.format("%02d", state.cur % 100), 505, 110, C.numGhost, C.numOn)

    text("VOCAL", 568, 90, C.white, SANS_B, 11, "left")
    text("EFFECTS", 568, 110, C.white, SANS_B, 11, "left")
    text("PROCESSOR", 568, 130, C.white, SANS_B, 11, "left")
end

local function drawMatrix()
    rect(50, 170, 680, 398, C.face)
    for i, r in ipairs(ROWS) do
        local y1 = ROW_Y0 + (i - 1) * ROW_H
        local yc = y1 + ROW_H / 2
        local selected = i == state.row
        if selected then rect(54, y1 + 2, 676, y1 + ROW_H - 2, C.rowSel) end

        text(r.name, 62, yc, C.white, SANS_B, 10, "left")
        circle(LED_X, yc, 6, (state.vals[r.on] or 0) >= 0.5 and C.ledOn or C.ledOff)

        local ks = rowKnobs(i)
        for j = 1, 4 do
            local k = ks[j]
            local x1, x2 = CELL_X0 + (j - 1) * CELL_W + 4, CELL_X0 + j * CELL_W - 4
            rect(x1, y1 + 7, x2, y1 + ROW_H - 7, k and (selected and C.cellSel or C.cell) or C.cellEmpty)
            if k then
                local label = selected and (k[1] .. " " .. fmt(k, state.vals[k[2]])) or k[1]
                iup.DrawSetClipRect(cv, X(x1), Y(y1 + 7), X(x2), Y(y1 + ROW_H - 7))
                text(label, (x1 + x2) / 2, yc, C.cellText, SANS_B, 8, "center")
                iup.DrawResetClip(cv)
            end
        end
    end
end

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

local function drawControls()
    rect(50, 408, 680, 432, C.strip)
    local ks = rowKnobs(state.row)
    text("SELECT", 85, 420, C.stripText, SANS_B, 9, "center")
    for j, x in ipairs(KNOB_X) do
        if ks[j] then text(ks[j][1], x, 420, C.stripText, SANS_B, 9, "center") end
    end
    text("MASTER", MASTER_X, 420, C.stripText, SANS_B, 9, "center")
    text("STORE", 645, 420, C.stripText, SANS_B, 9, "center")

    drawButton(SELECT_BTN, state.pressed == "select")
    drawButton(STORE_BTN, state.pressed == "store")

    for j, x in ipairs(KNOB_X) do
        if ks[j] then drawKnob(x, KNOB_Y, knobT(j)) end
    end
    drawKnob(MASTER_X, KNOB_Y, knobT("master"))
end

local function drawSwitches()
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

local function drawPedal()
    rect(700, 30, 968, 690, C.well)
    rect(PEDAL[1], PEDAL[2], PEDAL[3], PEDAL[4], C.pedal)
    for y = PEDAL[2] + 14, PEDAL[4] - 14, 10 do
        line(PEDAL[1] + 10, y, PEDAL[3] - 10, y, C.pedalLine, 1)
    end
    rect(PEDAL[1] - 10, 344, PEDAL[3] + 10, 390, C.body)
    text("PITCHSHIFTER", (PEDAL[1] + PEDAL[3]) / 2, 367, C.white, SANS_B, 13, "center")

    local py = PEDAL[4] - state.pedal * (PEDAL[4] - PEDAL[2])
    rect(PEDAL[1] + 6, py - 3, PEDAL[3] - 6, py + 3, whammyActive() and C.ledOn or C.pedalMark)
end

function cv:action()
    iup.DrawBegin(self)
    local w, h = iup.DrawGetSize(self)
    if w > 0 and h > 0 then
        S.k = math.min(w / W, h / H)
        S.ox, S.oy = (w - W * S.k) / 2, (h - H * S.k) / 2
        self.drawcolor, self.drawstyle = C.bg, "FILL"
        iup.DrawRectangle(self, 0, 0, w, h)

        rect(20, 20, 980, 700, C.bodyDark)
        rect(26, 24, 974, 694, C.body)
        rect(26, 24, 974, 30, C.bodyLight)

        drawHeader()
        drawMatrix()
        drawControls()
        drawSwitches()
        drawPedal()
    end
    iup.DrawEnd(self)
end

------------------------------------------------------------------------------
-- Mouse
------------------------------------------------------------------------------

local function inside(r, x, y, pad)
    return x >= r[1] - pad and x <= r[3] + pad and y >= r[2] - pad and y <= r[4] + pad
end

local function hit(vx, vy)
    for i = 1, #ROWS do
        local y1 = ROW_Y0 + (i - 1) * ROW_H
        if vy >= y1 and vy < y1 + ROW_H and vx >= 54 and vx <= 676 then
            if math.abs(vx - LED_X) <= 12 then return "led", i end
            return "row", i
        end
    end
    if inside(SELECT_BTN, vx, vy, 6) then return "select" end
    if inside(STORE_BTN, vx, vy, 6) then return "store" end
    for j, x in ipairs(KNOB_X) do
        if (vx - x) ^ 2 + (vy - KNOB_Y) ^ 2 <= (KNOB_R + 10) ^ 2 then return "knob", j end
    end
    if (vx - MASTER_X) ^ 2 + (vy - KNOB_Y) ^ 2 <= (KNOB_R + 10) ^ 2 then return "knob", "master" end
    for j, x in ipairs(SWITCH_X) do
        if vx >= x - 85 and vx <= x + 85 and vy >= SWITCH_Y1 and vy <= SWITCH_Y2 then return "switch", j end
    end
    if inside(PEDAL, vx, vy, 0) then return "pedal" end
end

local function virtual(x, y) return (x - S.ox) / S.k, (y - S.oy) / S.k end

local function pedalAt(vy)
    return 1 - (vy - PEDAL[2]) / (PEDAL[4] - PEDAL[2])
end

function cv:button_cb(button, pressed, x, y, status)
    if button ~= iup.BUTTON1 then return iup.DEFAULT end
    local vx, vy = virtual(x, y)

    if pressed == 1 then
        local what, i = hit(vx, vy)
        if what == "row" then
            state.row = i
        elseif what == "led" then
            state.row = i
            toggleBlock(i)
        elseif what == "select" then
            state.pressed = "select"
            state.row = state.row % #ROWS + 1
        elseif what == "store" then
            state.pressed = "store"          -- acts on release, it opens a dialog
        elseif what == "knob" and knobSpec(i) then
            state.drag = { knob = i, y = y, t = knobT(i) or 0.5 }
        elseif what == "switch" then
            state.pressed = "switch" .. i
            if i == 1 then loadPreset(state.cur - 1)
            elseif i == 2 then loadPreset(state.cur + 1)
            else toggleBypass() end
        elseif what == "pedal" then
            state.drag = { pedal = true }
            setPedal(pedalAt(vy))
        end
    else
        local wasStore = state.pressed == "store"
        state.drag, state.pressed = nil, nil
        iup.Update(self)
        if wasStore then store() end
    end
    iup.Update(self)
    return iup.DEFAULT
end

function cv:motion_cb(x, y, status)
    local d = state.drag
    if not d or not iup.isbutton1(status) then return iup.DEFAULT end
    if d.pedal then
        local _, vy = virtual(x, y)
        setPedal(pedalAt(vy))
    else
        local travel = (iup.isshift(status) and 1000 or 200) * S.k   -- pixels for the full sweep
        turnKnob(d.knob, clamp(d.t + (d.y - y) / travel, 0, 1))
    end
    iup.Update(self)
    return iup.DEFAULT
end

function cv:wheel_cb(delta, x, y, status)
    local what, key = hit(virtual(x, y))
    local k = what == "knob" and knobSpec(key)
    if not k then return iup.DEFAULT end
    local step = k.step and k.step / (k[4] - k[3]) or 0.02
    turnKnob(key, clamp((knobT(key) or 0.5) + delta * step, 0, 1))
    iup.Update(self)
    return iup.DEFAULT
end

------------------------------------------------------------------------------
-- Go
------------------------------------------------------------------------------

state.presets = pcore.presets()
loadPreset(1)
if not audioOk then say("NO AUD", 40) end

local title = "PitchShifter"
if not audioOk then title = title .. "  (no audio: " .. tostring(audioErr) .. ")" end
if presetErr then title = title .. "  [" .. presetErr .. "]" end

local dlg

-- Drives the LCD: messages time out and long preset names scroll.
local timer = iup.timer{ time = 120 }
function timer:action_cb()
    state.tick = state.tick + 1

    -- ponytail: under WSLg the canvas sometimes starts up smaller than the
    -- window, as if IUP subtracted the frame twice. Re-run the layout when
    -- that happens; five tries covers the start-up race without ever looping.
    if dlg and state.relayouts < 5 then
        local cw, ch = (dlg.clientsize or ""):match("(%d+)x(%d+)")
        local dw, dh = (cv.drawsize or ""):match("(%d+)x(%d+)")
        if cw and dw and (math.abs(cw - dw) > 2 or math.abs(ch - dh) > 2) then
            state.relayouts = state.relayouts + 1
            iup.Refresh(dlg)
        end
    end

    if state.msg then
        state.msgTicks = state.msgTicks - 1
        if state.msgTicks <= 0 then state.msg, state.scroll = nil, 0 end
    end
    if state.tick % 3 == 0 then state.scroll = state.scroll + 1 end
    iup.Update(cv)
    return iup.DEFAULT
end

-- Open at the board's own size, or smaller when the screen can't fit it.
local sw, sh = (iup.GetGlobal("SCREENSIZE") or "1000x720"):match("(%d+)x(%d+)")
local fit = math.min(1, 0.9 * tonumber(sw) / W, 0.85 * tonumber(sh) / H)
dlg = iup.dialog{ cv; title = title, minsize = "500x360",
                        rastersize = string.format("%dx%d", math.floor(W * fit), math.floor(H * fit)) }

function dlg:close_cb()
    timer.run = "NO"
    pcore.close()
    return iup.CLOSE
end

dlg:showxy(iup.CENTER, iup.CENTER)
timer.run = "YES"

if iup.MainLoopLevel() == 0 then
    iup.MainLoop()
    iup.Close()
end
