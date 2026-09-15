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

#ifndef PCORE_EFFECTS_EQ_H
#define PCORE_EFFECTS_EQ_H

#include "../audio_node.h"
#include "dsp.h"

#include <string>
#include <vector>

namespace PCore {
    //
    // Ten-band graphic EQ on octave centres from 31.25Hz to 16kHz: a low
    // shelf, eight peaking bands and a high shelf. Bands that disagree with
    // their neighbours get a higher Q, so a lone boost stays narrow while a
    // whole tilted curve stays smooth.
    //
    class Eq : public AudioNode {
    public:
        static const int NUM_BANDS = 10;

        explicit Eq(int sampleRate);
        virtual ~Eq() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        // Gain in dB, -20 .. +20. Band 0 is 31.25Hz, band 9 is 16kHz.
        void setBandGain(int band, float db);
        float bandFrequency(int band) const;

        // "band0".."band9", plus "low"/"mid"/"high" for the shelves and 1kHz.
        void setParameters(const std::string& param, float value);

    private:
        int sampleRate_;

        float gainsDb_[NUM_BANDS] = {};

        struct ChannelState {
            dsp::Biquad bands[NUM_BANDS];
        };
        std::vector<ChannelState> channels_;

        dsp::BiquadCoeff coeff_[NUM_BANDS];

        void recalculate();
    };
}

#endif // PCORE_EFFECTS_EQ_H
