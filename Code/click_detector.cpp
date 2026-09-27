#include "click_detector.h"

ClickDetector::ClickDetector(uint32_t max_short_ms) : max_short_ms_(max_short_ms) {}

ClickEvent ClickDetector::update(bool pressed, uint32_t now_ms) {
    if (pressed && !was_pressed_) press_start_ms_ = now_ms;
    bool released = !pressed && was_pressed_;
    was_pressed_ = pressed;
    if (!released) return ClickEvent::None;

    // Unsigned subtraction keeps the duration right across a counter wrap
    return (uint32_t)(now_ms - press_start_ms_) <= max_short_ms_ ? ClickEvent::Short
                                                                 : ClickEvent::Long;
}
