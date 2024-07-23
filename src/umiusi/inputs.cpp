#include <array>
#include <cstdint>
#include <mutex>

#include "umiusi/inputs.hpp"

InputValues::InputValues(
    uint16_t flex1, uint16_t flex2, uint16_t current, uint16_t voltage
) :
    flex1(flex1),
    flex2(flex2),
    current(current),
    voltage(voltage) {}

auto InputValues::packet_data() const -> std::array<uint8_t, 8> {
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

// FIXME: ビルダーを与えたい
Inputs::Inputs() :
    flex1(FLEX1_PIN),
    flex2(FLEX2_PIN),
    current(CURRENT_PIN),
    voltage(VOLTAGE_PIN) {}

void Inputs::read(InputValues& values) {
    values.flex1   = this->flex1.read_u16();
    values.flex2   = this->flex2.read_u16();
    values.current = this->current.read_u16();
    values.voltage = this->voltage.read_u16();
}

auto Inputs::read() -> InputValues {
    return InputValues{ this->flex1.read_u16(),
                        this->flex2.read_u16(),
                        this->current.read_u16(),
                        this->voltage.read_u16() };
}

CachedInputs::CachedInputs() : inputs(), set_values(false), values{ 0, 0, 0, 0 } {}

CachedInputs::CachedInputs(Inputs&& i) :
    inputs(i),
    set_values(false),
    values{ 0, 0, 0, 0 } {}

void CachedInputs::read() {
    std::lock_guard<rtos::Mutex> _guard(this->mutex);
    if (!this->set_values) {
        this->set_values = true;
    }
    this->inputs.read(this->values);
}

auto CachedInputs::get() -> InputValues {
    std::lock_guard<rtos::Mutex> _guard(this->mutex);
    if (!this->set_values) {
        this->read();
    }
    return this->values;
}
