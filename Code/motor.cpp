#include "motor.h"

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"

namespace {

constexpr int kUpdateMs = 5;  // ramp step period

// Smoothstep: zero slope at both ends, so the motor starts and settles
// without a jerk.
float sCurve(float t) {
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

Motor::Motor(uint pwm_pin, uint32_t ramp_ms, uint32_t pwm_hz)
    : pwm_pin_(pwm_pin),
      slice_(pwm_gpio_to_slice_num(pwm_pin)),
      channel_(pwm_gpio_to_channel(pwm_pin)),
      ramp_us_(ramp_ms * 1000),
      pwm_hz_(pwm_hz > 0 ? pwm_hz : kDefaultPwmHz) {}

void Motor::init() {
    // Drive the pin high (motor off) before handing it to the PWM, so the
    // motor does not twitch while the slice is being configured.
    gpio_init(pwm_pin_);
    gpio_set_dir(pwm_pin_, GPIO_OUT);
    gpio_put(pwm_pin_, 1);

    // Pick the smallest integer divider that keeps TOP + 1 (the full-speed
    // level) within 16 bits, for the finest duty resolution.
    uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t div = (sys_hz / pwm_hz_ + 65534) / 65535;
    if (div < 1) div = 1;
    if (div > 255) div = 255;
    top_ = (uint16_t)(sys_hz / (div * pwm_hz_) - 1);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&config, div);
    pwm_config_set_wrap(&config, top_);
    // Inverted output: level 0 = pin always high = motor stopped
    pwm_config_set_output_polarity(&config, channel_ == PWM_CHAN_A, channel_ == PWM_CHAN_B);
    pwm_init(slice_, &config, false);
    pwm_set_chan_level(slice_, channel_, 0);
    pwm_set_enabled(slice_, true);
    gpio_set_function(pwm_pin_, GPIO_FUNC_PWM);

    // Negative delay = fixed period, measured from the previous start
    add_repeating_timer_ms(-kUpdateMs, timerCallback, this, &timer_);
}

void Motor::start() {
    running_ = true;
    rampTo(max_percent_ / 100.0f);
}

void Motor::stop() {
    running_ = false;
    rampTo(0.0f);
}

void Motor::setMaxSpeed(uint8_t percent) {
    max_percent_ = percent > 100 ? 100 : percent;
    if (running_) rampTo(max_percent_ / 100.0f);
}

uint8_t Motor::getMaxSpeed() const {
    return max_percent_;
}

float Motor::getSpeed() const {
    return speed_;
}

bool Motor::isRunning() const {
    return running_ || ramping_ || speed_ > 0.0f;
}

// Starts a new ramp from wherever the motor is now, so calling stop() in the
// middle of a start (or vice versa) does not cause a speed jump.
void Motor::rampTo(float target) {
    uint32_t irq_state = save_and_disable_interrupts();
    ramp_from_ = speed_;
    ramp_to_ = target;
    ramp_start_us_ = time_us_64();
    ramping_ = true;
    restore_interrupts(irq_state);
}

void Motor::applySpeed(float speed) {
    // With the inverted polarity the pin is low (motor on) for `level` counts
    // out of TOP + 1, so level = TOP + 1 is a constant low = full speed.
    uint32_t level = (uint32_t)(speed * (top_ + 1u) + 0.5f);
    pwm_set_chan_level(slice_, channel_, (uint16_t)(level > top_ + 1u ? top_ + 1u : level));
}

void Motor::update() {
    if (!ramping_) return;

    uint64_t elapsed = time_us_64() - ramp_start_us_;
    float speed;
    if (ramp_us_ == 0 || elapsed >= ramp_us_) {
        speed = ramp_to_;
        ramping_ = false;
    } else {
        float t = (float)elapsed / (float)ramp_us_;
        speed = ramp_from_ + (ramp_to_ - ramp_from_) * sCurve(t);
    }
    speed_ = speed;
    applySpeed(speed);
}

bool Motor::timerCallback(repeating_timer_t *t) {
    static_cast<Motor *>(t->user_data)->update();
    return true;  // keep repeating
}
