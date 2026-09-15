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

#include "preset.h"
#include "vocal_chain.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <istream>
#include <ostream>

namespace PCore {

    const std::map<std::string, float>& PresetBank::defaults() {
        // Everything off and every level at unity, so an empty preset is a wire.
        static const std::map<std::string, float> d = {
            { "in.level", 0.0f }, { "out.level", 0.0f },

            { "comp.on", 0.0f }, { "comp.threshold", -20.0f }, { "comp.ratio", 4.0f },
            { "comp.attack", 15.0f }, { "comp.release", 150.0f }, { "comp.makeup", 0.0f },

            { "voice.on", 0.0f }, { "voice.type", 0.0f }, { "voice.mix", 1.0f },

            { "eq.on", 0.0f },
            { "eq.band0", 0.0f }, { "eq.band1", 0.0f }, { "eq.band2", 0.0f }, { "eq.band3", 0.0f },
            { "eq.band4", 0.0f }, { "eq.band5", 0.0f }, { "eq.band6", 0.0f }, { "eq.band7", 0.0f },
            { "eq.band8", 0.0f }, { "eq.band9", 0.0f },

            { "gate.on", 0.0f }, { "gate.threshold", -60.0f },
            { "gate.attack", 1.0f }, { "gate.release", 80.0f },

            { "fx.on", 0.0f }, { "fx.type", 0.0f }, { "fx.pedal", 0.0f },

            { "delay.on", 0.0f }, { "delay.type", 0.0f }, { "delay.time_ms", 350.0f },
            { "delay.feedback", 0.3f }, { "delay.mix", 0.3f },

            { "reverb.on", 0.0f }, { "reverb.decay", 0.5f },
            { "reverb.mix", 0.25f }, { "reverb.time_s", 0.08f },
        };
        return d;
    }

    const std::vector<Preset>& PresetBank::factory() {
        // The names are real Vocal 300 factory presets, from its manual. The
        // settings are ours: DigiTech's values aren't published.
        static const std::vector<Preset> f = {
            { "Vocal Delay", {
                { "comp.on", 1 }, { "comp.threshold", -18 }, { "comp.makeup", 3 },
                { "gate.on", 1 }, { "gate.threshold", -55 },
                { "delay.on", 1 }, { "delay.time_ms", 380 }, { "delay.feedback", 0.35f }, { "delay.mix", 0.3f },
                { "reverb.on", 1 }, { "reverb.mix", 0.2f },
            } },
            { "Monster", {
                { "voice.on", 1 }, { "voice.type", Voice::Monster },
                { "gate.on", 1 }, { "gate.threshold", -50 },
                { "reverb.on", 1 }, { "reverb.mix", 0.15f },
            } },
            { "50's Slapback Delay", {
                { "voice.on", 1 }, { "voice.type", Voice::TubePre },
                { "delay.on", 1 }, { "delay.type", VocalChain::DelayAnalog },
                { "delay.time_ms", 110 }, { "delay.feedback", 0.1f }, { "delay.mix", 0.35f },
            } },
            { "Grunge Vocal", {
                { "comp.on", 1 }, { "comp.ratio", 6 }, { "comp.makeup", 2 },
                { "voice.on", 1 }, { "voice.type", Voice::Grunge },
                { "gate.on", 1 }, { "gate.threshold", -45 },
                { "reverb.on", 1 }, { "reverb.mix", 0.15f },
            } },
            { "Thicken", {
                { "comp.on", 1 }, { "comp.makeup", 2 },
                { "fx.on", 1 }, { "fx.type", VocalChain::FxDoubler },
                { "reverb.on", 1 }, { "reverb.mix", 0.2f },
            } },
            { "Vocal Plate", {
                { "comp.on", 1 },
                { "eq.on", 1 }, { "eq.band0", -3 }, { "eq.band9", 3 },
                { "reverb.on", 1 }, { "reverb.decay", 0.7f }, { "reverb.time_s", 0.12f }, { "reverb.mix", 0.35f },
            } },
            { "Wizard", {
                { "voice.on", 1 }, { "voice.type", Voice::Wizard },
                { "delay.on", 1 }, { "delay.time_ms", 300 }, { "delay.feedback", 0.4f }, { "delay.mix", 0.25f },
                { "reverb.on", 1 }, { "reverb.mix", 0.3f },
            } },
            { "Warm Tube Delay", {
                { "voice.on", 1 }, { "voice.type", Voice::TubePre },
                { "delay.on", 1 }, { "delay.type", VocalChain::DelayAnalog },
                { "delay.time_ms", 420 }, { "delay.feedback", 0.45f }, { "delay.mix", 0.3f },
                { "reverb.on", 1 }, { "reverb.mix", 0.15f },
            } },
            { "Distorted Tremolo", {
                { "voice.on", 1 }, { "voice.type", Voice::Overdrive },
                { "fx.on", 1 }, { "fx.type", VocalChain::FxTremolo }, { "fx.rate", 6 }, { "fx.depth", 0.8f },
            } },
            { "Karaoke Delay", {
                { "comp.on", 1 }, { "comp.threshold", -22 }, { "comp.makeup", 4 },
                { "eq.on", 1 }, { "eq.band5", 2 },
                { "delay.on", 1 }, { "delay.time_ms", 320 }, { "delay.feedback", 0.3f }, { "delay.mix", 0.25f },
                { "reverb.on", 1 }, { "reverb.decay", 0.6f }, { "reverb.mix", 0.3f },
            } },
            { "Mars Man", {
                { "voice.on", 1 }, { "voice.type", Voice::Alien },
                { "fx.on", 1 }, { "fx.type", VocalChain::FxFlanger },
                { "reverb.on", 1 }, { "reverb.mix", 0.2f },
            } },
        };
        return f;
    }

