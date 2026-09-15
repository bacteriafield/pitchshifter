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

/*
    DSP primitives ported from torvalds/AudioNoise (audio/util.h, audio/lfo.h,
    audio/biquad.h), which is licensed GPL-2.0. See NOTICE in the repo root.

    The original targets an RP2354 with no FPU trig, so it uses lookup tables
    for sin/log2/pow2. Here we just call <cmath> -- same math, less code.
*/

#ifndef PCORE_EFFECTS_DSP_H
#define PCORE_EFFECTS_DSP_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace PCore {
namespace dsp {

    constexpr float kPi    = 3.14159265358979323846f;
    constexpr float kTwoPi = 6.283185307179586f;

    // Turn 0..1 into a range (AudioNoise's `linear` macro)
    inline float lerp(float t, float a, float b) { return a + t * (b - a); }

    // Smoothly limit x to -1 .. 1
    inline float limit(float x) { return x / (1.0f + std::fabs(x)); }

    inline float dbToLevel(float db) { return std::pow(10.0f, db * 0.05f); }

    // Half-time coefficient: 0.5 ^ (1 / samples) for a `ms` millisecond half-life
    inline float timeConstant(float ms, int sampleRate) {
        if (ms < 0.01f) ms = 0.01f;
        return std::exp2(-1000.0f / (ms * static_cast<float>(sampleRate)));
    }

    //
    // AudioNoise's wavefolder: reflect everything above `level` back under it.
    // Bounded at 32 passes -- each pass halves the overshoot so it converges
    // long before that. The cap only stops a bad `level` spinning forever
    // inside the audio callback.
    //
    inline float foldAbove(float in, float level) {
        for (int i = 0; i < 32; ++i) {
            float over = (in - level) * 0.5f;
            in = level - over;
            if (in >= -level) return in;

            over = (in + level) * 0.5f;
            in = -level - over;
            if (in <= level) return in;
        }
        return std::clamp(in, -level, level);
    }

    // The same curve applied symmetrically around zero.
    inline float foldSym(float x, float level) {
        if (level < 1e-4f) level = 1e-4f;
        if (x >  level) return  foldAbove( x, level);
        if (x < -level) return -foldAbove(-x, level);
        return x;
    }

    // Bit-depth reduction. `bits` is the effective word length, 1 .. 16.
    inline float quantise(float x, float bits) {
        const float steps = std::exp2(std::clamp(bits, 1.0f, 16.0f)) * 0.5f;
        return std::round(x * steps) / steps;
    }

    struct SinCos { float sin, cos; };

    // Normalised angular frequency of `freq`, as sin/cos pair
    inline SinCos omega(float freq, int sampleRate) {
        const float a = kTwoPi * freq / static_cast<float>(sampleRate);
        return { std::sin(a), std::cos(a) };
    }

    //
    // RBJ cookbook biquad coefficients. Coefficients are kept apart from the
    // history so the phaser can run one shared coefficient set over a chain of
    // states, where each stage's output history is the next stage's input.
    //
    struct BiquadCoeff {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;

        // Direct form 1 needs more state than the canonical DF2, but it gets
        // noisy far more slowly when the coefficients move every sample.
        float step(float in, float x[2], float y[2]) const {
            const float out = b0 * in + b1 * x[0] + b2 * x[1] - a1 * y[0] - a2 * y[1];
            x[1] = x[0]; x[0] = in;
            y[1] = y[0]; y[0] = out;
            return out;
        }

        void lowpass(SinCos w, float Q) {
            const float alpha  = w.sin / (2.0f * Q);
            const float a0_inv = 1.0f / (1.0f + alpha);
            const float b = (1.0f - w.cos) * a0_inv;
            b0 = b * 0.5f; b1 = b; b2 = b * 0.5f;
            a1 = -2.0f * w.cos * a0_inv;
            a2 = (1.0f - alpha) * a0_inv;
        }

        void highpass(SinCos w, float Q) {
            const float alpha  = w.sin / (2.0f * Q);
            const float a0_inv = 1.0f / (1.0f + alpha);
            const float b = (1.0f + w.cos) * a0_inv;
            b0 = b * 0.5f; b1 = -b; b2 = b * 0.5f;
            a1 = -2.0f * w.cos * a0_inv;
            a2 = (1.0f - alpha) * a0_inv;
        }

        void allpass(SinCos w, float Q) {
            const float alpha  = w.sin / (2.0f * Q);
            const float a0_inv = 1.0f / (1.0f + alpha);
            b0 = (1.0f - alpha) * a0_inv;
            b1 = (-2.0f * w.cos) * a0_inv;
            b2 = 1.0f;              // same as a0
            a1 = b1;
            a2 = b0;
        }

        void peaking(SinCos w, float Q, float gain) {
            const float A      = std::sqrt(gain);
            const float alpha  = w.sin / (2.0f * Q);
            const float a0_inv = 1.0f / (1.0f + alpha / A);
            b0 = (1.0f + alpha * A) * a0_inv;
            b1 = (-2.0f * w.cos) * a0_inv;
            b2 = (1.0f - alpha * A) * a0_inv;
            a1 = b1;
            a2 = (1.0f - alpha / A) * a0_inv;
        }

