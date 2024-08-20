#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

#include "BufferedSerial.h"
#include "events/Event.h"
#include "events/EventQueue.h"
#include "ThisThread.h"
#include "Thread.h"

#include "umiusi/defered_delay.hpp"
#include "umiusi/inputs.hpp"
#include "umiusi/outputs.hpp"

using namespace std::chrono_literals;

class WatchDog {
private:
    std::atomic_bool*      received_input;
    events::Event<void()>* suspend_event;

public:
    WatchDog(std::atomic_bool* received_input, events::Event<void()>* suspend_event) :
        received_input(received_input),
        suspend_event(suspend_event) {}

    void check() {
        const bool received = this->received_input->load(std::memory_order_acquire);
        if (!received) {
            this->suspend_event->post();
        }
    }
};

int main() {
    constexpr size_t EVENT_LOOP_THREAD_STACK_SIZE = 1024;
    constexpr size_t INPUTS_THREAD_STACK_SIZE = 1024;

    CachedInputs         inputs{};
    OutputMachine        output{};
    mbed::BufferedSerial pc(USBTX, USBRX);
    std::atomic_bool     received_input;

    unsigned char equeue_buffer[1024] = {};
    unsigned char event_loop_thread_stack[EVENT_LOOP_THREAD_STACK_SIZE] = {};
    unsigned char inputs_thread_stack[INPUTS_THREAD_STACK_SIZE] = {};

    events::EventQueue equeue(1024, equeue_buffer);
    rtos::Thread       event_loop_thread(
        osPriorityNormal,
        EVENT_LOOP_THREAD_STACK_SIZE,
        event_loop_thread_stack,
        "event_loop"
    );
    rtos::Thread inputs_thread(
        osPriorityBelowNormal3, INPUTS_THREAD_STACK_SIZE, inputs_thread_stack, "inputs"
    );
    // TODO: handle osStatus
    event_loop_thread.start(mbed::callback(&equeue, &events::EventQueue::dispatch_forever)
    );
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
    auto     initialize_event = equeue.event(&output, &OutputMachine::initialize);
    auto     suspend_event = equeue.event(&output, &OutputMachine::suspend);
    WatchDog watchdog(&received_input, &suspend_event);
    equeue.call_every(1s, &watchdog, &WatchDog::check);
    while (true) {
        DeferedDelay _delay(10ms);
        pc.sync();
        uint8_t header = 0;
        ssize_t read = pc.read(&header, 1);
        if (read < 1) {
            initialize_event.post();
            continue;
        }
        received_input.store(true, std::memory_order_release);
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
            initialize_event.post();
        } break;
        case 0xFF:
            // suspend
            suspend_event.post();
            break;
        default:
            // unexpected
            continue;
        }
    }
    return 0;
}
