#include <array>
#include <chrono>
#include <cstdint>

#include "AnalogIn.h"
#include "CriticalSectionLock.h"
#include "mbed.h"
#include "PinNames.h"
#include "ThisThread.h"

// Pin Map:
// targets/TARGET_STM/TARGET_STM32F3/TARGET_STM32F3x8/TARGET_NUCLEO_F303K8/PeripheralPins.c
// in https://github.com/ARMmbed/mbed-os/tree/869f0d7

#ifndef INIT_PIN

    #define INIT_PIN PA_1

#endif // INIT_PIN

#ifndef BLDC1_PIN

    #define BLDC1_PIN PA_12 // TIM1_CH2N

#endif // BLDC1_PIN

#ifndef BLDC2_PIN

    #define BLDC2_PIN PA_8 // TIM1_CH1

#endif // BLDC2_PIN

#ifndef BLDC3_PIN

    #define BLDC3_PIN PA_7_ALT1 // TIM17_CH1

#endif // BLDC3_PIN

#ifndef BLDC4_PIN

    #define BLDC4_PIN PA_6 // TIM3_CH1

#endif // BLDC4_PIN

#ifndef SERVO1_PIN

    #define SERVO1_PIN PB_0_ALT0 // TIM3_CH3

#endif // SERVO1_PIN

#ifndef SERVO2_PIN

    #define SERVO2_PIN PA_11_ALT0 // TIM1_CH4

#endif // SERVO2_PIN

#ifndef SERVO3_PIN

    #define SERVO3_PIN PB_5 // TIM3_CH2

#endif // SERVO3_PIN

#ifndef SERVO4_PIN

    #define SERVO4_PIN PA_3 // TIM15_CH2

#endif // SERVO4_PIN

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

constexpr size_t THRUSTER_NUM = 4;

struct DeferedDelay {
    const uint16_t duration_ms;

    DeferedDelay(uint16_t duration_ms) : duration_ms(duration_ms) {}

    ~DeferedDelay() {
        ThisThread::sleep_for(std::chrono::milliseconds(duration_ms));
    }
};

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

public:
    CachedInputs() : inputs(), set_values(false), values{ 0, 0, 0, 0 } {}

    CachedInputs(Inputs&& i) : inputs(i), set_values(false), values{ 0, 0, 0, 0 } {}

    void read() {
        if (!this->set_values) {
            this->set_values = true;
        }
        this->inputs.read(this->values);
    }

    auto get() -> InputValues {
        mbed::CriticalSectionLock _lock;
        if (!this->set_values) {
            this->read();
        }
        return this->values;
    }
};

class Outputs {
private:
    DigitalOut init_status;

    std::array<PwmOut, THRUSTER_NUM> bldcs;
    std::array<PwmOut, THRUSTER_NUM> servos;

public:
    // FIXME: ビルダーを与えたい
    Outputs() :
        init_status(INIT_PIN),
        bldcs{
            PwmOut(BLDC1_PIN), PwmOut(BLDC2_PIN), PwmOut(BLDC3_PIN), PwmOut(BLDC4_PIN)
        },
        servos{
            PwmOut(SERVO1_PIN), PwmOut(SERVO2_PIN), PwmOut(SERVO3_PIN), PwmOut(SERVO4_PIN)
        } {
        for (size_t i = 0; i < THRUSTER_NUM; ++i) {
            this->bldcs[i].period_ms(20);
            this->bldcs[i].pulsewidth_us(0);
            this->servos[i].period_ms(20);
            this->servos[i].pulsewidth_us(0);
        }
    }

    void activate() {
        this->init_status.write(1);
    }

    void deactivate() {
        this->init_status.write(0);
    }

    /// ESC の起動待ち
    void wake_up() {
        DeferedDelay _(2000);
        for (PwmOut& bldc : this->bldcs) {
            bldc.pulsewidth_us(100);
        }
    }

    void setup() {
        this->activate();
        this->wake_up();
    }

    void set_powers(
        const std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM >& pulsewidths_us
    ) {
        for (size_t i = 0; i < THRUSTER_NUM; ++i) {
            const uint16_t& bldc_us  = pulsewidths_us[i].first;
            const uint16_t& servo_us = pulsewidths_us[i].second;
            // TODO: C++17にしたらこう書けるようになる
            // const auto [bldc_us, servo_us] = pulsewidths_us[i];
            this->bldcs[i].pulsewidth_us(bldc_us);
            this->servos[i].pulsewidth_us(servo_us);
        }
    }
};

int main() {
    Inputs         inputs;
    Outputs        outputs;
    BufferedSerial pc(USBTX, USBRX);
    outputs.setup();
    while (true) {
        DeferedDelay _delay(10);
        pc.sync();
        uint8_t header = 0;
        ssize_t read   = pc.read(&header, 1);
        if (read < 1) {
            continue;
        }
        // なぜかこれがないと動かない
        ThisThread::sleep_for(20ms);
        switch (header) {
        case 0: {
            // write
            std::array<uint8_t, 16> buffer{}; // FIXME: 16 == THRUSTER_NUM * 2 * 2
            pc.read(buffer.data(), 16);
            std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM> pulsewidths_us{};
            for (size_t i = 0; i < THRUSTER_NUM; ++i) {
                // bldc
                uint16_t pulsewidth_us_lsb = static_cast<uint16_t>(buffer[i * 2 + 0]);
                uint16_t pulsewidth_us_msb = static_cast<uint16_t>(buffer[i * 2 + 1]);
                pulsewidths_us[i].first    = (pulsewidth_us_lsb << 0)
                                          | (pulsewidth_us_msb << 8);
                // servo
                pulsewidth_us_lsb
                    = static_cast<uint16_t>(buffer[i * 2 + 0 + THRUSTER_NUM * 2]);
                pulsewidth_us_msb
                    = static_cast<uint16_t>(buffer[i * 2 + 1 + THRUSTER_NUM * 2]);
                pulsewidths_us[i].second = (pulsewidth_us_lsb << 0)
                                           | (pulsewidth_us_msb << 8);
            }
            outputs.set_powers(pulsewidths_us);
        } break;
        case 1: {
            // read
            std::array<uint8_t, 8> buffer = inputs.read().packet_data();
            pc.write(buffer.data(), 8);
        } break;
        case 0xFF:
            // quit
            goto END;
        default:
            // unexpected
            continue;
        }
    }
END:
    return 0;
}
