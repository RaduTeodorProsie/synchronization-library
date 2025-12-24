#ifndef SPINLOCK_H
#define SPINLOCK_H

#include<atomic>

class SpinLock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock();
    void unlock() noexcept;
};



#endif //SPINLOCK_H
