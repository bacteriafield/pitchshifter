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

// Ported from torvalds/AudioNoise audio/compressor.h (GPL-2.0). See NOTICE.

#include "compressor.h"

#include <algorithm>
#include <cmath>

namespace PCore {

    // How fast the gain itself follows its target, to keep clicks out.
    static constexpr float kGainSlew = 0.01f;

    Compressor::Compressor(int sampleRate) : sampleRate_(sampleRate) {
        recalculate();
    }

    void Compressor::prepare(int sampleRate, int /*maxBlock*/, int inChans, int outChans) {
        sampleRate_ = sampleRate;
        channels_.assign(std::max(1, std::max(inChans, outChans)), ChannelState());
        recalculate();
    }

    void Compressor::recalculate() {
        level_        = dsp::dbToLevel(thresholdDb_);
        attackCoeff_  = dsp::timeConstant(attackMs_, sampleRate_);
        releaseCoeff_ = dsp::timeConstant(releaseMs_, sampleRate_);
        ratioExp_     = 1.0f - 1.0f / ratio_;
        makeup_       = dsp::dbToLevel(makeupDb_);
    }

    void Compressor::setParameters(const std::string& param, float value) {
        if (param == "threshold" || param == "level")
                                       thresholdDb_ = std::clamp(value, -40.0f, 0.0f);
        else if (param == "attack")    attackMs_    = std::clamp(value, 2.0f, 100.0f);
        else if (param == "release")   releaseMs_   = std::clamp(value, 50.0f, 500.0f);
        else if (param == "ratio")     ratio_       = std::clamp(value, 1.0f, 20.0f);
        else if (param == "makeup" || param == "boost")
                                       makeupDb_    = std::clamp(value, 0.0f, 24.0f);
        else return;
        recalculate();
    }

    void Compressor::process(const float* const* in, float* const* out, unsigned long frames) {
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

                // Envelope follower: rises at the attack rate, falls at release.
                const float mag  = std::fabs(x);
                const float coef = (mag > st.env) ? attackCoeff_ : releaseCoeff_;
                st.env = dsp::lerp(coef, mag, st.env);

                float target = 1.0f;
                if (st.env > level_)
                    target = std::pow(level_ / st.env, ratioExp_);

                st.gain = dsp::lerp(kGainSlew, st.gain, target);

                outp[i] = x * st.gain * makeup_;
            }
        }
    }
}
