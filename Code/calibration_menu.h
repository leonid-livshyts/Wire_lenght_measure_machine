#pragma once

#include <stdint.h>

#include "calibration.h"
#include "knob_accel.h"
#include "menu.h"
#include "settings.h"

class Max7219;
class Motor;
class RotaryEncoder;

enum class CalibrationMode : uint8_t {
    Motor,  // the motor pulls the wire (shown as "Auto": no M on 7 segments)
    Hand,   // the user pulls the wire; the attach page is skipped
};

// Hardware and state shared by the calibration pages.
struct CalibrationContext {
    Max7219 &display;
    Motor &motor;
    RotaryEncoder &measure;  // measuring-roll encoder
    Settings &settings;      // live settings; the new ratio is written here
    bool (*save)(const Settings &);

    int32_t length_mm = kDefaultCalLengthMm;  // kept between calibrations
    CalibrationMode mode = CalibrationMode::Motor;
    int32_t ticks = 0;  // |count| when the move page was left
};

// Page 1: length of the test wire in mm, speed-dependent knob step.
class CalLengthTab final : public MenuTab {
public:
    explicit CalLengthTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    void onTurn(int32_t detents) override;
    TabAction update(uint32_t now_ms) override;

private:
    void draw();
    CalibrationContext &ctx_;
    KnobAccel accel_;
    uint32_t now_ms_ = 0;  // time of the latest pass, for KnobAccel
};

// Page 2: by motor ("Auto") or by hand ("HAnd"); each detent toggles.
class CalModeTab final : public MenuTab {
public:
    explicit CalModeTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    void onTurn(int32_t detents) override;

private:
    void draw();
    CalibrationContext &ctx_;
};

// Page 2.5 (motor mode only): counting starts, motor still off, the user
// ties the wire end to the motor.
class CalAttachTab final : public MenuTab {
public:
    explicit CalAttachTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    bool isSkipped() const override;

private:
    CalibrationContext &ctx_;
};

// Page 3: the wire goes through; shows the length by the old ratio.
class CalMoveTab final : public MenuTab {
public:
    explicit CalMoveTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;
    void onExit() override;

private:
    void draw();
    CalibrationContext &ctx_;
    int64_t shown_mm_ = -1;  // last value drawn, to redraw only on change
};

// Page 4: shows the tick count, then saves the new ratio once the motor
// (left ramping down by CalMoveTab::onExit()) has actually stopped. Saving
// disables interrupts for the flash erase, which would freeze the motor's
// braking ramp if done immediately.
class CalResultTab final : public MenuTab {
public:
    explicit CalResultTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;
    TabAction onClick() override;

private:
    CalibrationContext &ctx_;
    double new_mm_per_tick_ = 0.0;  // computed in onEnter(), saved once the motor is stopped
    bool save_pending_ = false;
};

// The calibration menu: pages 1, 2, 2.5, 3, 4 in order.
class CalibrationMenu final : public Menu {
public:
    explicit CalibrationMenu(CalibrationContext &ctx);

protected:
    void onClose() override;

private:
    CalibrationContext &ctx_;
    CalLengthTab length_;
    CalModeTab mode_;
    CalAttachTab attach_;
    CalMoveTab move_;
    CalResultTab result_;
};
