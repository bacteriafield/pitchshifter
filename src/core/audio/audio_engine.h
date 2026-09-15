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

#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <thread>
#include <atomic>
#include <vector>
#include <memory>

#include "../rt/ring_buffer.h"
#include "../rt/audio_block.h"
#include "../io/audio_driver.h"
#include "audio_graph.h"
#include "vocal_chain.h"

namespace PCore {
    class AudioEngine {
        private:
            void engineThreadMain();

            int sampleRate_, blockSize_, inChans_, outChans_;
            std::atomic<bool> running_{false};
            std::thread engineThread_;
            SpscRing<AudioBlock> ringIn_;
            SpscRing<std::vector<float>> ringOut_; // contains interleaved or deinterleaved frames ready
            std::unique_ptr<VocalChain> chain_;

            // Work buffers (preallocated)
            std::vector<float> engineInBuf_;
            std::vector<float> engineOutBuf_;
            std::vector<const float*> inPtrs_;
            std::vector<float*> outPtrs_;

            std::atomic<uint64_t> xruns_{0};
            std::atomic<double> cpuLoad_{0.0};
            uint64_t frameCounter_ = 0;
        public:
            explicit AudioEngine(int sampleRate, int blockSize, int inChans, int outChans);
            ~AudioEngine();
           // Called by driver RT callback (hard-RT safe)
            static IO::CallbackResult driverCallback(const float* const* in, float* const* out, unsigned long frames, void* userData);

            // Engine control
            void start();
            void stop();

            // The effect chain. Set parameters on it from this thread only --
            // it queues them across to the audio callback itself.
            VocalChain& chain() { return *chain_; }

            // Diagnostics
            double cpuLoad() const { return cpuLoad_; }
            uint64_t xruns() const { return xruns_.load(std::memory_order_relaxed); }
    };
};

#endif // AUDIO_ENGINE_H