#include "TicketLock.h"

void TicketLock::lock() {
  int myTicket = counter.fetch_add(1, std::memory_order_acq_rel);
  for (int serving = now.load(std::memory_order_acquire);
       serving != myTicket;
       serving = now.load(std::memory_order_acquire)) {
    now.wait(serving);
  }
}

void TicketLock::unlock() {
  now.fetch_add(1, std::memory_order_acq_rel);
  now.notify_all();
}


