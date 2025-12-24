#include "Mutex.h"

void Mutex::lock(){
    while (flag.test_and_set(std::memory_order_acquire))
        flag.wait(true, std::memory_order_relaxed);
}

void Mutex::unlock() {
    flag.clear(std::memory_order_release);
    flag.notify_one();
}






