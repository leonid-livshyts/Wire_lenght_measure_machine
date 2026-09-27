#include <cmath>
#include <cstdint>

#include "calibration.h"
#include "check.h"

namespace {

void testAdjustLengthClamps() {
    CHECK(adjustLength(1000, 5) == 1005);
    CHECK(adjustLength(1000, -100) == 900);
    CHECK(adjustLength(10, -100) == kMinCalLengthMm);
    CHECK(adjustLength(kMaxCalLengthMm - 1, 100) == kMaxCalLengthMm);
}

void testComputeMmPerTick() {
    double r = 0.0;
    CHECK(computeMmPerTick(1000, 4000, r));
    CHECK(std::fabs(r - 0.25) < 1e-12);
    CHECK(computeMmPerTick(1000, -4000, r));  // roll turned the other way
    CHECK(std::fabs(r - 0.25) < 1e-12);
}

void testComputeRejectsBadInput() {
    double r = 7.0;
    CHECK(!computeMmPerTick(1000, 0, r));
    CHECK(!computeMmPerTick(0, 100, r));
    CHECK(r == 7.0);
}

void testTicksToMm() {
    CHECK(ticksToMm(4000, 0.25) == 1000);
    CHECK(ticksToMm(-4000, 0.25) == 1000);
    CHECK(ticksToMm(3, 0.5) == 2);  // 1.5 rounds away from zero
    CHECK(ticksToMm(0, 1.0) == 0);
    CHECK(ticksToMm(INT32_MAX, 1000.0) == kMaxDisplayValue + 1);  // capped
}

void testFitsDisplay() {
    CHECK(fitsDisplay(0));
    CHECK(fitsDisplay(kMaxDisplayValue));
    CHECK(!fitsDisplay(kMaxDisplayValue + 1));
}

}  // namespace

void runCalibrationTests() {
    testAdjustLengthClamps();
    testComputeMmPerTick();
    testComputeRejectsBadInput();
    testTicksToMm();
    testFitsDisplay();
}
