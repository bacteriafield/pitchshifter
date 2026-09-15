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

#ifndef CHORUS_H
#define CHORUS_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

#include "../audio_node.h"


namespace PCore {
    class Chorus : public AudioNode {
        private:
            int sampleRate_ = 44100;
            float lfoPhase_ = 0.0f;     // [rad]
            float lfoRate_  = 0.8f;     // Hz
            float depthMs_  = 8.0f;     // delay variation (ms)
            float mix_      = 0.3f;    // 0..1
            int   baseDelayMs_ = 10;    // ms

            std::vector<float> dBuffer_; // buffer de delay (circle)
            size_t writePos_ = 0;

        public:
            explicit Chorus(int sampleRate);
            void prepare(int sr, int block, int inCh, int outCh) override;
            void process(const float* const* in, float* const* out, unsigned long frames) override;
            void setParameters(const std::string &param, float value);

            // Helper
            inline size_t msToSamples(float ms) const {
                return static_cast<size_t>(std::max(1, (int)std::lround(ms * 0.001f * sampleRate_)));
            }
    };
}
#endif // CHORUS_H