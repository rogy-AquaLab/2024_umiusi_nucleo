#include "umiusi/defered_delay.hpp"
#include "ThisThread.h"
#include <chrono>

DeferedDelay::DeferedDelay(uint16_t duration_ms) : duration_ms(duration_ms) {}

DeferedDelay::~DeferedDelay() {
    rtos::ThisThread::sleep_for(std::chrono::milliseconds(duration_ms));
}
