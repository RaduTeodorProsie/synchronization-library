#include "LockGuard.h"
#include "SpinLock.h"
#include <benchmark/benchmark.h>
#include <mutex>
#include <thread>
#include <vector>

// --- SpinLock Benchmark ---
static void BM_SpinLock(benchmark::State &state) {
  static SpinLock spinlock;
  for (auto _ : state) {
    LockGuard<SpinLock> guard(spinlock);
    benchmark::DoNotOptimize(0);
  }
}
BENCHMARK(BM_SpinLock)->ThreadRange(1, 16)->UseRealTime();

// --- StdMutex Benchmark ---
static void BM_StdMutex(benchmark::State &state) {
  static std::mutex mtx;
  for (auto _ : state) {
    std::lock_guard<std::mutex> guard(mtx);
    benchmark::DoNotOptimize(0);
  }
}
BENCHMARK(BM_StdMutex)->ThreadRange(1, 256)->UseRealTime();

// --- TicketLock Benchmark ---
#include "TicketLock.h"
static void BM_TicketLock(benchmark::State &state) {
  static TicketLock ticket;
  for (auto _ : state) {
    ticket.lock();
    benchmark::DoNotOptimize(0);
    ticket.unlock();
  }
}
BENCHMARK(BM_TicketLock)->ThreadRange(1, 16)->UseRealTime();

// --- RwLock Benchmarks ---
#include "RwLock.h"
#include <shared_mutex>

static void BM_RwLock_ReadHeavy(benchmark::State &state) {
  static RwLock rwlock;
  static int data = 0;

  if (state.thread_index() == 0) { // Single Writer
    for (auto _ : state) {
      rwlock.lockWrite();
      data++;
      rwlock.unlockWrite();
    }
  } else { // Readers
    for (auto _ : state) {
      rwlock.lockRead();
      int val = data;
      benchmark::DoNotOptimize(val);
      rwlock.unlockRead();
    }
  }
}
BENCHMARK(BM_RwLock_ReadHeavy)->ThreadRange(1, 256)->UseRealTime();

static void BM_StdSharedMutex_ReadHeavy(benchmark::State &state) {
  static std::shared_mutex rwlock;
  static int data = 0;

  if (state.thread_index() == 0) { // Single Writer
    for (auto _ : state) {
      std::unique_lock lock(rwlock);
      data++;
    }
  } else { // Readers
    for (auto _ : state) {
      std::shared_lock lock(rwlock);
      int val = data;
      benchmark::DoNotOptimize(val);
    }
  }
}
BENCHMARK(BM_StdSharedMutex_ReadHeavy)->ThreadRange(1, 256)->UseRealTime();

// --- SeqLock Benchmark ---
#include "SeqLock.h"
static void BM_SeqLock_ReadHeavy(benchmark::State &state) {
  static SeqLock<int> seqlock;

  if (state.thread_index() == 0) { // Single Writer
    for (auto _ : state) {
      seqlock.write([](int &data) { data++; });
    }
  } else { // Readers
    for (auto _ : state) {
      int val = seqlock.read();
      benchmark::DoNotOptimize(val);
    }
  }
}
BENCHMARK(BM_SeqLock_ReadHeavy)->ThreadRange(1, 256)->UseRealTime();

BENCHMARK_MAIN();
