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

#ifndef PCORE_RT_RING_BUFFER_H
#define PCORE_RT_RING_BUFFER_H

#include <vector>
#include <atomic>
#include <cstddef>

// Single-producer single-consumer lock-free ring buffer (power-of-two capacity preferred).
template<typename T>
class SpscRing {
    public:
        explicit SpscRing(size_t capacity)
            : buf_(capacity), mask_(capacity - 1), head_(0), tail_(0) {}
        bool push(const T& v) {
            auto h = head_.load(std::memory_order_relaxed);
            auto n = (h + 1) & mask_;
            if (n == tail_.load(std::memory_order_acquire)) return false; // full
            buf_[h] = v;
            head_.store(n, std::memory_order_release);
            return true;
        }
        bool pop(T& v) {
            auto t = tail_.load(std::memory_order_relaxed);
            if (t == head_.load(std::memory_order_acquire)) return false; // empty
            v = buf_[t];
            tail_.store((t + 1) & mask_, std::memory_order_release);
            return true;
        }
        void clear() {
            tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
    private:
        std::vector<T> buf_;
        size_t mask_;
        std::atomic<size_t> head_, tail_;
};

#endif // PCORE_RT_RING_BUFFER_H
