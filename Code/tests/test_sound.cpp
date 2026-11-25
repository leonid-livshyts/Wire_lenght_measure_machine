#include "check.h"
#include "sound.h"
#include <cstdint>

namespace {

void testSilentByDefault() {
    SoundSequencer s;
    CHECK(s.frequencyAt(0) == 0);
    CHECK(s.frequencyAt(123456) == 0);
    CHECK(!s.alarmActive());
}

void testBeepIsShort() {
    SoundSequencer s;
    s.beep(1000);
    CHECK(s.frequencyAt(1000) == SoundSequencer::kBeepHz);
    CHECK(s.frequencyAt(1000 + SoundSequencer::kBeepMs - 1) == SoundSequencer::kBeepHz);
    CHECK(s.frequencyAt(1000 + SoundSequencer::kBeepMs) == 0);
}

void testMelodyLastsFiveSeconds() {
    SoundSequencer s;
    CHECK(SoundSequencer::melodyLengthMs() == 5000);
    s.playMelody(2000);
    CHECK(s.melodyPlaying(2000));
    CHECK(s.frequencyAt(2000) != 0);       // first note
    CHECK(s.frequencyAt(2000 + 4990) == 0);  // gap at the end of the last note
    CHECK(s.frequencyAt(2000 + 4900) != 0);  // last note
    CHECK(s.melodyPlaying(2000 + 4999));
    CHECK(!s.melodyPlaying(2000 + 5000));
    CHECK(s.frequencyAt(2000 + 5000) == 0);
}

void testNotesAreSeparated() {
    SoundSequencer s;
    s.playMelody(0);
    // First note is 200 ms: its last kNoteGapMs are silent
    CHECK(s.frequencyAt(200 - SoundSequencer::kNoteGapMs - 1) != 0);
    CHECK(s.frequencyAt(200 - SoundSequencer::kNoteGapMs) == 0);
    CHECK(s.frequencyAt(200) != 0);  // second note
}

void testBeepPlaysOverMelodyWhichContinues() {
    SoundSequencer s;
    s.playMelody(0);
    uint16_t later = s.frequencyAt(300);
    s.beep(250);
    CHECK(s.frequencyAt(260) == SoundSequencer::kBeepHz);
    CHECK(s.frequencyAt(300) == later);
}

void testAlarmBeepsUntilStopped() {
    SoundSequencer s;
    s.startAlarm(500);
    CHECK(s.alarmActive());
    CHECK(s.frequencyAt(500) == SoundSequencer::kAlarmHz);
    CHECK(s.frequencyAt(500 + SoundSequencer::kAlarmOnMs) == 0);
    uint32_t period = SoundSequencer::kAlarmOnMs + SoundSequencer::kAlarmOffMs;
    CHECK(s.frequencyAt(500 + 100 * period) == SoundSequencer::kAlarmHz);  // still going much later
    s.stopAlarm();
    CHECK(!s.alarmActive());
    CHECK(s.frequencyAt(500 + 100 * period) == 0);
}

void testAlarmCancelsMelody() {
    SoundSequencer s;
    s.playMelody(0);
    s.startAlarm(100);
    CHECK(!s.melodyPlaying(150));
    s.stopAlarm();
    CHECK(s.frequencyAt(150) == 0);  // the melody does not come back
}

void testRestartingAlarmKeepsRhythm() {
    SoundSequencer s;
    s.startAlarm(0);
    s.startAlarm(SoundSequencer::kAlarmOnMs + 10);  // in the off half
    CHECK(s.frequencyAt(SoundSequencer::kAlarmOnMs + 10) == 0);
}

void testWrapAround() {
    SoundSequencer s;
    s.beep(UINT32_MAX - 5);
    CHECK(s.frequencyAt(10) == SoundSequencer::kBeepHz);
    CHECK(s.frequencyAt(100) == 0);
}

}  // namespace

void runSoundTests() {
    testSilentByDefault();
    testBeepIsShort();
    testMelodyLastsFiveSeconds();
    testNotesAreSeparated();
    testBeepPlaysOverMelodyWhichContinues();
    testAlarmBeepsUntilStopped();
    testAlarmCancelsMelody();
    testRestartingAlarmKeepsRhythm();
    testWrapAround();
}
