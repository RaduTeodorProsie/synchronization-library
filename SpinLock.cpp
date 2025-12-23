//
// Created by Radu Prosie on 12/23/2025.
//

#include "SpinLock.h"


void SpinLock::lock() {
    while (flag.test_and_set(std::memory_order_acquire))
        ;
}

void SpinLock::release() {
    flag.clear(std::memory_order_release);
}
