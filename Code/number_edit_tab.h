#pragma once

#include <stdint.h>

#include "knob_accel.h"
#include "menu.h"

class Max7219;

// A page that edits one number with the user knob. The step grows with
// turning speed (KnobAccel, capped at max_factor). The value lives outside
// the tab, so it is kept between menu runs.
class NumberEditTab final : public MenuTab {
public:
    // label: up to 2 characters shown on the left (e.g. "SP"), with the value
    // right-aligned in the remaining 6 digits; nullptr shows the bare number.
    NumberEditTab(Max7219 &display, int32_t &value, int32_t min_value, int32_t max_value,
                  int32_t max_factor, const char *label = nullptr);

    void onEnter() override;
    void onTurn(int32_t detents) override;
    TabAction update(uint32_t now_ms) override;

private:
    void draw();

    Max7219 &display_;
    int32_t &value_;
    int32_t min_value_;
    int32_t max_value_;
    const char *label_;
    KnobAccel accel_;
    uint32_t now_ms_ = 0;  // time of the latest pass, for KnobAccel
};
