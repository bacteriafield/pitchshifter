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

#include "pixelator.h"

#include <algorithm>

namespace PCore {

    Pixelator::Pixelator(int sampleRate) : sampleRate_(sampleRate) {}

    void Pixelator::prepare(int sampleRate, int /*maxBlock*/, int inChans, int outChans) {
        sampleRate_ = sampleRate;
        channels_.assign(std::max(1, std::max(inChans, outChans)), ChannelState());
    }

    void Pixelator::setParameters(const std::string& param, float value) {
        if (param == "bits")            bits_       = std::clamp(value, 1.0f, 16.0f);
        else if (param == "downsample") downsample_ = std::clamp(value, 1.0f, 64.0f);
        else if (param == "mix")        mix_        = std::clamp(value, 0.0f, 1.0f);
    }

    void Pixelator::process(const float* const* in, float* const* out, unsigned long frames) {
        if (!out || !out[0]) return;

        const size_t numC = channels_.size();
        for (size_t c = 0; c < numC; ++c) {
            float* outp = out[c];
            if (!outp) break;

            const float* inp = (in && in[c]) ? in[c] : nullptr;
            if (!inp) { std::fill_n(outp, frames, 0.0f); continue; }

            ChannelState& st = channels_[c];
            for (unsigned long i = 0; i < frames; ++i) {
                const float x = inp[i];

                // Sample and hold: only look at the input every `downsample`
                // samples, which aliases exactly the way cheap hardware did.
                st.counter += 1.0f;
                if (st.counter >= downsample_) {
                    st.counter -= downsample_;
                    st.held = dsp::quantise(x, bits_);
                }

                outp[i] = dsp::lerp(mix_, x, st.held);
            }
        }
    }
}
