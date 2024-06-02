#include <chrono>

#include "mbed.h"

#ifndef BLDC1_PIN
#define BLDC1_PIN PA_12
#endif // BLDC1_PIN

#ifndef BLDC2_PIN
#define BLDC2_PIN PA_8
#endif // BLDC2_PIN

#ifndef BLDC3_PIN
#define BLDC3_PIN PA_7
#endif // BLDC3_PIN

#ifndef BLDC4_PIN
#define BLDC4_PIN PA_6
#endif // BLDC4_PIN

#ifndef SERVO1_PIN
#define SERVO1_PIN PB_0
#endif // SERVO1_PIN

#ifndef SERVO2_PIN
#define SERVO2_PIN PA_11
#endif // SERVO2_PIN

#ifndef SERVO3_PIN
#define SERVO3_PIN PB_5
#endif // SERVO3_PIN

#ifndef SERVO4_PIN
#define SERVO4_PIN PA_6
#endif // SERVO4_PIN

struct DeferedDelay {
    const uint16_t duration_ms;

    DeferedDelay(uint16_t duration_ms): duration_ms(duration_ms) {}

    ~DeferedDelay() {
        ThisThread::sleep_for(std::chrono::milliseconds(duration_ms));
    }
};

int main() {
    PwmOut bldcs[4] = {
        PwmOut(BLDC1_PIN),
        PwmOut(BLDC2_PIN),
        PwmOut(BLDC3_PIN),
        PwmOut(BLDC4_PIN),
    };
    PwmOut servos[4] = {
            PwmOut(SERVO1_PIN),
            PwmOut(SERVO2_PIN),
            PwmOut(SERVO3_PIN),
            PwmOut(SERVO4_PIN),
    };
    BufferedSerial pc(USBTX, USBRX);
    // Init modules
    for (size_t i = 0; i < 4; ++i) {
        bldcs[i].period_ms(20);
        servos[i].period_ms(20);
    }
    // fake; TODO
    uint16_t flexsensor1_value = 0xF001;
    uint16_t flexsensor2_value = 0xF020;
    uint16_t current_value = 0x0CAE;
    uint16_t voltage_value = 0xAE00;
    while (true) {
        DeferedDelay _delay(10);
        pc.sync();
        uint8_t header = 0;
        ssize_t read = pc.read(&header, 1);
        if (read < 1) {
            continue;
        }
        switch (header) {
        case 0: {
            // write
            uint8_t buffer[16] = {};
            pc.read(buffer, 16);
            for (size_t i = 0; i < 4; ++i) {
                uint16_t pulsewidth_us_lsb = static_cast<uint16_t>(buffer[i * 2 + 0]);
                uint16_t pulsewidth_us_msb = static_cast<uint16_t>(buffer[i * 2 + 1]);
                bldcs[i].pulsewidth_us((pulsewidth_us_lsb << 0) | (pulsewidth_us_msb << 8));
            }
            for (size_t i = 0; i < 4; ++i) {
                uint16_t pulsewidth_us_lsb = static_cast<uint16_t>(buffer[i * 2 + 0 + 8]);
                uint16_t pulsewidth_us_msb = static_cast<uint16_t>(buffer[i * 2 + 1 + 8]);
                servos[i].pulsewidth_us((pulsewidth_us_lsb << 0) | (pulsewidth_us_msb << 8));
            }
        }
            break;
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
        }
        break;
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
