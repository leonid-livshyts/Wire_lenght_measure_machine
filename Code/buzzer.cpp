#include "buzzer.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/time.h"

Buzzer::Buzzer(uint pin) : pin_(pin) {}

void Buzzer::init() {
    gpio_set_function(pin_, GPIO_FUNC_PWM);
    slice_ = pwm_gpio_to_slice_num(pin_);
    channel_ = pwm_gpio_to_channel(pin_);
    pwm_set_chan_level(slice_, channel_, 0);
    pwm_set_enabled(slice_, true);
    playing_hz_ = 0;
}

void Buzzer::beep() { seq_.beep(nowMs()); }
void Buzzer::playMelody() { seq_.playMelody(nowMs()); }
void Buzzer::startAlarm() { seq_.startAlarm(nowMs()); }
void Buzzer::stopAlarm() { seq_.stopAlarm(); }
bool Buzzer::alarmActive() const { return seq_.alarmActive(); }

void Buzzer::update(uint32_t now_ms) {
    uint16_t hz = seq_.frequencyAt(now_ms);
    if (hz != playing_hz_) setTone(hz);
}

void Buzzer::setTone(uint16_t hz) {
    playing_hz_ = hz;
    if (hz == 0) {
        pwm_set_chan_level(slice_, channel_, 0);  // pin low: silent, no DC through the buzzer
        return;
    }
    // Smallest integer divider that keeps the counter's top within 16 bits
    // (150 MHz: 523 Hz -> /5, 2700 Hz -> /1)
    uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t div = sys_hz / ((uint32_t)hz * 65536u) + 1;
    if (div > 255) div = 255;
    uint32_t top = sys_hz / (div * hz) - 1;
    if (top > 65535) top = 65535;
    pwm_set_clkdiv_int_frac4(slice_, (uint8_t)div, 0);
    pwm_set_wrap(slice_, (uint16_t)top);
    pwm_set_chan_level(slice_, channel_, (uint16_t)((top + 1) / 2));
}

uint32_t Buzzer::nowMs() {
    return to_ms_since_boot(get_absolute_time());
}
