#pragma once

#include <stdint.h>

#include "menu.h"
#include "number_edit_tab.h"
#include "run_logic.h"
#include "settings.h"

class Max7219;
class Motor;
class RotaryEncoder;

// Hardware and state shared by the working pages. target_mm and
// speed_percent live in RAM: kept between runs, reset at power-up.
struct WorkingContext {
    Max7219 &display;
    Motor &motor;
    RotaryEncoder &measure;    // measuring-roll encoder
    const Settings &settings;  // mm_per_tick from the last calibration

    int32_t target_mm = kDefaultTargetMm;
    int32_t speed_percent = kDefaultSpeedPercent;
};

// Page 3: counting starts at 0, the motor stays off; the user threads the wire.
class RunThreadTab final : public MenuTab {
public:
    explicit RunThreadTab(WorkingContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;

private:
    WorkingContext &ctx_;
    int64_t shown_mm_ = -1;  // last value drawn, to redraw only on change
};

// Page 4: the motor pulls the wire, slows down to a creep speed near the
// target and stops at target - kStopBeforeMm. Long click pauses and resumes;
// the page only advances on a short click.
class RunPullTab final : public MenuTab {
public:
    explicit RunPullTab(WorkingContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;
    TabAction onLongClick() override;
    void onExit() override;

private:
    enum class State : uint8_t {
        Running,      // motor at the max speed
        SlowingDown,  // ramping down to the first guess of the creep power
        Creeping,     // holding the creep speed from the measured wire speed
        Paused,       // stopped by a long click (display blinks)
        Stalled,      // stopped: no wire movement for 10 s (display blinks)
        Stopped,      // target - kStopBeforeMm reached
    };

    bool isDriving() const;  // Running, SlowingDown or Creeping
    float maxPower() const;  // the user's max speed as motor power
    // The driving state's own step: start slowing down, finish slowing
    // down, or correct the creep power
    void steer(double speed_mm_per_s, double mm, uint32_t now_ms);
    void creepAt(float power);
    // Starts the motor (or creeps, if close) unless the target is reached.
    void resume(int32_t count, double mm, uint32_t now_ms);
    void draw(int64_t mm, uint32_t now_ms);
    // Cuts power at once and marks the target reached; shared by every stop
    // path (driving, paused or stalled).
    void stopAtTarget(double mm);

    WorkingContext &ctx_;
    State state_ = State::Stopped;
    bool start_pending_ = false;  // set by onEnter(); the start needs update()'s time
    SpeedMeter speed_;
    CreepController creep_;       // keeps its power between runs
    StallDetector stall_;
    float driven_power_ = 0.0f;   // last power given to the motor while creeping
    uint32_t now_ms_ = 0;         // time of the latest pass, for long clicks
    int64_t shown_mm_ = -1;
    bool shown_blank_ = false;
};

// Page 5: the final length. It follows the wire until the motor has fully
// stopped (the braking wire is real wire), then freezes: counting ends.
class RunDoneTab final : public MenuTab {
public:
    explicit RunDoneTab(WorkingContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;

private:
    WorkingContext &ctx_;
    int64_t shown_mm_ = -1;
    bool frozen_ = false;
};

// The working menu: length, speed, thread, pull, done.
class WorkingMenu final : public Menu {
public:
    explicit WorkingMenu(WorkingContext &ctx);

protected:
    void onClose() override;

private:
    WorkingContext &ctx_;
    NumberEditTab length_;  // page 1: target length in mm
    NumberEditTab speed_;   // page 2: max motor speed in %
    RunThreadTab thread_;
    RunPullTab pull_;
    RunDoneTab done_;
};
