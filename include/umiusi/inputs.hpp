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
#include <cstdint>

#include "AnalogIn.h"

struct InputValues {
    uint16_t flex1;
    uint16_t flex2;
    uint16_t current;
    uint16_t voltage;

    InputValues() = delete;

    auto packet_data() const -> std::array<uint8_t, 8>;
};

class Inputs {
private:
    mbed::AnalogIn flex1;
    mbed::AnalogIn flex2;
    mbed::AnalogIn current;
    mbed::AnalogIn voltage;

public:
    Inputs();
    void read(InputValues& values);
    auto read() -> InputValues;
};

class CachedInputs {
private:
    Inputs inputs;
    bool   set_values;
    // FIXME: std::optional使う
    InputValues values;
    rtos::Mutex mutex;

public:
    CachedInputs();
    CachedInputs(Inputs&& i);
    void read();
    auto get() -> InputValues;
};

#endif // UMIUSI_INPUTS_HPP
