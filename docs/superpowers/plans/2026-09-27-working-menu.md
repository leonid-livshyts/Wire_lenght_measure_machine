# Working Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the placeholder Working menu (`run 1` / `run 2`) with the real five-page flow: set the target length, set the max speed, thread the wire (counting on, motor off), pull (motor on, long-click pause/resume, stall stop, slow-down to a closed-loop creep speed and stop at target − 1 mm), and show the final length.

**Architecture:** Same split as the calibration menu.
- Hardware-free, host-tested pieces:
  - `ClickDetector` also reports **long clicks**.
  - `Menu` / `MenuManager` pass a user-knob long click to the shown tab.
  - `KnobAccel` gets an optional ×1000 step and a `clampAdd()` helper.
  - `run_logic.{h,cpp}` holds the pure maths of a run: when to slow down and stop, `SpeedMeter`, `CreepController` (holds the creep speed by measuring the wire), `StallDetector`.
- `Motor` gets `setSpeed(speed, ramp_ms)` (an exact power with its own ramp time, 0 = at once) and `isRamping()`, so the creep phase can steer the power and stop instantly.
- SDK-touching pages:
  - `number_edit_tab.{h,cpp}` is a generic "edit a number with the knob" page. It is used by the working menu's pages 1–2 and replaces `CalLengthTab`.
  - `working_menu.{h,cpp}` holds pages 3–5 and `WorkingMenu`.

**Tech Stack:** C++17, Raspberry Pi Pico SDK 2.3.1 (RP2350B, Ninja), host tests with the system `g++` and the existing `CHECK` macro.

## Global Constraints

