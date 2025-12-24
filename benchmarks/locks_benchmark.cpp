#include <benchmark/benchmark.h>
#include "../SpinLock.h"
#include "../LockGuard.h"
#include <vector>
#include <thread>
#include <mutex>

static void BM_SpinLock(benchmark::State& state) {
    SpinLock spinlock;
    int counter = 0;
    for (auto _ : state) {
        std::vector<std::jthread> threads;
        threads.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            threads.emplace_back([&] {
                LockGuard<SpinLock> guard(spinlock);
                counter++;
            });
        }
        threads.clear(); 
    }
}

// Register the function as a benchmark
BENCHMARK(BM_SpinLock)->Range(1, 256)->UseRealTime();

static void BM_StdMutex(benchmark::State& state) {
    std::mutex mtx;
    int counter = 0;
    for (auto _ : state) {
        std::vector<std::jthread> threads;
        threads.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            threads.emplace_back([&] {
                std::lock_guard<std::mutex> guard(mtx);
                counter++;
            });
        }
        threads.clear();
    }
}
BENCHMARK(BM_StdMutex)->Range(1, 256)->UseRealTime();

BENCHMARK_MAIN();
