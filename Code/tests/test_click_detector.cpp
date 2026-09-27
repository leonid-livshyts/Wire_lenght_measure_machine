#include "check.h"
#include "click_detector.h"

namespace {

void testNoPressNoClick() {
    ClickDetector d;
    CHECK(d.update(false, 0) == ClickEvent::None);
    CHECK(d.update(false, 100) == ClickEvent::None);
}

void testShortPressClicksOnceOnRelease() {
    ClickDetector d;
    CHECK(d.update(true, 1000) == ClickEvent::None);   // pressed: nothing yet
    CHECK(d.update(true, 1100) == ClickEvent::None);   // still held
    CHECK(d.update(false, 1150) == ClickEvent::Short); // released after 150 ms
    CHECK(d.update(false, 1200) == ClickEvent::None);  // reported only once
}

void testPressExactlyAtLimitIsShort() {
    ClickDetector d;
    d.update(true, 0);
    CHECK(d.update(false, ClickDetector::kShortClickMaxMs) == ClickEvent::Short);
}

void testLongPressReportsLongOnRelease() {
    ClickDetector d;
    d.update(true, 0);
    CHECK(d.update(true, 5000) == ClickEvent::None);  // nothing while held, however long
    CHECK(d.update(false, 5001) == ClickEvent::Long);
    CHECK(d.update(false, 5100) == ClickEvent::None); // reported only once
}

void testJustOverLimitIsLong() {
    ClickDetector d;
    d.update(true, 0);
    CHECK(d.update(false, ClickDetector::kShortClickMaxMs + 1) == ClickEvent::Long);
}

void testCustomLimit() {
    ClickDetector d(500);
    d.update(true, 0);
    CHECK(d.update(false, 600) == ClickEvent::Long);   // 600 ms > 500 ms
    d.update(true, 1000);
    CHECK(d.update(false, 1400) == ClickEvent::Short); // 400 ms
}

void testMillisecondCounterWraparound() {
    ClickDetector d;
    d.update(true, 0xFFFFFF00u);                      // 256 ms before the uint32_t wrap
    CHECK(d.update(false, 100) == ClickEvent::Short); // 356 ms later
    d.update(true, 0xFFFFFF00u);
    CHECK(d.update(false, 3000) == ClickEvent::Long); // 3256 ms later
}

}  // namespace

void runClickDetectorTests() {
    testNoPressNoClick();
    testShortPressClicksOnceOnRelease();
    testPressExactlyAtLimitIsShort();
    testLongPressReportsLongOnRelease();
    testJustOverLimitIsLong();
    testCustomLimit();
    testMillisecondCounterWraparound();
}
