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

// Ported from torvalds/AudioNoise audio/echo.h (GPL-2.0). See NOTICE.

#ifndef DELAY_H
#define DELAY_H

#include "../audio_node.h"
#include "dsp.h"

#include <string>
#include <vector>

namespace PCore {
    //
    // Minimal echo: one long delay line with feedback. The delay time itself
    // is slewed towards its target so turning the knob glides instead of
    // clicking.
    //
    class Delay : public AudioNode {
    public:
        explicit Delay(int sampleRate);
        virtual ~Delay() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        void setParameters(const std::string& param, float value);

    private:
        int sampleRate_;

        float timeMs_   = 250.0f; // 0 .. 1000 ms
        float feedback_ = 0.35f;  // 0 .. 1
        float mix_      = 0.3f;   // 0 .. 1 (the original's "depth")
        float tone_     = 1.0f;   // 1 = clean digital repeats, lower = darker analog

        float targetDelay_ = 0.0f; // samples

        struct ChannelState {
            dsp::DelayLine line;
            float delay = 0.0f;    // slewed towards targetDelay_
            float lp    = 0.0f;    // tone filter state
        };
        std::vector<ChannelState> channels_;

        void recalculate();
    };
}

#endif // DELAY_H
