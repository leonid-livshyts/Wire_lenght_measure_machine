#include "calibration.h"

#include <cmath>

namespace {

int64_t magnitude(int32_t ticks) {
    return ticks < 0 ? -(int64_t)ticks : (int64_t)ticks;  // int64: safe for INT32_MIN
}

}  // namespace

int32_t adjustLength(int32_t length_mm, int32_t delta_mm) {
    int64_t length = (int64_t)length_mm + delta_mm;
    if (length < kMinCalLengthMm) return kMinCalLengthMm;
    if (length > kMaxCalLengthMm) return kMaxCalLengthMm;
    return (int32_t)length;
}

bool computeMmPerTick(int32_t length_mm, int32_t ticks, double &mm_per_tick) {
    if (ticks == 0 || length_mm <= 0) return false;
    mm_per_tick = (double)length_mm / (double)magnitude(ticks);
    return true;
}

int64_t ticksToMm(int32_t ticks, double mm_per_tick) {
    double mm = (double)magnitude(ticks) * mm_per_tick;
    // Cap on the rounded value: +0.5 accounts for rounding, so values that round
    // down to kMaxDisplayValue still fit, but values rounding up to 100000000+ don't.
    if (mm >= (double)kMaxDisplayValue + 0.5) return kMaxDisplayValue + 1;
    return std::llround(mm);
}

bool fitsDisplay(int64_t value) {
    return value >= 0 && value <= kMaxDisplayValue;
}
