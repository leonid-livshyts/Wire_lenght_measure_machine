#pragma once

#include <stdint.h>

// One step of a tune: a tone (hz > 0) or a rest (hz = 0) for `ms`.
struct Note {
    uint16_t hz;
    uint16_t ms;
};

// Decides what the buzzer sounds at a given time. Pure logic (no SDK calls),
// so it is unit-tested on the host; the caller passes the time in and plays
// the returned frequency (see Buzzer).
//
// Three sounds, highest priority first:
//  - beep:   one short tone (a click); plays over the others, which keep
//            their timing underneath and continue after it;
//  - alarm:  beeps on and off until stopAlarm() (an error);
//  - melody: the ~5 s tune for a finished hank.
// startAlarm() cancels the melody: the error matters more.
class SoundSequencer {
public:
    static constexpr uint16_t kBeepHz = 2700;   // near a typical 12 mm buzzer's resonance: loudest
    static constexpr uint16_t kBeepMs = 40;
    static constexpr uint16_t kAlarmHz = 2700;
    static constexpr uint16_t kAlarmOnMs = 250;
    static constexpr uint16_t kAlarmOffMs = 250;
    static constexpr uint16_t kNoteGapMs = 20;  // silent end of every note, so repeated notes stay separate

    void beep(uint32_t now_ms);
    void playMelody(uint32_t now_ms);
    void startAlarm(uint32_t now_ms);  // restarting a running alarm keeps its rhythm
    void stopAlarm();

    bool alarmActive() const;
    bool melodyPlaying(uint32_t now_ms) const;

    // Frequency to play now, 0 = silent. now_ms may wrap around.
    uint16_t frequencyAt(uint32_t now_ms) const;

    // Total length of the finished-hank melody in ms.
    static uint32_t melodyLengthMs();

private:
    bool beeping_ = false;
    uint32_t beep_start_ms_ = 0;
    bool alarm_ = false;
    uint32_t alarm_start_ms_ = 0;
    bool melody_ = false;
    uint32_t melody_start_ms_ = 0;
};
