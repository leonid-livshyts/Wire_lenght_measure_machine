#pragma once

#include <stdint.h>
#include "pico/types.h"

// Driver for a KY-040 style quadrature rotary encoder with a push button.
// Several instances can run at once (up to kMaxEncoders).
//
// Rotation is decoded in a GPIO interrupt on every edge of CLK and DT, so no
// steps are lost even if the main loop is busy. The button is sampled and
// debounced by a shared 1 ms repeating timer.
//
// Wiring: power the module from 3.3 V (not 5 V) — its on-board pull-ups
// would otherwise put 5 V on the RP2350 pins. Internal pull-ups are enabled
// on all three pins as well, since many KY-040 boards have no pull-up on SW.
class RotaryEncoder {
public:
    static constexpr int kMaxEncoders = 4;
    static constexpr uint kNoPin = 0xFFFFFFFFu;  // pass as sw_pin if the button is unused

    // steps_per_detent: quadrature steps per mechanical click (4 for most KY-040).
    // reversed: swap the counting direction instead of swapping wires.
    RotaryEncoder(uint clk_pin, uint dt_pin, uint sw_pin = kNoPin,
                  int steps_per_detent = 4, bool reversed = false);

    // Configures pins and interrupts. Returns false if kMaxEncoders are already in use.
    bool init();

    // Raw quadrature steps since init/reset (full resolution, 4x the detents).
    // Use this for the measuring roll.
    int32_t getCount() const;

    // Count in detents (clicks).
    int32_t getPosition() const;

    // Sets the count to `position` detents and clears the pending delta.
    void reset(int32_t position = 0);

    // Detents turned since the previous call (positive = clockwise).
    // Use this for menus / value editing.
    int32_t readDelta();

    // Debounced current button state.
    bool isPressed() const;

    // True once per button press; clears the flag.
    bool wasClicked();

private:
    void handleGpioIrq();
    void pollButton();

    template <int Slot> static void gpioIrqTrampoline();
    static bool buttonTimerCallback(struct repeating_timer *t);

    uint clk_pin_;
    uint dt_pin_;
    uint sw_pin_;
    int steps_per_detent_;
    bool reversed_;

    volatile int32_t count_ = 0;
    uint8_t last_ab_ = 0;
    int32_t delta_base_ = 0;  // count already reported by readDelta()

    // Button debounce state (touched only by the timer IRQ, except the click flag)
    bool button_raw_ = false;
    bool button_stable_ = false;
    uint8_t button_same_ms_ = 0;
    volatile bool button_pressed_ = false;
    volatile bool clicked_ = false;
};