- The pages, in order (from the user's spec):
  1. **Length:** the user sets the wanted wire length with the user knob. Uses the calibration menu's speed-dependent step.
  2. **Max speed:** in percent, default **100 %**, with the same knob system.
  3. **Thread:** counting is active and the display shows how much wire has gone through, but the **motor does not start**. The user puts the wire into the machine.
  4. **Pull:** the motor starts and the display shows the pulled length.
     - A **long click** pauses the motor, and another long click resumes it. The measuring encoder keeps counting while paused.
     - A **short click** goes to page 5. The page never advances by itself.
     - If the target is not reached yet and the motor runs while the measuring encoder sees **no movement for 10 s**, the motor stops.
  5. **Done:** stop the motor if it is not stopped already, show the final length, and stop counting.
- User decisions (asked 2026-09-27):
  - **Long click** = the user knob is released after being held **more than 2 s** (`ClickDetector::kShortClickMaxMs`). Nothing happens while it is held. A press of 2 s or less is a short click, as today.
  - **Target reached:** brake **early**, so the wire stops as close to the target as possible. The motor is not started at all if the target is already reached.
  - **Landing (user, 2026-09-28):** one fixed braking distance cannot be right, so the machine steers by the measured speed instead. Near the target, the motor slows down to a **creep speed in mm/s**. That speed is a constant, to be determined on the machine. During the creep the power is corrected from the measured wire speed. When the pulled length reaches **target − 1 mm**, the motor stops **at once**.
  - Target length and speed are **kept in RAM** between runs. At power-up they are reset to the defaults (1000 mm, 100 %). **Nothing is written to flash.**
  - The length is shown in **mm**. The knob step is **1 / 10 / 100 / 1000 mm** per detent, depending on turning speed.
- Page 1 range: **1 … 99 999 999 mm** (8 digits), default **1000**. Page 2 range: **1 … 100 %**, step ×1 / ×10 at most (×100 would just jump to a limit).
- Display texts (the font has no `M`/`X`):
  - Page 2 shows `SP` plus the number, right-aligned (`SP   100`).
  - A pulled length that needs more than 8 digits shows `--------`, as in calibration.
  - While paused or stalled, page 4 **blinks** (500 ms on / 500 ms off).
- The pulled length is `ticksToMm(measure.getCount(), settings.mm_per_tick)` (from `calibration.h`). The sign of the count is ignored.
- Stall detection runs **only while the motor is driving**: running, slowing down or creeping. It does not run while paused or stopped. One count step of jitter is not movement: movement is **≥ 4 ticks** (one detent).
- Landing model (every constant marked TUNE is a first guess, to be set on the machine):
  - **Slow-down point.** `Motor` ramps along a smoothstep over `Motor::kDefaultRampMs`, and a smoothstep's average is the mean of the start and end speeds. Ramping from speed `v` down to the creep speed therefore passes `(v + kCreepSpeedMmPerS) / 2 × ramp`. The slow-down starts when `pulled + that distance + kCreepMinMm ≥ target`.
    - `kCreepSpeedMmPerS = 20` (TUNE)
    - `kCreepMinMm = 20` (TUNE): the shortest creep, so the speed has time to settle.
  - **First creep power.** Wire speed is assumed proportional to power, so the first guess is `current power × creep / measured speed`. It is clamped to `[kCreepMinPower, current power]`.
  - **Creep control.** Every `SpeedMeter::kWindowMs` (100 ms), `power += kCreepGainPerMmS × (creep − measured)`, clamped to `[kCreepMinPower, max speed %]`.
    - `kCreepMinPower = 0.05` (TUNE)
    - `kCreepGainPerMmS = 0.002` (TUNE)
    - Power changes use a 100 ms ramp (`kCreepRampMs`).
    - The last creep power is remembered and used when a run resumes close to the target.
  - **Stop.** At `pulled ≥ target − kStopBeforeMm`, where `kStopBeforeMm = 1`, the power is set to 0 at once (`Motor::setSpeed(0, 0)`).
  - **Short target or resume.** If the target is no more than `kCreepMinMm` away when the motor starts or resumes, there is no fast phase: it creeps straight away.
- The hardware-free files (`click_detector.*`, `menu*.*`, `knob_accel.*`, `run_logic.*`) must not include Pico SDK headers.
- No heap: no `new`, `std::function`, `std::vector` or `std::string`.
- Code style: `snake_case_` members, `camelCase()` methods, `kConstant` constants, 4-space indent, `#pragma once`, and the comment density of the existing files.
- Every new firmware `.cpp` goes into `add_executable(Code ...)` in `Code/CMakeLists.txt`. Every new hardware-free `.cpp` also goes into `SOURCES` in `Code/tests/run_tests.sh`. Do not edit the `DO NOT EDIT` block.
- The working tree has unrelated changes (Case/, PCB/). **Commit only the paths listed in each task** (`git commit -- <paths>` after `git add <paths>`). Never use `git add -A` or `git commit -a`.

## File Structure

All paths are relative to the repository root.

| File | Responsibility |
|---|---|
| `Code/click_detector.{h,cpp}` (modify) | `ClickEvent { None, Short, Long }`; `update()` returns it |
| `Code/menu.{h,cpp}` (modify) | `MenuTab::onLongClick()`, `Menu::longClick()` |
| `Code/menu_manager.{h,cpp}` (modify) | `MenuInput::user_long_click`, forwarded to the open menu |
| `Code/knob_accel.{h,cpp}` (modify) | `max_factor` cap, ×1000 level, `clampAdd()` |
| `Code/run_logic.{h,cpp}` (create) | Target/speed limits, landing constants, `slowDownDistanceMm()`, `shouldSlowDown()`, `shouldStop()`, `estimateCreepPower()`, `SpeedMeter`, `CreepController`, `StallDetector` |
| `Code/motor.{h,cpp}` (modify) | `setSpeed(speed, ramp_ms)`, `isRamping()`; per-ramp duration |
| `Code/number_edit_tab.{h,cpp}` (create) | `NumberEditTab`: generic knob-edited number page with an optional 2-char label |
| `Code/calibration_menu.{h,cpp}` (modify) | `CalLengthTab` replaced by `NumberEditTab` |
| `Code/working_menu.{h,cpp}` (create) | `WorkingContext`, `RunThreadTab`, `RunPullTab`, `RunDoneTab`, `WorkingMenu` |
| `Code/Code.cpp` (modify) | Long clicks into `MenuInput`; real Working menu instead of the placeholders |
| `Code/CMakeLists.txt` (modify) | New sources |
| `Code/tests/*` (modify / create) | Tests per unit, registration |
| `Code/CLAUDE.md`, `Instructions/working.md` (modify / create) | Developer and user docs |

Commands used throughout (run from the repo root):
- Host tests: `Code/tests/run_tests.sh` → last line `All tests passed`
- Firmware build: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build` → ends without `error:`, and `Code/build/Code.uf2` is updated

---

### Task 1: Long clicks in `ClickDetector`

**Files:**
- Modify: `Code/click_detector.h`, `Code/click_detector.cpp`
- Modify: `Code/tests/test_click_detector.cpp`
- Modify: `Code/Code.cpp` (keep the firmware compiling)

**Interfaces:**
- Consumes: nothing new.
- Produces: `enum class ClickEvent : uint8_t { None, Short, Long };` and `ClickEvent ClickDetector::update(bool pressed, uint32_t now_ms)`. On release, the result is `Short` if the press lasted ≤ `max_short_ms`, otherwise `Long`. It is `None` in every other pass.

- [ ] **Step 1: Rewrite the tests for the new return type**

Replace the whole content of `Code/tests/test_click_detector.cpp` with:

```cpp
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
```

- [ ] **Step 2: Run the tests and check that they fail**

Run: `Code/tests/run_tests.sh`
Expected: compile error, `'ClickEvent' has not been declared`.

- [ ] **Step 3: Implement `ClickEvent`**

Replace the whole content of `Code/click_detector.h` with:

```cpp
#pragma once

#include <stdint.h>

// What one press of a button turned out to be, reported on release.
enum class ClickEvent : uint8_t {
    None,   // no release in this pass
    Short,  // held for at most max_short_ms: flips menu pages
    Long,   // held longer: e.g. pause/resume while pulling wire
};

// Turns a debounced button level into short and long clicks.
//
// Both are reported once, on release, so nothing happens while the button
// is held. Pure logic (no SDK calls), so it is unit-tested on the host; the
// caller passes the time in.
class ClickDetector {
public:
    static constexpr uint32_t kShortClickMaxMs = 2000;

    explicit ClickDetector(uint32_t max_short_ms = kShortClickMaxMs);

    // Call regularly with the current button state and time. Returns Short
    // or Long exactly once per press (on release), None otherwise.
    // now_ms may wrap around.
    ClickEvent update(bool pressed, uint32_t now_ms);

private:
    uint32_t max_short_ms_;
    bool was_pressed_ = false;
    uint32_t press_start_ms_ = 0;
};
```

Replace the whole content of `Code/click_detector.cpp` with:

```cpp
#include "click_detector.h"

ClickDetector::ClickDetector(uint32_t max_short_ms) : max_short_ms_(max_short_ms) {}

ClickEvent ClickDetector::update(bool pressed, uint32_t now_ms) {
    if (pressed && !was_pressed_) press_start_ms_ = now_ms;
    bool released = !pressed && was_pressed_;
    was_pressed_ = pressed;
    if (!released) return ClickEvent::None;

    // Unsigned subtraction keeps the duration right across a counter wrap
    return (uint32_t)(now_ms - press_start_ms_) <= max_short_ms_ ? ClickEvent::Short
                                                                 : ClickEvent::Long;
}
```

In `Code/Code.cpp`, in the main loop, replace:

```cpp
        input.user_click = userClicks.update(userEncoder.isPressed(), now);
        input.measure_click = measureClicks.update(measureEncoder.isPressed(), now);
```

with:

```cpp
        input.user_click = userClicks.update(userEncoder.isPressed(), now) == ClickEvent::Short;
        input.measure_click = measureClicks.update(measureEncoder.isPressed(), now) == ClickEvent::Short;
```

- [ ] **Step 4: Run the tests and the firmware build**

Run: `Code/tests/run_tests.sh` → Expected: `All tests passed`
Run: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build` → Expected: builds without errors.

- [ ] **Step 5: Commit**

```bash
git add Code/click_detector.h Code/click_detector.cpp Code/tests/test_click_detector.cpp Code/Code.cpp
git commit -m "feat(firmware): report long clicks from ClickDetector" -- Code/click_detector.h Code/click_detector.cpp Code/tests/test_click_detector.cpp Code/Code.cpp
```

---

### Task 2: Long click through `Menu` and `MenuManager`

**Files:**
- Modify: `Code/menu.h`, `Code/menu.cpp`, `Code/menu_manager.h`, `Code/menu_manager.cpp`
- Modify: `Code/tests/fake_tab.h`, `Code/tests/test_menu.cpp`, `Code/tests/test_menu_manager.cpp`
- Modify: `Code/Code.cpp`

**Interfaces:**
- Consumes: `ClickEvent` (Task 1).
- Produces:
  - `virtual TabAction MenuTab::onLongClick()`, default `TabAction::Stay`.
  - `void Menu::longClick()`, ignored while the menu is closed.
  - `bool MenuInput::user_long_click`. It is forwarded to the open menu. It **never opens a menu** while idle.

- [ ] **Step 1: Extend `FakeTab`**

In `Code/tests/fake_tab.h`, add the members below after `bool skipped = false;  // isSkipped() result`:

```cpp
    int long_clicks = 0;
    TabAction long_click_action = TabAction::Stay;
```

and this override after the `onClick()` override:

```cpp
    TabAction onLongClick() override { long_clicks++; return long_click_action; }
```

- [ ] **Step 2: Write the failing menu tests**

In `Code/tests/test_menu.cpp`, add these inside the anonymous namespace, before its closing `}  // namespace`:

```cpp
void testLongClickGoesToShownTab() {
    FakeTab a, b;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.longClick();
    CHECK(a.long_clicks == 1);
    CHECK(a.clicks == 0);
    CHECK(m.currentTab() == 0);  // default: Stay

    a.long_click_action = TabAction::Next;
    m.longClick();
    CHECK(m.currentTab() == 1);
    CHECK(b.long_clicks == 0);
}

void testLongClickWhileClosedIsIgnored() {
    FakeTab a;
    Menu m;
    m.addTab(a);
    m.longClick();
    CHECK(a.long_clicks == 0);
    CHECK(!m.isOpen());
}
```

and add at the end of `runMenuTests()`:

```cpp
    testLongClickGoesToShownTab();
    testLongClickWhileClosedIsIgnored();
```

In `Code/tests/test_menu_manager.cpp`, add this helper after `MenuInput userTurn(...)`:

```cpp
MenuInput userLongClick() { MenuInput in; in.user_long_click = true; return in; }
```

add these tests inside the anonymous namespace, before its closing `}  // namespace`:

```cpp
void testLongClickDoesNotOpenMenu() {
    Fixture f;
    f.manager.process(userLongClick(), 0);
    CHECK(f.manager.activeMenu() == nullptr);
    CHECK(f.work1.enters == 0);
    CHECK(f.cal1.enters == 0);
}

void testLongClickReachesOpenTab() {
    Fixture f;
    f.manager.process(userClick(), 0);
    f.manager.process(userLongClick(), 5);
    CHECK(f.work1.long_clicks == 1);
    CHECK(f.work1.clicks == 0);
    CHECK(f.manager.activeMenu() == &f.working);
}
```

and add at the end of `runMenuManagerTests()`:

```cpp
    testLongClickDoesNotOpenMenu();
    testLongClickReachesOpenTab();
```

- [ ] **Step 3: Run the tests and check that they fail**

Run: `Code/tests/run_tests.sh`
Expected: compile errors, e.g. `'class Menu' has no member named 'longClick'`, and `'onLongClick' marked 'override', but does not override`.

- [ ] **Step 4: Implement**

In `Code/menu.h`, in `MenuTab`, after the `onClick()` declaration add:

```cpp
    // Long click on the user encoder (held > 2 s, reported on release).
    // Default: ignored.
    virtual TabAction onLongClick() { return TabAction::Stay; }
```

In `Code/menu.h`, in `Menu`, change the input declarations to:

```cpp
    // Input for the shown tab; ignored while closed.
    void turn(int32_t detents);
    void click();
    void longClick();
    void update(uint32_t now_ms);
```

In `Code/menu.cpp`, after `Menu::click()` add:

```cpp
void Menu::longClick() {
    if (isOpen()) apply(tabs_[current_]->onLongClick());
}
```

In `Code/menu_manager.h`, change `MenuInput` to:

```cpp
struct MenuInput {
    int32_t user_turn = 0;         // user knob detents since the last pass
    bool user_click = false;       // short click on the user knob
    bool user_long_click = false;  // long click on the user knob; never opens a menu
    bool measure_click = false;    // short click on the measuring-roll encoder
};
```

In `Code/menu_manager.cpp`, in `process()`, change the open-menu branch to:

```cpp
        active_->turn(input.user_turn);
        if (input.user_click) active_->click();
        if (input.user_long_click) active_->longClick();
```

In `Code/Code.cpp`, replace the line

```cpp
        input.user_click = userClicks.update(userEncoder.isPressed(), now) == ClickEvent::Short;
```

with:

```cpp
        ClickEvent user_click = userClicks.update(userEncoder.isPressed(), now);
        input.user_click = user_click == ClickEvent::Short;
        input.user_long_click = user_click == ClickEvent::Long;
```

- [ ] **Step 5: Run the tests and the firmware build**

Run: `Code/tests/run_tests.sh` → Expected: `All tests passed`
Run: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build` → Expected: builds without errors.

- [ ] **Step 6: Commit**

```bash
git add Code/menu.h Code/menu.cpp Code/menu_manager.h Code/menu_manager.cpp Code/tests/fake_tab.h Code/tests/test_menu.cpp Code/tests/test_menu_manager.cpp Code/Code.cpp
git commit -m "feat(firmware): pass user-knob long clicks to the shown tab" -- Code/menu.h Code/menu.cpp Code/menu_manager.h Code/menu_manager.cpp Code/tests/fake_tab.h Code/tests/test_menu.cpp Code/tests/test_menu_manager.cpp Code/Code.cpp
```

---

### Task 3: `KnobAccel` ×1000 level and `clampAdd()`

**Files:**
- Modify: `Code/knob_accel.h`, `Code/knob_accel.cpp`, `Code/tests/test_knob_accel.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `explicit KnobAccel(int32_t max_factor = 100)`. The step is ×1000 for gaps ≤ `kFastestGapMs` (20 ms), then capped at `max_factor`. The default of 100 keeps the calibration page exactly as it is today.
  - `int32_t clampAdd(int32_t value, int32_t delta, int32_t min_value, int32_t max_value)`: overflow-safe add, then clamp.

- [ ] **Step 1: Write the failing tests**

In `Code/tests/test_knob_accel.cpp`, add inside the anonymous namespace, before `}  // namespace`:

```cpp
void testFastestTurnsNeedCap1000() {
    KnobAccel wide(1000);
    wide.scale(1, 1000);
    CHECK(wide.scale(1, 1000 + KnobAccel::kFastestGapMs) == 1000);
    CHECK(wide.scale(-1, 1020 + KnobAccel::kFastGapMs) == -100);  // 40 ms: x100

    KnobAccel normal;  // default cap 100: calibration unchanged
    normal.scale(1, 1000);
    CHECK(normal.scale(1, 1000 + KnobAccel::kFastestGapMs) == 100);
}

void testSmallCap() {
    KnobAccel narrow(10);
    narrow.scale(1, 1000);
    CHECK(narrow.scale(1, 1005) == 10);  // fastest turn still only x10
    CHECK(narrow.scale(1, 5000) == 1);
}

void testClampAdd() {
    CHECK(clampAdd(50, 10, 1, 100) == 60);
    CHECK(clampAdd(95, 10, 1, 100) == 100);
    CHECK(clampAdd(5, -10, 1, 100) == 1);
    CHECK(clampAdd(INT32_MAX - 1, INT32_MAX, 1, INT32_MAX) == INT32_MAX);  // no overflow
    CHECK(clampAdd(1, INT32_MIN, 1, 100) == 1);
}
```

Add `#include <cstdint>` at the top of the file, and add at the end of `runKnobAccelTests()`:

```cpp
    testFastestTurnsNeedCap1000();
    testSmallCap();
    testClampAdd();
```

- [ ] **Step 2: Run the tests and check that they fail**

Run: `Code/tests/run_tests.sh`
Expected: compile errors: `'kFastestGapMs' is not a member of 'KnobAccel'`, and `'clampAdd' was not declared`.

- [ ] **Step 3: Implement**

Replace the whole content of `Code/knob_accel.h` with:

```cpp
#pragma once

#include <stdint.h>

// Speed-dependent step for editing a number with the user knob: slow turns
// change it by 1 per detent, faster turns by 10, 100 or 1000, so both fine
// adjustment and big jumps are quick. Pure logic; the caller passes the time.
class KnobAccel {
public:
    static constexpr uint32_t kFastestGapMs = 20;  // turns this close: x1000
    static constexpr uint32_t kFastGapMs = 40;     // turns this close: x100
    static constexpr uint32_t kMediumGapMs = 100;  // turns this close: x10

    // max_factor caps the step: 100 by default, 1000 for long lengths,
    // 10 for small ranges where x100 would only jump to a limit.
    explicit KnobAccel(int32_t max_factor = 100) : max_factor_(max_factor) {}

    // Returns detents scaled by the factor for the gap since the previous
    // turn. detents == 0 returns 0 and is not counted as a turn. now_ms may wrap around.
    int32_t scale(int32_t detents, uint32_t now_ms);

    // Forgets the previous turn, so the next one is x1.
    void reset();

private:
    int32_t max_factor_;
    bool has_last_ = false;
    uint32_t last_ms_ = 0;
};

// value + delta, clamped to [min_value, max_value]. Safe for any int32 inputs.
int32_t clampAdd(int32_t value, int32_t delta, int32_t min_value, int32_t max_value);
```

Replace the whole content of `Code/knob_accel.cpp` with:

```cpp
#include "knob_accel.h"

int32_t KnobAccel::scale(int32_t detents, uint32_t now_ms) {
    if (detents == 0) return 0;
    int32_t factor = 1;
    if (has_last_) {
        uint32_t gap = now_ms - last_ms_;  // unsigned: correct across the wrap
        if (gap <= kFastestGapMs) {
            factor = 1000;
        } else if (gap <= kFastGapMs) {
            factor = 100;
        } else if (gap <= kMediumGapMs) {
            factor = 10;
        }
    }
    if (factor > max_factor_) factor = max_factor_;
    has_last_ = true;
    last_ms_ = now_ms;
    return detents * factor;
}

void KnobAccel::reset() {
    has_last_ = false;
}

int32_t clampAdd(int32_t value, int32_t delta, int32_t min_value, int32_t max_value) {
    int64_t sum = (int64_t)value + delta;  // int64: no overflow
    if (sum < min_value) return min_value;
    if (sum > max_value) return max_value;
    return (int32_t)sum;
}
```

- [ ] **Step 4: Run the tests**

Run: `Code/tests/run_tests.sh` → Expected: `All tests passed`. This includes the old KnobAccel tests, which are unchanged.

- [ ] **Step 5: Commit**

```bash
git add Code/knob_accel.h Code/knob_accel.cpp Code/tests/test_knob_accel.cpp
git commit -m "feat(firmware): optional x1000 knob step and clampAdd helper" -- Code/knob_accel.h Code/knob_accel.cpp Code/tests/test_knob_accel.cpp
```

---

### Task 4: Run maths (`run_logic.{h,cpp}`)

**Files:**
- Create: `Code/run_logic.h`, `Code/run_logic.cpp`, `Code/tests/test_run_logic.cpp`
- Modify: `Code/tests/run_tests.sh`, `Code/tests/test_main.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces (all in `run_logic.h`):
  - `kMinTargetMm = 1`, `kMaxTargetMm = 99999999`, `kDefaultTargetMm = 1000`
  - `kMinSpeedPercent = 1`, `kMaxSpeedPercent = 100`, `kDefaultSpeedPercent = 100`
  - `kCreepSpeedMmPerS = 20.0`, `kCreepMinMm = 20`, `kStopBeforeMm = 1`, `kCreepMinPower = 0.05f`, `kCreepGainPerMmS = 0.002f`
  - `double slowDownDistanceMm(double speed_mm_per_s, uint32_t ramp_ms)`
  - `bool shouldSlowDown(int64_t pulled_mm, int32_t target_mm, double speed_mm_per_s, uint32_t ramp_ms)`
  - `bool shouldStop(int64_t pulled_mm, int32_t target_mm)`
  - `float estimateCreepPower(float power, double speed_mm_per_s)`
  - `class SpeedMeter { void reset(int64_t mm, uint32_t now_ms); double update(int64_t mm, uint32_t now_ms); }`, with `kWindowMs = 100`
  - `class CreepController { void start(float power, float max_power, uint32_t now_ms); float update(double speed_mm_per_s, uint32_t now_ms); float power() const; }`
  - `class StallDetector { void reset(int32_t count, uint32_t now_ms); bool update(int32_t count, uint32_t now_ms); }`, with `kTimeoutMs = 10000` and `kMinMoveTicks = 4`

The tests compute their expected values from the TUNE constants, so they keep passing after the constants are tuned.

- [ ] **Step 1: Write the failing tests**

Create `Code/tests/test_run_logic.cpp`:

```cpp
#include <cmath>

#include "check.h"
#include "run_logic.h"

namespace {

bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

void testSlowDownDistance() {
    // Smoothstep ramp from 120 mm/s to the creep speed over 2.5 s
    CHECK(near(slowDownDistanceMm(120.0, 2500), (120.0 + kCreepSpeedMmPerS) / 2.0 * 2.5));
    CHECK(near(slowDownDistanceMm(kCreepSpeedMmPerS, 2500), 0.0));  // already at creep speed
    CHECK(near(slowDownDistanceMm(0.0, 2500), 0.0));
}

void testShouldSlowDown() {
    double lead = slowDownDistanceMm(120.0, 2500) + kCreepMinMm;
    int64_t at = 1000 - (int64_t)std::floor(lead);  // first whole mm where the lead reaches 1000
    CHECK(shouldSlowDown(at, 1000, 120.0, 2500));
    CHECK(!shouldSlowDown(at - 1, 1000, 120.0, 2500));
    // Standing still: only the creep distance is needed
    CHECK(shouldSlowDown(1000 - kCreepMinMm, 1000, 0.0, 2500));
    CHECK(!shouldSlowDown(1000 - kCreepMinMm - 1, 1000, 0.0, 2500));
}

void testShouldStop() {
    CHECK(shouldStop(1000 - kStopBeforeMm, 1000));
    CHECK(!shouldStop(1000 - kStopBeforeMm - 1, 1000));
    CHECK(shouldStop(1005, 1000));  // overshot
}

void testEstimateCreepPower() {
    CHECK(near(estimateCreepPower(1.0f, kCreepSpeedMmPerS * 5.0), 0.2));  // speed ~ power
    CHECK(near(estimateCreepPower(0.5f, kCreepSpeedMmPerS / 2.0), 0.5));  // never above the current power
    CHECK(near(estimateCreepPower(0.3f, 0.0), 0.3));                      // no speed yet: keep the power
    CHECK(near(estimateCreepPower(1.0f, kCreepSpeedMmPerS * 1000.0), kCreepMinPower));
    CHECK(near(estimateCreepPower(0.02f, 0.0), kCreepMinPower));
}

void testCreepControllerAdjustsEveryWindow() {
    CreepController c;
    c.start(0.2f, 1.0f, 0);
    CHECK(near(c.update(kCreepSpeedMmPerS - 10.0, SpeedMeter::kWindowMs - 1), 0.2));  // not yet
    double faster = 0.2 + 10.0 * kCreepGainPerMmS;                                     // too slow: more power
    CHECK(near(c.update(kCreepSpeedMmPerS - 10.0, SpeedMeter::kWindowMs), faster));
    CHECK(near(c.update(kCreepSpeedMmPerS + 10.0, 2 * SpeedMeter::kWindowMs), 0.2)); // too fast: less
    CHECK(near(c.power(), 0.2));
}

void testCreepControllerClamps() {
    CreepController c;
    c.start(0.0f, 1.0f, 0);
    CHECK(near(c.power(), kCreepMinPower));
    CHECK(near(c.update(kCreepSpeedMmPerS + 1000.0, SpeedMeter::kWindowMs), kCreepMinPower));
    c.start(0.9f, 0.5f, 0);  // user max speed 50 %
    CHECK(near(c.power(), 0.5));
    CHECK(near(c.update(0.0, SpeedMeter::kWindowMs), 0.5));
}

void testSpeedMeterNeedsFullWindow() {
    SpeedMeter m;
    m.reset(0, 1000);
    CHECK(near(m.update(5, 1050), 0.0));     // window not full yet
    CHECK(near(m.update(10, 1100), 100.0));  // 10 mm in 100 ms
    CHECK(near(m.update(12, 1150), 100.0));  // keeps the last value mid-window
    CHECK(near(m.update(30, 1200), 200.0));  // 20 mm in the next 100 ms
}

void testSpeedMeterResetClearsSpeed() {
    SpeedMeter m;
    m.reset(0, 0);
    m.update(50, 100);
    m.reset(50, 200);
    CHECK(near(m.update(50, 250), 0.0));
}

void testSpeedMeterWraparound() {
    SpeedMeter m;
    m.reset(0, 0xFFFFFFC0u);                        // 64 ms before the wrap
    CHECK(near(m.update(10, 0x00000024u), 100.0));  // 100 ms later
}

void testStallAfterTimeout() {
    StallDetector s;
    s.reset(0, 0);
    CHECK(!s.update(0, StallDetector::kTimeoutMs - 1));
    CHECK(s.update(0, StallDetector::kTimeoutMs));
}

void testJitterIsNotMovement() {
    StallDetector s;
    s.reset(100, 0);
    CHECK(!s.update(103, 5000));                     // 3 ticks: jitter
    CHECK(s.update(97, StallDetector::kTimeoutMs));  // still no real movement
}

void testMovementRestartsTimeout() {
    StallDetector s;
    s.reset(0, 0);
    CHECK(!s.update(4, 5000));  // one detent: moving
    CHECK(!s.update(4, 14999));
    CHECK(s.update(4, 15000));
    StallDetector back;
    back.reset(0, 0);
    CHECK(!back.update(-4, 5000));  // either direction counts
    CHECK(!back.update(-4, 14999));
}

void testStallWraparound() {
    StallDetector s;
    s.reset(0, 0xFFFFF000u);
    CHECK(!s.update(0, 0x00001000u));  // 8192 ms across the wrap
}

}  // namespace

void runRunLogicTests() {
    testSlowDownDistance();
    testShouldSlowDown();
    testShouldStop();
    testEstimateCreepPower();
    testCreepControllerAdjustsEveryWindow();
    testCreepControllerClamps();
    testSpeedMeterNeedsFullWindow();
    testSpeedMeterResetClearsSpeed();
    testSpeedMeterWraparound();
    testStallAfterTimeout();
    testJitterIsNotMovement();
    testMovementRestartsTimeout();
    testStallWraparound();
}
```

In `Code/tests/test_main.cpp`, add `void runRunLogicTests();` after `void runCalibrationTests();`, and `runRunLogicTests();` after `runCalibrationTests();`.

In `Code/tests/run_tests.sh`, change the `SOURCES` line to:

```sh
SOURCES="../click_detector.cpp ../menu.cpp ../menu_manager.cpp ../knob_accel.cpp ../settings.cpp ../calibration.cpp ../run_logic.cpp"
```

- [ ] **Step 2: Run the tests and check that they fail**

Run: `Code/tests/run_tests.sh`
Expected: `fatal error: run_logic.h: No such file or directory`.

- [ ] **Step 3: Implement**

Create `Code/run_logic.h`:

```cpp
#pragma once

#include <stdint.h>

// Pure maths of the working menu (host-tested).

constexpr int32_t kMinTargetMm = 1;
constexpr int32_t kMaxTargetMm = 99999999;  // 8 display digits
constexpr int32_t kDefaultTargetMm = 1000;

constexpr int32_t kMinSpeedPercent = 1;
constexpr int32_t kMaxSpeedPercent = 100;
constexpr int32_t kDefaultSpeedPercent = 100;

// --- Landing on the target ---
// No single braking distance fits every speed and wire, so near the target
// the motor slows down to a creep speed, the power is corrected from the
// measured wire speed, and the motor stops at once kStopBeforeMm before the
// target; the wire coasts the rest. TUNE = first guess, set on the machine.
constexpr double kCreepSpeedMmPerS = 20.0;  // TUNE
constexpr int32_t kCreepMinMm = 20;         // shortest creep, so the speed settles; TUNE
constexpr int32_t kStopBeforeMm = 1;
constexpr float kCreepMinPower = 0.05f;     // lowest motor power while creeping; TUNE
constexpr float kCreepGainPerMmS = 0.002f;  // power change per mm/s of error, per adjustment; TUNE

// Wire that passes while the motor ramps from speed_mm_per_s down to the
// creep speed over ramp_ms. The ramp is a smoothstep, whose average speed is
// the mean of both ends. 0 if already at or below the creep speed.
double slowDownDistanceMm(double speed_mm_per_s, uint32_t ramp_ms);

// True once the motor must start slowing down to still creep kCreepMinMm
// before the target.
bool shouldSlowDown(int64_t pulled_mm, int32_t target_mm, double speed_mm_per_s, uint32_t ramp_ms);

// True once pulled_mm >= target_mm - kStopBeforeMm: stop the motor now.
bool shouldStop(int64_t pulled_mm, int32_t target_mm);

// First guess for the creep power, taking wire speed as proportional to
// power: power * creep / speed, at most `power`, at least kCreepMinPower.
// `power` itself (at least kCreepMinPower) while no speed is measured.
float estimateCreepPower(float power, double speed_mm_per_s);

// Wire speed from the pulled length, measured over windows of kWindowMs.
class SpeedMeter {
public:
    static constexpr uint32_t kWindowMs = 100;

    // Starts measuring from `mm`; the speed is 0 until a full window has passed.
    void reset(int64_t mm, uint32_t now_ms);

    // Call every pass. Returns the speed of the last full window in mm/s.
    // now_ms may wrap around.
    double update(int64_t mm, uint32_t now_ms);

private:
    int64_t window_mm_ = 0;
    uint32_t window_start_ms_ = 0;
    double speed_mm_per_s_ = 0.0;
};

// Holds the creep speed: every SpeedMeter::kWindowMs it moves the motor
// power by kCreepGainPerMmS per mm/s of error (a simple integral controller).
class CreepController {
public:
    // Starts from `power`, clamped to [kCreepMinPower, max_power]
    // (max_power = the user's max speed). The first adjustment comes one
    // window later.
    void start(float power, float max_power, uint32_t now_ms);

    // Call every pass with the measured wire speed. Returns the power to
    // drive. now_ms may wrap around.
    float update(double speed_mm_per_s, uint32_t now_ms);

    // Current power. Kept between runs as the best guess for the next creep.
    float power() const { return power_; }

private:
    float power_ = 0.2f;  // TUNE: guess before any creep has run
    float max_power_ = 1.0f;
    uint32_t last_adjust_ms_ = 0;
};

// Detects "motor driving but the wire does not move" from the raw count.
class StallDetector {
public:
    static constexpr uint32_t kTimeoutMs = 10000;
    static constexpr int32_t kMinMoveTicks = 4;  // one detent; less is encoder jitter

    void reset(int32_t count, uint32_t now_ms);

    // True once the count has not moved by kMinMoveTicks for kTimeoutMs.
    // now_ms may wrap around.
    bool update(int32_t count, uint32_t now_ms);

private:
    int32_t last_count_ = 0;
    uint32_t last_move_ms_ = 0;
};
```

Create `Code/run_logic.cpp`:

```cpp
#include "run_logic.h"

namespace {

// Clamped to [kCreepMinPower, max_power]; the minimum wins if they cross.
float clampPower(float power, float max_power) {
    if (power > max_power) power = max_power;
    if (power < kCreepMinPower) power = kCreepMinPower;
    return power;
}

}  // namespace

double slowDownDistanceMm(double speed_mm_per_s, uint32_t ramp_ms) {
    if (speed_mm_per_s <= kCreepSpeedMmPerS) return 0.0;
    return (speed_mm_per_s + kCreepSpeedMmPerS) / 2.0 * (ramp_ms / 1000.0);
}

bool shouldSlowDown(int64_t pulled_mm, int32_t target_mm, double speed_mm_per_s, uint32_t ramp_ms) {
    double lead = slowDownDistanceMm(speed_mm_per_s, ramp_ms) + kCreepMinMm;
    return (double)pulled_mm + lead >= (double)target_mm;
}

bool shouldStop(int64_t pulled_mm, int32_t target_mm) {
    return pulled_mm >= (int64_t)target_mm - kStopBeforeMm;
}

float estimateCreepPower(float power, double speed_mm_per_s) {
    float guess = speed_mm_per_s > 0.0 ? (float)(power * kCreepSpeedMmPerS / speed_mm_per_s) : power;
    return clampPower(guess, power);
}

void SpeedMeter::reset(int64_t mm, uint32_t now_ms) {
    window_mm_ = mm;
    window_start_ms_ = now_ms;
    speed_mm_per_s_ = 0.0;
}

double SpeedMeter::update(int64_t mm, uint32_t now_ms) {
    uint32_t elapsed = now_ms - window_start_ms_;  // unsigned: correct across the wrap
    if (elapsed >= kWindowMs) {
        speed_mm_per_s_ = (double)(mm - window_mm_) * 1000.0 / (double)elapsed;
        window_mm_ = mm;
        window_start_ms_ = now_ms;
    }
    return speed_mm_per_s_;
}

void CreepController::start(float power, float max_power, uint32_t now_ms) {
    max_power_ = max_power;
    power_ = clampPower(power, max_power);
    last_adjust_ms_ = now_ms;
}

float CreepController::update(double speed_mm_per_s, uint32_t now_ms) {
    if ((uint32_t)(now_ms - last_adjust_ms_) >= SpeedMeter::kWindowMs) {
        float error = (float)(kCreepSpeedMmPerS - speed_mm_per_s);  // > 0: too slow
        power_ = clampPower(power_ + kCreepGainPerMmS * error, max_power_);
        last_adjust_ms_ = now_ms;
    }
    return power_;
}

void StallDetector::reset(int32_t count, uint32_t now_ms) {
    last_count_ = count;
    last_move_ms_ = now_ms;
}

bool StallDetector::update(int32_t count, uint32_t now_ms) {
    int64_t moved = (int64_t)count - last_count_;  // int64: no overflow
    if (moved >= kMinMoveTicks || moved <= -kMinMoveTicks) {
        last_count_ = count;
        last_move_ms_ = now_ms;
        return false;
    }
    return (uint32_t)(now_ms - last_move_ms_) >= kTimeoutMs;
}
```

- [ ] **Step 4: Run the tests**

Run: `Code/tests/run_tests.sh` → Expected: `All tests passed`.

- [ ] **Step 5: Commit**

```bash
git add Code/run_logic.h Code/run_logic.cpp Code/tests/test_run_logic.cpp Code/tests/test_main.cpp Code/tests/run_tests.sh
git commit -m "feat(firmware): landing maths: slow-down point, creep speed control, stall detection" -- Code/run_logic.h Code/run_logic.cpp Code/tests/test_run_logic.cpp Code/tests/test_main.cpp Code/tests/run_tests.sh
```

---

### Task 5: `Motor::setSpeed()` and `isRamping()`

**Files:**
- Modify: `Code/motor.h`, `Code/motor.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `void Motor::setSpeed(float speed, uint32_t ramp_ms)`: drives at exactly `speed` (0.0 … 1.0, **not** limited by the max speed), ramping over `ramp_ms` (0 = at once). `speed > 0` counts as running; `0` stops.
  - `bool Motor::isRamping() const`
  - `start()` / `stop()` / `setMaxSpeed()` keep their behaviour and still use the constructor's `ramp_ms`.

`Motor` uses the SDK, so it has no host test. It is verified by the build and by a hardware check that calibration start/stop is unchanged.

- [ ] **Step 1: Header**

In `Code/motor.h`, after the `isRunning()` declaration add:

```cpp
    // Drives at exactly `speed` (0.0 .. 1.0, not limited by the max speed),
    // reaching it over ramp_ms (0 = at once). 0.0 stops the motor. Used to
    // hold a measured wire speed, e.g. creeping onto the target length.
    void setSpeed(float speed, uint32_t ramp_ms);

    // True while a ramp started by any command is still in progress.
    bool isRamping() const;
```

In the private part, change `void rampTo(float target);` to:

```cpp
    void rampTo(float target, uint64_t ramp_us);
```

change `uint32_t ramp_us_;` to:

```cpp
    uint32_t ramp_us_;  // ramp time of start() / stop() / setMaxSpeed()
```

and in the ramp state block, after `uint64_t ramp_start_us_ = 0;`, add:

```cpp
    uint64_t ramp_len_us_ = 0;  // duration of the current ramp
```

- [ ] **Step 2: Implementation**

In `Code/motor.cpp`, replace `start()`, `stop()` and `setMaxSpeed()` with:

```cpp
void Motor::start() {
    running_ = true;
    rampTo(max_percent_ / 100.0f, ramp_us_);
}

void Motor::stop() {
    running_ = false;
    rampTo(0.0f, ramp_us_);
}

void Motor::setMaxSpeed(uint8_t percent) {
    max_percent_ = percent > 100 ? 100 : percent;
    if (running_) rampTo(max_percent_ / 100.0f, ramp_us_);
}
```

After `isRunning()` add:

```cpp
void Motor::setSpeed(float speed, uint32_t ramp_ms) {
    if (speed < 0.0f) speed = 0.0f;
    if (speed > 1.0f) speed = 1.0f;
    running_ = speed > 0.0f;
    rampTo(speed, (uint64_t)ramp_ms * 1000);
}

bool Motor::isRamping() const {
    return ramping_;
}
```

Replace `rampTo()` with:

```cpp
// Starts a new ramp from wherever the motor is now, so calling stop() in the
// middle of a start (or vice versa) does not cause a speed jump.
void Motor::rampTo(float target, uint64_t ramp_us) {
    uint32_t irq_state = save_and_disable_interrupts();
    ramp_from_ = speed_;
    ramp_to_ = target;
    ramp_start_us_ = time_us_64();
    ramp_len_us_ = ramp_us;
    ramping_ = true;
    restore_interrupts(irq_state);
}
```

In `update()`, replace the two uses of `ramp_us_` with `ramp_len_us_`:

```cpp
    if (ramp_len_us_ == 0 || elapsed >= ramp_len_us_) {
        speed = ramp_to_;
        ramping_ = false;
    } else {
        float t = (float)elapsed / (float)ramp_len_us_;
        speed = ramp_from_ + (ramp_to_ - ramp_from_) * sCurve(t);
    }
```

- [ ] **Step 3: Build and run the tests**

Run: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build` → Expected: builds without errors.
Run: `Code/tests/run_tests.sh` → Expected: `All tests passed`.
Run: `grep -n "ramp_us_" Code/motor.cpp` → Expected: only in the constructor, `start()`, `stop()` and `setMaxSpeed()`.

- [ ] **Step 4: Commit**

```bash
git add Code/motor.h Code/motor.cpp
git commit -m "feat(firmware): Motor::setSpeed with its own ramp time, isRamping" -- Code/motor.h Code/motor.cpp
```

---

### Task 6: Generic `NumberEditTab`, replacing `CalLengthTab`

**Files:**
- Create: `Code/number_edit_tab.h`, `Code/number_edit_tab.cpp`
- Modify: `Code/calibration_menu.h`, `Code/calibration_menu.cpp`, `Code/CMakeLists.txt`

**Interfaces:**
- Consumes: `KnobAccel(int32_t max_factor)` and `clampAdd()` (Task 3).
- Produces: `NumberEditTab(Max7219 &display, int32_t &value, int32_t min_value, int32_t max_value, int32_t max_factor, const char *label = nullptr)`.
  - With no label it shows `printNumber(value)`, which is exactly what `CalLengthTab` showed.
  - With a label (≤ 2 chars) it shows the label left and the value right, in a 6-digit field.

This task has no host test, because the tab draws on the SDK `Max7219`. The edit logic it uses (`KnobAccel`, `clampAdd`) is already tested. Verification is the firmware build plus a hardware check that calibration page 1 behaves as before.

- [ ] **Step 1: Create the tab**

Create `Code/number_edit_tab.h`:

```cpp
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
```

Create `Code/number_edit_tab.cpp`:

```cpp
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
```

- [ ] **Step 2: Use it for calibration page 1**

In `Code/calibration_menu.h`:
- Replace `#include "knob_accel.h"` with `#include "number_edit_tab.h"`.
- Delete the whole `CalLengthTab` class, including its comment `// Page 1: length of the test wire in mm, speed-dependent knob step.`
- In `CalibrationMenu`, change the member `CalLengthTab length_;` to:

```cpp
    NumberEditTab length_;  // page 1: length of the test wire in mm
```

In `Code/calibration_menu.cpp`, delete the whole `// --- Page 1: length ---` section (`CalLengthTab::onEnter`, `onTurn`, `update`, `draw`), and change the constructor's initializer list to:

```cpp
CalibrationMenu::CalibrationMenu(CalibrationContext &ctx)
    : ctx_(ctx),
      length_(ctx.display, ctx.length_mm, kMinCalLengthMm, kMaxCalLengthMm, 100),
      mode_(ctx), attach_(ctx), move_(ctx), result_(ctx) {
```

(The `addTab(...)` calls are unchanged. `adjustLength()` stays in `calibration.h`; it is still covered by its tests.)

In `Code/CMakeLists.txt`, add `number_edit_tab.cpp` to the end of the `add_executable(Code ...)` source list:

```cmake
add_executable(Code Code.cpp max7219.cpp rotary_encoder.cpp motor.cpp
        click_detector.cpp menu.cpp menu_manager.cpp knob_accel.cpp settings.cpp calibration.cpp settings_store.cpp calibration_menu.cpp
        number_edit_tab.cpp)
```

- [ ] **Step 3: Build and run the tests**

Run: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build` → Expected: builds without errors or warnings about `CalLengthTab`.
Run: `Code/tests/run_tests.sh` → Expected: `All tests passed`.
Run: `grep -rn CalLengthTab Code --include=*.h --include=*.cpp` → Expected: no output.

- [ ] **Step 4: Commit**

```bash
git add Code/number_edit_tab.h Code/number_edit_tab.cpp Code/calibration_menu.h Code/calibration_menu.cpp Code/CMakeLists.txt
git commit -m "refactor(firmware): generic NumberEditTab replaces CalLengthTab" -- Code/number_edit_tab.h Code/number_edit_tab.cpp Code/calibration_menu.h Code/calibration_menu.cpp Code/CMakeLists.txt
```

---

### Task 7: The Working menu pages and wiring

**Files:**
- Create: `Code/working_menu.h`, `Code/working_menu.cpp`
- Modify: `Code/Code.cpp`, `Code/CMakeLists.txt`

**Interfaces:**
- Consumes:
  - `NumberEditTab` (Task 6); `run_logic.h` (Task 4); `Motor::setSpeed()` / `isRamping()` (Task 5); `MenuTab::onLongClick()` (Task 2)
  - `ticksToMm()` and `fitsDisplay()` from `calibration.h`
  - `Motor::start/stop/setMaxSpeed/getSpeed/isRunning`, `Motor::kDefaultRampMs`
  - `RotaryEncoder::getCount/reset`, `Max7219::printNumber/printText/clear`
- Produces: `struct WorkingContext { Max7219 &display; Motor &motor; RotaryEncoder &measure; const Settings &settings; int32_t target_mm; int32_t speed_percent; }` and `class WorkingMenu final : public Menu` with `explicit WorkingMenu(WorkingContext &ctx)`.

Page 4 is a small state machine. The *driving* states are `Running`, `SlowingDown` and `Creeping`. In every driving state, each pass checks these in order:
1. `shouldStop()` → stop, `Stopped`. The stop is **instant** (`setSpeed(0, 0)`) from `SlowingDown`/`Creeping`, and a normal ramped `stop()` from `Running`, where it only happens after an overshoot.
2. Stall → `stop()`, `Stalled`.
3. Otherwise, the state's own step from the table.

| State | Own step each pass | Long click |
|---|---|---|
| `Running` (motor at the max speed) | `shouldSlowDown()` → `creep_.start(estimateCreepPower(motor power, measured speed))`, `setSpeed(that power, kDefaultRampMs)`, `SlowingDown` | `stop()`, `Paused` |
| `SlowingDown` (ramping to the creep power) | ramp finished (`!isRamping()`) → `creep_.start(creep_.power())`, `Creeping` | `stop()`, `Paused` |
| `Creeping` (holding `kCreepSpeedMmPerS`) | `creep_.update(measured speed)`; if the power changed, `setSpeed(power, kCreepRampMs)` | `stop()`, `Paused` |
| `Paused` (blinks) | — | resume |
| `Stalled` (blinks) | — | resume |
| `Stopped` (target − 1 mm reached) | — | resume (does nothing: target reached) |

"Resume" works like this:
- If `shouldStop()`, go to `Stopped`.
- Otherwise reset `SpeedMeter` and `StallDetector`. Then:
  - if `shouldSlowDown(pulled, target, 0.0, …)` (no more than `kCreepMinMm` left), go straight to `Creeping` with the remembered `creep_.power()`;
  - else call `motor.start()` and go to `Running`.

For tuning, the creep phase prints the measured speed and power over USB serial (`printf`, stdio is USB CDC), and so does the stop.

- [ ] **Step 1: Create the header**

Create `Code/working_menu.h`:

```cpp
#pragma once

#include <stdint.h>

#include "menu.h"
#include "number_edit_tab.h"
#include "run_logic.h"
#include "settings.h"

class Max7219;
class Motor;
class RotaryEncoder;

// Hardware and state shared by the working pages. target_mm and
// speed_percent live in RAM: kept between runs, reset at power-up.
struct WorkingContext {
    Max7219 &display;
    Motor &motor;
    RotaryEncoder &measure;    // measuring-roll encoder
    const Settings &settings;  // mm_per_tick from the last calibration

    int32_t target_mm = kDefaultTargetMm;
    int32_t speed_percent = kDefaultSpeedPercent;
};

// Page 3: counting starts at 0, the motor stays off; the user threads the wire.
class RunThreadTab final : public MenuTab {
public:
    explicit RunThreadTab(WorkingContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;

private:
    WorkingContext &ctx_;
    int64_t shown_mm_ = -1;  // last value drawn, to redraw only on change
};

// Page 4: the motor pulls the wire, slows down to a creep speed near the
// target and stops at target - kStopBeforeMm. Long click pauses and resumes;
// the page only advances on a short click.
class RunPullTab final : public MenuTab {
public:
    explicit RunPullTab(WorkingContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;
    TabAction onLongClick() override;
    void onExit() override;

private:
    enum class State : uint8_t {
        Running,      // motor at the max speed
        SlowingDown,  // ramping down to the first guess of the creep power
        Creeping,     // holding the creep speed from the measured wire speed
        Paused,       // stopped by a long click (display blinks)
        Stalled,      // stopped: no wire movement for 10 s (display blinks)
        Stopped,      // target - kStopBeforeMm reached
    };

    bool isDriving() const;  // Running, SlowingDown or Creeping
    float maxPower() const;  // the user's max speed as motor power
    // The driving state's own step: start slowing down, finish slowing
    // down, or correct the creep power
    void steer(double speed_mm_per_s, int64_t mm, uint32_t now_ms);
    void creepAt(float power);
    // Starts the motor (or creeps, if close) unless the target is reached.
    void resume(int32_t count, int64_t mm, uint32_t now_ms);
    void draw(int64_t mm, uint32_t now_ms);

    WorkingContext &ctx_;
    State state_ = State::Stopped;
    bool start_pending_ = false;  // set by onEnter(); the start needs update()'s time
    SpeedMeter speed_;
    CreepController creep_;       // keeps its power between runs
    StallDetector stall_;
    float driven_power_ = 0.0f;   // last power given to the motor while creeping
    uint32_t now_ms_ = 0;         // time of the latest pass, for long clicks
    int64_t shown_mm_ = -1;
    bool shown_blank_ = false;
};

// Page 5: the final length. It follows the wire until the motor has fully
// stopped (the braking wire is real wire), then freezes: counting ends.
class RunDoneTab final : public MenuTab {
public:
    explicit RunDoneTab(WorkingContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;

private:
    WorkingContext &ctx_;
    int64_t shown_mm_ = -1;
    bool frozen_ = false;
};

// The working menu: length, speed, thread, pull, done.
class WorkingMenu final : public Menu {
public:
    explicit WorkingMenu(WorkingContext &ctx);

protected:
    void onClose() override;

private:
    WorkingContext &ctx_;
    NumberEditTab length_;  // page 1: target length in mm
    NumberEditTab speed_;   // page 2: max motor speed in %
    RunThreadTab thread_;
    RunPullTab pull_;
    RunDoneTab done_;
};
```

- [ ] **Step 2: Create the implementation**

Create `Code/working_menu.cpp`:

```cpp
#include "working_menu.h"

#include <stdio.h>

#include "calibration.h"
#include "max7219.h"
#include "motor.h"
#include "rotary_encoder.h"

namespace {

constexpr uint32_t kBlinkHalfPeriodMs = 500;
constexpr uint32_t kCreepRampMs = 100;  // smooths each creep power change

int64_t pulledMm(const WorkingContext &ctx, int32_t count) {
    return ticksToMm(count, ctx.settings.mm_per_tick);
}

void showPulledMm(Max7219 &display, int64_t mm) {
    if (fitsDisplay(mm)) {
        display.printNumber((int32_t)mm);
    } else {
        display.printText("--------");
    }
}

}  // namespace

// --- Page 3: thread the wire ---

void RunThreadTab::onEnter() {
    ctx_.measure.reset();  // counting starts now; the motor stays off
    shown_mm_ = -1;
}

TabAction RunThreadTab::update(uint32_t) {
    int64_t mm = pulledMm(ctx_, ctx_.measure.getCount());
    if (mm != shown_mm_) {
        shown_mm_ = mm;
        showPulledMm(ctx_.display, mm);
    }
    return TabAction::Stay;
}

// --- Page 4: pull ---

void RunPullTab::onEnter() {
    ctx_.motor.setMaxSpeed((uint8_t)ctx_.speed_percent);
    state_ = State::Stopped;
    start_pending_ = true;
    shown_mm_ = -1;
    shown_blank_ = false;
}

TabAction RunPullTab::update(uint32_t now_ms) {
    int32_t count = ctx_.measure.getCount();
    int64_t mm = pulledMm(ctx_, count);

    if (start_pending_) {
        start_pending_ = false;
        resume(count, mm, now_ms);
    }

    if (isDriving()) {
        double speed = speed_.update(mm, now_ms);
        if (shouldStop(mm, ctx_.target_mm)) {
            if (state_ == State::Running) {
                ctx_.motor.stop();  // overshot at full speed: brake gently
            } else {
                ctx_.motor.setSpeed(0.0f, 0);  // creeping: stop at once, the wire coasts ~1 mm
            }
            printf("stop: %lld mm pulled, target %ld mm\n", (long long)mm, (long)ctx_.target_mm);
            state_ = State::Stopped;
        } else if (stall_.update(count, now_ms)) {
            ctx_.motor.stop();
            state_ = State::Stalled;
        } else {
            steer(speed, mm, now_ms);
        }
    }

    draw(mm, now_ms);
    now_ms_ = now_ms;
    return TabAction::Stay;
}

void RunPullTab::steer(double speed_mm_per_s, int64_t mm, uint32_t now_ms) {
    switch (state_) {
        case State::Running:
            // Motor::kDefaultRampMs: Code.cpp builds the motor with the default ramp
            if (shouldSlowDown(mm, ctx_.target_mm, speed_mm_per_s, Motor::kDefaultRampMs)) {
                creep_.start(estimateCreepPower(ctx_.motor.getSpeed(), speed_mm_per_s), maxPower(), now_ms);
                driven_power_ = creep_.power();
                ctx_.motor.setSpeed(driven_power_, Motor::kDefaultRampMs);
                state_ = State::SlowingDown;
            }
            break;
        case State::SlowingDown:
            if (!ctx_.motor.isRamping()) {
                creep_.start(creep_.power(), maxPower(), now_ms);  // first adjustment one window later
                state_ = State::Creeping;
            }
            break;
        case State::Creeping: {
            float power = creep_.update(speed_mm_per_s, now_ms);
            if (power != driven_power_) {
                creepAt(power);
                // For tuning kCreep* over USB serial
                printf("creep: %.1f mm/s, power %.3f\n", speed_mm_per_s, (double)power);
            }
            break;
        }
        case State::Paused:
        case State::Stalled:
        case State::Stopped:
            break;
    }
}

TabAction RunPullTab::onLongClick() {
    if (isDriving()) {
        ctx_.motor.stop();
        state_ = State::Paused;
    } else {
        // now_ms_ is from the previous pass (<= one loop period old)
        int32_t count = ctx_.measure.getCount();
        resume(count, pulledMm(ctx_, count), now_ms_);
    }
    return TabAction::Stay;
}

void RunPullTab::onExit() {
    // Page 5 needs a stopped motor. Only the driving states still drive it:
    // the others already stopped it, and a second stop() would restart the ramp.
    if (isDriving()) ctx_.motor.stop();
    start_pending_ = false;
}

bool RunPullTab::isDriving() const {
    return state_ == State::Running || state_ == State::SlowingDown || state_ == State::Creeping;
}

float RunPullTab::maxPower() const {
    return ctx_.speed_percent / 100.0f;
}

void RunPullTab::creepAt(float power) {
    driven_power_ = power;
    ctx_.motor.setSpeed(power, kCreepRampMs);
}

void RunPullTab::resume(int32_t count, int64_t mm, uint32_t now_ms) {
    if (shouldStop(mm, ctx_.target_mm)) {  // nothing left to pull
        state_ = State::Stopped;
        return;
    }
    speed_.reset(mm, now_ms);
    stall_.reset(count, now_ms);
    if (shouldSlowDown(mm, ctx_.target_mm, 0.0, Motor::kDefaultRampMs)) {
        // Too close to speed up first: creep straight away, from the last
        // creep power (the best guess this machine has)
        creep_.start(creep_.power(), maxPower(), now_ms);
        creepAt(creep_.power());
        state_ = State::Creeping;
    } else {
        ctx_.motor.start();
        state_ = State::Running;
    }
}

void RunPullTab::draw(int64_t mm, uint32_t now_ms) {
    // Paused or stalled: blink, so it is clear the machine waits for a long click
    bool blinking = state_ == State::Paused || state_ == State::Stalled;
    bool blank = blinking && (now_ms / kBlinkHalfPeriodMs) % 2 == 1;
    if (blank == shown_blank_ && mm == shown_mm_) return;
    shown_blank_ = blank;
    shown_mm_ = mm;
    if (blank) {
        ctx_.display.clear();
    } else {
        showPulledMm(ctx_.display, mm);
    }
}

// --- Page 5: done ---

void RunDoneTab::onEnter() {
    // The motor was stopped by RunPullTab::onExit() and may still be braking
    shown_mm_ = -1;
    frozen_ = false;
}

TabAction RunDoneTab::update(uint32_t) {
    if (frozen_) return TabAction::Stay;
    int64_t mm = pulledMm(ctx_, ctx_.measure.getCount());
    if (mm != shown_mm_) {
        shown_mm_ = mm;
        showPulledMm(ctx_.display, mm);
    }
    if (!ctx_.motor.isRunning()) frozen_ = true;  // counting ends here
    return TabAction::Stay;
}

// --- Menu ---

WorkingMenu::WorkingMenu(WorkingContext &ctx)
    : ctx_(ctx),
      length_(ctx.display, ctx.target_mm, kMinTargetMm, kMaxTargetMm, 1000),
      speed_(ctx.display, ctx.speed_percent, kMinSpeedPercent, kMaxSpeedPercent, 10, "SP"),
      thread_(ctx), pull_(ctx), done_(ctx) {
    addTab(length_);
    addTab(speed_);
    addTab(thread_);
    addTab(pull_);
    addTab(done_);
}

void WorkingMenu::onClose() {
    // Safety net, whichever way the menu closes; only if actually needed, so
    // this does not start a pointless 0->0 ramp on an already-stopped motor.
    if (ctx_.motor.isRunning()) ctx_.motor.stop();
    // Calibration runs the motor with start() too: give it back full speed.
    // Not running (stop() above cleared that), so this only stores the value.
    ctx_.motor.setMaxSpeed(kMaxSpeedPercent);
}
```

- [ ] **Step 3: Wire it into the firmware**

In `Code/CMakeLists.txt`, extend the source list to:

```cmake
add_executable(Code Code.cpp max7219.cpp rotary_encoder.cpp motor.cpp
        click_detector.cpp menu.cpp menu_manager.cpp knob_accel.cpp settings.cpp calibration.cpp settings_store.cpp calibration_menu.cpp
        number_edit_tab.cpp run_logic.cpp working_menu.cpp)
```

In `Code/Code.cpp`:
- Add `#include "working_menu.h"` after `#include "calibration_menu.h"`.
- In the anonymous namespace, delete the `LabelTab` class with its `// PLACEHOLDER tab ...` comment, the two lines `LabelTab workTab1("run 1");` and `LabelTab workTab2("run 2");`, and the line `Menu workingMenu;`.
- Directly after `Settings settings;  // loaded from flash in main(); the calibration menu updates it`, add:

```cpp
WorkingContext working{display, motor, measureEncoder, settings};
WorkingMenu workingMenu(working);
```

- In `main()`, delete the two lines `workingMenu.addTab(workTab1);` and `workingMenu.addTab(workTab2);`.

- [ ] **Step 4: Build and run the tests**

Run: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build` → Expected: builds without errors.
Run: `Code/tests/run_tests.sh` → Expected: `All tests passed`.
Run: `grep -n "LabelTab\|workTab" Code/Code.cpp` → Expected: no output.

- [ ] **Step 5: Hardware check** (flash with `picotool load Code/build/Code.elf -fx`. If the board is not on USB: BOOTSEL + reflash. Keep a serial monitor open on the USB port to see the `creep:` / `stop:` lines.)

Walk through the checklist and write down every mismatch:
1. User-knob click: the display shows `1000`. Turn slowly (±1), faster (±10 / ±100), and very fast (±1000). The value stays within 1 … 99999999.
2. Click: `SP   100`. Turning moves it by 1 or 10, and it stays within 1 … 100. Set 50.
3. Click: `0`. The motor is off. Pull some wire by hand: the number grows.
4. Click: the motor starts at about half speed, and the number grows.
   - Long click (hold > 2 s, release): the motor brakes and the number blinks. Pull the wire by hand: it still counts.
   - Long click again: the motor resumes.
   - Hold the wire so the roll does not turn: after 10 s the motor stops and the number blinks. A long click resumes.
   - Let it run. It should slow down before the target, creep, and stop at once at target − 1 mm.
     - The serial log shows the `creep:` speed settling near `kCreepSpeedMmPerS`.
     - Write down the final number on the display and the `stop:` line, for tuning the `kCreep*` constants.
5. Click: the number freezes once the motor has stopped. Click: idle screen (all segments lit).
6. Open the working menu again: page 1 shows the last target, page 2 shows `SP    50`.
7. Set a target of 10 mm and run it: there is no fast phase; the motor creeps straight away and stops at 9 mm.
8. Measuring-encoder click: calibration page 1 still behaves as before, and its motor starts and stops with the same smooth 2.5 s ramps at full speed.
9. Mid-run, while the motor is running, short-click on page 4: the motor brakes, page 5 follows the braking wire and then freezes.

**Tuning order** (write the values found into `run_logic.h`, then run `Code/tests/run_tests.sh` again):
1. `kCreepSpeedMmPerS`: the slowest speed at which the motor still turns smoothly, from the `creep:` lines.
2. `kCreepGainPerMmS`: raise it if the speed settles too slowly, lower it if the speed swings.
3. `kCreepMinMm`: make it long enough that the speed has settled before the stop.
4. `kCreepMinPower` and the initial `CreepController::power_`: the power at which the motor just keeps turning.

- [ ] **Step 6: Commit**

```bash
git add Code/working_menu.h Code/working_menu.cpp Code/Code.cpp Code/CMakeLists.txt
git commit -m "feat(firmware): real working menu with pause, stall stop and creep landing" -- Code/working_menu.h Code/working_menu.cpp Code/Code.cpp Code/CMakeLists.txt
```

---

### Task 8: Documentation

**Files:**
- Modify: `Code/CLAUDE.md`
- Create: `Instructions/working.md`

**Interfaces:** none (docs only).

- [ ] **Step 1: Update `Code/CLAUDE.md`**

Make these edits:
- In the `Code.cpp` bullet, replace `The Working menu's `LabelTab`s are still placeholders.` with `It builds the Working menu from a `WorkingContext` and the Calibration menu from a `CalibrationContext`.`
- Replace the `click_detector.{h,cpp}` bullet with:
  `` - `click_detector.{h,cpp}` — `ClickDetector`: on release reports `ClickEvent::Short` (held ≤ 2 s, `kShortClickMaxMs`) or `ClickEvent::Long` (held longer); nothing while held. User-knob long clicks reach the shown tab via `MenuInput::user_long_click` → `MenuTab::onLongClick()` (default: ignored) and never open a menu. ``
- At the end of the `motor.{h,cpp}` bullet, add: `` `setSpeed(speed, ramp_ms)` drives an exact power (not limited by the max speed) with its own ramp time, 0 = at once; `isRamping()` tells when a ramp has finished. ``
- In the `KnobAccel` mention in the Calibration bullet, change `is 1/10/100 mm depending on turning speed` to `is 1/10/100 mm depending on turning speed (`KnobAccel(max_factor)` caps the step; the working menu's length uses 1000)`.
- Add a new bullet after the Calibration bullet:

```markdown
- Working menu (`run_logic.{h,cpp}` = pure maths, host-tested; `working_menu.{h,cpp}` = pages; `number_edit_tab.{h,cpp}` = generic knob-edited number page, also calibration page 1): target length in mm (1 … 99 999 999, step up to ×1000) → max speed `SP` 1 … 100 % → thread (counting from 0, motor off) → pull → done. Target and speed are kept in RAM only (defaults after power-up), never written to flash. Pull page: long click pauses/resumes (counting continues); no movement of ≥ 4 ticks for 10 s while driving stops the motor (`StallDetector`); paused/stalled blinks. Landing: once `pulled + (speed + kCreepSpeedMmPerS) / 2 × ramp + kCreepMinMm ≥ target` (`SpeedMeter` over 100 ms, `Motor::kDefaultRampMs`), the motor ramps to a first-guess creep power (power × creep / speed); `CreepController` then corrects the power every 100 ms from the measured speed, and at `target − kStopBeforeMm` (1 mm) the motor stops at once via `Motor::setSpeed(0, 0)`. Within `kCreepMinMm` of the target it creeps from the start. The `kCreep*` constants in `run_logic.h` are first guesses (marked TUNE); the creep phase prints `creep:` / `stop:` lines over USB serial for tuning. Done page follows the wire until the motor has stopped, then freezes. `WorkingMenu::onClose()` resets the motor max speed to 100 % for calibration.
```

- [ ] **Step 2: Write the user instructions**

Create `Instructions/working.md`:

```markdown
# Winding a hank of wire

Calibrate the machine first (see `calibration.md`), and again whenever you change to a different wire.

## Run

1. **Start:** click the **user knob**.
2. **Length:** turn the knob until the display shows the length you want, in mm.
   - Turn slowly for 1 mm steps, and faster for 10, 100 or 1000 mm steps.
   - Click.
3. **Speed:** the display shows `SP` and the maximum motor speed in percent (100 = full speed). Turn to change it, then click.
4. **Thread the wire:** the display shows `0`, and the motor stays off.
   1. Put the wire through the measuring roll and tie it to the motor. The display already counts the wire that goes through.
   2. Click. The motor starts.
5. **Pulling:** the display shows how much wire has passed.
   - **Pause:** hold the knob for more than 2 s, then release. The motor stops and the number blinks. The wire is still counted if you move it by hand.
   - **Continue:** hold for more than 2 s and release again.
   - Shortly before the target, the motor slows down to a crawl by itself and stops right at the target.
   - If the wire does not move for 10 s while the motor runs (for example, it is stuck), the motor stops and the number blinks. Fix the problem, then hold and release to continue.
   - Click when you are done. You can do this at any time; it also stops the motor.
6. **Result:** the display shows the final length once the motor has stopped. Click to finish.

The next run starts with the same length and speed, until the machine is switched off.
```

- [ ] **Step 3: Commit**

```bash
git add Code/CLAUDE.md Instructions/working.md
git commit -m "docs: document the working menu" -- Code/CLAUDE.md Instructions/working.md
```
