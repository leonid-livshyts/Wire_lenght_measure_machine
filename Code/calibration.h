#pragma once

#include <stdint.h>

// Pure maths of the calibration menu (host-tested).

constexpr int32_t kMinCalLengthMm = 1;
constexpr int32_t kMaxCalLengthMm = 99999999;  // 8 display digits
constexpr int32_t kDefaultCalLengthMm = 1000;
constexpr int64_t kMaxDisplayValue = 99999999;

// length_mm + delta_mm, clamped to [kMinCalLengthMm, kMaxCalLengthMm].
int32_t adjustLength(int32_t length_mm, int32_t delta_mm);

// New ratio = length_mm / |ticks|. The sign of ticks is ignored: the roll
// may turn either way depending on how the wire is threaded. Returns false
// (output untouched) if ticks == 0 or length_mm <= 0.
bool computeMmPerTick(int32_t length_mm, int32_t ticks, double &mm_per_tick);

// Wire length for `ticks` in whole mm (rounded, sign ignored). Capped at
// kMaxDisplayValue + 1, so the result never overflows.
int64_t ticksToMm(int32_t ticks, double mm_per_tick);

// True if value fits the 8-digit display.
bool fitsDisplay(int64_t value);
