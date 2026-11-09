--
-- Works the whole front panel on a fake IUP, so it needs no display: every
-- control goes through the mouse callbacks the real canvas calls, then a preset
-- with all seven blocks on goes through STORE and comes back.
-- ctest runs it (see src/gui/CMakeLists.txt); by hand:
--
--   lua tests/panel_smoke.lua build/lib src/gui/main.lua --scratch     (from an empty directory)
--
-- It saves presets.txt in the current directory, so it refuses to run where one
-- already exists unless --scratch says that directory is disposable.
--

local moddir, script = arg[1], arg[2]
assert(moddir and script, "usage: lua panel_smoke.lua <directory with pcore> <main.lua> [--scratch]")
package.cpath = moddir .. "/?.so;" .. moddir .. "/?.dll;" .. package.cpath

if arg[3] == "--scratch" then
    os.remove("presets.txt")
elseif io.open("presets.txt") then
    error("presets.txt exists here and this test would overwrite it; run it from an empty directory")
end

local function check(cond, msg)
    if not cond then error("FAIL: " .. msg, 2) end
    print("ok  " .. msg)
end

local size, drawCalls = { 1000, 720 }, 0
local CV, TIMER, DLG
local function drew() drawCalls = drawCalls + 1 end

iup = {
    BUTTON1 = 49, DEFAULT = -2, CLOSE = -3, CENTER = 65535,
    SetGlobal = function() end,
    GetGlobal = function(k) return k == "SCREENSIZE" and "1920x1080" or nil end,
    Message = function(_, text) error("the panel reported: " .. tostring(text)) end,
    GetText = function() return "Mix Test" end,
    isbutton1 = function() return true end,
    isshift = function(s) return s == "shift" end,
    canvas = function(t) CV = t return t end,
    timer = function(t) TIMER = t return t end,
    imagergba = function(t) return t end,
    dialog = function(t) DLG = t t.showxy = function() end return t end,
    Update = function() end, Refresh = function() end,
    MainLoopLevel = function() return 1 end,
    DrawBegin = function() end, DrawEnd = function() end,
    DrawGetSize = function() return size[1], size[2] end,
    DrawGetTextSize = function(_, s) return #s * 8, 12 end,
    DrawRectangle = drew, DrawArc = drew, DrawLine = drew, DrawText = drew,
    DrawSetClipRect = function() end, DrawResetClip = function() end,
}
package.preload["iuplua"] = function() return iup end

arg = { [0] = script }
assert(loadfile(script))()
local pcore = require "pcore"

-- Board coordinates, which are window coordinates at 1000 x 720.
local B = iup.BUTTON1
local BLOCKS = { "comp", "voice", "eq", "gate", "fx", "delay", "reverb" }
local KNOBS = { 180, 280, 380, 480, 580 }
local KNOB_Y, STORE_X, BUTTON_Y, LED_X, CELL_X = 520, 645, 451, 165, 300
local PREV, NEXT, BYPASS, SWITCH_Y, PEDAL_X = 145, 345, 545, 640, 830

local function click(x, y) CV:button_cb(B, 1, x, y, "") CV:button_cb(B, 0, x, y, "") end
local function rowY(r) return 180 + (r - 1) * 30 + 15 end
local function drag(x, y1, y2) CV:button_cb(B, 1, x, y1, "") CV:motion_cb(x, y2, "b") CV:button_cb(B, 0, x, y2, "") end
local function draws() local n = drawCalls CV:action() return drawCalls > n end
local function on(block) return pcore.get(block .. ".on") == 1 end

check(draws(), "the panel draws")
check(DLG.icon and #DLG.icon.pixels == 64 * 64 * 4, "the window has its icon")

for r = 1, #BLOCKS do
    click(CELL_X, rowY(r))
    for _, x in ipairs(KNOBS) do
        drag(x, KNOB_Y, KNOB_Y - 20)
        CV:wheel_cb(1, x, KNOB_Y, "")
        assert(draws())
    end
end
check(true, "every knob of every row turns and redraws")

click(CELL_X, rowY(1))
local gain = pcore.get("in.level")
drag(KNOBS[1], KNOB_Y, KNOB_Y - 20)
check(pcore.get("in.level") > gain, "dragging a knob up reaches the engine")

local was = on("comp")
click(LED_X, rowY(1))
check(on("comp") ~= was, "a row's LED switches its block")

for r, block in ipairs(BLOCKS) do
    if not on(block) then click(LED_X, rowY(r)) end
end
for _, block in ipairs(BLOCKS) do check(on(block), block .. " is on, all together") end

click(BYPASS, SWITCH_Y)
local allOff = true
for _, block in ipairs(BLOCKS) do allOff = allOff and not on(block) end
check(allOff, "BYPASS switches everything off")
click(BYPASS, SWITCH_Y)
local allBack = true
for _, block in ipairs(BLOCKS) do allBack = allBack and on(block) end
check(allBack, "BYPASS brings everything back")

click(CELL_X, rowY(5))
pcore.set("fx.type", 0)
for _ = 1, #pcore.effects() - 1 do
    CV:wheel_cb(1, KNOBS[1], KNOB_Y, "")
    assert(draws())
end
check(pcore.get("fx.type") == #pcore.effects() - 1, "every effect type can be selected and drawn")

drag(PEDAL_X, 300, 100)
check(pcore.get("fx.pedal") > 0, "the pedal drives Whammy")

click(STORE_X, BUTTON_Y)
local list = pcore.presets()
check(list[#list].name == "Mix Test" and list[#list].user, "STORE adds a user preset")
local stored = pcore.values()

click(PREV, SWITCH_Y)
check(pcore.get("comp.on") == 0, "the previous footswitch changes preset")
click(NEXT, SWITCH_Y)
local same = true
for k, v in pairs(stored) do
    if math.abs((pcore.get(k) or math.huge) - v) > 1e-4 then same = false print("    differs: " .. k) end
end
check(same, "the mixed preset comes back exactly as stored")

for _ = 1, 40 do TIMER:action_cb() end
size = { 800, 900 }
check(draws(), "the panel redraws at another window size")

check(DLG:close_cb() == iup.CLOSE, "closing the window closes the dialog")
check(not pcall(pcore.get, "comp.on"), "closing the window stops the engine")

os.remove("presets.txt")
print("panel smoke OK")
