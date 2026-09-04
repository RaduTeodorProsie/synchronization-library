#include "RwLock.h"

void RwLock::lockRead() {
  std::scoped_lock lock(revolvingDoor, counter);
  if (activeReaders == 0) {
    data.acquire();
  }

  activeReaders++;
}

void RwLock::unlockRead() {
  LockGuard<std::mutex> lock(counter);
  if (activeReaders == 1) {
    data.release();
  }

  activeReaders--;
}

void RwLock::lockWrite() {
  revolvingDoor.lock();
  data.acquire();
}

void RwLock::unlockWrite() {
  data.release();
  revolvingDoor.unlock();
}
