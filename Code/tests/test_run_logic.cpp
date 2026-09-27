#include <cmath>

#include "check.h"
#include "run_logic.h"

namespace {

bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

void testSlowDownDistance() {
    // Smoothstep ramp from 120 mm/s to the creep speed over 2.5 s
    CHECK(near(slowDownDistanceMm(120.0, 2500), (120.0 + kCreepSpeedMmPerS) / 2.0 * 2.5));
    CHECK(near(slowDownDistanceMm(kCreepSpeedMmPerS, 2500), 0.0));  // already at creep speed
    CHECK(near(slowDownDistanceMm(0.0, 2500), 0.0));
}

void testShouldSlowDown() {
    double lead = slowDownDistanceMm(120.0, 2500) + kCreepMinMm;
    int64_t at = 1000 - (int64_t)std::floor(lead);  // first whole mm where the lead reaches 1000
    CHECK(shouldSlowDown(at, 1000, 120.0, 2500));
    CHECK(!shouldSlowDown(at - 1, 1000, 120.0, 2500));
    // Standing still: only the creep distance is needed
    CHECK(shouldSlowDown(1000 - kCreepMinMm, 1000, 0.0, 2500));
    CHECK(!shouldSlowDown(1000 - kCreepMinMm - 1, 1000, 0.0, 2500));
}

void testShouldStop() {
    CHECK(shouldStop(1000 - kStopBeforeMm, 1000));
    CHECK(!shouldStop(1000 - kStopBeforeMm - 1, 1000));
    CHECK(shouldStop(1005, 1000));  // overshot
}

void testShouldStopUnroundedMm() {
    // Control on unrounded mm: the stop fires exactly at target - kStopBeforeMm,
    // not half a mm early as it did when the caller rounded first.
    CHECK(!shouldStop(1000 - kStopBeforeMm - 0.1, 1000));
    CHECK(shouldStop(1000 - kStopBeforeMm, 1000));
}

void testEstimateCreepPower() {
    CHECK(near(estimateCreepPower(1.0f, kCreepSpeedMmPerS * 5.0), 0.2));  // speed ~ power
    CHECK(near(estimateCreepPower(0.5f, kCreepSpeedMmPerS / 2.0), 0.5));  // never above the current power
    CHECK(near(estimateCreepPower(0.3f, 0.0), 0.3));                      // no speed yet: keep the power
    CHECK(near(estimateCreepPower(1.0f, kCreepSpeedMmPerS * 1000.0), kCreepMinPower));
    CHECK(near(estimateCreepPower(0.02f, 0.0), kCreepMinPower));
}

void testCreepControllerAdjustsEveryWindow() {
    CreepController c;
    c.start(0.2f, 1.0f, 0);
    CHECK(near(c.update(kCreepSpeedMmPerS - 10.0, SpeedMeter::kWindowMs - 1), 0.2));  // not yet
    double faster = 0.2 + 10.0 * kCreepGainPerMmS;                                     // too slow: more power
    CHECK(near(c.update(kCreepSpeedMmPerS - 10.0, SpeedMeter::kWindowMs), faster));
    CHECK(near(c.update(kCreepSpeedMmPerS + 10.0, 2 * SpeedMeter::kWindowMs), 0.2)); // too fast: less
    CHECK(near(c.power(), 0.2));
}

void testCreepControllerClamps() {
    CreepController c;
    c.start(0.0f, 1.0f, 0);
    CHECK(near(c.power(), kCreepMinPower));
    CHECK(near(c.update(kCreepSpeedMmPerS + 1000.0, SpeedMeter::kWindowMs), kCreepMinPower));
    c.start(0.9f, 0.5f, 0);  // user max speed 50 %
    CHECK(near(c.power(), 0.5));
    CHECK(near(c.update(0.0, SpeedMeter::kWindowMs), 0.5));
}

void testSpeedMeterNeedsFullWindow() {
    SpeedMeter m;
    m.reset(0, 1000);
    CHECK(near(m.update(5, 1050), 0.0));     // window not full yet
    CHECK(near(m.update(10, 1100), 100.0));  // 10 mm in 100 ms
    CHECK(near(m.update(12, 1150), 100.0));  // keeps the last value mid-window
    CHECK(near(m.update(30, 1200), 200.0));  // 20 mm in the next 100 ms
}

void testSpeedMeterFractionalMm() {
    // Unrounded mm: a 100 ms window sees the true fractional distance, not a
    // coarse +-1 mm reading.
    SpeedMeter m;
    m.reset(0.0, 0);
    CHECK(near(m.update(2.5, 100), 25.0));
}

void testSpeedMeterResetClearsSpeed() {
    SpeedMeter m;
    m.reset(0, 0);
    m.update(50, 100);
    m.reset(50, 200);
    CHECK(near(m.update(50, 250), 0.0));
}

void testSpeedMeterWraparound() {
    SpeedMeter m;
    m.reset(0, 0xFFFFFFC0u);                        // 64 ms before the wrap
    CHECK(near(m.update(10, 0x00000024u), 100.0));  // 100 ms later
}

void testStallAfterTimeout() {
    StallDetector s;
    s.reset(0, 0);
    CHECK(!s.update(0, StallDetector::kTimeoutMs - 1));
    CHECK(s.update(0, StallDetector::kTimeoutMs));
}

void testJitterIsNotMovement() {
    StallDetector s;
    s.reset(100, 0);
    CHECK(!s.update(103, 5000));                     // 3 ticks: jitter
    CHECK(s.update(97, StallDetector::kTimeoutMs));  // still no real movement
}

void testMovementRestartsTimeout() {
    StallDetector s;
    s.reset(0, 0);
    CHECK(!s.update(4, 5000));  // one detent: moving
    CHECK(!s.update(4, 14999));
    CHECK(s.update(4, 15000));
    StallDetector back;
    back.reset(0, 0);
    CHECK(!back.update(-4, 5000));  // either direction counts
    CHECK(!back.update(-4, 14999));
}

void testStallWraparound() {
    StallDetector s;
    s.reset(0, 0xFFFFF000u);
    CHECK(!s.update(0, 0x00001000u));  // 8192 ms across the wrap
}

}  // namespace

void runRunLogicTests() {
    testSlowDownDistance();
    testShouldSlowDown();
    testShouldStop();
    testShouldStopUnroundedMm();
    testEstimateCreepPower();
    testCreepControllerAdjustsEveryWindow();
    testCreepControllerClamps();
    testSpeedMeterNeedsFullWindow();
    testSpeedMeterFractionalMm();
    testSpeedMeterResetClearsSpeed();
    testSpeedMeterWraparound();
    testStallAfterTimeout();
    testJitterIsNotMovement();
    testMovementRestartsTimeout();
    testStallWraparound();
}
