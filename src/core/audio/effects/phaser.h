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

// Ported from torvalds/AudioNoise audio/phaser.h (GPL-2.0). See NOTICE.

#ifndef PCORE_EFFECTS_PHASER_H
#define PCORE_EFFECTS_PHASER_H

#include "../audio_node.h"
#include "dsp.h"

#include <string>
#include <vector>

namespace PCore {
    //
    // Three cascaded biquad all-pass stages whose centre frequency is swept by
    // a triangle LFO, with the last stage fed back into the input. This is the
    // RC-network phaser emulated as an all-pass, nothing cleverer.
    //
    class Phaser : public AudioNode {
    public:
        explicit Phaser(int sampleRate);
        virtual ~Phaser() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        void setParameters(const std::string& param, float value);

    private:
        static const int NUM_STAGES = 3;

        int sampleRate_;

        float rateHz_   = 2.0f;     // LFO rate (the original dials 25ms .. 2s)
        float centerHz_ = 1000.0f;  // 220 .. 6460 Hz
        float octaves_  = 0.5f;     // sweep half an octave either way
        float q_        = 0.7f;     // 0.25 .. 2
        float feedback_ = 0.5f;     // 0 .. 0.75
        float mix_      = 1.0f;     // 1.0 is the original's in+out

        struct ChannelState {
            dsp::Lfo lfo;
            // s[n] is stage n's input history and stage n+1's output history,
            // exactly as the original chains them.
            float s[NUM_STAGES + 1][2] = {};
        };
        std::vector<ChannelState> channels_;

        void recalculate();
    };
}

#endif // PCORE_EFFECTS_PHASER_H
