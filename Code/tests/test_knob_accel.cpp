#include "check.h"
#include "knob_accel.h"

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

}  // namespace

void runKnobAccelTests() {
    testFirstTurnIsFine();
    testMediumAndFastTurns();
    testZeroIsNotATurn();
    testResetForgetsLastTurn();
    testTimeWraparound();
}
