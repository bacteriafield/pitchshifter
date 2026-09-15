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

#include "pitchshifter.h"

#include <algorithm>
#include <cmath>

namespace PCore {

    PitchShifter::PitchShifter(int sampleRate) : sampleRate_(sampleRate) {
        recalculate();
    }

    void PitchShifter::prepare(int sampleRate, int /*maxBlock*/, int inChans, int outChans) {
        sampleRate_ = sampleRate;
        channels_.assign(std::max(1, std::max(inChans, outChans)), ChannelState());

        for (ChannelState& st : channels_) {
            st.line.resize(4 * DISCONT_STEPS);
            st.phase = 0;
        }
        recalculate();
    }

    void PitchShifter::recalculate() {
        // Walking speed relative to the write head: 0.25x .. 4x, so the tap
        // drifts by -0.75 .. +3 samples per sample.
        step_ = std::exp2(octave_) - 1.0f;
    }

    void PitchShifter::setParameters(const std::string& param, float value) {
        if (param == "octave")          octave_ = std::clamp(value, -2.0f, 2.0f);
        else if (param == "semitones")  octave_ = std::clamp(value / 12.0f, -2.0f, 2.0f);
        else if (param == "mix")      { mix_ = std::clamp(value, 0.0f, 1.0f); return; }
        else return;
        recalculate();
    }

    void PitchShifter::process(const float* const* in, float* const* out, unsigned long frames) {
        if (!out || !out[0]) return;

        const uint32_t mask = DISCONT_STEPS - 1;
        // Shifting up walks the tap towards the write head by `step` samples
        // per sample, so it must start step * grain samples back. Sizing that to
        // the actual step instead of the +2 octave worst case cuts the delay a
        // pitched-up voice lags behind the singer by up to two thirds.
        const float base = (step_ > 0.0f) ? step_ * static_cast<float>(DISCONT_STEPS) + 1.0f : 0.0f;

        const size_t numC = channels_.size();
        for (size_t c = 0; c < numC; ++c) {
            float* outp = out[c];
            if (!outp) break;

            const float* inp = (in && in[c]) ? in[c] : nullptr;
            if (!inp) { std::fill_n(outp, frames, 0.0f); continue; }

            ChannelState& st = channels_[c];
            for (unsigned long f = 0; f < frames; ++f) {
                const float x = inp[f];

                const uint32_t phase = st.phase++;
                const uint32_t i  = phase & mask;
                const uint32_t ni = (i + DISCONT_STEPS / 2) & mask;

                // Only half the circle is used: sin walks 0..0.5 turns while
                // cos walks 0.25..0.75, so each is zero at the other's wrap.
                const float turns = static_cast<float>(
                    static_cast<double>(phase << (31 - DISCONT_SHIFT)) / 4294967296.0);
                const float angle = dsp::kTwoPi * turns;

                st.line.write(x);

                const float d1 = st.line.read(base - static_cast<float>(i)  * step_) * std::sin(angle);
                const float d2 = st.line.read(base - static_cast<float>(ni) * step_) * std::cos(angle);

                outp[f] = dsp::lerp(mix_, x, d1 + d2);
            }
        }
    }
}
