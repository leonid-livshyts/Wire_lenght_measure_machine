#include "calibration_menu.h"

#include "max7219.h"
#include "motor.h"
#include "rotary_encoder.h"

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
    // Only stop the motor if it was actually driving: stop() on an
    // already-stopped motor starts a pointless 0->0 ramp that would make
    // isRunning() true (and so delay the save on page 4) for no reason.
    if (ctx_.mode == CalibrationMode::Motor) ctx_.motor.stop();
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
    save_pending_ = false;
    double mm_per_tick = 0.0;
    if (!computeMmPerTick(ctx_.length_mm, ctx_.ticks, mm_per_tick)) {
        ctx_.display.printText("Err");  // no ticks counted: nothing to save
        return;
    }
    ctx_.display.printNumber(ctx_.ticks);
    new_mm_per_tick_ = mm_per_tick;
    save_pending_ = true;  // saved once the motor has stopped, see update()
}

TabAction CalResultTab::update(uint32_t) {
    // Deferred from onEnter(): the motor left by CalMoveTab::onExit() is
    // still ramping down, and saveSettings() turns interrupts off for the
    // flash erase, which would freeze that braking ramp mid-stop.
    if (save_pending_ && !ctx_.motor.isRunning()) {
        ctx_.settings.mm_per_tick = new_mm_per_tick_;
        if (!ctx_.save(ctx_.settings)) ctx_.display.printText("FLSH Err");
        save_pending_ = false;
    }
    return TabAction::Stay;
}

TabAction CalResultTab::onClick() {
    // Ignore the click until the save above has happened (at most ~2.5 s,
    // the motor's ramp time), so leaving the menu can't race the flash write.
    if (save_pending_) return TabAction::Stay;
    return TabAction::Next;
}

// --- Menu ---

CalibrationMenu::CalibrationMenu(CalibrationContext &ctx)
    : ctx_(ctx),
      length_(ctx.display, ctx.length_mm, kMinCalLengthMm, kMaxCalLengthMm, 100),
      mode_(ctx), attach_(ctx), move_(ctx), result_(ctx) {
    addTab(length_);
    addTab(mode_);
    addTab(attach_);
    addTab(move_);
    addTab(result_);
}

void CalibrationMenu::onClose() {
    // Safety net, whichever way the menu closes; only if actually needed, so
    // this does not start a pointless 0->0 ramp on an already-stopped motor.
    if (ctx_.motor.isRunning()) ctx_.motor.stop();
}
