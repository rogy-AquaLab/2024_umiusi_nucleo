#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "BufferedSerial.h"
#include "EventFlags.h"
#include "ThisThread.h"
#include "Thread.h"

#include "umiusi/defered_delay.hpp"
#include "umiusi/inputs.hpp"
#include "umiusi/outputs.hpp"
#include "umiusi/state.hpp"

using namespace std::chrono_literals;

class OutputMachine {
private:
    Outputs     outputs;
    State       _state;
    rtos::Mutex state_mutex;

    void set_state(State s) {
        std::lock_guard _guard(this->state_mutex);
        this->_state = s;
    }

public:
    explicit OutputMachine() : outputs(), _state(State::SUSPEND), state_mutex() {}

    auto state() -> State {
        std::lock_guard _guard(this->state_mutex);
        return this->_state;
    }

    void set_powers(
        const std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM>& pulsewidths_us
    ) {
        if (this->state() != State::RUNNING) {
            return;
        }
        this->outputs.set_powers(pulsewidths_us);
    }

    void suspend() {
        this->outputs.reset();
        this->set_state(State::SUSPEND);
        this->outputs.deactivate();
    }

    void initialize() {
        if (this->state() == State::INITIALIZING) {
            return;
        }
        this->set_state(State::INITIALIZING);
        this->outputs.reset();
        this->outputs.setup();
        if (this->state() == State::INITIALIZING) {
            // setup前後で値が変化する可能性がある
            this->set_state(State::RUNNING);
        }
    }
};

int main() {
    constexpr size_t INPUTS_THREAD_STACK_SIZE = 1024;
    constexpr size_t SETUP_THREAD_STACK_SIZE  = 512;

    CachedInputs         inputs{};
    OutputMachine        output{};
    mbed::BufferedSerial pc(USBTX, USBRX);

    rtos::EventFlags trigger_setup{};
    unsigned char    setup_thread_stack[SETUP_THREAD_STACK_SIZE]   = {};
    unsigned char    inputs_thread_stack[INPUTS_THREAD_STACK_SIZE] = {};

    rtos::Thread setup_thread(
        osPriorityBelowNormal, SETUP_THREAD_STACK_SIZE, setup_thread_stack
    );
    rtos::Thread inputs_thread(
        osPriorityBelowNormal, INPUTS_THREAD_STACK_SIZE, inputs_thread_stack
    );

    // TODO: handle osStatus
    setup_thread.start([&output, &trigger_setup]() {
        while (true) {
            trigger_setup.wait_any(1, osWaitForever, false);
            output.initialize();
            trigger_setup.clear();
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
        // TODO: timeout
        ssize_t read = pc.read(&header, 1);
        if (read < 1) {
            continue;
        }
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
            trigger_setup.set(1);
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
