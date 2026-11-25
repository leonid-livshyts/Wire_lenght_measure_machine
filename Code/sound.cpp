#include "sound.h"

namespace {

// Note frequencies (Hz)
constexpr uint16_t C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784, A5 = 880, C6 = 1047;
constexpr uint16_t REST = 0;

// Finished hank: a rising fanfare, a falling run home, and a short final call. 5000 ms.
constexpr Note kMelody[] = {
    {C5, 200}, {E5, 200}, {G5, 200}, {C6, 400}, {REST, 100}, {G5, 200}, {C6, 600}, {REST, 200},
    {A5, 200}, {G5, 200}, {F5, 200}, {E5, 200}, {D5, 200}, {E5, 200}, {C5, 400}, {REST, 200},
    {G5, 150}, {E5, 150}, {G5, 150}, {C6, 650},
};

constexpr uint32_t sumMs() {
    uint32_t total = 0;
    for (const Note &n : kMelody) total += n.ms;
    return total;
}

constexpr uint32_t kMelodyMs = sumMs();
static_assert(kMelodyMs == 5000, "the finished-hank melody is meant to last 5 s");

}  // namespace

void SoundSequencer::beep(uint32_t now_ms) {
    beeping_ = true;
    beep_start_ms_ = now_ms;
}

void SoundSequencer::playMelody(uint32_t now_ms) {
    melody_ = true;
    melody_start_ms_ = now_ms;
}

void SoundSequencer::startAlarm(uint32_t now_ms) {
    melody_ = false;
    if (alarm_) return;
    alarm_ = true;
    alarm_start_ms_ = now_ms;
}

void SoundSequencer::stopAlarm() {
    alarm_ = false;
}

bool SoundSequencer::alarmActive() const {
    return alarm_;
}

bool SoundSequencer::melodyPlaying(uint32_t now_ms) const {
    return melody_ && now_ms - melody_start_ms_ < kMelodyMs;
}

uint16_t SoundSequencer::frequencyAt(uint32_t now_ms) const {
    if (beeping_ && now_ms - beep_start_ms_ < kBeepMs) return kBeepHz;

    if (alarm_) {
        uint32_t phase = (now_ms - alarm_start_ms_) % (kAlarmOnMs + kAlarmOffMs);
        return phase < kAlarmOnMs ? kAlarmHz : 0;
    }

    if (melodyPlaying(now_ms)) {
        uint32_t t = now_ms - melody_start_ms_;
        for (const Note &n : kMelody) {
            if (t < n.ms) return (n.hz != REST && t < (uint32_t)n.ms - kNoteGapMs) ? n.hz : 0;
            t -= n.ms;
        }
    }
    return 0;
}

uint32_t SoundSequencer::melodyLengthMs() {
    return kMelodyMs;
}