        void lowShelf(SinCos w, float Q, float gain) {
            const float A     = std::sqrt(gain);
            const float alpha = w.sin / (2.0f * Q);
            const float ap1 = A + 1.0f, am1 = A - 1.0f;
            const float sq2a  = 2.0f * std::sqrt(A) * alpha;
            const float a0_inv = 1.0f / (ap1 + am1 * w.cos + sq2a);
            b0 =        A * (ap1 - am1 * w.cos + sq2a) * a0_inv;
            b1 = 2.0f * A * (am1 - ap1 * w.cos)        * a0_inv;
            b2 =        A * (ap1 - am1 * w.cos - sq2a) * a0_inv;
            a1 =     -2.0f * (am1 + ap1 * w.cos)       * a0_inv;
            a2 =             (ap1 + am1 * w.cos - sq2a) * a0_inv;
        }

        void highShelf(SinCos w, float Q, float gain) {
            const float A     = std::sqrt(gain);
            const float alpha = w.sin / (2.0f * Q);
            const float ap1 = A + 1.0f, am1 = A - 1.0f;
            const float sq2a  = 2.0f * std::sqrt(A) * alpha;
            const float a0_inv = 1.0f / (ap1 - am1 * w.cos + sq2a);
            b0 =         A * (ap1 + am1 * w.cos + sq2a) * a0_inv;
            b1 = -2.0f * A * (am1 + ap1 * w.cos)        * a0_inv;
            b2 =         A * (ap1 + am1 * w.cos - sq2a) * a0_inv;
            a1 =      2.0f * (am1 - ap1 * w.cos)        * a0_inv;
            a2 =             (ap1 - am1 * w.cos - sq2a) * a0_inv;
        }
    };

    // Coefficients plus their own history, for the common one-filter case.
    struct Biquad {
        BiquadCoeff c;
        float x[2] = { 0.0f, 0.0f };
        float y[2] = { 0.0f, 0.0f };

        float step(float in) { return c.step(in, x, y); }
        void reset() { x[0] = x[1] = y[0] = y[1] = 0.0f; }
    };

    enum class LfoShape { Sine, Triangle, Sawtooth };

    //
    // 32-bit phase accumulator. The top two bits pick the quarter, so one
    // quarter-wave (0..1) turns into [ 0..1, 1..0, 0..-1, -1..0 ].
    //
    struct Lfo {
        uint32_t phase = 0;
        uint32_t inc   = 0;

        void setFreq(float hz, int sampleRate) {
            const double fstep = 4294967296.0 / static_cast<double>(sampleRate);
            inc = static_cast<uint32_t>(std::lround(hz * fstep));
        }

        // Period in milliseconds (capped at 10kHz, like the original)
        void setPeriodMs(float ms, int sampleRate) {
            if (ms < 0.1f) ms = 0.1f;
            setFreq(1000.0f / ms, sampleRate);
        }

        void reset() { phase = 0; }

        float step(LfoShape shape) {
            const uint32_t now = phase;
            phase = now + inc;

            if (shape == LfoShape::Sawtooth)
                return fraction(now);

            const uint32_t quarter = now >> 30;
            uint32_t q = now << 2;

            // Second and fourth quarter run backwards
            if (quarter & 1) q = ~q;

            float val = fraction(q);
            if (shape == LfoShape::Sine)
                val = std::sin(val * (kPi * 0.5f));

            // Last two quarters are negative
            return (quarter & 2) ? -val : val;
        }

    private:
        static float fraction(uint32_t v) { return v * (1.0f / 4294967296.0f); }
    };

    //
    // Power-of-two circular buffer with linear interpolation on read.
    // The index points at the newest sample, so read(0) is what was just
    // written and read(n) is n samples back.
    //
    class DelayLine {
    public:
        void resize(size_t minSamples) {
            size_t n = 2;
            while (n < minSamples) n <<= 1;
            buf_.assign(n, 0.0f);
            mask_ = static_cast<unsigned>(n - 1);
            idx_  = 0;
        }

        void clear() {
            std::fill(buf_.begin(), buf_.end(), 0.0f);
            idx_ = 0;
        }

        size_t size() const { return buf_.size(); }

        void write(float v) {
            idx_ = (idx_ + 1) & mask_;
            buf_[idx_] = v;
        }

        float read(float delay) const {
            const float maxDelay = static_cast<float>(buf_.size() - 2);
            delay = std::clamp(delay, 0.0f, maxDelay);

            const int i = static_cast<int>(delay);
            const float frac = delay - static_cast<float>(i);
            const unsigned a = (idx_ - static_cast<unsigned>(i)) & mask_;
            const unsigned b = (a - 1u) & mask_;
            return lerp(frac, buf_[a], buf_[b]);
        }

    private:
        std::vector<float> buf_ = std::vector<float>(2, 0.0f);
        unsigned idx_  = 0;
        unsigned mask_ = 1;
    };

} // namespace dsp
} // namespace PCore

#endif // PCORE_EFFECTS_DSP_H
