#include "TreiberStack.h"
#include <benchmark/benchmark.h>
#include <mutex>
#include <stack>

// --- Lock-free TreiberStack ---
static void BM_TreiberStack_PushPop(benchmark::State &state) {
  static TreiberStack<int> stack;
  for (auto _ : state) {
    stack.push(1);
    benchmark::DoNotOptimize(stack.pop());
  }
}
BENCHMARK(BM_TreiberStack_PushPop)->ThreadRange(1, 16)->UseRealTime();

// --- std::stack behind a std::mutex ---
static void BM_MutexStack_PushPop(benchmark::State &state) {
  static std::stack<int> stack;
  static std::mutex mtx;
  for (auto _ : state) {
    {
      std::lock_guard lock(mtx);
      stack.push(1);
    }
    std::lock_guard lock(mtx);
    if (!stack.empty()) {
      benchmark::DoNotOptimize(stack.top());
      stack.pop();
    }
  }
}
BENCHMARK(BM_MutexStack_PushPop)->ThreadRange(1, 16)->UseRealTime();

BENCHMARK_MAIN();
