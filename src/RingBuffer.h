#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <atomic>
#include <new>
#include <stdexcept>
#include <vector>

template <typename Data>
  requires std::is_default_constructible_v<Data>
class RingBuffer {

  constexpr static size_t cacheLineSize =
      std::hardware_destructive_interference_size;

  alignas(cacheLineSize) std::atomic<size_t> head{0};
  size_t cachedTail{0};

  alignas(cacheLineSize) std::atomic<size_t> tail{0};
  size_t cachedHead{0};

  alignas(cacheLineSize) std::vector<Data> data;

  size_t next(size_t indx) {
    indx++;
    if (indx >= data.size())
      indx -= data.size();
    return indx;
  }

public:
  explicit RingBuffer(const size_t &capacity) { data.resize(capacity + 1); }

  template <typename T>
    requires std::is_assignable_v<Data &, T>
  bool push(T &&cargo) noexcept(std::is_nothrow_assignable_v<Data &, T>) {
    size_t myHead = head.load(std::memory_order_relaxed);

    if (next(myHead) == cachedTail) {
      cachedTail = tail.load(std::memory_order_acquire);
      if (next(myHead) == cachedTail)
        return false;
    }

    data[myHead] = std::forward<T>(cargo);
    head.store(next(myHead), std::memory_order_release);

    return true;
  }

  Data pop() {
    size_t myTail = tail.load(std::memory_order_relaxed);

    if (cachedHead == myTail) {
      cachedHead = head.load(std::memory_order_acquire);
      if (cachedHead == myTail) {
        throw std::runtime_error("Ringbuffer is empty, cannot pop from it");
      }
    }

    Data result = std::move(data[myTail]);
    tail.store(next(myTail), std::memory_order_release);

    return result;
  }
};

#endif // RINGBUFFER_H
