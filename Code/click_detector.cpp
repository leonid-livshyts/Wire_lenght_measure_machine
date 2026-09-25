#include "click_detector.h"

ClickDetector::ClickDetector(uint32_t max_short_ms) : max_short_ms_(max_short_ms) {}

bool ClickDetector::update(bool pressed, uint32_t now_ms) {
    if (pressed && !was_pressed_) press_start_ms_ = now_ms;

    // Unsigned subtraction keeps the duration right across a counter wrap
    bool click = !pressed && was_pressed_ &&
                 (uint32_t)(now_ms - press_start_ms_) <= max_short_ms_;
    was_pressed_ = pressed;
    return click;
}
