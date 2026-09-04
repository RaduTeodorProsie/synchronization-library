#ifndef RWLOCK_H
#define RWLOCK_H
#include "LockGuard.h"
#include <mutex>
#include <new>
#include <semaphore>

class RwLock {
  constexpr static size_t cacheLineSize =
      std::hardware_destructive_interference_size;

  alignas(cacheLineSize) std::mutex revolvingDoor;
  // Semaphore, not a mutex: acquired by the first reader, released by the last.
  alignas(cacheLineSize) std::binary_semaphore data{1};
  alignas(cacheLineSize) std::mutex counter;
  alignas(cacheLineSize) size_t activeReaders = 0;

public:
  void lockRead();
  void unlockRead();
  void lockWrite();
  void unlockWrite();
};

#endif // RWLOCK_H
