#include "RingBuffer.h"
#include <benchmark/benchmark.h>
#include <thread>
#include <vector>

static void BM_RingBuffer_Throughput(benchmark::State &state) {
  for (auto _ : state) {
    RingBuffer<int> buf(1024);
    const int count = state.range(0);

    std::jthread consumer([&] {
      for (int i = 0; i < count; ++i) {
        for (;;) {
          try {
            int val = buf.pop();
            (void)val;
            break;
          } catch (...) {
            // spin or yield
            std::atomic_thread_fence(std::memory_order_acquire);
          }
        }
      }
    });

    for (int i = 0; i < count; ++i) {
      while (!buf.push(i)) {
        std::atomic_thread_fence(std::memory_order_acquire);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_RingBuffer_Throughput)->Range(1024, 65536)->UseRealTime();

BENCHMARK_MAIN();
