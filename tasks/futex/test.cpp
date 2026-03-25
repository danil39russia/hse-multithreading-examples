#include "futex_mutex.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

int main() {
    std::cout << "[Scena 1] Stress increment with lock_guard..." << std::endl;
    FutexMutex mutex;
    int counter = 0;

    constexpr int kThreads = 8;
    constexpr int kIncrementsPerThread = 200000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < kIncrementsPerThread; ++j) {
                std::lock_guard<FutexMutex> guard(mutex);
                ++counter;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(counter == kThreads * kIncrementsPerThread);
    std::cout << "  counter=" << counter << " OK" << std::endl;

    std::cout << "[Scena 2] try_lock should fail when already locked..." << std::endl;
    std::atomic<bool> try_lock_failed{false};
    std::atomic<bool> acquired_after_unlock{false};

    mutex.lock();
    std::thread checker([&]() {
        if (!mutex.try_lock()) {
            try_lock_failed.store(true, std::memory_order_relaxed);
        }

        while (!mutex.try_lock()) {
            std::this_thread::yield();
        }

        acquired_after_unlock.store(true, std::memory_order_relaxed);
        mutex.unlock();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mutex.unlock();
    checker.join();

    assert(try_lock_failed.load(std::memory_order_relaxed));
    assert(acquired_after_unlock.load(std::memory_order_relaxed));
    std::cout << "  try_lock behavior OK" << std::endl;
    std::cout << "All futex mutex checks passed." << std::endl;
    return 0;
}
