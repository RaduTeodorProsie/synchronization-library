#include "RwLock.h"

void RwLock::lockRead() {
  std::scoped_lock lock(revolvingDoor, counter);
  if (activeReaders == 0) {
    data.lock();
  }

  activeReaders++;
}

void RwLock::unlockRead() {
  LockGuard<std::mutex> lock(counter);
  if (activeReaders == 1) {
    data.unlock();
  }

  activeReaders--;
}

void RwLock::lockWrite() {
  revolvingDoor.lock();
  data.lock();
}

void RwLock::unlockWrite() {
  data.unlock();
  revolvingDoor.unlock();
}
