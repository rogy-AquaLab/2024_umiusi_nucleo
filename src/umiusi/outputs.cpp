#include "umiusi/outputs.hpp"
#include "umiusi/defered_delay.hpp"

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
    DeferedDelay _(2000);
    for (mbed::PwmOut& bldc : this->bldcs) {
        bldc.pulsewidth_us(100);
    }
}

void Outputs::setup() {
    this->activate();
    this->wake_up();
}

void Outputs::set_powers(
    const std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM >& pulsewidths_us
) {
    for (size_t i = 0; i < THRUSTER_NUM; ++i) {
        const uint16_t& bldc_us  = pulsewidths_us[i].first;
        const uint16_t& servo_us = pulsewidths_us[i].second;
        // TODO: C++17にしたらこう書けるようになる
        // const auto [bldc_us, servo_us] = pulsewidths_us[i];
        this->bldcs[i].pulsewidth_us(bldc_us);
        this->servos[i].pulsewidth_us(servo_us);
    }
}
