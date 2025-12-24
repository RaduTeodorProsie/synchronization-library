#ifndef RWLOCK_H
#define RWLOCK_H
#include <condition_variable>
#include <mutex>

class RwLock {
  std::mutex revolvingDoor;
  std::mutex data;
  std::mutex counter;
  size_t activeReaders = 0;

public:
  void lockRead() {
    std::scoped_lock lock(revolvingDoor, counter);
    if (activeReaders == 0) {
      data.lock();
    }

    activeReaders++;
  }

#include "LockGuard.h"

  void unlockRead() {
    LockGuard<std::mutex> lock(counter);
    if (activeReaders == 1) {
      data.unlock();
    }

    activeReaders--;
  }

  void lockWrite() {
    revolvingDoor.lock();
    data.lock();
  }

  void unlockWrite() {
    data.unlock();
    revolvingDoor.unlock();
  }
};

#endif // RWLOCK_H
