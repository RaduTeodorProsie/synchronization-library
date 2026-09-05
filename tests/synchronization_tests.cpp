#include "LockGuard.h"
#include "SpinLock.h"
#include <algorithm>
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

#include "Mutex.h"
#include "RwLock.h"
#include "SeqLock.h"
#include "TicketLock.h"

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

// --- SeqLock Tests ---
TEST(SeqLockTest, BasicReadWrite) {
  SeqLock<int> seqlock;
  seqlock.write([](int &data) { data = 42; });
  int val = seqlock.read();
  EXPECT_EQ(val, 42);
}

TEST(SeqLockTest, ConcurrentReadWrite) {
  SeqLock<int> seqlock;
  seqlock.write([](int &data) { data = 0; });

  const int num_writers = 1;
  const int writes_per_thread = 1000;
  const int num_readers = 4;

  std::vector<std::jthread> threads;

  // Writers increment data
  for (int i = 0; i < num_writers; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < writes_per_thread; ++j) {
        seqlock.write([](int &data) { data++; });
      }
    });
  }

  // Readers read data
  std::atomic<bool> stop_readers = false;
  std::vector<int> reads;
  std::mutex reads_mtx;

  for (int i = 0; i < num_readers; ++i) {
    threads.emplace_back([&] {
      while (!stop_readers) {
        int val = seqlock.read();
        if (val % 2 != 0 && val % 2 != 1) {
          // This condition is practically impossible for int,
          // but if we were reading a struct, we'd check for consistency.
          // For int, just ensure we can read without hanging.
        }
      }
    });
  }

  // Let readers run for a bit while writers are writing
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  stop_readers =
      true; // threads.clear() will join writers first, then readers might run a
            // bit longer or join. Actually jthread destructors request stop if
            // they have a token, but here we just join. We need to signal
            // readers to stop loop.

  threads.clear();

  int final_val = seqlock.read();
  EXPECT_EQ(final_val, num_writers * writes_per_thread);
}

// --- TreiberStack Tests ---
#include "TreiberStack.h"
#include <optional>

TEST(TreiberStackTest, LifoOrder) {
  TreiberStack<int> stack;
  stack.push(1);
  stack.push(2);
  stack.push(3);

  EXPECT_EQ(stack.pop(), 3);
  EXPECT_EQ(stack.pop(), 2);
  EXPECT_EQ(stack.pop(), 1);
  EXPECT_EQ(stack.pop(), std::nullopt);
}

TEST(TreiberStackTest, ConcurrentPushDrains) {
  TreiberStack<int> stack;
  const int num_threads = 8;
  const int per_thread = 10000;

  std::vector<std::jthread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < per_thread; ++j) {
        stack.push(j);
      }
    });
  }
  threads.clear();

  long sum = 0;
  int count = 0;
  while (auto value = stack.pop()) {
    sum += *value;
    count++;
  }

  EXPECT_EQ(count, num_threads * per_thread);
  // Each thread pushed 0..per_thread-1, so the totals are known.
  EXPECT_EQ(sum, (long)num_threads * per_thread * (per_thread - 1) / 2);
}

TEST(TreiberStackTest, ConcurrentPushPop) {
  TreiberStack<int> stack;
  const int num_producers = 4;
  const int num_consumers = 4;
  const int per_producer = 20000;
  const int total = num_producers * per_producer;

  std::vector<std::vector<int>> consumed(num_consumers);
  std::atomic<int> producers_done = 0;

  std::vector<std::jthread> threads;
  for (int i = 0; i < num_producers; ++i) {
    threads.emplace_back([&, i] {
      // Every value pushed is distinct, so a lost or duplicated one shows up.
      for (int j = 0; j < per_producer; ++j) {
        stack.push(i * per_producer + j);
      }
      producers_done++;
    });
  }
  for (int i = 0; i < num_consumers; ++i) {
    threads.emplace_back([&, i] {
      while (producers_done < num_producers) {
        if (auto value = stack.pop()) {
          consumed[i].push_back(*value);
        }
      }
    });
  }
  threads.clear();

  std::vector<int> seen(total, 0);
  for (const auto& values : consumed) {
    for (int value : values) {
      seen[value]++;
    }
  }
  while (auto value = stack.pop()) {
    seen[*value]++;
  }

  EXPECT_EQ(std::ranges::count(seen, 1), total);
}
