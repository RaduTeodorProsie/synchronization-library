#ifndef TREIBERSTACK_H
#define TREIBERSTACK_H

#include "HazardPointers.h"
#include <atomic>
#include <optional>
#include <type_traits>
#include <utility>

// Lock-free LIFO stack (Treiber's algorithm), reclaiming through hazard pointers.
template <typename T> class TreiberStack {
  struct Node {
    T value;
    Node *next;
  };

  std::atomic<Node *> head{nullptr};

public:
  TreiberStack() = default;
  TreiberStack(const TreiberStack &) = delete;
  TreiberStack &operator=(const TreiberStack &) = delete;

  // Not thread-safe.
  ~TreiberStack() {
    for (Node *node = head.load(std::memory_order_relaxed); node;) {
      Node *next = node->next;
      delete node;
      node = next;
    }
  }

  template <typename U>
    requires std::is_constructible_v<T, U>
  void push(U &&value) {
    Node *node = new Node{T(std::forward<U>(value)), nullptr};
    node->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(node->next, node,
                                      std::memory_order_release,
                                      std::memory_order_relaxed)) {
    }
  }

  std::optional<T> pop() {
    Node *node = head.load(std::memory_order_acquire);

    for (;;) {
      if (node == nullptr) {
        HazardPointers::clear();
        return std::nullopt;
      }

      // Guard it, then re-check head: if it moved, the node may be retired.
      HazardPointers::protect(node);
      Node *current = head.load(std::memory_order_acquire);
      if (node != current) {
        node = current;
        continue;
      }

      if (head.compare_exchange_weak(node, node->next,
                                     std::memory_order_acq_rel,
                                     std::memory_order_acquire)) {
        break;
      }
    }

    T value = std::move(node->value);
    HazardPointers::clear();
    HazardPointers::retire(node);
    return value;
  }
};

#endif // TREIBERSTACK_H
