#ifndef STATE_HPP
#define STATE_HPP

#include <cstdint>

enum class State : std::uint8_t {
    INITIALIZING = 0,
    SUSPEND = 1,
    RUNNING = 2,
};

#endif // STATE_HPP