    static std::string trim(const std::string& s) {
        const size_t first = s.find_first_not_of(" \t\r");
        if (first == std::string::npos) return std::string();
        const size_t last = s.find_last_not_of(" \t\r");
        return s.substr(first, last - first + 1);
    }

    static bool fail(std::string* err, int line, const char* what) {
        if (err) *err = "line " + std::to_string(line) + ": " + what;
        return false;
    }

    const Preset* PresetBank::find(const std::string& name) const {
        for (const Preset& p : user_)
            if (p.name == name) return &p;
        for (const Preset& p : factory())
            if (p.name == name) return &p;
        return nullptr;
    }

    bool PresetBank::store(const Preset& preset, std::string* err) {
        // A name the text format can't round-trip would silently come back
        // different next session, so refuse it up front.
        if (preset.name.empty() || preset.name != trim(preset.name) ||
            preset.name.find_first_of("[]\r\n") != std::string::npos) {
            if (err) *err = "invalid preset name '" + preset.name + "'";
            return false;
        }

        for (Preset& p : user_) {
            if (p.name == preset.name) {
                p = preset;
                return true;
            }
        }

        if (user_.size() >= kUserSlots) {
            if (err) *err = "all " + std::to_string(kUserSlots) + " user slots are full";
            return false;
        }
        user_.push_back(preset);
        return true;
    }

    bool PresetBank::parse(std::istream& in, std::vector<Preset>& out, std::string* err) {
        std::string raw;
        int lineNo = 0;
        bool inPreset = false;

        while (std::getline(in, raw)) {
            ++lineNo;
            const std::string line = trim(raw);
            if (line.empty() || line[0] == '#') continue;

            if (line[0] == '[') {
                const std::string name = (line.size() >= 2 && line.back() == ']')
                                       ? trim(line.substr(1, line.size() - 2))
                                       : std::string();
                if (name.empty()) return fail(err, lineNo, "expected [preset name]");
                out.push_back(Preset{ name, {} });
                inPreset = true;
                continue;
            }

            const size_t eq = line.find('=');
            if (!inPreset || eq == std::string::npos)
                return fail(err, lineNo, "expected key=value inside a [preset]");

            const std::string key = trim(line.substr(0, eq));
            const std::string val = trim(line.substr(eq + 1));

            char* end = nullptr;
            errno = 0;
            const float v = std::strtof(val.c_str(), &end);
            if (key.empty() || val.empty() || *end != '\0' || errno == ERANGE || !std::isfinite(v))
                return fail(err, lineNo, "expected key=<number>");

            out.back().values[key] = v;
        }
        return true;
    }

    void PresetBank::write(std::ostream& out, const std::vector<Preset>& presets) {
        out << "# PitchShifter user presets\n";
        for (const Preset& p : presets) {
            out << "\n[" << p.name << "]\n";
            for (const auto& kv : p.values)
                out << kv.first << '=' << kv.second << '\n';
        }
    }

    bool PresetBank::load(const std::string& file, std::string* err) {
        std::ifstream f(file);
        if (!f) {
            if (err) *err = "cannot open " + file;
            return false;
        }

        std::vector<Preset> parsed;
        if (!parse(f, parsed, err)) return false;

        // All or nothing: a bad file leaves the current user presets alone.
        std::vector<Preset> previous;
        previous.swap(user_);
        for (const Preset& p : parsed) {
            if (!store(p, err)) {
                user_.swap(previous);
                return false;
            }
        }
        return true;
    }

    bool PresetBank::save(const std::string& file, std::string* err) const {
        // Write beside the target and rename over it, so a crash mid-write
        // can't destroy the presets that were already there.
        const std::string tmp = file + ".tmp";
        {
            std::ofstream f(tmp, std::ios::trunc);
            if (!f) {
                if (err) *err = "cannot write " + tmp;
                return false;
            }
            write(f, user_);
            f.flush();
            if (!f) {
                if (err) *err = "write failed for " + tmp;
                return false;
            }
        }

        if (std::rename(tmp.c_str(), file.c_str()) != 0) {
            // Windows refuses to rename over an existing file.
            std::remove(file.c_str());
            if (std::rename(tmp.c_str(), file.c_str()) != 0) {
                if (err) *err = "cannot replace " + file;
                return false;
            }
        }
        return true;
    }
}
