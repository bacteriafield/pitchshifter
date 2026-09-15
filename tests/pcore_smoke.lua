--
-- End-to-end check of the pcore Lua module: parameters, presets, save and load.
-- ctest runs it (see src/gui/CMakeLists.txt); by hand:
--
--   lua tests/pcore_smoke.lua build/lib --scratch     (from an empty directory)
--
-- It opens the audio device when there is one, and passes without it.
-- It saves presets.txt in the current directory, so it refuses to run where
-- one already exists unless --scratch says that directory is disposable.
--

local moddir = assert(arg[1], "usage: lua pcore_smoke.lua <directory with pcore.so> [--scratch]")
package.cpath = moddir .. "/?.so;" .. package.cpath

if arg[2] == "--scratch" then
    os.remove("presets.txt")
elseif io.open("presets.txt") then
    error("presets.txt exists here and this test would overwrite it; run it from an empty directory")
end

local p = require "pcore"

local function check(cond, msg)
    if not cond then error("FAIL: " .. msg, 2) end
    print("ok  " .. msg)
end

check(not pcall(p.set, "comp.on", 1), "set before open is an error")
check(#p.voices() == 12 and p.voices()[5] == "Monster", "12 voices, #5 = Monster")
check(#p.effects() == 12 and p.effects()[12] == "Whammy", "12 effects, #12 = Whammy")
check(#p.delays() == 2 and p.delays()[2] == "Analog", "2 delay types")

local audioOk, audioErr = p.open{ rate = 48000, block = 512, inputs = 1, outputs = 2 }
print("    audio: " .. (audioOk and "running" or ("not available (" .. tostring(audioErr) .. ")")))
check(not pcall(p.open, {}), "second open is an error")

check(p.set("delay.time_ms", 275) == true, "set a known path")
check(p.get("delay.time_ms") == 275, "get returns what was set")
check(p.set("bogus.x", 1) == false, "unknown path rejected")
check(p.get("reverb.mix") == nil, "never-set path is nil")

local list = p.presets()
check(#list == 11 and list[1].name == "Vocal Delay" and list[1].user == false, "11 factory presets listed")
check(p.load(4) == true, "load preset 4")
local v = p.values()
check(v["voice.on"] == 1 and v["voice.type"] == 2, "Grunge Vocal: voice on, type Grunge")
check(v["delay.time_ms"] == 350, "load resets delay.time_ms to its default")
check(p.load(99) == false and p.load(0) == false, "out-of-range load rejected")

local saved, saveErr, index = p.save("Smoke Test")
check(saved == true and index == 12, "save goes to user slot 12 (" .. tostring(saveErr) .. ")")
local after = p.presets()
check(#after == 12 and after[12].user and after[12].name == "Smoke Test", "saved preset listed as user")
local fh = assert(io.open("presets.txt"))
local body = fh:read("a")
fh:close()
check(body:find("[Smoke Test]", 1, true) ~= nil, "presets.txt written")
check(p.load(12) and p.get("voice.type") == 2, "user preset loads back")

p.close()
check(not pcall(p.get, "comp.on"), "calls after close are errors")

os.remove("presets.txt")
print("pcore smoke OK")
