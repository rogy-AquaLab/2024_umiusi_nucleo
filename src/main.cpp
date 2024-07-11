#include "mbed.h"
#include "nucleo/defereddelay.hpp"
#include "nucleo/outputs.hpp"
#include "ThisThread.h"
#include <array>
#include <chrono>

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

constexpr size_t THRUSTER_NUM = 4;

int main() {
    Outputs        outputs;
    BufferedSerial pc(USBTX, USBRX);
    outputs.setup();
    // fake; TODO
    uint16_t flexsensor1_value = 0xF001;
    uint16_t flexsensor2_value = 0xF020;
    uint16_t current_value     = 0x0CAE;
    uint16_t voltage_value     = 0xAE00;
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
            uint8_t buffer[8] = {
                static_cast<uint8_t>((flexsensor1_value >> 0) & 0xFF),
                static_cast<uint8_t>((flexsensor1_value >> 8) & 0xFF),
                static_cast<uint8_t>((flexsensor2_value >> 0) & 0xFF),
                static_cast<uint8_t>((flexsensor2_value >> 8) & 0xFF),
                static_cast<uint8_t>((current_value >> 0) & 0xFF),
                static_cast<uint8_t>((current_value >> 8) & 0xFF),
                static_cast<uint8_t>((voltage_value >> 0) & 0xFF),
                static_cast<uint8_t>((voltage_value >> 8) & 0xFF),
            };
            pc.write(buffer, 8);
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
