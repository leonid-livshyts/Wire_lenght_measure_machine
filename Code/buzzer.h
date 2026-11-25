#pragma once

#include <stdint.h>
#include "pico/types.h"

#include "sound.h"

// Buzzer on one GPIO (BZ1, + on the pin, - to GND), driven by PWM: a square
// wave at the tone frequency, 50 % duty. Needs a passive buzzer (no built-in
// oscillator), which is what lets it play a melody.
//
// Non-blocking: beep() / playMelody() / startAlarm() only record what to play;
// update() (every main-loop pass) sets the tone. The timing lives in
// SoundSequencer, so the tune resolution is one loop pass (5 ms).
class Buzzer {
public:
    explicit Buzzer(uint pin);

    void init();  // pin to PWM, silent

    void beep();        // short click tone
    void playMelody();  // ~5 s tune: the hank is finished
    void startAlarm();  // beeps until stopAlarm(): something went wrong
    void stopAlarm();
    bool alarmActive() const;

    void update(uint32_t now_ms);

private:
    void setTone(uint16_t hz);
    static uint32_t nowMs();

    uint pin_;
    uint slice_ = 0;
    uint channel_ = 0;
    uint16_t playing_hz_ = 0;
    SoundSequencer seq_;
};
