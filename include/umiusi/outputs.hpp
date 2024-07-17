#ifndef OUTPUTS_HPP
#define OUTPUTS_HPP

#include "mbed.h"
#include "umiusi/defereddelay.hpp"
#include <array>
#include <chrono>

#ifndef INIT_PIN

    #define INIT_PIN PA_1

#endif // INIT_PIN

#ifndef BLDC1_PIN

    #define BLDC1_PIN PA_12 // TIM1_CH2N

#endif // BLDC1_PIN

#ifndef BLDC2_PIN

    #define BLDC2_PIN PA_8 // TIM1_CH1

#endif // BLDC2_PIN

#ifndef BLDC3_PIN

    #define BLDC3_PIN PA_7_ALT1 // TIM17_CH1

#endif // BLDC3_PIN

#ifndef BLDC4_PIN

    #define BLDC4_PIN PA_6 // TIM3_CH1

#endif // BLDC4_PIN

#ifndef SERVO1_PIN

    #define SERVO1_PIN PB_0_ALT0 // TIM3_CH3

#endif // SERVO1_PIN

#ifndef SERVO2_PIN

    #define SERVO2_PIN PA_11_ALT0 // TIM1_CH4

#endif // SERVO2_PIN

#ifndef SERVO3_PIN

    #define SERVO3_PIN PB_5 // TIM3_CH2

#endif // SERVO3_PIN

#ifndef SERVO4_PIN

    #define SERVO4_PIN PA_3 // TIM15_CH2

#endif // SERVO4_PIN

constexpr size_t THRUSTER_NUM = 4;

class Outputs {
private:
    DigitalOut                       init_status;
    std::array<PwmOut, THRUSTER_NUM> bldcs;
    std::array<PwmOut, THRUSTER_NUM> servos;

public:
    // FIXME: ビルダーを与えたい
    Outputs();
    void activate();
    void deactivate();
    void wake_up();
    void setup();
    void set_powers(
        const std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM>& pulsewidths_us
    );
};

Outputs::Outputs() :
    init_status(INIT_PIN),
    bldcs{ PwmOut(BLDC1_PIN), PwmOut(BLDC2_PIN), PwmOut(BLDC3_PIN), PwmOut(BLDC4_PIN) },
    servos{
        PwmOut(SERVO1_PIN), PwmOut(SERVO2_PIN), PwmOut(SERVO3_PIN), PwmOut(SERVO4_PIN)
    } {
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
    for (PwmOut& bldc : this->bldcs) {
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

#endif
