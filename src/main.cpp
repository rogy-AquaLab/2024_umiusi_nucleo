#include <array>
#include <cstdint>
#include <mutex>

#include "BufferedSerial.h"
#include "EventFlags.h"
#include "ThisThread.h"
#include "Thread.h"

#include "umiusi/defered_delay.hpp"
#include "umiusi/inputs.hpp"
#include "umiusi/outputs.hpp"

using namespace std::chrono_literals;

int main() {
    constexpr uint32_t TRIGGER_INITIALIZE_FLAG = 0b0001;
    constexpr uint32_t RECEIVED_INPUT_FLAG = 0b0010;
    constexpr size_t   INPUTS_THREAD_STACK_SIZE = 1024;
    constexpr size_t   WATCH_THREAD_STACK_SIZE = 1024;

    CachedInputs         inputs{};
    OutputMachine        output{};
    mbed::BufferedSerial pc(USBTX, USBRX);

    rtos::EventFlags flags{};
    unsigned char    watch_flags_thread_stack[WATCH_THREAD_STACK_SIZE] = {};
    unsigned char    inputs_thread_stack[INPUTS_THREAD_STACK_SIZE] = {};

    rtos::Thread watch_flags_thread(
        osPriorityBelowNormal, WATCH_THREAD_STACK_SIZE, watch_flags_thread_stack
    );
    rtos::Thread inputs_thread(
        osPriorityBelowNormal, INPUTS_THREAD_STACK_SIZE, inputs_thread_stack
    );
    // TODO: handle osStatus
    watch_flags_thread.start([&output, &flags]() {
        constexpr uint32_t WATCH_FLAGS = TRIGGER_INITIALIZE_FLAG | RECEIVED_INPUT_FLAG;
        while (true) {
            const uint32_t res = flags.wait_any_for(WATCH_FLAGS, 5s);
            if ((res & TRIGGER_INITIALIZE_FLAG) != 0) {
                output.initialize();
            } else if ((res & RECEIVED_INPUT_FLAG) != 0) {
                // do nothing
            } else {
                // received no inputs for 5s
                output.suspend();
            }
        }
    });
    osStatus inputs_thread_status = inputs_thread.start([&inputs]() {
        while (true) {
            inputs.read();
            rtos::ThisThread::sleep_for(10ms);
        }
    });
    if (inputs_thread_status != osOK) {
        // 本来ここに入ることはあってはならないが、一応書いておく
        // スレッドを開始することができなかったので、入力が読み取られなくなる
        // そのため、一度だけ値を読んでおくことにする
        // 得られる値を見て実行状況を判断すること
        inputs.read();
    }
    while (true) {
        DeferedDelay _delay(10ms);
        pc.sync();
        uint8_t header = 0;
        ssize_t read = pc.read(&header, 1);
        if (read < 1) {
            continue;
        }
        flags.set(RECEIVED_INPUT_FLAG);
        // なぜかこれがないと動かない
        rtos::ThisThread::sleep_for(20ms);
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
                pulsewidths_us[i].first = (pulsewidth_us_lsb << 0)
                                          | (pulsewidth_us_msb << 8);
                // servo
                pulsewidth_us_lsb
                    = static_cast<uint16_t>(buffer[i * 2 + 0 + THRUSTER_NUM * 2]);
                pulsewidth_us_msb
                    = static_cast<uint16_t>(buffer[i * 2 + 1 + THRUSTER_NUM * 2]);
                pulsewidths_us[i].second = (pulsewidth_us_lsb << 0)
                                           | (pulsewidth_us_msb << 8);
            }
            output.set_powers(pulsewidths_us);
        } break;
        case 1: {
            // read
            std::array<uint8_t, 8> buffer = inputs.get().packet_data();
            pc.write(buffer.data(), 8);
        } break;
        case 2: {
            // read state
            uint8_t state_val = static_cast<uint8_t>(output.state());
            pc.write(&state_val, 1);
        } break;
        case 0xFE: {
            // (re)start
            if (output.state() == State::INITIALIZING) {
                continue;
            }
            flags.set(TRIGGER_INITIALIZE_FLAG);
        } break;
        case 0xFF:
            // suspend
            output.suspend();
            break;
        default:
            // unexpected
            continue;
        }
    }
    return 0;
}
