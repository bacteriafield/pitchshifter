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
-- can look like the hardware instead of a stock GTK form. Each part of the
-- panel has its own folder under view/; this file is the state and the wiring.
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
package.path = here .. "/?.lua;" .. here .. "/?/init.lua;" .. package.path

-- One line, not a traceback: PitchShifter falls back to its prompt after this.
local iupOk, iupErr = pcall(require, "iuplua")
if not iupOk then
    io.stderr:write("pitchshifter: the front panel can't start: ", tostring(iupErr):match("[^\n]*"), "\n")
    os.exit(1)
end
iup.SetGlobal("UTF8MODE", "YES")

local loaded, pcore = pcall(require, "pcore")
if not loaded then
    iup.Message("PitchShifter",
        "The audio engine module (pcore) did not load:\n\n" .. tostring(pcore) ..
        "\n\nBuild it with:\n    cmake --build build --target pcore")
    return
end

local draw = require "view.draw"
local header = require "view.header"
local matrix = require "view.matrix"
local controls = require "view.controls"
local switches = require "view.switches"
local pedal = require "view.pedal"
local cv, W, H = draw.cv, draw.W, draw.H

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

-- What the views read to draw themselves.
local panel = {
    state = state, rows = ROWS,
    rowKnobs = rowKnobs, fmt = fmt, knobT = knobT, whammyActive = whammyActive,
}

function cv:action()
    iup.DrawBegin(self)
    local w, h = iup.DrawGetSize(self)
    if w > 0 and h > 0 then
        draw.fit(w, h)
        self.drawcolor, self.drawstyle = draw.C.bg, "FILL"
        iup.DrawRectangle(self, 0, 0, w, h)

        draw.rect(20, 20, 980, 700, draw.C.bodyDark)
        draw.rect(26, 24, 974, 694, draw.C.body)
        draw.rect(26, 24, 974, 30, draw.C.bodyLight)

        header.draw(panel)
        matrix.draw(panel)
        controls.draw(panel)
        switches.draw(panel)
        pedal.draw(panel)
    end
    iup.DrawEnd(self)
end

------------------------------------------------------------------------------
-- Mouse
------------------------------------------------------------------------------

local function hit(vx, vy)
    for _, view in ipairs{ matrix, controls, switches, pedal } do
        local what, i = view.hit(panel, vx, vy)
        if what then return what, i end
    end
end

function cv:button_cb(button, pressed, x, y, status)
    if button ~= iup.BUTTON1 then return iup.DEFAULT end
    local vx, vy = draw.virtual(x, y)

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
            setPedal(pedal.valueAt(vy))
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
        local _, vy = draw.virtual(x, y)
        setPedal(pedal.valueAt(vy))
    else
        local travel = (iup.isshift(status) and 1000 or 200) * draw.S.k   -- pixels for the full sweep
        turnKnob(d.knob, clamp(d.t + (d.y - y) / travel, 0, 1))
    end
    iup.Update(self)
    return iup.DEFAULT
end

function cv:wheel_cb(delta, x, y, status)
    local what, key = hit(draw.virtual(x, y))
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

-- The window and taskbar icon, from pkg/make-icons.sh. The panel works without it.
local function windowIcon()
    local f = io.open(here .. "/view/icon.rgba", "rb")
    if not f then return nil end
    local rgba = f:read("a")
    f:close()
    return iup.imagergba{ width = 64, height = 64, pixels = { rgba:byte(1, -1) } }
end

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
dlg = iup.dialog{ cv; title = title, icon = windowIcon(), minsize = "500x360",
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
