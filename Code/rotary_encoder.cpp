#include "rotary_encoder.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

namespace {

constexpr uint32_t kEdges = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
constexpr int kButtonPollMs = 1;
constexpr uint8_t kButtonDebounceMs = 10;  // level must be stable this long

// Quadrature decoding table, indexed by (previous AB << 2) | current AB.
// +1 / -1 for a valid step, 0 for no change or an invalid jump (both pins
// changed, e.g. a missed edge during contact bounce). Bounce on one contact
// produces +1/-1 pairs that cancel out.
const int8_t kQuadratureTable[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0,
};

RotaryEncoder *g_encoders[RotaryEncoder::kMaxEncoders] = {};
repeating_timer_t g_button_timer;
bool g_button_timer_started = false;

}  // namespace

// The raw GPIO IRQ API takes a plain function without context, so each slot
// gets its own handler that forwards to the registered encoder.
template <int Slot>
void RotaryEncoder::gpioIrqTrampoline() {
    g_encoders[Slot]->handleGpioIrq();
}

static_assert(RotaryEncoder::kMaxEncoders == 4, "update kTrampolines");

RotaryEncoder::RotaryEncoder(uint clk_pin, uint dt_pin, uint sw_pin,
                             int steps_per_detent, bool reversed)
    : clk_pin_(clk_pin), dt_pin_(dt_pin), sw_pin_(sw_pin),
      steps_per_detent_(steps_per_detent > 0 ? steps_per_detent : 1),
      reversed_(reversed) {}

bool RotaryEncoder::init() {
    static const irq_handler_t kTrampolines[kMaxEncoders] = {
        &RotaryEncoder::gpioIrqTrampoline<0>,
        &RotaryEncoder::gpioIrqTrampoline<1>,
        &RotaryEncoder::gpioIrqTrampoline<2>,
        &RotaryEncoder::gpioIrqTrampoline<3>,
    };

    int slot = 0;
    while (slot < kMaxEncoders && g_encoders[slot] != nullptr) slot++;
    if (slot == kMaxEncoders) return false;

    auto initInput = [](uint pin) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    };
    initInput(clk_pin_);
    initInput(dt_pin_);
    if (sw_pin_ != kNoPin) initInput(sw_pin_);
    sleep_us(10);  // let the pull-ups settle before taking the initial state

    last_ab_ = (gpio_get(clk_pin_) << 1) | gpio_get(dt_pin_);

    g_encoders[slot] = this;  // must be set before the IRQ can fire

    // Rotation: interrupt on every edge of both pins, on the calling core
    gpio_add_raw_irq_handler_masked64((1ull << clk_pin_) | (1ull << dt_pin_),
                                      kTrampolines[slot]);
    gpio_set_irq_enabled(clk_pin_, kEdges, true);
    gpio_set_irq_enabled(dt_pin_, kEdges, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

    // Button: one shared timer polls every registered encoder
    if (!g_button_timer_started) {
        // Negative delay = fixed period, measured from the previous start
        add_repeating_timer_ms(-kButtonPollMs, buttonTimerCallback, nullptr, &g_button_timer);
        g_button_timer_started = true;
    }
    return true;
}

int32_t RotaryEncoder::getCount() const {
    return count_;
}

int32_t RotaryEncoder::getPosition() const {
    return count_ / steps_per_detent_;
}

void RotaryEncoder::reset(int32_t position) {
    uint32_t irq_state = save_and_disable_interrupts();
    count_ = position * steps_per_detent_;
    delta_base_ = count_;
    restore_interrupts(irq_state);
}

int32_t RotaryEncoder::readDelta() {
    // Report a detent only after a full detent of travel in either direction,
    // so a knob resting between clicks does not jitter.
    int32_t detents = (count_ - delta_base_) / steps_per_detent_;
    delta_base_ += detents * steps_per_detent_;
    return detents;
}

bool RotaryEncoder::isPressed() const {
    return button_pressed_;
}

bool RotaryEncoder::wasClicked() {
    uint32_t irq_state = save_and_disable_interrupts();
    bool clicked = clicked_;
    clicked_ = false;
    restore_interrupts(irq_state);
    return clicked;
}

void RotaryEncoder::handleGpioIrq() {
    bool clk_event = gpio_get_irq_event_mask(clk_pin_) & kEdges;
    bool dt_event = gpio_get_irq_event_mask(dt_pin_) & kEdges;
    if (!clk_event && !dt_event) return;  // IRQ was for another pin
    if (clk_event) gpio_acknowledge_irq(clk_pin_, kEdges);
    if (dt_event) gpio_acknowledge_irq(dt_pin_, kEdges);

    uint8_t ab = (gpio_get(clk_pin_) << 1) | gpio_get(dt_pin_);
    int8_t step = kQuadratureTable[(last_ab_ << 2) | ab];
    last_ab_ = ab;
    count_ = count_ + (reversed_ ? -step : step);
}

void RotaryEncoder::pollButton() {
    bool raw = !gpio_get(sw_pin_);  // active low
    if (raw != button_raw_) {
        button_raw_ = raw;
        button_same_ms_ = 0;
        return;
    }
    if (button_same_ms_ < kButtonDebounceMs) {
        button_same_ms_++;
        if (button_same_ms_ == kButtonDebounceMs && raw != button_stable_) {
            button_stable_ = raw;
            button_pressed_ = raw;
            if (raw) clicked_ = true;
        }
    }
}

bool RotaryEncoder::buttonTimerCallback(repeating_timer_t *) {
    for (RotaryEncoder *encoder : g_encoders) {
        if (encoder != nullptr && encoder->sw_pin_ != kNoPin) encoder->pollButton();
    }
    return true;  // keep repeating
}
