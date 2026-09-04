#ifndef TICKETLOCK_H
#define TICKETLOCK_H

#include<atomic>

class TicketLock {
    std::atomic<unsigned> now{0};
    std::atomic<unsigned> counter{0};

public:
    void lock();
    void unlock();
};



#endif //TICKETLOCK_H
