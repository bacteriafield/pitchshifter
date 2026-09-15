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

// Ported from torvalds/AudioNoise audio/eq.h (GPL-2.0). See NOTICE.

#include "eq.h"

#include <algorithm>
#include <cmath>

namespace PCore {

    static constexpr float kBaseFreq = 31.25f;
    static constexpr float kMaxDb    = 20.0f;

    Eq::Eq(int sampleRate) : sampleRate_(sampleRate) {
        recalculate();
    }

    void Eq::prepare(int sampleRate, int /*maxBlock*/, int inChans, int outChans) {
        sampleRate_ = sampleRate;
        channels_.assign(std::max(1, std::max(inChans, outChans)), ChannelState());
        recalculate();
    }

    float Eq::bandFrequency(int band) const {
        return kBaseFreq * std::exp2(static_cast<float>(std::clamp(band, 0, NUM_BANDS - 1)));
    }

    void Eq::setBandGain(int band, float db) {
        if (band < 0 || band >= NUM_BANDS) return;
        gainsDb_[band] = std::clamp(db, -kMaxDb, kMaxDb);
        recalculate();
    }

    void Eq::setParameters(const std::string& param, float value) {
        if (param.size() == 5 && param.compare(0, 4, "band") == 0 &&
            param[4] >= '0' && param[4] <= '9') {
            setBandGain(param[4] - '0', value);
        }
        else if (param == "low")  setBandGain(0, value);
        else if (param == "mid")  setBandGain(5, value);   // 1kHz
        else if (param == "high") setBandGain(9, value);
    }

    void Eq::recalculate() {
        // A band that disagrees with its neighbours gets a tighter Q. The
        // original works in pot units (-100..100 == -20..+20dB), hence /60.
        auto bandQ = [&](int b) {
            const float prev = gainsDb_[b > 0 ? b - 1 : b];
            const float next = gainsDb_[b < NUM_BANDS - 1 ? b + 1 : b];
            const float here = gainsDb_[b];
            return 0.707f + (std::fabs(prev - here) + std::fabs(next - here)) / 60.0f;
        };

        const float nyquist = 0.45f * static_cast<float>(sampleRate_);

        for (int b = 0; b < NUM_BANDS; ++b) {
            const float freq = std::min(bandFrequency(b), nyquist);
            const dsp::SinCos w = dsp::omega(freq, sampleRate_);
            const float gain = dsp::dbToLevel(gainsDb_[b]);
            const float Q = bandQ(b);

            if (b == 0)                   coeff_[b].lowShelf(w, Q, gain);
            else if (b == NUM_BANDS - 1)  coeff_[b].highShelf(w, Q, gain);
            else                          coeff_[b].peaking(w, Q, gain);
        }

        for (ChannelState& st : channels_)
            for (int b = 0; b < NUM_BANDS; ++b)
                st.bands[b].c = coeff_[b];
    }

    void Eq::process(const float* const* in, float* const* out, unsigned long frames) {
        if (!out || !out[0]) return;

        const size_t numC = channels_.size();
        for (size_t c = 0; c < numC; ++c) {
            float* outp = out[c];
            if (!outp) break;

            const float* inp = (in && in[c]) ? in[c] : nullptr;
            if (!inp) { std::fill_n(outp, frames, 0.0f); continue; }

            ChannelState& st = channels_[c];
            for (unsigned long i = 0; i < frames; ++i) {
                float y = inp[i];
                for (int b = 0; b < NUM_BANDS; ++b)
                    y = st.bands[b].step(y);
                outp[i] = y;
            }
        }
    }
}
