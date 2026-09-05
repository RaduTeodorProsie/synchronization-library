# Synchronization Library

C++23 synchronization primitives and lock-free data structures.

## Components

| | |
|---|---|
| **RingBuffer** | Lock-free SPSC ring buffer. Head and tail sit on separate cache lines, and each side caches the other's index so the common path never touches the other thread's atomic. |
| **TreiberStack** | Lock-free LIFO stack. Popped nodes are reclaimed through the hazard-pointer domain and recycled rather than freed, so a steady workload stops calling `new`. |
| **SeqLock** | Sequence lock for frequent reads and rare writes. Readers take no lock and issue no stores; they re-check a counter afterwards and retry. |
| **HazardPointers** | Fixed-capacity hazard-pointer domain. `retire<T>()` keeps the static type, so there is no erased deleter, and objects a scan proves unguarded are parked for `reuse<T>()` instead of deleted. |
| **Backoff** | Randomized exponential backoff for contended compare-exchange loops. |
| **SpinLock**, **TicketLock**, **RwLock**, **Mutex**, **LockGuard** | Locks. TicketLock is FIFO-fair; LockGuard is the RAII wrapper. |

## Build

Requires a C++23 compiler and CMake 3.30 or newer.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

```bash
cd build && ctest --output-on-failure
```

Benchmarks build alongside: `./build/locks_benchmark`, `./build/ringbuffer_benchmark`, `./build/stack_benchmark`.

## Results

Apple M5 (4 performance + 6 efficiency cores), macOS 25.6, Apple Clang 21, Release,
`--benchmark_min_time=0.3s --benchmark_repetitions=7`; every figure is the median. Benchmarks use
`UseRealTime()`, so a number is wall-clock time per iteration *aggregated over all threads* —
lower is more total throughput, and only comparisons within a row mean anything. Thread counts
above 10 are oversubscribed. Laptop numbers: read them as ratios, not absolutes.

### Mutual exclusion (empty critical section)

Time per lock/unlock pair, in nanoseconds:

| Threads | SpinLock | `std::mutex` | TicketLock |
|--------:|---------:|-------------:|-----------:|
| 1       | 3.60     | 3.78         | 3.27       |
| 2       | 29.6     | 8.41         | 60.3       |
| 4       | 41.0     | 10.4         | 89.7       |
| 8       | 157      | 14.2         | 202        |
| 16      | 286      | 12.4         | 7173       |

Uncontended, all three cost one successful atomic RMW. Under contention the spinning locks lose
badly to `std::mutex`, which parks waiters instead of burning cache-line ownership. TicketLock is
worst because it is also *fair*: a descheduled thread still owns its turn and everyone behind it
waits for it to be rescheduled. Note what the benchmark asks, though — the critical section is
*empty*, so it measures pure handoff cost, which is the case that flatters parking most and
spinning least.

### Read-heavy (one writer, the rest readers)

Time per operation, in nanoseconds:

| Threads | RwLock | `std::shared_mutex` | SeqLock |
|--------:|-------:|--------------------:|--------:|
| 1       | 20.1   | 8.92                | 3.23    |
| 4       | 92.5   | 39.2                | 4.03    |
| 16      | 282    | 30.2                | 0.534   |
| 64      | 140    | 22.4                | 0.155   |
| 256     | 95.5   | 21.6                | 0.106   |

At one thread only the writer runs, so that row is write cost; later rows add readers. SeqLock is
an order of magnitude faster and the only one whose aggregate time keeps dropping as readers are
added: its readers issue **no stores at all**, so the counter's cache line stays shared in every
core and readers never invalidate one another. RwLock and `std::shared_mutex` both make every
reader do a read-modify-write on a shared counter, which makes readers behave like writers as far
as cache coherence is concerned. Caveats: the payload is an `int`, and with one writer against 255
readers a reader almost never retries — SeqLock readers are not wait-free, and a write-heavy mix
would look very different.

### TreiberStack

Time per push+pop pair, in nanoseconds, with nothing between operations:

| Threads | TreiberStack | `std::stack` + `std::mutex` | + SpinLock | + TicketLock |
|--------:|-------------:|----------------------------:|-----------:|-------------:|
| 1       | 11.1         | 11.1                        | 8.72       | 7.31         |
| 2       | 44.2         | 20.6                        | 101        | 195          |
| 4       | 80.2         | 21.3                        | 125        | 332          |
| 8       | 241          | 29.3                        | 406        | 597          |
| 16      | 285          | 28.2                        | 770        | 15616        |

The stack loses to `std::mutex` and beats every spinning lock here — 2.7× the SpinLock and 55× the
TicketLock at 16 threads, protecting the same `std::stack`. The gap is not lock-free versus
locked; it is spinning versus parking.

