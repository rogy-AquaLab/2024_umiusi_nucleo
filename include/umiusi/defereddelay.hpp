#ifndef DEFEREDDELAY_HPP
#define DEFEREDDELAY_HPP

#include "ThisThread.h"
#include <chrono>

struct DeferedDelay {
    const uint16_t duration_ms;

    DeferedDelay(uint16_t duration_ms) : duration_ms(duration_ms) {}

    ~DeferedDelay() {
        rtos::ThisThread::sleep_for(std::chrono::milliseconds(duration_ms));
    }
};

#endif // DEFEREDDELAY_HPP
