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


#include "reverb.h"
#include <cstddef>
#include <algorithm>

namespace PCore {
	static constexpr float kMaxTimeS = 1.5f;
    Reverb::Reverb(int sampleRate) : sampleRate_(sampleRate) {}

    void Reverb::prepare(int sr, int block, int inCh, int outCh) {
		sampleRate_ = sr;

		// Sized for the longest time_s up front, so setParameters never
		// reallocates on the audio thread.
		const double maxMul = std::max({ 1.0, static_cast<double>(t2mul_), static_cast<double>(t3mul_) });
		const size_t total = std::max<size_t>(secToSamples(static_cast<float>(kMaxTimeS * maxMul)) + 8, 1024);
		dBuffer_.assign(total, 0.0f);
		writePos_ = 0;
	}

	void Reverb::process(const float* const *in, float* const *out, unsigned long frames) {
		if (!out || !out[0]) return;

		const float* x = (in && in[0]) ? in[0] : nullptr;
		float* y = out[0];

		if (!x) { std::fill_n(y, frames, 0.0f); return; }

		const size_t N = dBuffer_.size();
		if (N == 0) { std::copy(x, x + frames, y); return; }

		const size_t d1 = secToSamples(baseTime_);
		const size_t d2 = secToSamples(baseTime_ * t2mul_);
		const size_t d3 = secToSamples(baseTime_ * t3mul_);

		size_t wp = writePos_;

		// Mask to wrap fast when N be potency of 2; if not, using %N
		const bool pow2 = (N & (N - 1)) == 0;
		auto wrap = [&](size_t idx) -> size_t {
			return pow2 ? (idx & (N - 1)) : (idx % N);
		};

		for (unsigned long i = 0; i < frames; ++i) {
			const float inS = x[i];

			// Taps of delay
			float dly1 = dBuffer_[wrap(wp + N - d1)];
			float dly2 = dBuffer_[wrap(wp + N - d2)];
			float dly3 = dBuffer_[wrap(wp + N - d3)];

			// Balanced sum of taps (can adjust weights)
			float rev = (dly1 + 0.7f * dly2 + 0.5f * dly3) / 3.0f;

			// Global feedback simple (Tank)
			dBuffer_[wp] = inS + decay_ * rev;

			// Mix dry/wet
			y[i] = (1.0f - mix_) * inS + mix_ * rev;

			wp = (wp + 1) % N;
		}

		writePos_ = wp;
	}

	void Reverb::setParameters(const std::string& param, float value) {
		if (param == "decay") {
			decay_ = std::clamp(value, 0.0f, 0.99f);
		} else if (param == "mix") {
			mix_ = std::clamp(value, 0.0f, 1.0f);
		} else if (param == "time_s") {
			baseTime_ = std::clamp(value, 0.05f, kMaxTimeS);
		}
	}
};