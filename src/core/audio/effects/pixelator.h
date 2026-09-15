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

#ifndef PCORE_EFFECTS_PIXELATOR_H
#define PCORE_EFFECTS_PIXELATOR_H

#include "../audio_node.h"
#include "dsp.h"

#include <string>
#include <vector>

namespace PCore {
    //
    // Bitcrusher: drop the word length and hold each sample for a while, which
    // is the whole of "lo-fi" and "pixelator" on an early digital vocal box.
    //
    class Pixelator : public AudioNode {
    public:
        explicit Pixelator(int sampleRate);
        virtual ~Pixelator() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        void setParameters(const std::string& param, float value);

    private:
        int sampleRate_;

        float bits_     = 6.0f;     // 1 .. 16
        float downsample_ = 8.0f;   // 1 .. 64, samples held per step
        float mix_      = 1.0f;

        struct ChannelState {
            float held    = 0.0f;
            float counter = 0.0f;
        };
        std::vector<ChannelState> channels_;
    };
}

#endif // PCORE_EFFECTS_PIXELATOR_H
