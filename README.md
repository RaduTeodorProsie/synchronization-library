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

The Treiber stack's recycling path also has a stress test — eight threads, 480k push/pop pairs,
verifying no value is lost or duplicated — which runs clean under ThreadSanitizer and
AddressSanitizer/UBSan.

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
