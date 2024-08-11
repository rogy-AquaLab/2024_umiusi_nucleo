#ifndef OUTPUTS_HPP
#define OUTPUTS_HPP

#include <array>

#include <DigitalOut.h>
#include <PwmOut.h>

// Pin Map:
// targets/TARGET_STM/TARGET_STM32F3/TARGET_STM32F3x8/TARGET_NUCLEO_F303K8/PeripheralPins.c
// in https://github.com/ARMmbed/mbed-os/tree/869f0d7

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
    mbed::DigitalOut                       init_status;
    std::array<mbed::PwmOut, THRUSTER_NUM> bldcs;
    std::array<mbed::PwmOut, THRUSTER_NUM> servos;

public:
    // FIXME: ビルダーを与えたい
    Outputs();
    void activate();
    void deactivate();
    /// BLDC(に繋がっているESC)を起動する。完了までに2秒を要する。
    /// 完了時点でbldcのパルス幅は各100usとなる
    void wake_up();
    /// activate->wake_up
    void setup();
    void set_powers(
        const std::array<std::pair<uint16_t, uint16_t>, THRUSTER_NUM>& pulsewidths_us
    );
    void reset();
};

#endif
