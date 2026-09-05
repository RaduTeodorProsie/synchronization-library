# Synchronization Library

A collection of high-performance, C++23 synchronization primitives and concurrent data structures designed for low-latency applications.

## Features

This library provides a set of thread-safe components:

*   **RingBuffer**: A lock-free, fixed-size single-producer, single-consumer (SPSC) ring buffer. Head and tail live on separate cache lines and each side caches the other index so the common path never touches the other thread's atomic.
*   **TreiberStack**: A lock-free LIFO stack. Popped nodes are reclaimed through the hazard pointer domain instead of being freed straight away, so concurrent pops stay safe, and they are recycled rather than returned to the allocator, so a steady workload stops calling `new` altogether.
*   **SeqLock**: A sequence lock optimized for scenarios with frequent reads and rare writes. It allows readers to read data without locking, checking for consistency afterwards.
*   **TicketLock**: A fair locking mechanism that grants the lock to threads in the order they requested it (FIFO).
*   **SpinLock**: A lightweight lock that causes a thread trying to acquire it to simply wait in a loop ("spin") while checking if the lock is available.
*   **RwLock**: A Read-Write lock allowing multiple readers or a single writer.
*   **Mutex**: Standard mutual exclusion primitive.
*   **LockGuard**: RAII wrapper for convenient lock management.
*   **HazardPointers**: A minimal, fixed-capacity hazard pointer domain for safe memory reclamation in lock-free structures. Readers publish a pointer through an RAII `Guard`, and `retire<T>()` keeps the static type, so reclamation needs no erased deleter. Objects that a scan proves unguarded are parked for `reuse<T>()` instead of deleted.
*   **Backoff**: Randomized exponential backoff for contended compare-exchange loops, so threads that lose a CAS stop stealing the cache line back from the one that won.

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

### Results

Measured on an Apple M5 (10 cores), macOS 25.6, Apple Clang 21, `CMAKE_BUILD_TYPE=Release`,
with `--benchmark_min_time=0.3s --benchmark_repetitions=7`; every figure is the median of the
seven runs. Each benchmark uses `UseRealTime()`, so the reported time is wall-clock time per
iteration *aggregated over all threads*: lower is higher total throughput, and a number that
keeps falling as threads are added means the primitive is still scaling. Thread counts above 10
are oversubscribed on this machine. These are laptop numbers — treat them as ratios, not
absolutes.

#### Mutual exclusion (empty critical section)

Time per lock/unlock pair, in nanoseconds:

| Threads | SpinLock | `std::mutex` | TicketLock |
|--------:|---------:|-------------:|-----------:|
| 1       | 3.60     | 3.78         | 3.27       |
| 2       | 29.6     | 8.41         | 60.3       |
| 4       | 41.0     | 10.4         | 89.7       |
| 8       | 157      | 14.2         | 202        |
| 16      | 286      | 12.4         | 7173       |

Uncontended, all three cost about the same: one successful atomic RMW. Under contention the
spinning locks lose badly to `std::mutex`, which parks waiters in the kernel instead of burning
cache-line ownership on a contended atomic. TicketLock is the worst of the three because it is
also *fair*: a thread that has been descheduled still owns its turn, and everyone behind it
waits for it to be rescheduled — hence the collapse at 16 threads on a 10-core machine.

Note what this benchmark asks, though. The critical section is *empty*, so it measures pure
handoff cost, which is the case that flatters a parking mutex most and a spinning lock least.
The question a spinlock exists to answer — tens of nanoseconds of real work under the lock, with
no more threads than cores — is not on this table.

#### Read-heavy (one writer, the rest readers)

Time per operation, in nanoseconds:

| Threads | RwLock | `std::shared_mutex` | SeqLock |
|--------:|-------:|--------------------:|--------:|
| 1       | 20.1   | 8.92                | 3.23    |
| 2       | 32.4   | 21.0                | 6.38    |
| 4       | 92.5   | 39.2                | 4.03    |
| 8       | 235    | 49.9                | 3.07    |
| 16      | 282    | 30.2                | 0.534   |
| 32      | 260    | 28.5                | 0.225   |
| 64      | 140    | 22.4                | 0.155   |
| 128     | 92.0   | 25.0                | 0.120   |
| 256     | 95.5   | 21.6                | 0.106   |

