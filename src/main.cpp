#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "BufferedSerial.h"
#include "ThisThread.h"
#include "Thread.h"

#include "umiusi/defered_delay.hpp"
#include "umiusi/inputs.hpp"
#include "umiusi/outputs.hpp"
#include "umiusi/state.hpp"

using namespace std::chrono_literals;

class Machine {
private:
    static constexpr size_t INPUTS_THREAD_STACK_SIZE = 1024;
    static constexpr size_t SETUP_THREAD_STACK_SIZE  = 512;

    CachedInputs         inputs;
    Outputs              outputs;
    mbed::BufferedSerial pc;

    State       state;
    rtos::Mutex state_mutex;

    std::array<unsigned char, SETUP_THREAD_STACK_SIZE>  setup_thread_stack;
    std::array<unsigned char, INPUTS_THREAD_STACK_SIZE> inputs_thread_stack;

    rtos::Thread setup_thread;
    rtos::Thread inputs_thread;

    void reset_outputs() {
        static constexpr std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM> reset_pulsewidths_us{};
        this->outputs.set_powers(reset_pulsewidths_us);
    }

    void op_write_outputs() {
        std::array<uint8_t, 16> buffer{}; // FIXME: 16 == THRUSTER_NUM * 2 * 2
        pc.read(buffer.data(), 16);
        std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM> pulsewidths_us{};
        {
            std::lock_guard<rtos::Mutex> _guard(this->state_mutex);
            if (this->state != State::RUNNING) {
                return;
            }
        }
        for (size_t i = 0; i < THRUSTER_NUM; ++i) {
            // bldc
            uint16_t pulsewidth_us_lsb = static_cast<uint16_t>(buffer[i * 2 + 0]);
            uint16_t pulsewidth_us_msb = static_cast<uint16_t>(buffer[i * 2 + 1]);
            pulsewidths_us[i].first = (pulsewidth_us_lsb << 0) | (pulsewidth_us_msb << 8);
            // servo
            pulsewidth_us_lsb = static_cast<uint16_t>(buffer[i * 2 + 0 + THRUSTER_NUM * 2]
            );
            pulsewidth_us_msb = static_cast<uint16_t>(buffer[i * 2 + 1 + THRUSTER_NUM * 2]
            );
            pulsewidths_us[i].second = (pulsewidth_us_lsb << 0)
                                       | (pulsewidth_us_msb << 8);
        }
        outputs.set_powers(pulsewidths_us);
    }

    void op_read_inputs() {
        std::array<uint8_t, 8> buffer = inputs.get().packet_data();
        pc.write(buffer.data(), 8);
    }

    void op_read_state() {
        std::lock_guard<rtos::Mutex> _guard(this->state_mutex);
        uint8_t                      state_val = static_cast<uint8_t>(this->state);
        this->pc.write(&state_val, 1);
    }

    auto op_trigger_setup() -> osStatus {
        std::lock_guard<rtos::Mutex> _guard(this->state_mutex);
        if (this->state == State::INITIALIZING) {
            return osOK;
        }
        this->state = State::INITIALIZING;
        this->reset_outputs();
        return this->setup_thread.start([this]() {
            this->outputs.setup();
            {
                std::lock_guard<rtos::Mutex> _guard(this->state_mutex);
                this->state = State::RUNNING;
            }
        });
    }

    void op_suspend() {
        std::lock_guard<rtos::Mutex> _guard(this->state_mutex);
        this->state = State::SUSPEND;
        this->reset_outputs();
        this->outputs.deactivate();
    }

public:
    explicit Machine() :
        inputs(),
        outputs(),
        pc(USBTX, USBRX),
        state(State::INITIALIZING),
        state_mutex(),
        setup_thread_stack(),
        inputs_thread_stack(),
        setup_thread(
            osPriorityNormal, SETUP_THREAD_STACK_SIZE, setup_thread_stack.data()
        ),
        inputs_thread(
            osPriorityNormal, INPUTS_THREAD_STACK_SIZE, inputs_thread_stack.data()
        ) {}

    auto start_inputs() -> osStatus {
        return this->inputs_thread.start([this] {
            this->inputs.read();
            rtos::ThisThread::sleep_for(10ms);
        });
    }

    void loop() {
        DeferedDelay _delay(10);
        pc.sync();
        uint8_t header = 0;
        // TODO: timeout
        ssize_t read = pc.read(&header, 1);
        if (read < 1) {
            this->op_suspend();
            return;
        }
        // なぜかこれがないと動かない
        rtos::ThisThread::sleep_for(20ms);
        switch (header) {
        case 0:
            // write
            this->op_write_outputs();
            break;
        case 1:
            // read
            this->op_read_inputs();
            break;
        case 2:
            // read state
            this->op_read_state();
            break;
        case 0xFE:
            // (re)start
            // TODO: handle returned osStatus
            this->op_trigger_setup();
            break;
        case 0xFF:
            // suspend
            this->op_suspend();
            break;
        default:
            // unexpected
            return;
        }
    }
};

int main() {
    Machine machine{};
    machine.start_inputs();
    while (true) {
        machine.loop();
    }
    return 0;
}
