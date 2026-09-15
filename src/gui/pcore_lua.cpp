/*
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

*/

//
// pcore: the audio engine as a Lua module, so the IUP front panel in
// src/gui/main.lua drives the same VocalChain the CLI does.
//
//   cmake --build build --target pcore      -> build/lib/pcore.so
//   lua src/gui/main.lua
//
// Everything here runs on the Lua (GUI) thread, which is the single producer
// VocalChain::set() requires; the audio callback is the consumer.
//

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "core/audio/audio_engine.h"
#include "core/audio/effects/voice.h"
#include "core/audio/preset.h"
#include "core/audio/vocal_chain.h"
#include "core/io/port_audio_driver.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

namespace {

    constexpr const char* kUserPresetFile = "presets.txt";
    constexpr const char* kBoxType = "pcore.engine";

    struct Engine {
        PCore::AudioEngine engine;
        IO::PortAudioDriver driver;   // declared after engine, so it is destroyed first
        PCore::PresetBank bank;
        bool running = false;

        explicit Engine(const IO::StreamConfig& cfg)
            : engine(cfg.sampleRate, cfg.framesPerBlock, cfg.inputChannels, cfg.outputChannels) {}

        ~Engine() { driver.stop(); }

        // With no audio thread nobody drains the chain's change queue, and
        // set() would start failing once it filled up. Offline this thread is
        // the only consumer, so draining it here is safe.
        void pump() {
            if (!running) engine.chain().process(nullptr, nullptr, 0);
        }
    };

    Engine*& box(lua_State* L) {
        return *static_cast<Engine**>(lua_touserdata(L, lua_upvalueindex(1)));
    }

    //
    // Lua errors longjmp, so they must never fire while a C++ object with a
    // destructor is alive on the stack. Every function below checks its Lua
    // arguments first and does its C++ work in an inner scope after that.
    //
    Engine& need(lua_State* L) {
        Engine* e = box(L);
        if (!e) luaL_error(L, "pcore.open() must be called first");
        return *e;
    }

    int optInt(lua_State* L, int table, const char* key, int def) {
        if (!lua_istable(L, table)) return def;
        lua_getfield(L, table, key);
        const lua_Integer v = luaL_optinteger(L, -1, def);
        lua_pop(L, 1);
        return static_cast<int>(v);
    }

    // Factory presets first, then user presets, 1-based -- the order presets() lists.
    const PCore::Preset* presetAt(const Engine& e, lua_Integer index) {
        const auto& factory = PCore::PresetBank::factory();
        const auto& user = e.bank.user();
        const auto nf = static_cast<lua_Integer>(factory.size());
        if (index >= 1 && index <= nf) return &factory[index - 1];
        if (index > nf && index <= nf + static_cast<lua_Integer>(user.size())) return &user[index - nf - 1];
        return nullptr;
    }

    // pcore.open{ rate=, block=, inputs=, outputs= } -> audio_ok, audio_err, presets_err
    // The engine exists even when the audio device fails, so the panel still works.
    int l_open(lua_State* L) {
        if (box(L)) return luaL_error(L, "pcore.open() was already called");

        IO::StreamConfig cfg;
        cfg.sampleRate     = optInt(L, 1, "rate", 48000);
        cfg.framesPerBlock = optInt(L, 1, "block", 512);
        cfg.inputChannels  = optInt(L, 1, "inputs", 1);
        cfg.outputChannels = optInt(L, 1, "outputs", 2);
        if (cfg.sampleRate <= 0 || cfg.framesPerBlock <= 0 || cfg.inputChannels < 0 || cfg.outputChannels <= 0)
            return luaL_error(L, "pcore.open: rate, block and outputs must be positive");

        char audioErr[512] = "";
        char presetErr[512] = "";
        bool audioOk = false;
        {
            Engine* e = new Engine(cfg);
            box(L) = e;

            std::string err;
            if (std::ifstream(kUserPresetFile) && !e->bank.load(kUserPresetFile, &err))
                std::snprintf(presetErr, sizeof presetErr, "%s: %s", kUserPresetFile, err.c_str());

            err.clear();
            audioOk = e->driver.initialize(cfg, &PCore::AudioEngine::driverCallback, &e->engine, &err) &&
                      e->driver.start(&err);
            e->running = audioOk;
            if (!audioOk)
                std::snprintf(audioErr, sizeof audioErr, "%s",
                              err.empty() ? "the audio device did not start" : err.c_str());
        }

        lua_pushboolean(L, audioOk);
        if (audioOk) lua_pushnil(L); else lua_pushstring(L, audioErr);
        if (presetErr[0]) lua_pushstring(L, presetErr); else lua_pushnil(L);
        return 3;
    }

    int l_close(lua_State* L) {
        Engine*& e = box(L);
        delete e;
        e = nullptr;
        return 0;
    }

    int l_gc(lua_State* L) {
        auto** b = static_cast<Engine**>(luaL_checkudata(L, 1, kBoxType));
        delete *b;
        *b = nullptr;
        return 0;
    }

    // pcore.set(path, value) -> ok
    int l_set(lua_State* L) {
        Engine& e = need(L);
        const char* path = luaL_checkstring(L, 1);
        const float value = static_cast<float>(luaL_checknumber(L, 2));
        bool ok;
        {
            ok = e.engine.chain().set(path, value);
            e.pump();
        }
        lua_pushboolean(L, ok);
        return 1;
    }

