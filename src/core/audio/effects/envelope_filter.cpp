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

#include "envelope_filter.h"

#include <algorithm>
#include <cmath>

namespace PCore {

    EnvelopeFilter::EnvelopeFilter(int sampleRate) : sampleRate_(sampleRate) {
        recalculate();
    }

    void EnvelopeFilter::prepare(int sampleRate, int /*maxBlock*/, int inChans, int outChans) {
        sampleRate_ = sampleRate;
        channels_.assign(std::max(1, std::max(inChans, outChans)), ChannelState());
        recalculate();
    }

    void EnvelopeFilter::recalculate() {
        attackCoeff_  = dsp::timeConstant(attackMs_, sampleRate_);
        releaseCoeff_ = dsp::timeConstant(releaseMs_, sampleRate_);
    }

    void EnvelopeFilter::setParameters(const std::string& param, float value) {
        if (param == "min_hz")           minHz_ = std::clamp(value, 80.0f, 2000.0f);
        else if (param == "max_hz")      maxHz_ = std::clamp(value, 200.0f, 8000.0f);
        else if (param == "sensitivity") sensitivity_ = std::clamp(value, 0.5f, 40.0f);
        else if (param == "q")           q_ = std::clamp(value, 0.5f, 8.0f);
        else if (param == "attack")    { attackMs_  = std::clamp(value, 1.0f, 100.0f); recalculate(); }
        else if (param == "release")   { releaseMs_ = std::clamp(value, 20.0f, 800.0f); recalculate(); }
        else if (param == "mix")         mix_ = std::clamp(value, 0.0f, 1.0f);
    }

    void EnvelopeFilter::process(const float* const* in, float* const* out, unsigned long frames) {
        if (!out || !out[0]) return;

        const float nyquist = 0.45f * static_cast<float>(sampleRate_);
        const size_t numC = channels_.size();

        for (size_t c = 0; c < numC; ++c) {
            float* outp = out[c];
            if (!outp) break;

            const float* inp = (in && in[c]) ? in[c] : nullptr;
            if (!inp) { std::fill_n(outp, frames, 0.0f); continue; }

            ChannelState& st = channels_[c];
            dsp::BiquadCoeff coeff;

            for (unsigned long i = 0; i < frames; ++i) {
                const float x = inp[i];

                const float mag  = std::fabs(x);
                const float coef = (mag > st.env) ? attackCoeff_ : releaseCoeff_;
                st.env = dsp::lerp(coef, mag, st.env);

                // Envelope drives the sweep, saturating so a shout doesn't
                // slam the filter straight into Nyquist.
                const float drive = dsp::limit(st.env * sensitivity_);
                const float freq  = std::clamp(dsp::lerp(drive, minHz_, maxHz_), 20.0f, nyquist);

                coeff.peaking(dsp::omega(freq, sampleRate_), q_, 8.0f);
                const float y = coeff.step(x, st.x, st.y);

                outp[i] = dsp::lerp(mix_, x, y);
            }
        }
    }
}
