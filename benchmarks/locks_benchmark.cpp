#include "../LockGuard.h"
#include "../SpinLock.h"
#include <benchmark/benchmark.h>
#include <mutex>
#include <thread>
#include <vector>


static void BM_SpinLock(benchmark::State &state) {
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

static void BM_StdMutex(benchmark::State &state) {
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

// --- TicketLock Benchmark ---
#include "../TicketLock.h"
static void BM_TicketLock(benchmark::State &state) {
  TicketLock ticket;
  int counter = 0;
  for (auto _ : state) {
    std::vector<std::jthread> threads;
    threads.reserve(state.range(0));
    for (int i = 0; i < state.range(0); ++i) {
      threads.emplace_back([&] {
        ticket.lock();
        counter++;
        ticket.unlock();
      });
    }
    threads.clear();
  }
}
BENCHMARK(BM_TicketLock)->Range(1, 256)->UseRealTime();

// --- RwLock Benchmarks ---
#include "../RwLock.h"
#include <shared_mutex>

// Read Heavy: 90% reads, 10% writes
static void BM_RwLock_ReadHeavy(benchmark::State &state) {
  RwLock rwlock;
  int data = 0;
  for (auto _ : state) {
    std::vector<std::jthread> threads;
    threads.reserve(state.range(0));
    for (int i = 0; i < state.range(0); ++i) {
      threads.emplace_back([&, i] {
        if (i % 10 == 0) { // 10% writers
          rwlock.lockWrite();
          data++;
          rwlock.unlockWrite();
        } else { // 90% readers
          rwlock.lockRead();
          volatile int val = data;
          (void)val;
          rwlock.unlockRead();
        }
      });
    }
    threads.clear();
  }
}
BENCHMARK(BM_RwLock_ReadHeavy)->Range(1, 256)->UseRealTime();

static void BM_StdSharedMutex_ReadHeavy(benchmark::State &state) {
  std::shared_mutex rwlock;
  int data = 0;
  for (auto _ : state) {
    std::vector<std::jthread> threads;
    threads.reserve(state.range(0));
    for (int i = 0; i < state.range(0); ++i) {
      threads.emplace_back([&, i] {
        if (i % 10 == 0) { // 10% writers
          std::unique_lock lock(rwlock);
          data++;
        } else { // 90% readers
          std::shared_lock lock(rwlock);
          volatile int val = data;
          (void)val;
        }
      });
    }
    threads.clear();
  }
}
BENCHMARK(BM_StdSharedMutex_ReadHeavy)->Range(1, 256)->UseRealTime();

BENCHMARK_MAIN();
