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

#ifndef PCORE_EFFECTS_VOICE_H
#define PCORE_EFFECTS_VOICE_H

#include "../audio_node.h"
#include "dsp.h"
#include "pitchshifter.h"

#include <string>
#include <vector>

namespace PCore {
    //
    // The Vocal 300's "Mic Pre / Voice" block: one character at a time, each a
    // recipe of pitch shift, drive, fold, band limit, bitcrush and ring mod.
    //
    // The twelve names are the original's, from its manual. The recipes are
    // ours -- DigiTech never published the DSP, so each one is built to sound
    // like its name rather than cloned.
    //
    class Voice : public AudioNode {
    public:
        enum Type {
            TubePre, Overdrive, Grunge, Darkside, Monster, Chipmunk,
            Lunar, LoFi, Robot, Wizard, Alien, Telephone,
            TypeCount
        };

        static const char* typeName(int type);

        explicit Voice(int sampleRate);
        virtual ~Voice() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        // Loads the character's recipe; knob tweaks start from there.
        void setType(int type);
        int type() const { return type_; }

        // "drive" dB, "level" dB (fold ceiling), "tone" Hz (low-pass), "mix"
        void setParameters(const std::string& param, float value);

    private:
        struct Recipe {
            const char* name;
            float octave;       // pitch shift, 0 = none
            float driveDb;
            float levelDb;      // fold ceiling
            float hpfHz, lpfHz;
            float bits;         // 0 = no bitcrush
            float ringHz;       // 0 = no ring mod
            float ringMix;
        };
        static const Recipe kRecipes[TypeCount];

        int sampleRate_;
        int maxBlock_ = 0;
        int type_ = TubePre;
        Recipe r_;
        float mix_   = 1.0f;
        float drive_ = 1.0f;
        float level_ = 1.0f;

        PitchShifter pitch_;
        std::vector<std::vector<float>> pitched_;
        std::vector<float*> pitchedPtrs_;

        struct ChannelState {
            dsp::Biquad hpf, lpf;
            dsp::Lfo ring;
        };
        std::vector<ChannelState> channels_;

        void recalculate();
    };
}

#endif // PCORE_EFFECTS_VOICE_H
