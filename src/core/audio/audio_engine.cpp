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

#include <algorithm>
#include <cstring>
#include <memory>

#include "audio_engine.h"
#include "vocal_chain.h"

#define AUDIO_ENGINE_DEBUG 0

namespace PCore {
    AudioEngine::AudioEngine(int sr, int block, int inCh, int outCh) : sampleRate_(sr), blockSize_(block), inChans_(inCh), outChans_(outCh), ringIn_( 1024) , ringOut_(1024) {
        engineInBuf_.resize(static_cast<size_t>(blockSize_) * std::max(1, inChans_));
        engineOutBuf_.resize(static_cast<size_t>(blockSize_) * std::max(1, outChans_));
        inPtrs_.resize(std::max(1, inChans_), nullptr);
        outPtrs_.resize(std::max(1, outChans_), nullptr);

        // The Vocal 300 signal path lives in VocalChain; it owns every block
        // and picks exactly one Effects type, rather than stacking them.
        chain_ = std::make_unique<VocalChain>(sampleRate_);
        chain_->prepare(sampleRate_, blockSize_, inChans_, outChans_);
    }

    AudioEngine::~AudioEngine() {
        stop();
    }

    void AudioEngine::start() {
        if (running_.exchange(true)) return;
        engineThread_ = std::thread(&AudioEngine::engineThreadMain, this);
    }

    void AudioEngine::stop() {
        if (!running_.exchange(false)) return;
        if (engineThread_.joinable()) engineThread_.join();
    }

    // Real-time callback called by the driver
    IO::CallbackResult AudioEngine::driverCallback(const float* const *in, float* const *out, unsigned long frames, void *userData) {
        auto *self = static_cast<AudioEngine*>(userData);

        if (!self) {
            // zero outputs if possible
            if (out) {
                for (int c = 0; c < (self ? self->outChans_ : 2); c++) {
                    if (out[c]) std::memset(out[c], 0, frames * sizeof(float));
                }
            }
            return IO::CallbackResult::Continue;
        }

        // Process directly via graph (simple model). If you use ring buffers, adapt accordingly.
        if (self->chain_) {
            self->chain_->process(in, out, frames);
        } else {
            // passthrough or zero
            if (out && in) {
                int chans = std::min(self->outChans_, self->inChans_);
                for (int c = 0; c < chans; ++c) {
                    if (out[c] && in[c]) {
                        std::memcpy(out[c], in[c], frames * sizeof(float));
                    } else if (out[c]) {
                        std::memset(out[c], 0, frames * sizeof(float));
                    }
                }
                for (int c = chans; c < self->outChans_; ++c) {
                    if (out[c]) std::memset(out[c], 0, frames * sizeof(float));
                }
            } else if (out) {
                for (int c = 0; c < self->outChans_; ++c) {
                    if (out[c]) std::memset(out[c], 0, frames * sizeof(float));
                }
            }
        }

        return IO::CallbackResult::Continue;
    }

    void AudioEngine::engineThreadMain() {
        // If you use ring buffers for non-RT processing, implement them here.
        // For now, this thread can be idle or collect metrics.
        while (running_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
