#pragma once

#include <vector>
#include <functional>
#include <thread>
#include <algorithm>

template <typename T>
void ApplyFunction(std::vector<T>& data, const std::function<void(T&)>& transform,
                   const int threadCount = 1) {
    if (data.empty()) {
        return;
    }

    int effectiveThreads = std::min(threadCount, static_cast<int>(data.size()));

    if (effectiveThreads <= 1) {
        for (auto& elem : data) {
            transform(elem);
        }
        return;
    }

    std::vector<std::thread> threads;
    threads.reserve(effectiveThreads);

    size_t chunkSize = data.size() / effectiveThreads;
    size_t remainder = data.size() % effectiveThreads;

    size_t start = 0;
    for (int i = 0; i < effectiveThreads; ++i) {
        size_t end = start + chunkSize + (i < static_cast<int>(remainder) ? 1 : 0);
        threads.emplace_back([&data, &transform, start, end]() {
            for (size_t j = start; j < end; ++j) {
                transform(data[j]);
            }
        });
        start = end;
    }

    for (auto& t : threads) {
        t.join();
    }
}
