# Synchronization Library

A collection of high-performance, C++23 synchronization primitives and concurrent data structures designed for low-latency applications.

## Features

This library provides a set of thread-safe components:

*   **RingBuffer**: A lock-free, fixed-size single-producer, single-consumer (SPSC) ring buffer. Head and tail live on separate cache lines and each side caches the other index so the common path never touches the other thread's atomic.
*   **TreiberStack**: A lock-free LIFO stack. Popped nodes are reclaimed through the hazard pointer domain instead of being freed straight away, so concurrent pops stay safe.
*   **SeqLock**: A sequence lock optimized for scenarios with frequent reads and rare writes. It allows readers to read data without locking, checking for consistency afterwards.
*   **TicketLock**: A fair locking mechanism that grants the lock to threads in the order they requested it (FIFO).
*   **SpinLock**: A lightweight lock that causes a thread trying to acquire it to simply wait in a loop ("spin") while checking if the lock is available.
*   **RwLock**: A Read-Write lock allowing multiple readers or a single writer.
*   **Mutex**: Standard mutual exclusion primitive.
*   **LockGuard**: RAII wrapper for convenient lock management.
*   **HazardPointers**: A minimal, fixed-capacity hazard pointer domain for safe memory reclamation in lock-free structures.

## Requirements

*   **C++23** compiler (e.g., GCC 13+, Clang 16+, MSVC)
*   **CMake** 3.30 or newer

## Build Instructions

The project uses CMake for build configuration.

1.  Clone the repository.
2.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
3.  Configure the project:
    ```bash
    cmake ..
    ```
4.  Build:
    ```bash
    cmake --build .
    ```

## Running Tests

The project uses Google Test for unit testing.

```bash
cd build
ctest --output-on-failure
```

Or run the test executable directly:
```bash
./unit_tests
```

## Running Benchmarks

Performance benchmarks are implemented using Google Benchmark.

```bash
cd build
./locks_benchmark
./ringbuffer_benchmark
./stack_benchmark
```

## Usage Examples

### RingBuffer

```cpp
#include "RingBuffer.h"

// Create a buffer with capacity 1024
RingBuffer<int> buffer(1024);

// Producer
buffer.push(42);

// Consumer
try {
    int value = buffer.pop();
} catch (const std::exception& e) {
    // Handle empty buffer
}
```

### SeqLock

Ideal for reading large structures where consistency is key but locking is too expensive.

```cpp
#include "SeqLock.h"

struct Config {
    int a, b, c;
};

SeqLock<Config> configLock;

// Writer
configLock.write([](Config& data) {
    data.a = 1;
    data.b = 2;
    data.c = 3;
});

// Reader
Config localConfig = configLock.read();
```

### TicketLock

```cpp
#include "TicketLock.h"

TicketLock lock;

void worker() {
    lock.lock();
    // Critical section
    lock.unlock();
}
```

### TreiberStack

```cpp
#include "TreiberStack.h"

TreiberStack<int> stack;

stack.push(42);

// pop() returns std::nullopt when the stack is empty
if (auto value = stack.pop()) {
    // use *value
}
```