    // pcore.get(path) -> value, or nil if this preset never set it
    int l_get(lua_State* L) {
        Engine& e = need(L);
        const char* path = luaL_checkstring(L, 1);
        float v;
        {
            v = e.engine.chain().get(path, NAN);
        }
        if (std::isnan(v)) lua_pushnil(L); else lua_pushnumber(L, v);
        return 1;
    }

    // pcore.values() -> { ["block.param"] = value, ... }
    int l_values(lua_State* L) {
        Engine& e = need(L);
        lua_newtable(L);
        // ponytail: lua_setfield can only longjmp here on out-of-memory, while
        // the snapshot copy is alive. At that point the process is done anyway.
        const PCore::Preset snap = e.engine.chain().snapshot("");
        for (const auto& kv : snap.values) {
            lua_pushnumber(L, kv.second);
            lua_setfield(L, -2, kv.first.c_str());
        }
        return 1;
    }

    // pcore.presets() -> { { name=, user= }, ... }  factory first, then user
    int l_presets(lua_State* L) {
        Engine& e = need(L);
        const auto& factory = PCore::PresetBank::factory();
        const auto& user = e.bank.user();

        lua_createtable(L, static_cast<int>(factory.size() + user.size()), 0);
        int n = 0;
        auto push = [&](const PCore::Preset& p, bool isUser) {
            lua_createtable(L, 0, 2);
            lua_pushstring(L, p.name.c_str());
            lua_setfield(L, -2, "name");
            lua_pushboolean(L, isUser);
            lua_setfield(L, -2, "user");
            lua_rawseti(L, -2, ++n);
        };
        for (const PCore::Preset& p : factory) push(p, false);
        for (const PCore::Preset& p : user) push(p, true);
        return 1;
    }

    // pcore.load(index) -> ok
    int l_load(lua_State* L) {
        Engine& e = need(L);
        const lua_Integer index = luaL_checkinteger(L, 1);
        bool ok = false;
        {
            if (const PCore::Preset* p = presetAt(e, index)) {
                e.pump();
                ok = e.engine.chain().loadPreset(*p);
                e.pump();
            }
        }
        lua_pushboolean(L, ok);
        return 1;
    }

    // pcore.save(name) -> true, nil, index  |  false, err
    int l_save(lua_State* L) {
        Engine& e = need(L);
        const char* name = luaL_checkstring(L, 1);

        char err[512] = "";
        lua_Integer index = 0;
        {
            const std::string n(name);
            std::string why;
            if (e.bank.store(e.engine.chain().snapshot(n), &why) && e.bank.save(kUserPresetFile, &why)) {
                const auto& user = e.bank.user();
                for (size_t i = 0; i < user.size(); ++i)
                    if (user[i].name == n)
                        index = static_cast<lua_Integer>(PCore::PresetBank::factory().size() + i + 1);
            } else {
                std::snprintf(err, sizeof err, "%s", why.c_str());
            }
        }

        if (index == 0) {
            lua_pushboolean(L, 0);
            lua_pushstring(L, err[0] ? err : "preset not stored");
            return 2;
        }
        lua_pushboolean(L, 1);
        lua_pushnil(L);
        lua_pushinteger(L, index);
        return 3;
    }

    int pushNames(lua_State* L, int count, const char* (*name)(int)) {
        lua_createtable(L, count, 0);
        for (int i = 0; i < count; ++i) {
            lua_pushstring(L, name(i));
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }

    // Type names, index 1 is type 0. These work before pcore.open().
    int l_voices(lua_State* L)  { return pushNames(L, PCore::Voice::TypeCount, &PCore::Voice::typeName); }
    int l_effects(lua_State* L) { return pushNames(L, PCore::VocalChain::FxTypeCount, &PCore::VocalChain::fxName); }
    int l_delays(lua_State* L)  { return pushNames(L, PCore::VocalChain::DelayTypeCount, &PCore::VocalChain::delayName); }

    // pcore.latency() -> input_ms, output_ms
    int l_latency(lua_State* L) {
        Engine& e = need(L);
        lua_pushnumber(L, e.driver.inputLatencySec() * 1000.0);
        lua_pushnumber(L, e.driver.outputLatencySec() * 1000.0);
        return 2;
    }
}

extern "C" int luaopen_pcore(lua_State* L) {
    static const luaL_Reg funcs[] = {
        { "open", l_open },       { "close", l_close },
        { "set", l_set },         { "get", l_get },         { "values", l_values },
        { "presets", l_presets }, { "load", l_load },       { "save", l_save },
        { "voices", l_voices },   { "effects", l_effects }, { "delays", l_delays },
        { "latency", l_latency },
        { nullptr, nullptr },
    };

    luaL_newlibtable(L, funcs);

    // One box holds the engine. Every function sees it as upvalue 1, and its
    // __gc stops the audio if the Lua state closes without pcore.close().
    auto** b = static_cast<Engine**>(lua_newuserdatauv(L, sizeof(Engine*), 0));
    *b = nullptr;
    if (luaL_newmetatable(L, kBoxType)) {
        lua_pushcfunction(L, l_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "_engine");   // the module table keeps the box alive
    luaL_setfuncs(L, funcs, 1);
    return 1;
}
