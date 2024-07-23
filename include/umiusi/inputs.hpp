#ifndef UMIUSI_INPUTS_HPP
#define UMIUSI_INPUTS_HPP

#ifndef FLEX1_PIN

    #define FLEX1_PIN PB_1

#endif // FLEX1_PIN

#ifndef FLEX2_PIN

    #define FLEX2_PIN PA_0

#endif // FLEX2_PIN

#ifndef CURRENT_PIN

    #define CURRENT_PIN PA_5

#endif // CURRENT_PIN

#ifndef VOLTAGE_PIN

    #define VOLTAGE_PIN PA_4

#endif // VOLTAGE_PIN

#include <array>
#include <mutex>
#include <stdint.h>

#include "AnalogIn.h"

struct InputValues {
    uint16_t flex1;
    uint16_t flex2;
    uint16_t current;
    uint16_t voltage;

    InputValues() = delete;

    auto packet_data() const -> std::array<uint8_t, 8> {
        return {
            static_cast<uint8_t>((this->flex1 >> 0) & 0xFF),
            static_cast<uint8_t>((this->flex1 >> 8) & 0xFF),
            static_cast<uint8_t>((this->flex2 >> 0) & 0xFF),
            static_cast<uint8_t>((this->flex2 >> 8) & 0xFF),
            static_cast<uint8_t>((this->current >> 0) & 0xFF),
            static_cast<uint8_t>((this->current >> 8) & 0xFF),
            static_cast<uint8_t>((this->voltage >> 0) & 0xFF),
            static_cast<uint8_t>((this->voltage >> 8) & 0xFF),
        };
    }
};

class Inputs {
private:
    mbed::AnalogIn flex1;
    mbed::AnalogIn flex2;
    mbed::AnalogIn current;
    mbed::AnalogIn voltage;

public:
    // FIXME: ビルダーを与えたい
    Inputs() :
        flex1(FLEX1_PIN),
        flex2(FLEX2_PIN),
        current(CURRENT_PIN),
        voltage(VOLTAGE_PIN) {}

    void read(InputValues& values) {
        values.flex1   = this->flex1.read_u16();
        values.flex2   = this->flex2.read_u16();
        values.current = this->current.read_u16();
        values.voltage = this->voltage.read_u16();
    }

    auto read() -> InputValues {
        return InputValues{ this->flex1.read_u16(),
                            this->flex2.read_u16(),
                            this->current.read_u16(),
                            this->voltage.read_u16() };
    }
};

class CachedInputs {
private:
    Inputs inputs;
    bool   set_values;
    // FIXME: std::optional使う
    InputValues values;
    rtos::Mutex mutex;

public:
    CachedInputs() : inputs(), set_values(false), values{ 0, 0, 0, 0 } {}

    CachedInputs(Inputs&& i) : inputs(i), set_values(false), values{ 0, 0, 0, 0 } {}

    void read() {
        std::lock_guard<rtos::Mutex> _guard(this->mutex);
        if (!this->set_values) {
            this->set_values = true;
        }
        this->inputs.read(this->values);
    }

    auto get() -> InputValues {
        std::lock_guard<rtos::Mutex> _guard(this->mutex);
        if (!this->set_values) {
            this->read();
        }
        return this->values;
    }
};

#endif // UMIUSI_INPUTS_HPP
