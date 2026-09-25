#include "check.h"
#include "click_detector.h"

namespace {

void testNoPressNoClick() {
    ClickDetector d;
    CHECK(!d.update(false, 0));
    CHECK(!d.update(false, 100));
}

void testShortPressClicksOnceOnRelease() {
    ClickDetector d;
    CHECK(!d.update(true, 1000));   // pressed: nothing yet
    CHECK(!d.update(true, 1100));   // still held
    CHECK(d.update(false, 1150));   // released after 150 ms: click
    CHECK(!d.update(false, 1200));  // reported only once
}

void testPressExactlyAtLimitIsShort() {
    ClickDetector d;
    d.update(true, 0);
    CHECK(d.update(false, ClickDetector::kShortClickMaxMs));
}

void testLongPressIsIgnored() {
    ClickDetector d;
    d.update(true, 0);
    CHECK(!d.update(true, 1000));
    CHECK(!d.update(false, ClickDetector::kShortClickMaxMs + 1));
}

void testCustomLimit() {
    ClickDetector d(500);
    d.update(true, 0);
    CHECK(!d.update(false, 600));  // 600 ms > 500 ms: too long
    d.update(true, 1000);
    CHECK(d.update(false, 1400));  // 400 ms: short
}

void testMillisecondCounterWraparound() {
    ClickDetector d;
    d.update(true, 0xFFFFFF00u);    // 256 ms before the uint32_t wrap
    CHECK(d.update(false, 100));    // 356 ms later
}

}  // namespace

void runClickDetectorTests() {
    testNoPressNoClick();
    testShortPressClicksOnceOnRelease();
    testPressExactlyAtLimitIsShort();
    testLongPressIsIgnored();
    testCustomLimit();
    testMillisecondCounterWraparound();
}
