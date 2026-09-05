#ifndef TREIBERSTACK_H
#define TREIBERSTACK_H

#include "Backoff.h"
#include "HazardPointers.h"
#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

// Lock-free LIFO stack (Treiber's algorithm), reclaiming through hazard
// pointers. Popped nodes are recycled through the hazard domain rather than
// returned to the allocator, so a steady push/pop workload stops calling new
// and delete altogether.
template <typename T> class TreiberStack {
  // The value is kept in an optional so a recycled node carries no leftover T:
  // it is destroyed on pop and constructed again on the next push.
  struct Node {
    std::optional<T> value;
    Node* next = nullptr;
  };

  std::atomic<Node*> head{nullptr};

public:
  TreiberStack() = default;
  TreiberStack(const TreiberStack&) = delete;
  TreiberStack& operator=(const TreiberStack&) = delete;

  // Not thread-safe.
  ~TreiberStack() {
    for (Node* node = head.load(std::memory_order_relaxed); node;) {
      Node* next = node->next;
      delete node;
      node = next;
    }
  }

  template <typename U>
    requires std::is_constructible_v<T, U>
  void push(U&& value) {
    std::unique_ptr<Node> owner = HazardPointers::reuse<Node>();
    if (owner == nullptr) {
      owner = std::make_unique<Node>();
    }
    owner->value.emplace(std::forward<U>(value));

    // Nothing below throws, so the stack can take ownership now.
    Node* node = owner.release();
    node->next = head.load(std::memory_order_relaxed);

    Backoff backoff;
    while (!head.compare_exchange_weak(node->next, node,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
      backoff.pause();
    }
  }

  std::optional<T> pop() {
    const HazardPointers::Guard guard;
    Node* node = head.load(std::memory_order_acquire);
    Backoff backoff;

    for (;;) {
      if (node == nullptr) {
        return std::nullopt;
      }

      // Guard it, then re-check head: if it moved, the node may be retired.
      // This path only reads head, so it doesn't need to back off.
      guard.protect(node);
      Node* current = head.load(std::memory_order_acquire);
      if (node != current) {
        node = current;
        continue;
      }

      // Success only needs acquire; a pop publishes nothing to other threads.
      if (head.compare_exchange_weak(node, node->next,
                                     std::memory_order_acquire,
                                     std::memory_order_acquire)) {
        break;
      }
      backoff.pause();
    }

    T value = std::move(*node->value);
    node->value.reset(); // Retire it empty so reuse() hands back a blank node.
    guard.clear();
    HazardPointers::retire(node);
    return value;
  }
};

#endif // TREIBERSTACK_H
