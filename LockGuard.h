#ifndef LOCKGUARD_H
#define LOCKGUARD_H
#include <utility>

template <typename T>
concept Lockable = requires(T &t) {
  t.lock();
  t.unlock();
};

template <Lockable L> class LockGuard {
  L &data;

public:
  explicit LockGuard(L &data) : data(data) { data.lock(); }
  ~LockGuard() noexcept { data.unlock(); }

  LockGuard(const LockGuard &) = delete;
  LockGuard &operator=(const LockGuard &) = delete;
};

#endif // LOCKGUARD_H
