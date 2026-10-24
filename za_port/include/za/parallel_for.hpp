#pragma once
// Executa fn(indice, thread) para indices em [0, count) distribuidos dinamicamente entre threads

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace za {

inline unsigned DefaultThreadCount() {
    const unsigned count = std::thread::hardware_concurrency();
    return count ? count : 1;
}

template <typename Fn>
void ParallelFor(size_t count, unsigned threads, Fn&& fn) {
    std::atomic<size_t> next{0};
    const auto worker = [&](unsigned thread_index) {
        for (size_t i = next++; i < count; i = next++) fn(i, thread_index);
    };
    std::vector<std::thread> pool;
    for (unsigned t = 1; t < threads; ++t) pool.emplace_back(worker, t);
    worker(0);
    for (std::thread& thread : pool) thread.join();
}

} // namespace za
