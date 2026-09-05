#ifndef BACKOFF_H
#define BACKOFF_H

#include <algorithm>
#include <atomic>

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) ||             \
    defined(_M_IX86)
#include <immintrin.h>
#endif

// One spin-loop hint to the CPU. A thread that retries a failed
// compare-exchange immediately steals the cache line back from the thread that
// won it, so pausing is what lets the winner finish.
inline void cpuRelax() noexcept {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) ||             \
    defined(_M_IX86)
  _mm_pause();
#elif defined(__GNUC__) && (defined(__aarch64__) || defined(__arm__))
  __asm__ __volatile__("yield" ::: "memory");
#else
  // No portable pause instruction; at least stop the loop being optimized out.
  std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

// Randomized exponential backoff for contended compare-exchange loops. The
// window doubles on every failure, and the wait is a random point inside it so
// that a crowd of losers doesn't wake in lockstep and collide all over again.
//
// The cap bounds a wait at roughly a microsecond. Raising it keeps helping on a
// benchmark that does nothing but hammer one cache line, but only because a
// thread that spins for tens of microseconds has effectively left the contest —
// past this point a lock that parks the waiter is the better tool.
class Backoff {
public:
  void pause() noexcept {
    for (unsigned spin = random() % delay; spin > 0; --spin) {
      cpuRelax();
    }
    delay = std::min(delay * 2, maxDelay);
  }

private:
  // xorshift32, seeded per thread so two threads don't draw the same sequence.
  static unsigned random() noexcept {
    thread_local unsigned state = seed();
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }

  static unsigned seed() noexcept {
    static std::atomic<unsigned> counter{0};
    // Odd, so it is never the zero fixed point of xorshift.
    return (0x9e3779b9u +
            counter.fetch_add(0x85ebca6bu, std::memory_order_relaxed)) |
           1u;
  }

  static constexpr unsigned maxDelay = 1024;

  unsigned delay = 4;
};

#endif // BACKOFF_H
