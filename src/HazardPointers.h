#ifndef HAZARDPOINTERS_H
#define HAZARDPOINTERS_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <vector>

// Minimal hazard pointer domain: one slot per thread, linear scans, per-thread
// retire lists. Not production grade; a dead thread's retired nodes leak.
class HazardPointers {
public:
  static constexpr size_t maxThreads = 128;
  static constexpr size_t scanThreshold = 2 * maxThreads;

private:
  constexpr static size_t cacheLineSize =
      std::hardware_destructive_interference_size;

  struct alignas(cacheLineSize) Slot {
    std::atomic<bool> active{false};
    std::atomic<void *> guarded{nullptr};
  };

  struct Retired {
    void *raw;
    void (*deleter)(void *);
  };

  // Function-local so it's instantiated after Slot is complete.
  static std::array<Slot, maxThreads> &slotTable() {
    static std::array<Slot, maxThreads> table;
    return table;
  }

  static Slot *claimSlot() {
    for (auto &slot : slotTable()) {
      bool free = false;
      if (slot.active.compare_exchange_strong(free, true,
                                              std::memory_order_acq_rel)) {
        return &slot;
      }
    }
    throw std::runtime_error("HazardPointers: ran out of slots");
  }

  struct SlotReleaser {
    Slot *slot;
    ~SlotReleaser() {
      slot->guarded.store(nullptr, std::memory_order_release);
      slot->active.store(false, std::memory_order_release);
    }
  };

  static Slot &mySlot() {
    thread_local Slot *slot = claimSlot();
    thread_local SlotReleaser releaser{slot};
    return *slot;
  }

  static std::vector<Retired> &myRetired() {
    thread_local std::vector<Retired> retired;
    return retired;
  }

  static void scan() {
    std::vector<void *> hazards;
    hazards.reserve(maxThreads);
    for (auto &slot : slotTable()) {
      if (void *p = slot.guarded.load(std::memory_order_acquire)) {
        hazards.push_back(p);
      }
    }

    std::erase_if(myRetired(), [&](const Retired &r) {
      if (std::find(hazards.begin(), hazards.end(), r.raw) != hazards.end()) {
        return false;
      }
      r.deleter(r.raw);
      return true;
    });
  }

public:
  // Caller must re-read the source of p afterwards to confirm it's still live.
  template <typename T> static T *protect(T *p) {
    mySlot().guarded.store(p, std::memory_order_release);
    return p;
  }

  static void clear() {
    mySlot().guarded.store(nullptr, std::memory_order_release);
  }

  template <typename T> static void retire(T *p) {
    myRetired().push_back({p, [](void *raw) { delete static_cast<T *>(raw); }});
    if (myRetired().size() >= scanThreshold) {
      scan();
    }
  }
};

#endif // HAZARDPOINTERS_H
