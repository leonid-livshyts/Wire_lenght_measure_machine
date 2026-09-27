#pragma once

#include <stdint.h>

// Speed-dependent step for editing a number with the user knob: slow turns
// change it by 1 per detent, faster turns by 10, 100 or 1000, so both fine
// adjustment and big jumps are quick. Pure logic; the caller passes the time.
class KnobAccel {
public:
    static constexpr uint32_t kFastestGapMs = 20;  // turns this close: x1000
    static constexpr uint32_t kFastGapMs = 40;     // turns this close: x100
    static constexpr uint32_t kMediumGapMs = 100;  // turns this close: x10

    // max_factor caps the step: 100 by default, 1000 for long lengths,
    // 10 for small ranges where x100 would only jump to a limit.
    explicit KnobAccel(int32_t max_factor = 100) : max_factor_(max_factor) {}

    // Returns detents scaled by the factor for the gap since the previous
    // turn. detents == 0 returns 0 and is not counted as a turn. now_ms may wrap around.
    int32_t scale(int32_t detents, uint32_t now_ms);

    // Forgets the previous turn, so the next one is x1.
    void reset();

private:
    int32_t max_factor_;
    bool has_last_ = false;
    uint32_t last_ms_ = 0;
};

// value + delta, clamped to [min_value, max_value]. Safe for any int32 inputs.
int32_t clampAdd(int32_t value, int32_t delta, int32_t min_value, int32_t max_value);
