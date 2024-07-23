#ifndef UMIUSI_MUTEX_GUARD_HPP
#define UMIUSI_MUTEX_GUARD_HPP

#include "rtos/Mutex.h"

class MutexGuard {
private:
    rtos::Mutex& mutex;

public:
    MutexGuard(rtos::Mutex& mutex) : mutex(mutex) {
        this->mutex.lock();
    }

    ~MutexGuard() {
        this->mutex.unlock();
    }
};

#endif // UMIUSI_MUTEX_GUARD_HPP