At one thread only the writer runs, so that row is write cost; every later row adds readers, and
the mix shifts toward reads as the thread count climbs. SeqLock is roughly an order of magnitude
faster than either reader-writer lock and is the only one whose aggregate time keeps dropping as
readers are added: its readers issue **no stores at all**, so the sequence counter's cache line
stays shared in every core and readers never invalidate one another — only the rare writer does.
RwLock and `std::shared_mutex` both make every reader do a read-modify-write on a shared
counter, which makes readers behave like writers as far as cache coherence is concerned.

Two caveats on that column. The payload here is an `int`; a large struct shifts the cost into
the reader's `memcpy` and makes a retry expensive. And with one writer against 255 readers, a
reader almost never has to retry — SeqLock readers are not wait-free, and a write-heavy mix
would look very different.

#### TreiberStack vs. a locked `std::stack`

Time per push+pop pair, in nanoseconds. "Before" is the stack as first written — a fresh `new`
per push, `delete` on reclamation, and no backoff on a failed CAS:

| Threads | TreiberStack (before) | TreiberStack | `std::stack` + `std::mutex` |
|--------:|----------------------:|-------------:|----------------------------:|
| 1       | 19.5                  | 10.1         | 7.79                        |
| 2       | 105                   | 44.0         | 17.6                        |
| 4       | 99.1                  | 80.8         | 16.5                        |
| 8       | 409                   | 233          | 25.8                        |
| 16      | 399                   | 288          | 23.9                        |

Two changes account for that, and they are worth separating because one is useless without the
other. Recycling nodes through the hazard domain instead of returning them to the allocator
takes a steady push/pop workload down to **0.0003 allocations per pair** (measured with a
counting `operator new`: 276 allocations over a million pairs, essentially the warm-up), which
is what halves the single-threaded number — `new` plus `delete` alone costs 13.7 ns of the
original 19.5. On its own, though, recycling makes contention *worse* (443 ns at 8 threads,
against 409 before), because a tighter loop hits the shared cache line more often. Randomized
exponential backoff on a failed compare-exchange is what pays that back.

The stack still loses to the mutex under contention, and it is worth being clear about why
rather than hiding the table. The useful comparison is not against the mutex but against this
repository's own SpinLock: at 16 threads the stack costs 288 ns while SpinLock costs 286 ns for
an *empty* critical section. The stack is already at the price of spinning itself, so there is
no stack-specific overhead left to remove — what remains is the cost of spinning on one
contended line.

That makes the real gap **spinning versus parking**, not lock-free versus locked. `std::mutex`
at 16 threads (12.4 ns) is only about three times its single-threaded cost, because a parked
waiter issues no memory traffic at all: the line stays in one core's L1 and the winner runs at
nearly single-thread speed. A lock-free structure cannot do that by construction — parking means
waiting for another thread to wake you, which is exactly the dependency lock-freedom forbids, so
every thread has to keep trying. Backoff is the only lever available, which is why it bought
1.4× and not 12×. The CPU column shows what that costs: 2956 ns of CPU per operation at 16
threads against 288 ns of wall time, roughly ten cores busy to produce one pop, because at 16
threads there are about 15 losers per winner and every failed attempt is work thrown away.

Backing off harder does keep improving the numbers here — a backoff cap 64× larger reaches 67 ns
at 16 threads — but only because a thread that spins for tens of microseconds has effectively
dropped out of the benchmark, and at that point a lock that parks the waiter is simply the
better tool. The cap is deliberately left at about a microsecond instead of tuned to this table.
What lock-freedom buys is a progress guarantee — no thread can stall the others by being
descheduled mid-operation — not throughput on a benchmark built out of pure contention.

The fix that would actually win here is architectural rather than a constant: an elimination
backoff array, where a push and a pop that collide hand off to each other directly and never
touch `head` at all. That removes the single contention point instead of scheduling around it.

Correctness of the recycling path is checked by a stress test (eight threads, 480k push/pop
pairs, verifying no value is lost or duplicated) run clean under ThreadSanitizer and
AddressSanitizer/UBSan, in addition to the unit tests.

#### SPSC RingBuffer

One producer and one consumer moving `N` `int`s through a 1024-slot buffer:

| Items   | Time per round | Throughput   |
|--------:|---------------:|-------------:|
| 1024    | 18.3 µs        | 56.0 M/s     |
| 4096    | 28.3 µs        | 145 M/s      |
| 32768   | 101 µs         | 326 M/s      |
| 65536   | 182 µs         | 360 M/s      |

Each iteration constructs a fresh buffer and spawns a consumer thread, so the small sizes are
dominated by thread startup; the larger rounds amortize it and show the steady-state rate of
roughly 360 M items/s, about 2.8 ns per item end to end.

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