`std::mutex` wins this table by avoiding contention rather than handling it. It is unfair, so a
thread that releases usually takes the lock straight back: over 8 threads its busiest thread
completes roughly 3× the iterations of its quietest, against 13× for the strictly FIFO TicketLock.
The lock word and the top of the deque stay in one core's L1 across a long run of iterations, so
one cache-line transfer covers many operations. For scale, a *single* contended CAS on one word
costs 228 ns at 8 threads — eight times the mutex's entire push+pop pair. A lock-free stack cannot
amortize that way, because `head` has to migrate on every successful CAS.

Reclamation is the second cost, and it is not free. The same algorithm with the hazard layer
removed costs 8.50 / 110 / 104 ns at 1 / 8 / 16 threads against 11.1 / 241 / 285 — so hazard
pointers are about 30% single-threaded and 2.7× at 16 threads, mostly the per-pop store to a
shared slot and the scan across other threads' slots. Removing them outright would still not reach
the mutex's 28 ns.

Recycling nodes through the hazard domain instead of returning them to the allocator takes a
steady workload to **0.0003 allocations per pair** (276 allocations over a million pairs) and
halved the single-threaded cost, 19.5 ns to 10.1. On its own it made contention *worse*, because a
tighter loop hits the shared line more often; the randomized backoff is what paid that back.

#### Where the lock-free stack wins

The table above hands the mutex its best case: with no work between operations, nothing stops it
batching. Adding work removes that. Time per pair, in nanoseconds, with `N` units of arithmetic
between each operation:

| Threads | Treiber (N=100) | mutex (N=100) | Treiber (N=500) | mutex (N=500) |
|--------:|----------------:|--------------:|----------------:|--------------:|
| 1       | 111             | 115           | 798             | 835           |
| 2       | 128             | 227           | 419             | 439           |
| 4       | 171             | 253           | 234             | 431           |
| 8       | 357             | 317           | 333             | 963           |
| 16      | 389             | 282           | 320             | 799           |

At 500 units the stack is 1.8–2.9× faster at 4, 8 and 16 threads, and its cost stays roughly flat
in the thread count while the mutex's climbs. Once threads are not re-acquiring instantly the
mutex loses its batching and every handoff becomes a park and a wake. What lock-freedom buys is a
progress guarantee — no thread can stall the others by being descheduled mid-operation — and that
shows up as throughput only once the workload is not pure contention.

Two fixes that did not pay off. An elimination-backoff array, where a colliding push and pop hand
off to each other directly and never touch `head`, is the textbook answer to this benchmark, but a
first untuned cut helped at 2 threads and hurt at 8 and 16; it needs an adaptive slot count to be
worth having. Bounding the hazard scan to the slots actually claimed rather than all 128, and
caching the slot pointer in the `Guard`, bought about 5%.

Two measurement notes, because both cost real numbers here. `Backoff`'s cap is in spin iterations,
not time — the same cap is roughly 10 µs on x86, where `pause` is tens of cycles, and 230 ns on an
M-series core, where `yield` is about one. And the work loop above deliberately does arithmetic
instead of calling `cpuRelax()`: a `yield` loop sitting next to the mutex's release-store CAS costs
about three times what the identical loop costs on its own, which silently made the two columns
incomparable.

Correctness of the recycling path is checked by a stress test — eight threads, 480k push/pop pairs,
verifying no value is lost or duplicated — clean under ThreadSanitizer and AddressSanitizer/UBSan.

### SPSC RingBuffer

One producer and one consumer moving `N` `int`s through a 1024-slot buffer:

| Items | 1024     | 4096    | 32768   | 65536   |
|------:|---------:|--------:|--------:|--------:|
| Time  | 18.3 µs  | 28.3 µs | 101 µs  | 182 µs  |
| Rate  | 56.0 M/s | 145 M/s | 326 M/s | 360 M/s |

Each iteration builds a fresh buffer and spawns a consumer thread, so the small sizes are
dominated by thread startup; the larger rounds amortize it and show the steady state, roughly
360 M items/s or 2.8 ns per item end to end.

## Usage

```cpp
RingBuffer<int> buffer(1024);
buffer.push(42);
int value = buffer.pop();          // throws when empty

TreiberStack<int> stack;
stack.push(42);
if (auto value = stack.pop()) {}   // std::nullopt when empty

SeqLock<Config> config;
config.write([](Config& data) { data.a = 1; });
Config snapshot = config.read();

TicketLock lock;                   // also SpinLock, RwLock, Mutex
{
    LockGuard guard(lock);
}
```
