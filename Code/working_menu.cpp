#include "working_menu.h"

#include <stdio.h>

#include "calibration.h"
#include "max7219.h"
#include "motor.h"
#include "rotary_encoder.h"

namespace {

constexpr uint32_t kBlinkHalfPeriodMs = 500;
constexpr uint32_t kCreepRampMs = 100;  // smooths each creep power change

int64_t pulledMm(const WorkingContext &ctx, int32_t count) {
    return ticksToMm(count, ctx.settings.mm_per_tick);
}

void showPulledMm(Max7219 &display, int64_t mm) {
    if (fitsDisplay(mm)) {
        display.printNumber((int32_t)mm);
    } else {
        display.printText("--------");
    }
}

}  // namespace

// --- Page 3: thread the wire ---

void RunThreadTab::onEnter() {
    ctx_.measure.reset();  // counting starts now; the motor stays off
    shown_mm_ = -1;
}

TabAction RunThreadTab::update(uint32_t) {
    int64_t mm = pulledMm(ctx_, ctx_.measure.getCount());
    if (mm != shown_mm_) {
        shown_mm_ = mm;
        showPulledMm(ctx_.display, mm);
    }
    return TabAction::Stay;
}

// --- Page 4: pull ---

void RunPullTab::onEnter() {
    ctx_.motor.setMaxSpeed((uint8_t)ctx_.speed_percent);
    state_ = State::Stopped;
    start_pending_ = true;
    shown_mm_ = -1;
    shown_blank_ = false;
}

TabAction RunPullTab::update(uint32_t now_ms) {
    int32_t count = ctx_.measure.getCount();
    int64_t mm = pulledMm(ctx_, count);

    if (start_pending_) {
        start_pending_ = false;
        resume(count, mm, now_ms);
    }

    if (isDriving()) {
        double speed = speed_.update(mm, now_ms);
        if (shouldStop(mm, ctx_.target_mm)) {
            if (state_ == State::Running) {
                ctx_.motor.stop();  // overshot at full speed: brake gently
            } else {
                ctx_.motor.setSpeed(0.0f, 0);  // creeping: stop at once, the wire coasts ~1 mm
            }
            printf("stop: %lld mm pulled, target %ld mm\n", (long long)mm, (long)ctx_.target_mm);
            state_ = State::Stopped;
        } else if (stall_.update(count, now_ms)) {
            ctx_.motor.stop();
            state_ = State::Stalled;
        } else {
            steer(speed, mm, now_ms);
        }
    }

    draw(mm, now_ms);
    now_ms_ = now_ms;
    return TabAction::Stay;
}

void RunPullTab::steer(double speed_mm_per_s, int64_t mm, uint32_t now_ms) {
    switch (state_) {
        case State::Running:
            // Motor::kDefaultRampMs: Code.cpp builds the motor with the default ramp
            if (shouldSlowDown(mm, ctx_.target_mm, speed_mm_per_s, Motor::kDefaultRampMs)) {
                creep_.start(estimateCreepPower(ctx_.motor.getSpeed(), speed_mm_per_s), maxPower(), now_ms);
                driven_power_ = creep_.power();
                ctx_.motor.setSpeed(driven_power_, Motor::kDefaultRampMs);
                state_ = State::SlowingDown;
            }
            break;
        case State::SlowingDown:
            if (!ctx_.motor.isRamping()) {
                creep_.start(creep_.power(), maxPower(), now_ms);  // first adjustment one window later
                state_ = State::Creeping;
            }
            break;
        case State::Creeping: {
            float power = creep_.update(speed_mm_per_s, now_ms);
            if (power != driven_power_) {
                creepAt(power);
                // For tuning kCreep* over USB serial
                printf("creep: %.1f mm/s, power %.3f\n", speed_mm_per_s, (double)power);
            }
            break;
        }
        case State::Paused:
        case State::Stalled:
        case State::Stopped:
            break;
    }
}

