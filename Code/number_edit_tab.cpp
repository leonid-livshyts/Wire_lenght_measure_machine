#include "number_edit_tab.h"

#include <stdio.h>

#include "max7219.h"

NumberEditTab::NumberEditTab(Max7219 &display, int32_t &value, int32_t min_value,
                             int32_t max_value, int32_t max_factor, const char *label)
    : display_(display), value_(value), min_value_(min_value), max_value_(max_value),
      label_(label), accel_(max_factor) {}

void NumberEditTab::onEnter() {
    accel_.reset();
    draw();
}

void NumberEditTab::onTurn(int32_t detents) {
    // now_ms_ is from the previous pass (<= one loop period old): turns are
    // delivered before update() within a pass
    value_ = clampAdd(value_, accel_.scale(detents, now_ms_), min_value_, max_value_);
    draw();
}

TabAction NumberEditTab::update(uint32_t now_ms) {
    now_ms_ = now_ms;
    return TabAction::Stay;
}

void NumberEditTab::draw() {
    if (label_ == nullptr) {
        display_.printNumber(value_);
        return;
    }
    char text[16];
    snprintf(text, sizeof(text), "%-2s%6ld", label_, (long)value_);
    display_.printText(text);
}
