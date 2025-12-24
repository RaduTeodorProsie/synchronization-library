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

#include "../Mutex.h"
#include "../RwLock.h"
#include "../TicketLock.h"


// --- Mutex Tests ---
TEST(MutexTest, BasicLockUnlock) {
  Mutex mtx;
  mtx.lock();
  mtx.unlock();
}

TEST(MutexTest, MutualExclusion) {
  Mutex mtx;
  int counter = 0;
  const int num_threads = 10;
  const int increments_per_thread = 1000;

  std::vector<std::jthread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < increments_per_thread; ++j) {
        mtx.lock();
        counter++;
        mtx.unlock();
      }
    });
  }
  threads.clear();
  EXPECT_EQ(counter, num_threads * increments_per_thread);
}

// --- TicketLock Tests ---
TEST(TicketLockTest, BasicLockUnlock) {
  TicketLock ticket;
  ticket.lock();
  ticket.unlock();
}

TEST(TicketLockTest, MutualExclusion) {
  TicketLock ticket;
  int counter = 0;
  const int num_threads = 10;
  const int increments_per_thread = 1000;

  std::vector<std::jthread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < increments_per_thread; ++j) {
        ticket.lock();
        counter++;
        ticket.unlock();
      }
    });
  }
  threads.clear();
  EXPECT_EQ(counter, num_threads * increments_per_thread);
}

// --- RwLock Tests ---
TEST(RwLockTest, MultipleReaders) {
  RwLock rwlock;
  std::atomic<int> read_count = 0;
  const int num_readers = 5;

  // We want to verify that multiple readers can be in the critical section at
  // once. We can't easily prove they ARE overlapping without delays etc, but we
  // can prove they don't deadlock.
  std::vector<std::jthread> threads;
  for (int i = 0; i < num_readers; ++i) {
    threads.emplace_back([&] {
      rwlock.lockRead();
      read_count++;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      rwlock.unlockRead();
    });
  }
  threads.clear();
  EXPECT_EQ(read_count, num_readers);
}

TEST(RwLockTest, WriterExclusion) {
  RwLock the_rwlock;
  int shared_data = 0;
  const int num_writers = 5;

  std::vector<std::jthread> threads;
  // Writers
  for (int i = 0; i < num_writers; ++i) {
    threads.emplace_back([&] {
      the_rwlock.lockWrite();
      int temp = shared_data;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      shared_data = temp + 1;
      the_rwlock.unlockWrite();
    });
  }
  // Readers concurrent
  for (int i = 0; i < num_writers; ++i) {
    threads.emplace_back([&] {
      the_rwlock.lockRead();
      int val = shared_data; // Just read
      (void)val;
      the_rwlock.unlockRead();
    });
  }

  threads.clear();
  // Verification is mainly that we didn't crash or corrupt heavily.
  // Ideally shared_data should be num_writers if updates were atomic via lock.
  EXPECT_EQ(shared_data, num_writers);
}
