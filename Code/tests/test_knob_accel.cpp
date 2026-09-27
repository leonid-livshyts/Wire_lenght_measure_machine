#include "check.h"
#include "knob_accel.h"
#include <cstdint>

namespace {

void testFirstTurnIsFine() {
    KnobAccel accel;
    CHECK(accel.scale(1, 1000) == 1);
    CHECK(accel.scale(-1, 5000) == -1);  // long pause: fine again
}

void testMediumAndFastTurns() {
    KnobAccel accel;
    accel.scale(1, 1000);
    CHECK(accel.scale(1, 1000 + KnobAccel::kMediumGapMs) == 10);
    CHECK(accel.scale(-2, 1100 + KnobAccel::kFastGapMs) == -200);
    CHECK(accel.scale(1, 1140 + KnobAccel::kMediumGapMs + 1) == 1);
}

void testZeroIsNotATurn() {
    KnobAccel accel;
    accel.scale(1, 1000);
    CHECK(accel.scale(0, 1010) == 0);
    CHECK(accel.scale(1, 1000 + KnobAccel::kMediumGapMs) == 10);  // gap still from t=1000
}

void testResetForgetsLastTurn() {
    KnobAccel accel;
    accel.scale(1, 1000);
    accel.reset();
    CHECK(accel.scale(1, 1010) == 1);
}

void testTimeWraparound() {
    KnobAccel accel;
    accel.scale(1, 0xFFFFFFF0u);
    CHECK(accel.scale(1, 0x00000010u) == 100);  // 32 ms across the wrap
}

void testFastestTurnsNeedCap1000() {
    KnobAccel wide(1000);
    wide.scale(1, 1000);
    CHECK(wide.scale(1, 1000 + KnobAccel::kFastestGapMs) == 1000);
    CHECK(wide.scale(-1, 1020 + KnobAccel::kFastGapMs) == -100);  // 40 ms: x100

    KnobAccel normal;  // default cap 100: calibration unchanged
    normal.scale(1, 1000);
    CHECK(normal.scale(1, 1000 + KnobAccel::kFastestGapMs) == 100);
}

void testSmallCap() {
    KnobAccel narrow(10);
    narrow.scale(1, 1000);
    CHECK(narrow.scale(1, 1005) == 10);  // fastest turn still only x10
    CHECK(narrow.scale(1, 5000) == 1);
}

void testClampAdd() {
    CHECK(clampAdd(50, 10, 1, 100) == 60);
    CHECK(clampAdd(95, 10, 1, 100) == 100);
    CHECK(clampAdd(5, -10, 1, 100) == 1);
    CHECK(clampAdd(INT32_MAX - 1, INT32_MAX, 1, INT32_MAX) == INT32_MAX);  // no overflow
    CHECK(clampAdd(1, INT32_MIN, 1, 100) == 1);
}

}  // namespace

void runKnobAccelTests() {
    testFirstTurnIsFine();
    testMediumAndFastTurns();
    testZeroIsNotATurn();
    testResetForgetsLastTurn();
    testTimeWraparound();
    testFastestTurnsNeedCap1000();
    testSmallCap();
    testClampAdd();
}
