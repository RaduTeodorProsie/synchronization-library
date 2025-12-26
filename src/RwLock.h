#ifndef RWLOCK_H
#define RWLOCK_H
#include "LockGuard.h"
#include <condition_variable>
#include <mutex>
#include <new>

class RwLock {
  constexpr static size_t cacheLineSize =
      std::hardware_destructive_interference_size;

  alignas(cacheLineSize) std::mutex revolvingDoor;
  alignas(cacheLineSize) std::mutex data;
  alignas(cacheLineSize) std::mutex counter;
  alignas(cacheLineSize) size_t activeReaders = 0;

public:
  void lockRead();
  void unlockRead();
  void lockWrite();
  void unlockWrite();
};

#endif // RWLOCK_H
