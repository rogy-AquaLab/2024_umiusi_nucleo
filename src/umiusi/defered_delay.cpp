#include "umiusi/defered_delay.hpp"
#include "ThisThread.h"
#include <chrono>

DeferedDelay::DeferedDelay(std::chrono::milliseconds duration) : duration(duration) {}

DeferedDelay::~DeferedDelay() {
    rtos::ThisThread::sleep_for(this->duration);
}
