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

// Ported from torvalds/AudioNoise audio/flanger.h (GPL-2.0), which in turn
// credits the MIT-licensed DaisySP by Electrosmith / Soundpipe. See NOTICE.

#ifndef FLANGER_H
#define FLANGER_H

#include "../audio_node.h"
#include "dsp.h"

#include <string>
#include <vector>

namespace PCore {
    //
    // Short modulated delay with feedback. A sine LFO sweeps the tap between
    // 1 sample and `delay` milliseconds.
    //
    class Flanger : public AudioNode {
    public:
        explicit Flanger(int sampleRate);
        virtual ~Flanger() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        void setParameters(const std::string& param, float value);

    private:
        int sampleRate_;

        float rateHz_   = 0.5f;  // 0 .. 10 Hz
        float delayMs_  = 2.0f;  // 0 .. 4 ms
        float depth_    = 0.7f;  // 0 .. 1
        float feedback_ = 0.5f;  // 0 .. 1
        float mix_      = 0.5f;  // 0.5 is the original's fixed (in+out)/2

        float delaySamples_ = 0.0f;

        struct ChannelState {
            dsp::Lfo lfo;
            dsp::DelayLine line;
        };
        std::vector<ChannelState> channels_;

        void recalculate();
    };
}

#endif // FLANGER_H
