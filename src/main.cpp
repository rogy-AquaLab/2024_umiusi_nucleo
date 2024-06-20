#include <array>
#include <chrono>

#include "mbed.h"
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

constexpr size_t THRUSTER_NUM = 4;

struct DeferedDelay {
    const uint16_t duration_ms;

    DeferedDelay(uint16_t duration_ms) : duration_ms(duration_ms) {}

    ~DeferedDelay() {
        ThisThread::sleep_for(std::chrono::milliseconds(duration_ms));
    }
};

// Takes 2 seconds
void wake_up_bldcs(std::array<PwmOut, THRUSTER_NUM>& bldc_power_pwms) {
    DeferedDelay _delay(2000);
    for (PwmOut& bldc : bldc_power_pwms) {
        bldc.pulsewidth_us(100);
    }
}

int main() {
    DigitalOut init_status(INIT_PIN);

    std::array<PwmOut, THRUSTER_NUM> bldcs{
        PwmOut(BLDC1_PIN),
        PwmOut(BLDC2_PIN),
        PwmOut(BLDC3_PIN),
        PwmOut(BLDC4_PIN),
    };
    std::array<PwmOut, THRUSTER_NUM> servos{
        PwmOut(SERVO1_PIN),
        PwmOut(SERVO2_PIN),
        PwmOut(SERVO3_PIN),
        PwmOut(SERVO4_PIN),
    };
    BufferedSerial pc(USBTX, USBRX);
    // Init modules
    init_status = 1;
    for (size_t i = 0; i < THRUSTER_NUM; ++i) {
        bldcs[i].period_ms(20);
        servos[i].period_ms(20);
    }
    wake_up_bldcs(bldcs);
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
            uint8_t buffer[16] = {};
            pc.read(buffer, 16);
            for (size_t i = 0; i < THRUSTER_NUM; ++i) {
                uint16_t pulsewidth_us_lsb = static_cast<uint16_t>(buffer[i * 2 + 0]);
                uint16_t pulsewidth_us_msb = static_cast<uint16_t>(buffer[i * 2 + 1]);
                bldcs[i].pulsewidth_us(
                    (pulsewidth_us_lsb << 0) | (pulsewidth_us_msb << 8)
                );
            }
            for (size_t i = 0; i < THRUSTER_NUM; ++i) {
                uint16_t pulsewidth_us_lsb = static_cast<uint16_t>(buffer[i * 2 + 0 + 8]);
                uint16_t pulsewidth_us_msb = static_cast<uint16_t>(buffer[i * 2 + 1 + 8]);
                servos[i].pulsewidth_us(
                    (pulsewidth_us_lsb << 0) | (pulsewidth_us_msb << 8)
                );
            }
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
