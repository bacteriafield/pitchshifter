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

// Ported from torvalds/AudioNoise audio/pitch.h (GPL-2.0). See NOTICE.

#ifndef PITCHSHIFTER_H
#define PITCHSHIFTER_H

#include "../audio_node.h"
#include "dsp.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PCore {
    //
    // Walk the delay line at a different speed than it is written: backwards
    // lowers the pitch, forwards raises it. Two taps half a grain apart are
    // windowed with sin and cos so each one's wrap-around discontinuity lands
    // where its own window is zero; sin^2 + cos^2 == 1 keeps the power flat.
    //
    // No FFT, no latency -- one sample in, one sample out.
    //
    class PitchShifter : public AudioNode {
    public:
        explicit PitchShifter(int sampleRate);
        virtual ~PitchShifter() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        void setParameters(const std::string& param, float value);

        // This shifter reads and writes the same sample, so there is none.
        int latencySamples() const { return 0; }

    private:
        // Grain length. 4096 samples is ~85ms at 48kHz.
        static const int DISCONT_SHIFT = 12;
        static const int DISCONT_STEPS = 1 << DISCONT_SHIFT;

        int sampleRate_;

        float octave_ = 0.0f;  // -2 .. +2
        float mix_    = 1.0f;

        float step_ = 0.0f;    // 2^octave - 1

        struct ChannelState {
            dsp::DelayLine line;
            uint32_t phase = 0;
        };
        std::vector<ChannelState> channels_;

        void recalculate();
    };
}

#endif // PITCHSHIFTER_H
