#ifndef MUTEX_H
#define MUTEX_H

#include<atomic>

class Mutex {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock();
    void unlock();
};



#endif //MUTEX_H
