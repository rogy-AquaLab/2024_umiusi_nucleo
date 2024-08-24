#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

#include "BufferedSerial.h"
#include "events/Event.h"
#include "events/EventQueue.h"

#include "umiusi/inputs.hpp"
#include "umiusi/outputs.hpp"
#include "umiusi/trylock_guard.hpp"

using namespace std::chrono_literals;

int main() {
    constexpr std::size_t EQUEUE_BUFFER_SIZE = 32 * EVENTS_EVENT_SIZE;

    unsigned char equeue_buffer[EQUEUE_BUFFER_SIZE] = {};

    CachedInputs         inputs{};
    OutputMachine        output{};
    mbed::BufferedSerial pc(USBTX, USBRX);
    rtos::Mutex          pc_mutex{};
    std::atomic_bool     received_order;
    pc.set_blocking(false);
    inputs.read();

    events::EventQueue equeue(EQUEUE_BUFFER_SIZE, equeue_buffer);

    auto trigger_initialize = [&output, &equeue]() {
        if (output.state() != State::INITIALIZING) {
            output.initialize_with_equeue(equeue);
        }
    };

    auto write_order_event = equeue.event(mbed::callback([&pc, &pc_mutex, &output]() {
        std::lock_guard<rtos::Mutex> _guard(pc_mutex);
        std::array<uint8_t, 16>      buffer{}; // FIXME: 16 == THRUSTER_NUM * 2 * 2
        pc.read(buffer.data(), 16);
        std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM> pulsewidths_us{};
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
        output.set_powers(pulsewidths_us);
        pc.write("\x00", 1);
    }));
    auto read_order_event = equeue.event(mbed::callback([&pc, &pc_mutex, &inputs]() {
        std::lock_guard<rtos::Mutex> _guard(pc_mutex);
        std::array<uint8_t, 8>       buffer = inputs.get().packet_data();
        pc.write(buffer.data(), 8);
        pc.write("\x01", 1);
    }));
    auto read_state_order_event
        = equeue.event(mbed::callback([&pc, &pc_mutex, &output]() {
              std::lock_guard<rtos::Mutex> _guard(pc_mutex);
              uint8_t state_val = static_cast<uint8_t>(output.state());
              pc.write(&state_val, 1);
              pc.write("\x02", 1);
          }));
    auto initialize_order_event
        = equeue.event(mbed::callback([&pc, &pc_mutex, &trigger_initialize]() {
              std::lock_guard<rtos::Mutex> _guard(pc_mutex);
              trigger_initialize();
              pc.write("\xFE", 1);
          }));
    auto suspend_order_event = equeue.event(mbed::callback([&pc, &pc_mutex, &output]() {
        std::lock_guard<rtos::Mutex> _guard(pc_mutex);
        output.suspend();
        pc.write("\xFF", 1);
    }));

    const auto process_order = [&write_order_event,
                                &read_order_event,
                                &read_state_order_event,
                                &initialize_order_event,
                                &suspend_order_event](std::uint8_t header) {
        switch (header) {
        case 0:    write_order_event.call(); break;
        case 1:    read_order_event.call(); break;
        case 2:    read_state_order_event.call(); break;
        case 0xFE: initialize_order_event.call(); break;
        case 0xFF: suspend_order_event.call(); break;
        default:
            // unexpected
            return;
        }
    };
    // FIXME: なぜか30msより短いと挙動がおかしくなる
    equeue.call_every(30ms, [&equeue, &pc, &pc_mutex, &received_order, &process_order]() {
        TrylockGuard pc_guard(pc_mutex);
        if (!pc_guard.locked()) {
            return;
        }
        std::uint8_t header = 0;
        ssize_t      res = pc.read(&header, 1);
        if (res < 1) {
            return;
        }
        received_order.store(true, std::memory_order_release);
        equeue.call(process_order, header);
    });

    equeue.call_every(10ms, [&inputs]() {
        inputs.read();
    });

    equeue.call_every(1s, [&received_order, &output]() {
        const bool received = received_order.load(std::memory_order_acquire);
        received_order.store(false, std::memory_order_release);
        if (!received) {
            output.suspend();
        }
    });

    equeue.dispatch_forever();
    return 0;
}
