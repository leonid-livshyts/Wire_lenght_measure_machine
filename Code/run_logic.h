#pragma once

#include <stdint.h>

// Pure maths of the working menu (host-tested).

constexpr int32_t kMinTargetMm = 1;
constexpr int32_t kMaxTargetMm = 99999999;  // 8 display digits
constexpr int32_t kDefaultTargetMm = 1000;

constexpr int32_t kMinSpeedPercent = 1;
constexpr int32_t kMaxSpeedPercent = 100;
constexpr int32_t kDefaultSpeedPercent = 100;

// --- Landing on the target ---
// No single braking distance fits every speed and wire, so near the target
// the motor slows down to a creep speed, the power is corrected from the
// measured wire speed, and the motor stops at once kStopBeforeMm before the
// target; the wire coasts the rest. TUNE = first guess, set on the machine.
constexpr double kCreepSpeedMmPerS = 20.0;  // TUNE
constexpr int32_t kCreepMinMm = 20;         // shortest creep, so the speed settles; TUNE
constexpr int32_t kStopBeforeMm = 1;
constexpr float kCreepMinPower = 0.05f;     // lowest motor power while creeping; TUNE
constexpr float kCreepGainPerMmS = 0.002f;  // power change per mm/s of error, per adjustment; TUNE

// Wire that passes while the motor ramps from speed_mm_per_s down to the
// creep speed over ramp_ms. The ramp is a smoothstep, whose average speed is
// the mean of both ends. 0 if already at or below the creep speed.
double slowDownDistanceMm(double speed_mm_per_s, uint32_t ramp_ms);

// True once the motor must start slowing down to still creep kCreepMinMm
// before the target. pulled_mm is the unrounded pulled length.
bool shouldSlowDown(double pulled_mm, int32_t target_mm, double speed_mm_per_s, uint32_t ramp_ms);

// True once pulled_mm >= target_mm - kStopBeforeMm: stop the motor now.
// pulled_mm is the unrounded pulled length, so the stop fires exactly at
// target_mm - kStopBeforeMm instead of up to 0.5 mm early.
bool shouldStop(double pulled_mm, int32_t target_mm);

// First guess for the creep power, taking wire speed as proportional to
// power: power * creep / speed, at most `power`, at least kCreepMinPower.
// `power` itself (at least kCreepMinPower) while no speed is measured.
float estimateCreepPower(float power, double speed_mm_per_s);

// Wire speed from the pulled length, measured over windows of kWindowMs.
class SpeedMeter {
public:
    static constexpr uint32_t kWindowMs = 100;

    // Starts measuring from `mm` (unrounded); the speed is 0 until a full
    // window has passed.
    void reset(double mm, uint32_t now_ms);

    // Call every pass with the unrounded pulled length. Returns the speed of
    // the last full window in mm/s. now_ms may wrap around.
    double update(double mm, uint32_t now_ms);

private:
    double window_mm_ = 0.0;
    uint32_t window_start_ms_ = 0;
    double speed_mm_per_s_ = 0.0;
};

// Holds the creep speed: every SpeedMeter::kWindowMs it moves the motor
// power by kCreepGainPerMmS per mm/s of error (a simple integral controller).
class CreepController {
public:
    // Starts from `power`, clamped to [kCreepMinPower, max_power]
    // (max_power = the user's max speed). The first adjustment comes one
    // window later.
    void start(float power, float max_power, uint32_t now_ms);

    // Call every pass with the measured wire speed. Returns the power to
    // drive. now_ms may wrap around.
    float update(double speed_mm_per_s, uint32_t now_ms);

    // Current power. Kept between runs as the best guess for the next creep.
    float power() const { return power_; }

private:
    float power_ = 0.2f;  // TUNE: guess before any creep has run
    float max_power_ = 1.0f;
    uint32_t last_adjust_ms_ = 0;
};

// Detects "motor driving but the wire does not move" from the raw count.
class StallDetector {
public:
    static constexpr uint32_t kTimeoutMs = 10000;
    static constexpr int32_t kMinMoveTicks = 4;  // one detent; less is encoder jitter

    void reset(int32_t count, uint32_t now_ms);

    // True once the count has not moved by kMinMoveTicks for kTimeoutMs.
    // now_ms may wrap around.
    bool update(int32_t count, uint32_t now_ms);

private:
    int32_t last_count_ = 0;
    uint32_t last_move_ms_ = 0;
};
