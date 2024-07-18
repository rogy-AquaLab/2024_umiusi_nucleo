#include <array>
#include <cstdint>

#include "AnalogIn.h"
#include "mbed.h"
#include "Mutex.h"
#include "PinNames.h"
#include "ThisThread.h"
#include "Thread.h"

#include "umiusi/defereddelay.hpp"
#include "umiusi/outputs.hpp"

// Pin Map:
// targets/TARGET_STM/TARGET_STM32F3/TARGET_STM32F3x8/TARGET_NUCLEO_F303K8/PeripheralPins.c
// in https://github.com/ARMmbed/mbed-os/tree/869f0d7

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

class MutexGuard {
private:
    rtos::Mutex& mutex;

public:
    MutexGuard(rtos::Mutex& mutex) : mutex(mutex) {
        this->mutex.lock();
    }

    ~MutexGuard() {
        this->mutex.unlock();
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
    rtos::Mutex mutex;

public:
    CachedInputs() : inputs(), set_values(false), values{ 0, 0, 0, 0 } {}

    CachedInputs(Inputs&& i) : inputs(i), set_values(false), values{ 0, 0, 0, 0 } {}

    void read() {
        MutexGuard _guard(this->mutex);
        if (!this->set_values) {
            this->set_values = true;
        }
        this->inputs.read(this->values);
    }

    auto get() -> InputValues {
        MutexGuard _guard(this->mutex);
        if (!this->set_values) {
            this->read();
        }
        return this->values;
    }
};

int main() {
    constexpr size_t INPUTS_THREAD_STACK_SIZE = 1024;

    unsigned char inputs_thread_stack[INPUTS_THREAD_STACK_SIZE] = {};
    rtos::Thread  inputs_thread(
        osPriorityNormal, INPUTS_THREAD_STACK_SIZE, inputs_thread_stack
    );

    CachedInputs   inputs;
    Outputs        outputs;
    BufferedSerial pc(USBTX, USBRX);

    osStatus status = inputs_thread.start([&inputs]() {
        while (true) {
            inputs.read();
            ThisThread::sleep_for(10ms);
        }
    });
    if (status != osOK) {
        // 本来ここに入ることはあってはならないが、一応書いておく
        // スレッドを開始することができなかったので、入力が読み取られなくなる
        // そのため、一度だけ値を読んでおくことにする
        // 得られる値を見て実行状況を判断すること
        inputs.read();
    }
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
            std::array<uint8_t, 8> buffer = inputs.get().packet_data();
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
    return status;
}
