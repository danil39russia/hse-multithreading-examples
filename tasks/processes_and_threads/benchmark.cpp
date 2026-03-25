#include <benchmark/benchmark.h>

#include "apply_function.h"

#include <cmath>
#include <numeric>

static void HeavyTransform(double& x) {
    for (int i = 0; i < 50; ++i) {
        x = std::sin(x) + std::cos(x);
    }
}

static void CheapTransform(double& x) {
    x *= 2.0;
}

const int kThreads = static_cast<int>(std::thread::hardware_concurrency());

// ============================================================
// Подбор размера вектора (функция одна и та же — тяжёлая).
// Маленький вектор → 1 поток быстрее.
// Большой вектор → N потоков быстрее.
// ============================================================

static void BM_Size_Small_1Thread(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> data(50, 1.0);
        ApplyFunction<double>(data, HeavyTransform, 1);
        benchmark::DoNotOptimize(data);
    }
}

static void BM_Size_Small_NThreads(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> data(50, 1.0);
        ApplyFunction<double>(data, HeavyTransform, kThreads);
        benchmark::DoNotOptimize(data);
    }
}

static void BM_Size_Large_1Thread(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> data(500000, 1.0);
        ApplyFunction<double>(data, HeavyTransform, 1);
        benchmark::DoNotOptimize(data);
    }
}

static void BM_Size_Large_NThreads(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> data(500000, 1.0);
        ApplyFunction<double>(data, HeavyTransform, kThreads);
        benchmark::DoNotOptimize(data);
    }
}

// ============================================================
// Подбор функции transform (размер вектора один и тот же).
// Лёгкая функция → 1 поток быстрее.
// Тяжёлая функция → N потоков быстрее.
// ============================================================

static void BM_Func_Cheap_1Thread(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> data(1000, 1.0);
        ApplyFunction<double>(data, CheapTransform, 1);
        benchmark::DoNotOptimize(data);
    }
}

static void BM_Func_Cheap_NThreads(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> data(1000, 1.0);
        ApplyFunction<double>(data, CheapTransform, kThreads);
        benchmark::DoNotOptimize(data);
    }
}

static void BM_Func_Heavy_1Thread(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> data(1000, 1.0);
        ApplyFunction<double>(data, HeavyTransform, 1);
        benchmark::DoNotOptimize(data);
    }
}

static void BM_Func_Heavy_NThreads(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<double> data(1000, 1.0);
        ApplyFunction<double>(data, HeavyTransform, kThreads);
        benchmark::DoNotOptimize(data);
    }
}

BENCHMARK(BM_Size_Small_1Thread)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Size_Small_NThreads)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Size_Large_1Thread)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Size_Large_NThreads)->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Func_Cheap_1Thread)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Func_Cheap_NThreads)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Func_Heavy_1Thread)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Func_Heavy_NThreads)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