TabAction RunPullTab::onLongClick() {
    if (isDriving()) {
        ctx_.motor.stop();
        state_ = State::Paused;
    } else {
        // now_ms_ is from the previous pass (<= one loop period old)
        int32_t count = ctx_.measure.getCount();
        resume(count, pulledMm(ctx_, count), now_ms_);
    }
    return TabAction::Stay;
}

void RunPullTab::onExit() {
    // Page 5 needs a stopped motor. Only the driving states still drive it:
    // the others already stopped it, and a second stop() would restart the ramp.
    if (isDriving()) ctx_.motor.stop();
    start_pending_ = false;
}

bool RunPullTab::isDriving() const {
    return state_ == State::Running || state_ == State::SlowingDown || state_ == State::Creeping;
}

float RunPullTab::maxPower() const {
    return ctx_.speed_percent / 100.0f;
}

void RunPullTab::creepAt(float power) {
    driven_power_ = power;
    ctx_.motor.setSpeed(power, kCreepRampMs);
}

void RunPullTab::resume(int32_t count, int64_t mm, uint32_t now_ms) {
    if (shouldStop(mm, ctx_.target_mm)) {  // nothing left to pull
        state_ = State::Stopped;
        return;
    }
    speed_.reset(mm, now_ms);
    stall_.reset(count, now_ms);
    if (shouldSlowDown(mm, ctx_.target_mm, 0.0, Motor::kDefaultRampMs)) {
        // Too close to speed up first: creep straight away, from the last
        // creep power (the best guess this machine has)
        creep_.start(creep_.power(), maxPower(), now_ms);
        creepAt(creep_.power());
        state_ = State::Creeping;
    } else {
        ctx_.motor.start();
        state_ = State::Running;
    }
}

void RunPullTab::draw(int64_t mm, uint32_t now_ms) {
    // Paused or stalled: blink, so it is clear the machine waits for a long click
    bool blinking = state_ == State::Paused || state_ == State::Stalled;
    bool blank = blinking && (now_ms / kBlinkHalfPeriodMs) % 2 == 1;
    if (blank == shown_blank_ && mm == shown_mm_) return;
    shown_blank_ = blank;
    shown_mm_ = mm;
    if (blank) {
        ctx_.display.clear();
    } else {
        showPulledMm(ctx_.display, mm);
    }
}

// --- Page 5: done ---

void RunDoneTab::onEnter() {
    // The motor was stopped by RunPullTab::onExit() and may still be braking
    shown_mm_ = -1;
    frozen_ = false;
}

TabAction RunDoneTab::update(uint32_t) {
    if (frozen_) return TabAction::Stay;
    int64_t mm = pulledMm(ctx_, ctx_.measure.getCount());
    if (mm != shown_mm_) {
        shown_mm_ = mm;
        showPulledMm(ctx_.display, mm);
    }
    if (!ctx_.motor.isRunning()) frozen_ = true;  // counting ends here
    return TabAction::Stay;
}

// --- Menu ---

WorkingMenu::WorkingMenu(WorkingContext &ctx)
    : ctx_(ctx),
      length_(ctx.display, ctx.target_mm, kMinTargetMm, kMaxTargetMm, 1000),
      speed_(ctx.display, ctx.speed_percent, kMinSpeedPercent, kMaxSpeedPercent, 10, "SP"),
      thread_(ctx), pull_(ctx), done_(ctx) {
    addTab(length_);
    addTab(speed_);
    addTab(thread_);
    addTab(pull_);
    addTab(done_);
}

void WorkingMenu::onClose() {
    // Safety net, whichever way the menu closes; only if actually needed, so
    // this does not start a pointless 0->0 ramp on an already-stopped motor.
    if (ctx_.motor.isRunning()) ctx_.motor.stop();
    // Calibration runs the motor with start() too: give it back full speed.
    // Not running (stop() above cleared that), so this only stores the value.
    ctx_.motor.setMaxSpeed(kMaxSpeedPercent);
}
