#ifndef DEFERED_DELAY_HPP
#define DEFERED_DELAY_HPP

#include <chrono>
#include <cstdint>

struct DeferedDelay {
    const std::chrono::milliseconds duration;

    DeferedDelay(std::chrono::milliseconds duration);
    ~DeferedDelay();
};

#endif // DEFERED_DELAY_HPP
