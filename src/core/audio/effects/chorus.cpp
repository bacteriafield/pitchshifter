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


#include "chorus.h"

namespace PCore {

    static constexpr float kMaxBaseMs  = 50.0f;
    static constexpr float kMaxDepthMs = 25.0f;
    Chorus::Chorus(int sampleRate) : sampleRate_(sampleRate) {}

    void Chorus::prepare(int sr, int block, int inCh, int outCh) {
        sampleRate_ = sr;

        // Sized for the largest base delay and depth up front, so changing
        // either later never reallocates on the audio thread.
        const size_t total = msToSamples(kMaxBaseMs + kMaxDepthMs) + 8;

        dBuffer_.assign(total, 0.0f);
        writePos_ = 0;
        lfoPhase_ = 0.0f;
    }

    void Chorus::process(const float* const *in, float* const *out, unsigned long frames) {
        if (!out || !out[0]) return;

        const float* x = (in && in[0]) ? in[0] : nullptr;
        float* y = out[0];

        if (!x) {
            std::fill_n(y, frames, 0.0f);
            return;
        }

        const size_t N = dBuffer_.size();
        if (N == 0) { std::copy(x, x + frames, y); return; }

        const float twoPi = 2.0f * M_PI;
        const float phaseInc = twoPi * (lfoRate_ / static_cast<float>(sampleRate_));

        const float baseSmpF  = static_cast<float>(msToSamples(static_cast<float>(baseDelayMs_)));
        const float depthSmpF = static_cast<float>(msToSamples(depthMs_));

        size_t wp = writePos_;
        float phase = lfoPhase_;

        for (unsigned long i = 0; i < frames; ++i) {
            float lfo = 0.5f * (1.0f + std::sin(phase));
            float delaySmpF = baseSmpF + depthSmpF * lfo;

            float rpF = static_cast<float>(wp) - delaySmpF;
            while (rpF < 0.0f) rpF += static_cast<float>(N);

            size_t rp0 = static_cast<size_t>(rpF);
            size_t rp1 = (rp0 + 1) % N;
            float frac = rpF - static_cast<float>(rp0);

            float d0 = dBuffer_[rp0];
            float d1 = dBuffer_[rp1];
            float delayed = d0 + (d1 - d0) * frac;

            dBuffer_[wp] = x[i] + 0.02f * delayed;

            float outSmp = (1.0f - mix_) * x[i] + mix_ * delayed;
            y[i] = outSmp;

            wp = (wp + 1) % N;
            phase += phaseInc;
            if (phase >= twoPi) phase -= twoPi;
        }

        writePos_ = wp;
        lfoPhase_ = phase;

        // if have more out channels you can duplicate y to out[1..]
    }

    void Chorus::setParameters(const std::string& param, float value) {
        if (param == "lfo_rate_hz") {
            lfoRate_ = std::clamp(value, 0.05f, 5.0f);
        } else if (param == "depth_ms") {
            depthMs_ = std::clamp(value, 1.0f, kMaxDepthMs);
        } else if (param == "mix") {
            mix_ = std::clamp(value, 0.0f, 1.0f);
        } else if (param == "base_delay_ms") {
            baseDelayMs_ = std::clamp(static_cast<int>(std::lround(value)), 1, static_cast<int>(kMaxBaseMs));
        }
    }
};