#pragma once

#include <stdint.h>

// What one press of a button turned out to be, reported on release.
enum class ClickEvent : uint8_t {
    None,   // no release in this pass
    Short,  // held for at most max_short_ms: flips menu pages
    Long,   // held longer: e.g. pause/resume while pulling wire
};

// Turns a debounced button level into short and long clicks.
//
// Both are reported once, on release, so nothing happens while the button
// is held. Pure logic (no SDK calls), so it is unit-tested on the host; the
// caller passes the time in.
class ClickDetector {
public:
    static constexpr uint32_t kShortClickMaxMs = 2000;

    explicit ClickDetector(uint32_t max_short_ms = kShortClickMaxMs);

    // Call regularly with the current button state and time. Returns Short
    // or Long exactly once per press (on release), None otherwise.
    // now_ms may wrap around.
    ClickEvent update(bool pressed, uint32_t now_ms);

private:
    uint32_t max_short_ms_;
    bool was_pressed_ = false;
    uint32_t press_start_ms_ = 0;
};
