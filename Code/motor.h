#pragma once

#include <stdint.h>
#include "pico/time.h"
#include "pico/types.h"

// PWM speed control for the 12 V pulling motor with smooth start and stop.
//
// The driver stage (BC547 -> IRF3205) is inverting: pin high = motor off,
// pin low = motor on. The PWM output polarity is inverted in hardware, so
// everywhere in this class a speed of 0.0 means stopped and 1.0 full speed.
//
// Speed changes are ramped by a repeating timer (IRQ) along an S-curve, so
// start(), stop() and setMaxSpeed() return immediately and the main loop
// keeps running while the motor accelerates or brakes.
class Motor {
public:
    static constexpr uint32_t kDefaultRampMs = 2500;
    static constexpr uint32_t kDefaultPwmHz = 2000;

    // ramp_ms: how long start()/stop()/setMaxSpeed() take to reach the new speed.
    Motor(uint pwm_pin, uint32_t ramp_ms = kDefaultRampMs, uint32_t pwm_hz = kDefaultPwmHz);

    // Configures the PWM with the motor stopped. Call as early as possible:
    // until then the pin is low, which the inverting driver treats as full speed.
    void init();

    // Smoothly accelerates to the max speed.
    void start();

    // Smoothly brakes to a standstill.
    void stop();

    // Max speed in percent (0..100). If the motor is running, it smoothly
    // moves to the new speed; otherwise the value is used by the next start().
    void setMaxSpeed(uint8_t percent);
    uint8_t getMaxSpeed() const;

    // Current (ramped) speed, 0.0 = stopped .. 1.0 = full.
    float getSpeed() const;

    // True while the motor turns or is still ramping; false once fully stopped.
    bool isRunning() const;

    // Drives at exactly `speed` (0.0 .. 1.0, not limited by the max speed),
    // reaching it over ramp_ms (0 = at once). 0.0 stops the motor. Used to
    // hold a measured wire speed, e.g. creeping onto the target length.
    void setSpeed(float speed, uint32_t ramp_ms);

    // True while a ramp started by any command is still in progress.
    bool isRamping() const;

private:
    void rampTo(float target, uint64_t ramp_us);
    void applySpeed(float speed);
    void update();
    static bool timerCallback(repeating_timer_t *t);

    uint pwm_pin_;
    uint slice_;
    uint channel_;
    uint16_t top_ = 0;
    uint32_t ramp_us_;  // ramp time of start() / stop() / setMaxSpeed()
    uint32_t pwm_hz_;
    repeating_timer_t timer_;

    uint8_t max_percent_ = 100;
    bool running_ = false;  // start() was called and stop() was not

    // Ramp state, shared with the timer IRQ
    volatile float speed_ = 0.0f;
    float ramp_from_ = 0.0f;
    float ramp_to_ = 0.0f;
    uint64_t ramp_start_us_ = 0;
    uint64_t ramp_len_us_ = 0;  // duration of the current ramp
    volatile bool ramping_ = false;
};
