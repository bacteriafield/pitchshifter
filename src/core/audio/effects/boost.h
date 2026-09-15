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

// Ported from torvalds/AudioNoise audio/boost.h (GPL-2.0). See NOTICE.

#ifndef PCORE_EFFECTS_BOOST_H
#define PCORE_EFFECTS_BOOST_H

#include "../audio_node.h"
#include "dsp.h"

#include <string>
#include <vector>

namespace PCore {
    class Boost : public AudioNode {
    public:
        explicit Boost(int sampleRate);
        virtual ~Boost() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        void setParameters(const std::string& param, float value);

    private:
        int sampleRate_;

        float boostDb_   = 20.0f;   // 0 .. 40 dB
        float levelDb_   = -12.0f;  // -40 .. 0 dB (fold ceiling)
        float bassCutHz_ = 80.0f;   // 10 .. 200 Hz
        float highCutHz_ = 6000.0f; // 1k .. 20k Hz
        float mix_       = 1.0f;

        float mult_  = 1.0f;
        float level_ = 1.0f;

        struct ChannelState {
            dsp::Biquad bassCut, highCut;
        };
        std::vector<ChannelState> channels_;

        void recalculate();
    };
}

#endif // PCORE_EFFECTS_BOOST_H
