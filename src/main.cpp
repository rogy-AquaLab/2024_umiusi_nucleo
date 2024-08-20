#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

#include "BufferedSerial.h"
#include "events/EventQueue.h"
#include "events/Event.h"

#include "umiusi/inputs.hpp"
#include "umiusi/outputs.hpp"

using namespace std::chrono_literals;

int main() {
    constexpr std::size_t EQUEUE_BUFFER_SIZE = 32 * EVENTS_EVENT_SIZE;

    unsigned char equeue_buffer[EQUEUE_BUFFER_SIZE] = {};

    CachedInputs         inputs{};
    OutputMachine        output{};
    mbed::BufferedSerial pc(USBTX, USBRX);
    std::atomic_bool     received_order;
    pc.set_blocking(false);
    inputs.read();

    events::EventQueue equeue(EQUEUE_BUFFER_SIZE, equeue_buffer);
    auto initialize_event = equeue.event(mbed::callback([&output, &equeue]() {
        if (output.state() != State::INITIALIZING) {
            output.initialize_with_equeue(equeue);
        }
    }));
    auto suspend_event = equeue.event(mbed::callback([&output]() {
        output.suspend();
    }));

    const auto process_order = [&pc, &inputs, &output, &initialize_event, &suspend_event](std::uint8_t header) {
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
                return;
            }
            initialize_event.call();
        } break;
        case 0xFF:
            // suspend
            suspend_event.call();
            break;
        default:
            // unexpected
            return;
        }
    };
    equeue.call_every(30ms, [&equeue, &pc, &received_order, &process_order]() {
        std::uint8_t header = 0;
        ssize_t res = pc.read(&header, 1);
        if (res < 1) {
            return;
        }
        received_order.store(true, std::memory_order_release);
        // FIXME: なぜか20msほど遅らせないとうまくいかない
        equeue.call_in(20ms, process_order, header);
    });

    equeue.call_every(10ms, [&inputs]() {
        inputs.read();
    });

    equeue.call_every(1s, [&received_order, &suspend_event]() {
        const bool received = received_order.load(std::memory_order_acquire);
        received_order.store(false, std::memory_order_release);
        if (!received) {
            suspend_event.call();
        }
    });

    equeue.dispatch_forever();
    return 0;
}
