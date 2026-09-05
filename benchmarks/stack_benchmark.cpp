#include "Backoff.h"
#include "TreiberStack.h"
#include <benchmark/benchmark.h>
#include <mutex>
#include <stack>

// Stand-in for whatever the caller does between stack operations.
//
// With no work at all, the benchmark measures nothing but a fight over one
// cache line, and that is the single workload a parking mutex wins outright: it
// is unfair, so it hands the lock straight back to the thread that just
// released it, and the lock word and the top of the stack stay in that core's
// L1 for a whole run of iterations. One cache-line transfer covers many
// operations. A lock-free stack has no equivalent trick — its head has to
// migrate on every successful CAS — so at zero work it is pinned to the cost
// of a contended CAS, which on a 10-core machine is an order of magnitude more
// than a mutex operation that never leaves L1.
//
// The interesting question is therefore not "which is faster" but "how much
// work has to sit between operations before the mutex stops being able to
// batch", which is what sweeping this parameter shows.
static void doWork(int units) {
  for (int i = 0; i < units; ++i) {
    benchmark::DoNotOptimize(i);
    cpuRelax();
  }
}

// --- Lock-free TreiberStack ---
static void BM_TreiberStack_PushPop(benchmark::State& state) {
  static TreiberStack<int> stack;
  const int work = static_cast<int>(state.range(0));
  for (auto _ : state) {
    stack.push(1);
    doWork(work);
    benchmark::DoNotOptimize(stack.pop());
    doWork(work);
  }
}
BENCHMARK(BM_TreiberStack_PushPop)
    ->ArgsProduct({{0, 100, 500}})
    ->ThreadRange(1, 16)
    ->UseRealTime();

// --- std::stack behind a std::mutex ---
static void BM_MutexStack_PushPop(benchmark::State& state) {
  static std::stack<int> stack;
  static std::mutex mtx;
  const int work = static_cast<int>(state.range(0));
  for (auto _ : state) {
    {
      std::lock_guard lock(mtx);
      stack.push(1);
    }
    doWork(work);
    {
      std::lock_guard lock(mtx);
      if (!stack.empty()) {
        benchmark::DoNotOptimize(stack.top());
        stack.pop();
      }
    }
    doWork(work);
  }
}
BENCHMARK(BM_MutexStack_PushPop)
    ->ArgsProduct({{0, 100, 500}})
    ->ThreadRange(1, 16)
    ->UseRealTime();

BENCHMARK_MAIN();
