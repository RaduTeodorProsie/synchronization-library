#include "../LockGuard.h"
#include "../SpinLock.h"
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>


// Test SpinLock basic locking and unlocking
TEST(SpinLockTest, BasicLockUnlock) {
  SpinLock spinlock;
  spinlock.lock();
  spinlock.unlock();
}

// Test Mutual Exclusion using a counter
TEST(SpinLockTest, MutualExclusion) {
  SpinLock spinlock;
  int counter = 0;
  const int num_threads = 10;
  const int increments_per_thread = 1000;

  std::vector<std::jthread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < increments_per_thread; ++j) {
        spinlock.lock();
        counter++;
        spinlock.unlock();
      }
    });
  }
  // threads join automatically on destruction
  threads.clear();

  EXPECT_EQ(counter, num_threads * increments_per_thread);
}

// Test LockGuard RAII
TEST(LockGuardTest, RAIICompliance) {
  SpinLock spinlock;
  int counter = 0;

  {
    LockGuard<SpinLock> guard(spinlock);
    counter++;
    // lock should be held here
  }
  // lock should be released here

  // Verify we can take the lock again immediately (if it wasn't released, this
  // would deadlock/hang)
  spinlock.lock();
  spinlock.unlock();

  EXPECT_EQ(counter, 1);
}

// Ensure LockGuard works with std::mutex as well (Concept check)
TEST(LockGuardTest, WorksWithStdMutex) {
  std::mutex mtx;
  {
    LockGuard<std::mutex> guard(mtx);
  }
  // minimal compile-time/runtime verification that it works
}
