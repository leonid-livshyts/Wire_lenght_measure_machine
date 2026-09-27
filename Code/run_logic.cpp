#include "run_logic.h"

namespace {

// Clamped to [kCreepMinPower, max_power]; the minimum wins if they cross.
float clampPower(float power, float max_power) {
    if (power > max_power) power = max_power;
    if (power < kCreepMinPower) power = kCreepMinPower;
    return power;
}

}  // namespace

double slowDownDistanceMm(double speed_mm_per_s, uint32_t ramp_ms) {
    if (speed_mm_per_s <= kCreepSpeedMmPerS) return 0.0;
    return (speed_mm_per_s + kCreepSpeedMmPerS) / 2.0 * (ramp_ms / 1000.0);
}

bool shouldSlowDown(double pulled_mm, int32_t target_mm, double speed_mm_per_s, uint32_t ramp_ms) {
    double lead = slowDownDistanceMm(speed_mm_per_s, ramp_ms) + kCreepMinMm;
    return pulled_mm + lead >= (double)target_mm;
}

bool shouldStop(double pulled_mm, int32_t target_mm) {
    return pulled_mm >= (double)target_mm - kStopBeforeMm;
}

float estimateCreepPower(float power, double speed_mm_per_s) {
    float guess = speed_mm_per_s > 0.0 ? (float)(power * kCreepSpeedMmPerS / speed_mm_per_s) : power;
    return clampPower(guess, power);
}

void SpeedMeter::reset(double mm, uint32_t now_ms) {
    window_mm_ = mm;
    window_start_ms_ = now_ms;
    speed_mm_per_s_ = 0.0;
}

double SpeedMeter::update(double mm, uint32_t now_ms) {
    uint32_t elapsed = now_ms - window_start_ms_;  // unsigned: correct across the wrap
    if (elapsed >= kWindowMs) {
        speed_mm_per_s_ = (mm - window_mm_) * 1000.0 / (double)elapsed;
        window_mm_ = mm;
        window_start_ms_ = now_ms;
    }
    return speed_mm_per_s_;
}

void CreepController::start(float power, float max_power, uint32_t now_ms) {
    max_power_ = max_power;
    power_ = clampPower(power, max_power);
    last_adjust_ms_ = now_ms;
}

float CreepController::update(double speed_mm_per_s, uint32_t now_ms) {
    if ((uint32_t)(now_ms - last_adjust_ms_) >= SpeedMeter::kWindowMs) {
        float error = (float)(kCreepSpeedMmPerS - speed_mm_per_s);  // > 0: too slow
        power_ = clampPower(power_ + kCreepGainPerMmS * error, max_power_);
        last_adjust_ms_ = now_ms;
    }
    return power_;
}

void StallDetector::reset(int32_t count, uint32_t now_ms) {
    last_count_ = count;
    last_move_ms_ = now_ms;
}

bool StallDetector::update(int32_t count, uint32_t now_ms) {
    int64_t moved = (int64_t)count - last_count_;  // int64: no overflow
    if (moved >= kMinMoveTicks || moved <= -kMinMoveTicks) {
        last_count_ = count;
        last_move_ms_ = now_ms;
        return false;
    }
    return (uint32_t)(now_ms - last_move_ms_) >= kTimeoutMs;
}
