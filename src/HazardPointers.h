#ifndef HAZARDPOINTERS_H
#define HAZARDPOINTERS_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

// Minimal hazard pointer domain: one slot per thread, linear scans, per-thread
// retire lists. Not production grade; a dead thread's pending nodes leak.
//
// Reclamation is type-safe: retired objects live in a per-type retire list of
// std::unique_ptr<T>, so there is no erased deleter to carry around. Objects
// that survive a scan unguarded are not deleted but parked in a per-type free
// list, so callers can take them back through reuse() instead of paying the
// allocator on every operation.
class HazardPointers {
public:
  static constexpr size_t maxThreads = 128;
  static constexpr size_t scanThreshold = 2 * maxThreads;
  // Anything reclaimed beyond this is deleted, so a thread that only retires
  // (and never reuses) can't grow a free list without bound.
  static constexpr size_t freeListCapacity = scanThreshold;

private:
  static constexpr size_t cacheLineSize =
      std::hardware_destructive_interference_size;

  struct alignas(cacheLineSize) Slot {
    std::atomic<bool> active{false};
    std::atomic<const void*> guarded{nullptr};
  };

  // Function-local so it's instantiated after Slot is complete.
  static std::array<Slot, maxThreads>& slotTable() {
    static std::array<Slot, maxThreads> table;
    return table;
  }

  // Owns one slot for the lifetime of the thread that created it.
  class SlotHandle {
  public:
    SlotHandle() : slot(claim()) {}
    SlotHandle(const SlotHandle&) = delete;
    SlotHandle& operator=(const SlotHandle&) = delete;

    ~SlotHandle() {
      slot->guarded.store(nullptr, std::memory_order_release);
      slot->active.store(false, std::memory_order_release);
    }

    Slot& operator*() const noexcept { return *slot; }

  private:
    static Slot* claim() {
      for (auto& slot : slotTable()) {
        bool free = false;
        if (slot.active.compare_exchange_strong(free, true,
                                                std::memory_order_acq_rel)) {
          return &slot;
        }
      }
      throw std::runtime_error("HazardPointers: ran out of slots");
    }

    Slot* slot;
  };

  static Slot& mySlot() {
    thread_local SlotHandle handle;
    return *handle;
  }

  // Deliberately leaked at thread exit: the objects still pending here may be
  // guarded by other threads, so this thread is not allowed to delete them.
  // Handing them to a surviving thread would need a global orphan list, which
  // this domain doesn't have.
  template <typename T>
  static std::vector<std::unique_ptr<T>>& retiredList() {
    thread_local auto* retired = new std::vector<std::unique_ptr<T>>();
    return *retired;
  }

  // Safe to destroy at thread exit: everything in here was proven unguarded by
  // a scan and is out of circulation, so no thread can reach it.
  template <typename T>
  static std::vector<std::unique_ptr<T>>& freeList() {
    thread_local std::vector<std::unique_ptr<T>> reusable;
    return reusable;
  }

  // Sorted so scan() can binary search it. The buffer is reused across scans to
  // keep reclamation allocation-free, so the result is only valid until this
  // thread takes another snapshot.
  static const std::vector<const void*>& snapshotHazards() {
    thread_local std::vector<const void*> hazards;
    hazards.clear();
    hazards.reserve(maxThreads);
    for (const auto& slot : slotTable()) {
      if (const void* guarded = slot.guarded.load(std::memory_order_acquire)) {
        hazards.push_back(guarded);
      }
    }
    std::ranges::sort(hazards);
    return hazards;
  }

  template <typename T>
  static void scan(std::vector<std::unique_ptr<T>>& retired) {
    const std::vector<const void*>& hazards = snapshotHazards();
    auto& reusable = freeList<T>();

    // Compacts in place, so the retire list keeps its buffer. Returning true
    // hands the node to erase, which deletes it unless it was moved out first.
    std::erase_if(retired, [&](std::unique_ptr<T>& node) {
      if (std::ranges::binary_search(hazards,
                                     static_cast<const void*>(node.get()))) {
        return false;
      }
      if (reusable.size() < freeListCapacity) {
        reusable.push_back(std::move(node));
      }
      return true;
    });
  }

public:
  // Publishes p as in-use for as long as the guard lives. The caller must
  // re-read the source of p after protecting it to confirm it's still live.
  class [[nodiscard]] Guard {
  public:
    Guard() = default;
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    ~Guard() { clear(); }

    template <typename T>
    T* protect(T* p) const {
      mySlot().guarded.store(p, std::memory_order_release);
      return p;
    }

    void clear() const {
      mySlot().guarded.store(nullptr, std::memory_order_release);
    }
  };

  // Hands ownership of p to this thread's retire list. Once no thread guards
  // it, it is either deleted or kept for reuse().
  template <typename T>
  static void retire(T* p) {
    auto& retired = retiredList<T>();
    retired.emplace_back(p);
    if (retired.size() >= scanThreshold) {
      scan(retired);
    }
  }

  // Takes back an object that was retired and has since been proven unguarded,
  // or nullptr if none is ready. Objects come back in the state they were
  // retired in, so a caller that reuses them must retire them empty.
  template <typename T>
  [[nodiscard]] static std::unique_ptr<T> reuse() {
    auto& reusable = freeList<T>();
    if (reusable.empty()) {
      return nullptr;
    }
    std::unique_ptr<T> node = std::move(reusable.back());
    reusable.pop_back();
    return node;
  }
};

#endif // HAZARDPOINTERS_H
