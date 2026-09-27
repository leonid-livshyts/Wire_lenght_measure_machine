#pragma once

#include <stdint.h>

// Speed-dependent step for editing a number with the user knob: slow turns
// change it by 1 per detent, faster turns by 10 or 100, so both fine
// adjustment and big jumps are quick. Pure logic; the caller passes the time.
class KnobAccel {
public:
    static constexpr uint32_t kFastGapMs = 40;     // turns this close: x100
    static constexpr uint32_t kMediumGapMs = 100;  // turns this close: x10

    // Returns detents scaled by the factor for the gap since the previous
    // turn. detents == 0 returns 0 and is not counted as a turn. now_ms may wrap around.
    int32_t scale(int32_t detents, uint32_t now_ms);

    // Forgets the previous turn, so the next one is x1.
    void reset();

private:
    bool has_last_ = false;
    uint32_t last_ms_ = 0;
};
