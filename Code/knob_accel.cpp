#include "knob_accel.h"

int32_t KnobAccel::scale(int32_t detents, uint32_t now_ms) {
    if (detents == 0) return 0;
    int32_t factor = 1;
    if (has_last_) {
        uint32_t gap = now_ms - last_ms_;  // unsigned: correct across the wrap
        if (gap <= kFastestGapMs) {
            factor = 1000;
        } else if (gap <= kFastGapMs) {
            factor = 100;
        } else if (gap <= kMediumGapMs) {
            factor = 10;
        }
    }
    if (factor > max_factor_) factor = max_factor_;
    has_last_ = true;
    last_ms_ = now_ms;
    return detents * factor;
}

void KnobAccel::reset() {
    has_last_ = false;
}

int32_t clampAdd(int32_t value, int32_t delta, int32_t min_value, int32_t max_value) {
    int64_t sum = (int64_t)value + delta;  // int64: no overflow
    if (sum < min_value) return min_value;
    if (sum > max_value) return max_value;
    return (int32_t)sum;
}
