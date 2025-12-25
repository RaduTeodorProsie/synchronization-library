#ifndef RWLOCK_H
#define RWLOCK_H
#include "LockGuard.h"
#include <condition_variable>
#include <mutex>


class RwLock {
  std::mutex revolvingDoor;
  std::mutex data;
  std::mutex counter;
  size_t activeReaders = 0;

public:
  void lockRead();
  void unlockRead();
  void lockWrite();
  void unlockWrite();
};

#endif // RWLOCK_H
