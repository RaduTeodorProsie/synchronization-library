#ifndef SEQLOCK_H
#define SEQLOCK_H

#include <atomic>
#include <cstring>
#include <new>
#include <type_traits>

template <typename T>
  requires std::is_trivially_copyable_v<T> &&
           std::is_trivially_destructible_v<T> &&
           std::is_default_constructible_v<T>
class SeqLock {
  constexpr static size_t cacheLineSize =
      std::hardware_destructive_interference_size;

  T data;
  alignas(cacheLineSize) std::atomic<size_t> counter = 0;

public:
  T read() {
    T result;

    for (;;) {
      size_t r1 = counter.load(std::memory_order_acquire);
      if (r1 & 1)
        continue;

      // May tear; the counter re-check below is what makes it trustworthy.
      std::memcpy(&result, &data, sizeof(T));
      std::atomic_thread_fence(std::memory_order_acq_rel);
      size_t r2 = counter.load(std::memory_order_relaxed);
      if (r1 == r2)
        return result;
    }
  }

  // Single writer only.
  template <typename Function>
    requires std::is_invocable_v<Function, T &> &&
             std::is_same_v<std::invoke_result_t<Function, T &>, void>
  void write(Function &&func) {
    counter.fetch_add(1, std::memory_order_acq_rel);

    func(data);

    counter.fetch_add(1, std::memory_order_acq_rel);
  }
};

#endif // SEQLOCK_H
