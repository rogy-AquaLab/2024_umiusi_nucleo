

struct DeferedDelay {
    const uint16_t duration_ms;

    DeferedDelay(uint16_t duration_ms) : duration_ms(duration_ms) {}

    ~DeferedDelay() {
        ThisThread::sleep_for(std::chrono::milliseconds(duration_ms));
    }
};