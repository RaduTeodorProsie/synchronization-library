#include <iostream>
#include <thread>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

// Your custom headers
// Your custom headers
#include "LockGuard.h" // Ensure this contains the fixed LockGuard
#include "Mutex.h"
#include "SpinLock.h"

int main() {
  // 1. Shared Resources
  int counter = 0;
  Mutex mutex;

  // 2. Container for threads
  // We reserve memory to prevent resizing overhead during spawn
  std::vector<std::jthread> threads;
  threads.reserve(10000);

  std::cout << "Spawning 10,000 threads..." << std::endl;
  auto start = std::chrono::high_resolution_clock::now();

  // 3. Spawn Loop
  for (int i = 0; i < 10000; ++i) {
    // emplace_back constructs the thread directly in the vector
    threads.emplace_back(
        [&] { // Lambda captures 'counter' & 'mutex' by reference
          // Critical Section
          {
            LockGuard<Mutex> guard(mutex);
            counter++;
          } // guard is destroyed here, releasing the lock automatically

        });
  }

  // 4. Synchronization Point
  // We don't need to call join().
  // When 'threads' vector goes out of scope here (or we clear it),
  // std::jthread destructors will automatically block until they are done.
  threads.clear();

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  // 5. Verify
  std::cout << "Finished." << std::endl;
  std::cout << "Expected: 10000" << std::endl;
  std::cout << "Actual:   " << counter << std::endl;
  std::cout << "Time:     " << duration.count() << "ms" << std::endl;

  return 0;
}