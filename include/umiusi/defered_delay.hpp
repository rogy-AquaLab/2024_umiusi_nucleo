#ifndef DEFERED_DELAY_HPP
#define DEFERED_DELAY_HPP

#include <cstdint>

struct DeferedDelay {
    const uint16_t duration_ms;

    DeferedDelay(uint16_t duration_ms);
    ~DeferedDelay();
};

#endif // DEFERED_DELAY_HPP
