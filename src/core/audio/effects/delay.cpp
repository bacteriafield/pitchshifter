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

#include "delay.h"

#include <algorithm>

namespace PCore {

    static constexpr float kMaxDelayMs = 1000.0f;
    // One pole towards the new delay time -- about 1s to settle at 48kHz.
    static constexpr float kSlew = 0.001f;

    Delay::Delay(int sampleRate) : sampleRate_(sampleRate) {
        recalculate();
    }

    void Delay::prepare(int sampleRate, int /*maxBlock*/, int inChans, int outChans) {
        sampleRate_ = sampleRate;
        channels_.assign(std::max(1, std::max(inChans, outChans)), ChannelState());

        const size_t history = static_cast<size_t>(kMaxDelayMs * 0.001f * sampleRate_) + 4;
        recalculate();
        for (ChannelState& st : channels_) {
            st.line.resize(history);
            st.delay = targetDelay_;   // start settled, don't sweep in on boot
        }
    }

    void Delay::recalculate() {
        targetDelay_ = timeMs_ * 0.001f * static_cast<float>(sampleRate_);
    }

    void Delay::setParameters(const std::string& param, float value) {
        if (param == "time_ms" || param == "time" || param == "delay") {
            timeMs_ = std::clamp(value, 0.0f, kMaxDelayMs);
            recalculate();
        }
        else if (param == "feedback") feedback_ = std::clamp(value, 0.0f, 1.0f);
        else if (param == "mix" || param == "depth") mix_ = std::clamp(value, 0.0f, 1.0f);
        else if (param == "tone") tone_ = std::clamp(value, 0.05f, 1.0f);
    }

    void Delay::process(const float* const* in, float* const* out, unsigned long frames) {
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

                st.delay = dsp::lerp(kSlew, st.delay, targetDelay_);

                // One-pole low-pass on the repeats. tone 1 leaves a clean digital
                // echo; lower values darken every trip round the loop, the way a
                // bucket-brigade analog delay does.
                st.lp += tone_ * (st.line.read(1.0f + st.delay) - st.lp);
                const float wet = st.lp;
                st.line.write(dsp::limit(x + wet * feedback_));

                outp[i] = dsp::lerp(mix_, x, wet);
            }
        }
    }
}
