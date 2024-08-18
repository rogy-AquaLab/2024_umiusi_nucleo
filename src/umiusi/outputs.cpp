#include "umiusi/outputs.hpp"
#include "umiusi/defered_delay.hpp"

using namespace std::chrono_literals;

Outputs::Outputs() :
    init_status(INIT_PIN),
    bldcs{ mbed::PwmOut(BLDC1_PIN),
           mbed::PwmOut(BLDC2_PIN),
           mbed::PwmOut(BLDC3_PIN),
           mbed::PwmOut(BLDC4_PIN) },
    servos{ mbed::PwmOut(SERVO1_PIN),
            mbed::PwmOut(SERVO2_PIN),
            mbed::PwmOut(SERVO3_PIN),
            mbed::PwmOut(SERVO4_PIN) } {
    for (size_t i = 0; i < THRUSTER_NUM; ++i) {
        bldcs[i].period_ms(20);
        bldcs[i].pulsewidth_us(0);
        servos[i].period_ms(20);
        servos[i].pulsewidth_us(0);
    }
}

void Outputs::activate() {
    this->init_status.write(1);
}

void Outputs::deactivate() {
    this->init_status.write(0);
}

/// ESC の起動待ち
void Outputs::wake_up() {
    DeferedDelay _(2s);
    for (mbed::PwmOut& bldc : this->bldcs) {
        bldc.pulsewidth_us(100);
    }
}

void Outputs::setup() {
    this->activate();
    this->wake_up();
}

void Outputs::set_powers(
    const std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM>& pulsewidths_us
) {
    for (size_t i = 0; i < THRUSTER_NUM; ++i) {
        const auto [bldc_us, servo_us] = pulsewidths_us[i];
        this->bldcs[i].pulsewidth_us(bldc_us);
        this->servos[i].pulsewidth_us(servo_us);
    }
}

void Outputs::reset() {
    static constexpr std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM>
        reset_pulsewidths_us{};
    this->set_powers(reset_pulsewidths_us);
}

void OutputMachine::set_state(State s) {
    std::lock_guard _guard(this->state_mutex);
    this->_state = s;
}

OutputMachine::OutputMachine() : outputs(), _state(State::SUSPEND), state_mutex() {}

auto OutputMachine::state() -> State {
    std::lock_guard _guard(this->state_mutex);
    return this->_state;
}

void OutputMachine::set_powers(
    const std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM>& pulsewidths_us
) {
    if (this->state() != State::RUNNING) {
        return;
    }
    this->outputs.set_powers(pulsewidths_us);
}

void OutputMachine::suspend() {
    this->outputs.reset();
    this->set_state(State::SUSPEND);
    this->outputs.deactivate();
}

void OutputMachine::initialize() {
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
