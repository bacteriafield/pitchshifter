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

#ifndef PCORE_AUDIO_PRESET_H
#define PCORE_AUDIO_PRESET_H

#include <cstddef>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace PCore {

    struct Preset {
        std::string name;
        std::map<std::string, float> values;   // "block.param" -> value
    };

    //
    // Factory presets are compiled in; user presets live in a plain text file
    // with the same 40 slots the original has:
    //
    //   # comment
    //   [Karaoke Delay]
    //   delay.on=1
    //   delay.time_ms=320
    //
    class PresetBank {
    public:
        static constexpr std::size_t kUserSlots = 40;

        // Every parameter a preset load resets before applying the preset.
        static const std::map<std::string, float>& defaults();
        static const std::vector<Preset>& factory();

        const std::vector<Preset>& user() const { return user_; }

        // User presets win over factory ones of the same name. nullptr if none.
        const Preset* find(const std::string& name) const;

        // Add a user preset, or replace the one with the same name.
        bool store(const Preset& preset, std::string* err = nullptr);

        bool load(const std::string& file, std::string* err = nullptr);
        bool save(const std::string& file, std::string* err = nullptr) const;

        static bool parse(std::istream& in, std::vector<Preset>& out, std::string* err = nullptr);
        static void write(std::ostream& out, const std::vector<Preset>& presets);

    private:
        std::vector<Preset> user_;
    };
}

#endif // PCORE_AUDIO_PRESET_H
