#include "knob_accel.h"

int32_t KnobAccel::scale(int32_t detents, uint32_t now_ms) {
    if (detents == 0) return 0;
    int32_t factor = 1;
    if (has_last_) {
        uint32_t gap = now_ms - last_ms_;  // unsigned: correct across the wrap
        if (gap <= kFastGapMs) {
            factor = 100;
        } else if (gap <= kMediumGapMs) {
            factor = 10;
        }
    }
    has_last_ = true;
    last_ms_ = now_ms;
    return detents * factor;
}

void KnobAccel::reset() {
    has_last_ = false;
}
