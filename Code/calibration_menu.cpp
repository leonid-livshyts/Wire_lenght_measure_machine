#include "calibration_menu.h"

#include "max7219.h"
#include "motor.h"
#include "rotary_encoder.h"

// --- Page 1: length ---

void CalLengthTab::onEnter() {
    accel_.reset();
    draw();
}

void CalLengthTab::onTurn(int32_t detents) {
    // now_ms_ is from the previous pass (<= one loop period old): turns are
    // delivered before update() within a pass
    ctx_.length_mm = adjustLength(ctx_.length_mm, accel_.scale(detents, now_ms_));
    draw();
}

TabAction CalLengthTab::update(uint32_t now_ms) {
    now_ms_ = now_ms;
    return TabAction::Stay;
}

void CalLengthTab::draw() {
    ctx_.display.printNumber(ctx_.length_mm);
}

// --- Page 2: mode ---

void CalModeTab::onEnter() {
    ctx_.mode = CalibrationMode::Motor;  // "by motor" is always offered first
    draw();
}

void CalModeTab::onTurn(int32_t detents) {
    if (detents % 2 != 0) {
        ctx_.mode = ctx_.mode == CalibrationMode::Motor ? CalibrationMode::Hand
                                                        : CalibrationMode::Motor;
    }
    draw();
}

void CalModeTab::draw() {
    ctx_.display.printText(ctx_.mode == CalibrationMode::Motor ? "Auto" : "HAnd");
}

// --- Page 2.5: attach the wire to the motor ---

void CalAttachTab::onEnter() {
    ctx_.measure.reset();  // counting starts now; the motor stays off
    ctx_.display.printText("tIE End");
}

bool CalAttachTab::isSkipped() const {
    return ctx_.mode == CalibrationMode::Hand;
}

// --- Page 3: move the wire ---

void CalMoveTab::onEnter() {
    if (ctx_.mode == CalibrationMode::Hand) {
        ctx_.measure.reset();  // motor mode has counted since the attach page
    } else {
        ctx_.motor.start();
    }
    shown_mm_ = -1;
    draw();
}

TabAction CalMoveTab::update(uint32_t) {
    draw();
    return TabAction::Stay;
}

void CalMoveTab::onExit() {
    ctx_.motor.stop();  // harmless in hand mode
    int32_t count = ctx_.measure.getCount();
    ctx_.ticks = count < 0 ? -count : count;
}

void CalMoveTab::draw() {
    int64_t mm = ticksToMm(ctx_.measure.getCount(), ctx_.settings.mm_per_tick);
    if (mm == shown_mm_) return;
    shown_mm_ = mm;
    if (fitsDisplay(mm)) {
        ctx_.display.printNumber((int32_t)mm);
    } else {
        ctx_.display.printText("--------");  // old ratio too far off to be worth showing
    }
}

// --- Page 4: result ---

void CalResultTab::onEnter() {
    double mm_per_tick = 0.0;
    if (!computeMmPerTick(ctx_.length_mm, ctx_.ticks, mm_per_tick)) {
        ctx_.display.printText("Err");  // no ticks counted: nothing to save
        return;
    }
    ctx_.display.printNumber(ctx_.ticks);  // drawn before the flash write pauses interrupts
    ctx_.settings.mm_per_tick = mm_per_tick;
    if (!ctx_.save(ctx_.settings)) ctx_.display.printText("FLSH Err");
}

// --- Menu ---

CalibrationMenu::CalibrationMenu(CalibrationContext &ctx)
    : ctx_(ctx), length_(ctx), mode_(ctx), attach_(ctx), move_(ctx), result_(ctx) {
    addTab(length_);
    addTab(mode_);
    addTab(attach_);
    addTab(move_);
    addTab(result_);
}

void CalibrationMenu::onClose() {
    ctx_.motor.stop();  // safety net, whichever way the menu closes
}
