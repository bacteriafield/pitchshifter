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

#include "voice.h"

#include <algorithm>

namespace PCore {

    const Voice::Recipe Voice::kRecipes[TypeCount] = {
        //  name          octave  drive  level    hpf       lpf     bits   ring   ringMix
        { "TubePre",      0.00f,   6.0f,  -1.0f,  60.0f, 14000.0f,  0.0f,   0.0f, 0.0f },
        { "Overdrive",    0.00f,  20.0f,  -6.0f,  90.0f,  7000.0f,  0.0f,   0.0f, 0.0f },
        { "Grunge",       0.00f,  32.0f, -10.0f, 150.0f,  4500.0f,  0.0f,   0.0f, 0.0f },
        { "Darkside",    -0.25f,  26.0f,  -8.0f, 100.0f,  2200.0f,  0.0f,  30.0f, 0.5f },
        { "Monster",     -1.00f,  10.0f,  -4.0f,  60.0f,  3500.0f,  0.0f,   0.0f, 0.0f },
        { "Chipmunk",     1.00f,   0.0f,   0.0f, 150.0f, 12000.0f,  0.0f,   0.0f, 0.0f },
        { "Lunar",        0.50f,   4.0f,  -2.0f, 200.0f,  6000.0f,  0.0f,   6.0f, 0.6f },
        { "Lo-Fi",        0.00f,   8.0f,  -3.0f, 300.0f,  3500.0f,  6.0f,   0.0f, 0.0f },
        { "Robot",        0.00f,   6.0f,  -3.0f, 200.0f,  5000.0f, 10.0f,  90.0f, 1.0f },
        { "Wizard",      -0.50f,   6.0f,  -3.0f,  80.0f,  5000.0f,  0.0f,   3.0f, 0.5f },
        { "Alien",        0.75f,   6.0f,  -3.0f, 250.0f,  8000.0f,  0.0f, 420.0f, 0.7f },
        { "Telephone",    0.00f,  14.0f,  -6.0f, 450.0f,  3000.0f,  0.0f,   0.0f, 0.0f },
    };

    const char* Voice::typeName(int type) {
        return kRecipes[std::clamp(type, 0, TypeCount - 1)].name;
    }

    Voice::Voice(int sampleRate)
        : sampleRate_(sampleRate), r_(kRecipes[TubePre]), pitch_(sampleRate) {
        recalculate();
    }

    void Voice::prepare(int sampleRate, int maxBlock, int inChans, int outChans) {
        sampleRate_ = sampleRate;
        maxBlock_ = std::max(1, maxBlock);

        const int nch = std::max(1, std::max(inChans, outChans));
        channels_.assign(nch, ChannelState());
        pitched_.assign(nch, std::vector<float>(maxBlock_, 0.0f));
        pitchedPtrs_.resize(nch);
        for (int c = 0; c < nch; ++c)
            pitchedPtrs_[c] = pitched_[c].data();

        pitch_.prepare(sampleRate, maxBlock_, nch, nch);
        pitch_.setParameters("mix", 1.0f);
        setType(type_);
    }

    void Voice::setType(int type) {
        type_ = std::clamp(type, 0, TypeCount - 1);
        r_ = kRecipes[type_];
        pitch_.setParameters("octave", r_.octave);
        recalculate();
    }

    void Voice::setParameters(const std::string& param, float value) {
        if (param == "drive")      r_.driveDb = std::clamp(value, 0.0f, 40.0f);
        else if (param == "level") r_.levelDb = std::clamp(value, -24.0f, 0.0f);
        else if (param == "tone")  r_.lpfHz   = std::clamp(value, 1000.0f, 16000.0f);
        else if (param == "mix") { mix_ = std::clamp(value, 0.0f, 1.0f); return; }
        else return;
        recalculate();
    }

    void Voice::recalculate() {
        drive_ = dsp::dbToLevel(r_.driveDb);
        level_ = dsp::dbToLevel(r_.levelDb);

        const float nyquist = 0.45f * static_cast<float>(sampleRate_);
        for (ChannelState& st : channels_) {
            st.hpf.c.highpass(dsp::omega(std::min(r_.hpfHz, nyquist), sampleRate_), 0.707f);
            st.lpf.c.lowpass(dsp::omega(std::min(r_.lpfHz, nyquist), sampleRate_), 0.707f);
            st.ring.setFreq(r_.ringHz, sampleRate_);
        }
    }

    void Voice::process(const float* const* in, float* const* out, unsigned long frames) {
        if (!out || !out[0]) return;

        // Pitch first, so the character's drive and filters colour the shifted
        // voice. Recipes without a shift skip the shifter entirely.
        const bool pitched = r_.octave != 0.0f &&
                             frames <= static_cast<unsigned long>(maxBlock_) &&
                             !pitchedPtrs_.empty();
        if (pitched)
            pitch_.process(in, pitchedPtrs_.data(), frames);

        const size_t numC = channels_.size();
        for (size_t c = 0; c < numC; ++c) {
            float* outp = out[c];
            if (!outp) break;

            const float* inp = (in && in[c]) ? in[c] : nullptr;
            if (!inp) { std::fill_n(outp, frames, 0.0f); continue; }

            const float* src = pitched ? pitchedPtrs_[c] : inp;
            ChannelState& st = channels_[c];

            for (unsigned long i = 0; i < frames; ++i) {
                float y = st.hpf.step(src[i]) * drive_;
                y = dsp::foldSym(y, level_);

                if (r_.bits > 0.0f)
                    y = dsp::quantise(y, r_.bits);

                if (r_.ringHz > 0.0f) {
                    const float carrier = st.ring.step(dsp::LfoShape::Sine);
                    y = dsp::lerp(r_.ringMix, y, y * carrier);
                }

                y = st.lpf.step(y);
                outp[i] = dsp::lerp(mix_, inp[i], y);
            }
        }
    }
}
