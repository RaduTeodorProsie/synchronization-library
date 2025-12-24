#ifndef TICKETLOCK_H
#define TICKETLOCK_H

#include<atomic>

class TicketLock {
    std::atomic<int> now{0};
    std::atomic<int> counter{0};

public:
    void lock();
    void unlock();
};



#endif //TICKETLOCK_H
