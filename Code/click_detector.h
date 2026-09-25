#pragma once

#include <stdint.h>

// Turns a debounced button level into "short click" events.
//
// A click is reported once, on release, and only if the button was held for
// at most max_short_ms. Longer presses are ignored, so leaning on the knob
// does not flip menu pages. Pure logic (no SDK calls), so it is unit-tested
// on the host; the caller passes the time in.
class ClickDetector {
public:
    static constexpr uint32_t kShortClickMaxMs = 2000;

    explicit ClickDetector(uint32_t max_short_ms = kShortClickMaxMs);

    // Call regularly with the current button state and time. Returns true
    // exactly once per short click. now_ms may wrap around.
    bool update(bool pressed, uint32_t now_ms);

private:
    uint32_t max_short_ms_;
    bool was_pressed_ = false;
    uint32_t press_start_ms_ = 0;
};
