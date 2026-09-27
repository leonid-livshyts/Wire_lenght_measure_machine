# Calibration Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the placeholder Calibration menu with the real five-page flow. The flow measures how many measuring-roll ticks a known length of wire produces, computes the new mm/tick ratio, and saves it as JSON in flash, so the ratio survives a power cycle.

**Architecture:** The pure logic is hardware-free and host-tested, like the existing menu core:
- a "skip this tab" hook in `Menu`, used to skip page 2.5 in hand mode;
- `KnobAccel`, which turns knob turns into a speed-dependent step;
- the settings JSON codec;
- the calibration maths.

Only two new files touch the SDK. `settings_store.cpp` reads and writes the JSON text in the last 4 KB flash sector. `calibration_menu.cpp` holds the five `MenuTab` pages, which draw on the display and drive the motor and the measuring encoder. `Code/settings.json` holds the defaults and is compiled into the firmware through a header that CMake generates.

**Tech Stack:** C++17, Raspberry Pi Pico SDK 2.3.1 (RP2350B, Ninja), `hardware_flash` + `pico_flash` (`flash_safe_execute`), host tests with the system `g++` and the existing `CHECK` macro.

## Global Constraints

- The pages, in order (from the user's spec):
  1. **Length:** the user sets how much wire (mm) will go through, with the user knob. Clockwise adds, counter-clockwise subtracts.
  2. **Mode:** shows "by motor" first. One knob detent switches to "by hand", and the next one switches back.
  - 2.5. **Attach (motor mode only):** tick counting starts, the motor does **not** run yet, and the user ties the rope end to the motor. In hand mode this page is **not shown**.
  3. **Move:** the rope goes through the machine. In motor mode the motor runs. The display shows the length computed with the **old** ratio. If that number needs more than 8 digits, it is no longer shown (`--------`). Ticks are counted all the time.
  4. **Result:** shows how many ticks were counted. On entering this page, the new mm/tick is computed and saved.
- User decisions (asked 2026-09-27):
  - The settings are stored as **JSON text in the last flash sector**. `Code/settings.json` provides the compiled-in defaults.
  - Page 3 ends with a **user-knob click**, which also stops the motor.
  - The page 1 step depends on **turning speed**: 1 / 10 / 100 mm per detent.
  - The default ratio is **`"mm_per_tick": 1.0`**, so before the first calibration the display shows raw ticks.
- Page 1 starts at **1000 mm**. The range is **1 … 99 999 999 mm** (8 display digits).
- Display texts (the 7-segment font has no `M` and no `X`): mode "by motor" = `Auto`, "by hand" = `HAnd`, attach page = `tIE End`, value too large = `--------`, invalid result = `Err`, flash write failed = `FLSH Err`.
- The mm/tick ratio is `length_mm / |ticks|`. The sign is ignored, because the roll may turn either way depending on how the wire is threaded. With 0 ticks there is no result and nothing is saved.
- The hardware-free files (`menu.*`, `knob_accel.*`, `settings.*`, `calibration.*`) must not include Pico SDK headers. They may use `<stdint.h>`, `<stddef.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>` and `<math.h>` (C++ `<c…>` forms in `.cpp`).
- No heap in the firmware: no `new`, `std::function`, `std::vector` or `std::string`.
- Code style: `snake_case_` members, `camelCase()` methods, `kConstant` constants, 4-space indent, `#pragma once`, and the comment density of the existing files.
- Every new firmware `.cpp` goes into `add_executable(Code ...)`. Every new hardware-free `.cpp` also goes into `SOURCES` in `Code/tests/run_tests.sh`. Do not edit the `DO NOT EDIT` block in `Code/CMakeLists.txt`.
- The working tree has unrelated changes (Case/, PCB/). **Commit only the paths listed in each task** (`git commit -- <paths>`). Never use `git add -A` or `git commit -a`.

## File Structure

All paths are relative to the repository root.

| File | Responsibility |
|---|---|
| `Code/menu.{h,cpp}` (modify) | `MenuTab::isSkipped()`; `Menu` skips tabs that return true |
| `Code/knob_accel.{h,cpp}` (create) | `KnobAccel`: detents → value step, ×1 / ×10 / ×100 by the gap between turns |
| `Code/settings.{h,cpp}` (create) | `Settings` struct, `parseSettingsJson()`, `formatSettingsJson()` |
| `Code/settings.json` (create) | Default settings, compiled into the firmware |
| `Code/calibration.{h,cpp}` (create) | Length limits, `adjustLength()`, `computeMmPerTick()`, `ticksToMm()`, `fitsDisplay()` |
| `Code/settings_store.{h,cpp}` (create) | `loadSettings()` / `saveSettings()`: the last flash sector, with fallback to the compiled-in defaults |
| `Code/calibration_menu.{h,cpp}` (create) | `CalibrationContext`, the five page tabs, `CalibrationMenu` |
| `Code/tests/fake_tab.h` (modify) | `skipped` flag |
| `Code/tests/test_menu.cpp` (modify) | Skip tests |
| `Code/tests/test_knob_accel.cpp`, `test_settings.cpp`, `test_calibration.cpp` (create) | One suite per unit |
| `Code/tests/test_main.cpp`, `Code/tests/run_tests.sh` (modify) | Register the new suites and sources |
| `Code/CMakeLists.txt` (modify) | Sources, the `settings_default.h` generation, flash libraries |
| `Code/Code.cpp` (modify) | Load the settings at boot and register the real Calibration menu |
| `Code/CLAUDE.md` (modify) | Document the settings, the calibration flow, and the skip hook |

Commands used throughout (run from the repo root):
- Host tests: `Code/tests/run_tests.sh`
- Firmware build: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build`

---

### Task 1: Skippable tabs in `Menu`

**Files:**
- Modify: `Code/menu.h`, `Code/menu.cpp`
- Modify: `Code/tests/fake_tab.h`, `Code/tests/test_menu.cpp`

**Interfaces:**
- Consumes: the existing `MenuTab`, `Menu`, `TabAction`.
- Produces: `virtual bool MenuTab::isSkipped() const` (default `false`). `Menu::open()` enters the first non-skipped tab. `TabAction::Next` goes to the next non-skipped tab, or closes the menu if there is none. Skipping is decided **at the moment of the transition**, so a tab can depend on a choice made on an earlier page.

- [ ] **Step 1: Add the `skipped` flag to `FakeTab`**

In `Code/tests/fake_tab.h`, add a member and an override to `FakeTab`:

```cpp
    bool skipped = false;  // isSkipped() result
```

(next to `Menu *close_on_exit = nullptr;`) and

```cpp
    bool isSkipped() const override { return skipped; }
```

(next to `void onEnter() override`).

- [ ] **Step 2: Write the failing tests**

In `Code/tests/test_menu.cpp`, add these functions inside the anonymous namespace, before its closing `}` (`HookMenu` is already defined at the top of that namespace):

```cpp
void testSkippedTabIsPassedOver() {
    FakeTab a, b, c;
    b.skipped = true;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.addTab(c);
    m.open();
    m.click();
    CHECK(m.currentTab() == 2);
    CHECK(b.enters == 0);
    CHECK(b.exits == 0);
    CHECK(c.enters == 1);
}

void testSkipIsDecidedAtTransition() {
    FakeTab a, b, c;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.addTab(c);
    m.open();
    b.skipped = true;  // e.g. a choice made on tab a
    m.click();
    CHECK(m.currentTab() == 2);
    CHECK(b.enters == 0);
}

void testSkippedLastTabCloses() {
    FakeTab a, b;
    b.skipped = true;
    HookMenu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.click();
    CHECK(!m.isOpen());
    CHECK(a.exits == 1);
    CHECK(b.enters == 0);
    CHECK(m.closes == 1);
}

void testSkippedFirstTab() {
    FakeTab a, b;
    a.skipped = true;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    CHECK(m.currentTab() == 1);
    CHECK(a.enters == 0);
    CHECK(b.enters == 1);
}

void testAllTabsSkippedClosesImmediately() {
    FakeTab a;
    a.skipped = true;
    HookMenu m;
    m.addTab(a);
    m.open();
    CHECK(!m.isOpen());
    CHECK(a.enters == 0);
    CHECK(m.opens == 1);
    CHECK(m.closes == 1);
}
```

Then call them from `runMenuTests()` (append after the existing calls):

```cpp
    testSkippedTabIsPassedOver();
    testSkipIsDecidedAtTransition();
    testSkippedLastTabCloses();
    testSkippedFirstTab();
    testAllTabsSkippedClosesImmediately();
```

- [ ] **Step 3: Run the tests and check that they fail**

Run: `Code/tests/run_tests.sh`
Expected: a compile error, `'bool FakeTab::isSkipped() const' marked 'override', but does not override`.

- [ ] **Step 4: Add the hook to `MenuTab`**

In `Code/menu.h`, inside `class MenuTab`, after `onExit()`:

```cpp
    // Return true to leave this tab out of the sequence. Asked every time
    // the menu moves to it, so it can depend on an earlier tab's choice.
    virtual bool isSkipped() const { return false; }
```

In `class Menu`, `private:` section, after `void finishClose();`:

```cpp
    // Index of the first tab at or after `index` that is not skipped, -1 if none.
    int firstShownFrom(int index) const;
```

- [ ] **Step 5: Implement skipping in `Menu`**

In `Code/menu.cpp`, replace `Menu::open()`:

```cpp
void Menu::open() {
    if (isOpen()) return;
    onOpen();
    int first = firstShownFrom(0);
    if (first < 0) {  // nothing to show: open and close right away
        onClose();
        return;
    }
    current_ = first;
    tabs_[current_]->onEnter();
}
```

Add after `Menu::finishClose()`:

```cpp
int Menu::firstShownFrom(int index) const {
    for (int i = index; i < tab_count_; i++) {
        if (!tabs_[i]->isSkipped()) return i;
    }
    return -1;
}
```

Replace the `case TabAction::Next:` block in `Menu::apply()`:

```cpp
        case TabAction::Next: {
            leaveCurrent();
            // Asked after onExit(), so the leaving tab's choice is already stored
            int next = firstShownFrom(current_ + 1);
            if (close_requested_ || next < 0) {  // past the last shown tab, or closed from onExit()
                finishClose();
                break;
            }
            current_ = next;
            tabs_[current_]->onEnter();
            break;
        }
```

- [ ] **Step 6: Run the tests and check that they pass**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`. The existing re-entrancy tests (`testCloseFromOnExitDuringNext`, `testClickOnLastTabCloses`) must still pass.

- [ ] **Step 7: Commit**

```bash
git add Code/menu.h Code/menu.cpp Code/tests/fake_tab.h Code/tests/test_menu.cpp
git commit -m "feat(firmware): let menu tabs be skipped based on earlier choices

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/menu.h Code/menu.cpp Code/tests/fake_tab.h Code/tests/test_menu.cpp
```

---

### Task 2: `KnobAccel` (speed-dependent step)

**Files:**
- Create: `Code/knob_accel.h`, `Code/knob_accel.cpp`, `Code/tests/test_knob_accel.cpp`
- Modify: `Code/tests/test_main.cpp`, `Code/tests/run_tests.sh`, `Code/CMakeLists.txt`

**Interfaces:**
- Consumes: none.
- Produces: `class KnobAccel { static constexpr uint32_t kFastGapMs = 40; static constexpr uint32_t kMediumGapMs = 100; int32_t scale(int32_t detents, uint32_t now_ms); void reset(); }`. `scale()` returns `detents × factor`. The factor is 100 if the previous turn was ≤ `kFastGapMs` ago, 10 if it was ≤ `kMediumGapMs` ago, and 1 otherwise (and always 1 for the first turn after `reset()`). `detents == 0` returns 0 and does not count as a turn.

- [ ] **Step 1: Write the failing test**

`Code/tests/test_knob_accel.cpp`:

```cpp
#include "check.h"
#include "knob_accel.h"

namespace {

void testFirstTurnIsFine() {
    KnobAccel accel;
    CHECK(accel.scale(1, 1000) == 1);
    CHECK(accel.scale(-1, 5000) == -1);  // long pause: fine again
}

void testMediumAndFastTurns() {
    KnobAccel accel;
    accel.scale(1, 1000);
    CHECK(accel.scale(1, 1000 + KnobAccel::kMediumGapMs) == 10);
    CHECK(accel.scale(-2, 1100 + KnobAccel::kFastGapMs) == -200);
    CHECK(accel.scale(1, 1140 + KnobAccel::kMediumGapMs + 1) == 1);
}

void testZeroIsNotATurn() {
    KnobAccel accel;
    accel.scale(1, 1000);
    CHECK(accel.scale(0, 1010) == 0);
    CHECK(accel.scale(1, 1000 + KnobAccel::kMediumGapMs) == 10);  // gap still from t=1000
}

void testResetForgetsLastTurn() {
    KnobAccel accel;
    accel.scale(1, 1000);
    accel.reset();
    CHECK(accel.scale(1, 1010) == 1);
}

void testTimeWraparound() {
    KnobAccel accel;
    accel.scale(1, 0xFFFFFFF0u);
    CHECK(accel.scale(1, 0x00000010u) == 100);  // 32 ms across the wrap
}

}  // namespace

void runKnobAccelTests() {
    testFirstTurnIsFine();
    testMediumAndFastTurns();
    testZeroIsNotATurn();
    testResetForgetsLastTurn();
    testTimeWraparound();
}
```

In `Code/tests/test_main.cpp`, declare `void runKnobAccelTests();` next to the other declarations and call `runKnobAccelTests();` after `runMenuManagerTests();`.

In `Code/tests/run_tests.sh`, change `SOURCES` to:

```sh
SOURCES="../click_detector.cpp ../menu.cpp ../menu_manager.cpp ../knob_accel.cpp"
```

- [ ] **Step 2: Run the test and check that it fails**

Run: `Code/tests/run_tests.sh`
Expected: a compile error, `knob_accel.h: No such file or directory`.

- [ ] **Step 3: Implement**

`Code/knob_accel.h`:

```cpp
#pragma once

#include <stdint.h>

// Speed-dependent step for editing a number with the user knob: slow turns
// change it by 1 per detent, faster turns by 10 or 100, so both fine
// adjustment and big jumps are quick. Pure logic; the caller passes the time.
class KnobAccel {
public:
    static constexpr uint32_t kFastGapMs = 40;     // turns this close: x100
    static constexpr uint32_t kMediumGapMs = 100;  // turns this close: x10

    // Returns detents scaled by the factor for the gap since the previous
    // turn. detents == 0 returns 0 and is not counted as a turn. now_ms may wrap around.
    int32_t scale(int32_t detents, uint32_t now_ms);

    // Forgets the previous turn, so the next one is x1.
    void reset();

private:
    bool has_last_ = false;
    uint32_t last_ms_ = 0;
};
```

`Code/knob_accel.cpp`:

```cpp
#include "knob_accel.h"

int32_t KnobAccel::scale(int32_t detents, uint32_t now_ms) {
    if (detents == 0) return 0;
    int32_t factor = 1;
    if (has_last_) {
        uint32_t gap = now_ms - last_ms_;  // unsigned: correct across the wrap
        if (gap <= kFastGapMs) {
            factor = 100;
        } else if (gap <= kMediumGapMs) {
            factor = 10;
        }
    }
    has_last_ = true;
    last_ms_ = now_ms;
    return detents * factor;
}

void KnobAccel::reset() {
    has_last_ = false;
}
```

In `Code/CMakeLists.txt`, extend `add_executable`:

```cmake
add_executable(Code Code.cpp max7219.cpp rotary_encoder.cpp motor.cpp
        click_detector.cpp menu.cpp menu_manager.cpp knob_accel.cpp)
```

- [ ] **Step 4: Run the tests and check that they pass**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`.

- [ ] **Step 5: Commit**

```bash
git add Code/knob_accel.h Code/knob_accel.cpp Code/tests/test_knob_accel.cpp
git commit -m "feat(firmware): add KnobAccel for speed-dependent knob steps

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/knob_accel.h Code/knob_accel.cpp Code/tests/test_knob_accel.cpp Code/tests/test_main.cpp Code/tests/run_tests.sh Code/CMakeLists.txt
```

---

### Task 3: Settings JSON codec + `settings.json`

**Files:**
- Create: `Code/settings.h`, `Code/settings.cpp`, `Code/settings.json`, `Code/tests/test_settings.cpp`
- Modify: `Code/tests/test_main.cpp`, `Code/tests/run_tests.sh`, `Code/CMakeLists.txt`

**Interfaces:**
- Consumes: none.
- Produces:
  - `struct Settings { double mm_per_tick = 1.0; };`
  - `bool parseSettingsJson(const char *text, Settings &out);` takes NUL-terminated text. It reads `"mm_per_tick"`, which must be a finite number > 0. It returns false and leaves `out` unchanged if the key is missing or invalid.
  - `int formatSettingsJson(const Settings &settings, char *buf, size_t size);` writes NUL-terminated JSON. It returns the length without the NUL, or -1 if `buf` is too small.
  - `Code/settings.json` is the default-settings file. Task 5 compiles it into the firmware.

- [ ] **Step 1: Create the defaults file**

`Code/settings.json`:

```json
{
    "mm_per_tick": 1.0
}
```

- [ ] **Step 2: Write the failing test**

`Code/tests/test_settings.cpp`:

```cpp
#include <cmath>
#include <cstdio>
#include <cstring>

#include "check.h"
#include "settings.h"

namespace {

void testParsesValue() {
    Settings s;
    CHECK(parseSettingsJson("{\"mm_per_tick\": 0.125}", s));
    CHECK(s.mm_per_tick == 0.125);
}

void testParsesWithWhitespace() {
    Settings s;
    CHECK(parseSettingsJson("{\n    \"mm_per_tick\" :\t2.5\n}\n", s));
    CHECK(s.mm_per_tick == 2.5);
}

void testRejectsBadValues() {
    const char *bad[] = {
        "",
        "{}",
        "{\"mm_per_tick\": }",
        "{\"mm_per_tick\": \"abc\"}",
        "{\"mm_per_tick\" 1.0}",
        "{\"mm_per_tick\": 0}",
        "{\"mm_per_tick\": -1.5}",
        "{\"mm_per_tick\": nan}",
        "{\"mm_per_tick\": inf}",
    };
    for (const char *text : bad) {
        Settings s;
        s.mm_per_tick = 3.0;
        CHECK(!parseSettingsJson(text, s));
        CHECK(s.mm_per_tick == 3.0);  // unchanged on failure
    }
    Settings s;
    CHECK(!parseSettingsJson(nullptr, s));
}

void testFormatRoundTrip() {
    Settings s;
    s.mm_per_tick = 0.0123456789;
    char buf[64];
    int n = formatSettingsJson(s, buf, sizeof(buf));
    CHECK(n > 0);
    CHECK((size_t)n == std::strlen(buf));
    Settings back;
    CHECK(parseSettingsJson(buf, back));
    CHECK(std::fabs(back.mm_per_tick - s.mm_per_tick) < s.mm_per_tick * 1e-8);
}

void testFormatBufferTooSmall() {
    Settings s;
    char buf[8];
    CHECK(formatSettingsJson(s, buf, sizeof(buf)) == -1);
}

void testRepoDefaultsFileIsValid() {
    // run_tests.sh runs from Code/tests
    std::FILE *f = std::fopen("../settings.json", "r");
    CHECK(f != nullptr);
    if (f == nullptr) return;
    char buf[512] = {};
    CHECK(std::fread(buf, 1, sizeof(buf) - 1, f) > 0);
    std::fclose(f);
    Settings s;
    s.mm_per_tick = 99.0;
    CHECK(parseSettingsJson(buf, s));
    CHECK(s.mm_per_tick == 1.0);
}

}  // namespace

void runSettingsTests() {
    testParsesValue();
    testParsesWithWhitespace();
    testRejectsBadValues();
    testFormatRoundTrip();
    testFormatBufferTooSmall();
    testRepoDefaultsFileIsValid();
}
```

In `Code/tests/test_main.cpp`, declare `void runSettingsTests();` and call it after `runKnobAccelTests();`.

In `Code/tests/run_tests.sh`:

```sh
SOURCES="../click_detector.cpp ../menu.cpp ../menu_manager.cpp ../knob_accel.cpp ../settings.cpp"
```

- [ ] **Step 3: Run the test and check that it fails**

Run: `Code/tests/run_tests.sh`
Expected: a compile error, `settings.h: No such file or directory`.

- [ ] **Step 4: Implement**

`Code/settings.h`:

```cpp
#pragma once

#include <stddef.h>

// Values that must survive a power cycle. Stored as JSON text (see
// settings_store.h); the defaults come from settings.json.
struct Settings {
    double mm_per_tick = 1.0;  // wire length per measuring-roll quadrature step
};

// Reads the settings from NUL-terminated JSON text. Only the keys it knows
// are read; returns false and leaves `out` unchanged if "mm_per_tick" is
// missing or not a finite number > 0.
bool parseSettingsJson(const char *text, Settings &out);

// Writes the settings as NUL-terminated JSON. Returns the length (without
// the NUL), or -1 if `size` is too small.
int formatSettingsJson(const Settings &settings, char *buf, size_t size);
```

`Code/settings.cpp`:

```cpp
#include "settings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr char kMmPerTickKey[] = "\"mm_per_tick\"";

}  // namespace

// A minimal reader for our own flat file, not a general JSON parser.
bool parseSettingsJson(const char *text, Settings &out) {
    if (text == nullptr) return false;
    const char *key = std::strstr(text, kMmPerTickKey);
    if (key == nullptr) return false;

    const char *p = key + sizeof(kMmPerTickKey) - 1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != ':') return false;
    p++;

    char *end = nullptr;
    double value = std::strtod(p, &end);  // skips leading whitespace itself
    if (end == p || !std::isfinite(value) || value <= 0.0) return false;

    out.mm_per_tick = value;
    return true;
}

int formatSettingsJson(const Settings &settings, char *buf, size_t size) {
    // 9 significant digits keep the ratio accurate to far below one tick per km
    int n = std::snprintf(buf, size, "{\"mm_per_tick\": %.9g}\n", settings.mm_per_tick);
    if (n < 0 || (size_t)n >= size) return -1;
    return n;
}
```

In `Code/CMakeLists.txt`, append `settings.cpp` to `add_executable(Code ...)` (after `knob_accel.cpp`).

- [ ] **Step 5: Run the tests and check that they pass**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`.

- [ ] **Step 6: Commit**

```bash
git add Code/settings.h Code/settings.cpp Code/settings.json Code/tests/test_settings.cpp
git commit -m "feat(firmware): add settings JSON codec and default settings.json

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/settings.h Code/settings.cpp Code/settings.json Code/tests/test_settings.cpp Code/tests/test_main.cpp Code/tests/run_tests.sh Code/CMakeLists.txt
```

---

### Task 4: Calibration maths

**Files:**
- Create: `Code/calibration.h`, `Code/calibration.cpp`, `Code/tests/test_calibration.cpp`
- Modify: `Code/tests/test_main.cpp`, `Code/tests/run_tests.sh`, `Code/CMakeLists.txt`

**Interfaces:**
- Consumes: none.
- Produces:
  - `constexpr int32_t kMinCalLengthMm = 1; kMaxCalLengthMm = 99999999; kDefaultCalLengthMm = 1000; constexpr int64_t kMaxDisplayValue = 99999999;`
  - `int32_t adjustLength(int32_t length_mm, int32_t delta_mm);` adds the delta and clamps the result to `[kMinCalLengthMm, kMaxCalLengthMm]`.
  - `bool computeMmPerTick(int32_t length_mm, int32_t ticks, double &mm_per_tick);` sets `length_mm / |ticks|`. It returns false and leaves the output unchanged if `ticks == 0` or `length_mm <= 0`.
  - `int64_t ticksToMm(int32_t ticks, double mm_per_tick);` returns `round(|ticks| × mm_per_tick)`, capped at `kMaxDisplayValue + 1`.
  - `bool fitsDisplay(int64_t value);` returns true for `0 ≤ value ≤ kMaxDisplayValue`.

- [ ] **Step 1: Write the failing test**

`Code/tests/test_calibration.cpp`:

```cpp
#include <cmath>
#include <cstdint>

#include "calibration.h"
#include "check.h"

namespace {

void testAdjustLengthClamps() {
    CHECK(adjustLength(1000, 5) == 1005);
    CHECK(adjustLength(1000, -100) == 900);
    CHECK(adjustLength(10, -100) == kMinCalLengthMm);
    CHECK(adjustLength(kMaxCalLengthMm - 1, 100) == kMaxCalLengthMm);
}

void testComputeMmPerTick() {
    double r = 0.0;
    CHECK(computeMmPerTick(1000, 4000, r));
    CHECK(std::fabs(r - 0.25) < 1e-12);
    CHECK(computeMmPerTick(1000, -4000, r));  // roll turned the other way
    CHECK(std::fabs(r - 0.25) < 1e-12);
}

void testComputeRejectsBadInput() {
    double r = 7.0;
    CHECK(!computeMmPerTick(1000, 0, r));
    CHECK(!computeMmPerTick(0, 100, r));
    CHECK(r == 7.0);
}

void testTicksToMm() {
    CHECK(ticksToMm(4000, 0.25) == 1000);
    CHECK(ticksToMm(-4000, 0.25) == 1000);
    CHECK(ticksToMm(3, 0.5) == 2);  // 1.5 rounds away from zero
    CHECK(ticksToMm(0, 1.0) == 0);
    CHECK(ticksToMm(INT32_MAX, 1000.0) == kMaxDisplayValue + 1);  // capped
}

void testFitsDisplay() {
    CHECK(fitsDisplay(0));
    CHECK(fitsDisplay(kMaxDisplayValue));
    CHECK(!fitsDisplay(kMaxDisplayValue + 1));
}

}  // namespace

void runCalibrationTests() {
    testAdjustLengthClamps();
    testComputeMmPerTick();
    testComputeRejectsBadInput();
    testTicksToMm();
    testFitsDisplay();
}
```

In `Code/tests/test_main.cpp`, declare `void runCalibrationTests();` and call it after `runSettingsTests();`.

In `Code/tests/run_tests.sh`:

```sh
SOURCES="../click_detector.cpp ../menu.cpp ../menu_manager.cpp ../knob_accel.cpp ../settings.cpp ../calibration.cpp"
```

- [ ] **Step 2: Run the test and check that it fails**

Run: `Code/tests/run_tests.sh`
Expected: a compile error, `calibration.h: No such file or directory`.

- [ ] **Step 3: Implement**

`Code/calibration.h`:

```cpp
#pragma once

#include <stdint.h>

// Pure maths of the calibration menu (host-tested).

constexpr int32_t kMinCalLengthMm = 1;
constexpr int32_t kMaxCalLengthMm = 99999999;  // 8 display digits
constexpr int32_t kDefaultCalLengthMm = 1000;
constexpr int64_t kMaxDisplayValue = 99999999;

// length_mm + delta_mm, clamped to [kMinCalLengthMm, kMaxCalLengthMm].
int32_t adjustLength(int32_t length_mm, int32_t delta_mm);

// New ratio = length_mm / |ticks|. The sign of ticks is ignored: the roll
// may turn either way depending on how the wire is threaded. Returns false
// (output untouched) if ticks == 0 or length_mm <= 0.
bool computeMmPerTick(int32_t length_mm, int32_t ticks, double &mm_per_tick);

// Wire length for `ticks` in whole mm (rounded, sign ignored). Capped at
// kMaxDisplayValue + 1, so the result never overflows.
int64_t ticksToMm(int32_t ticks, double mm_per_tick);

// True if value fits the 8-digit display.
bool fitsDisplay(int64_t value);
```

`Code/calibration.cpp`:

```cpp
#include "calibration.h"

#include <cmath>

namespace {

int64_t magnitude(int32_t ticks) {
    return ticks < 0 ? -(int64_t)ticks : (int64_t)ticks;  // int64: safe for INT32_MIN
}

}  // namespace

int32_t adjustLength(int32_t length_mm, int32_t delta_mm) {
    int64_t length = (int64_t)length_mm + delta_mm;
    if (length < kMinCalLengthMm) return kMinCalLengthMm;
    if (length > kMaxCalLengthMm) return kMaxCalLengthMm;
    return (int32_t)length;
}

bool computeMmPerTick(int32_t length_mm, int32_t ticks, double &mm_per_tick) {
    if (ticks == 0 || length_mm <= 0) return false;
    mm_per_tick = (double)length_mm / (double)magnitude(ticks);
    return true;
}

int64_t ticksToMm(int32_t ticks, double mm_per_tick) {
    double mm = (double)magnitude(ticks) * mm_per_tick;
    if (mm > (double)kMaxDisplayValue) return kMaxDisplayValue + 1;
    return std::llround(mm);
}

bool fitsDisplay(int64_t value) {
    return value >= 0 && value <= kMaxDisplayValue;
}
```

In `Code/CMakeLists.txt`, append `calibration.cpp` to `add_executable(Code ...)`.

- [ ] **Step 4: Run the tests and check that they pass**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`.

- [ ] **Step 5: Commit**

```bash
git add Code/calibration.h Code/calibration.cpp Code/tests/test_calibration.cpp
git commit -m "feat(firmware): add calibration maths (mm/tick ratio, length limits)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/calibration.h Code/calibration.cpp Code/tests/test_calibration.cpp Code/tests/test_main.cpp Code/tests/run_tests.sh Code/CMakeLists.txt
```

---

### Task 5: Flash settings store

This task touches the SDK, so it has no host test. Verification is a clean firmware build here plus the power-cycle check in Task 6.

**Files:**
- Create: `Code/settings_store.h`, `Code/settings_store.cpp`
- Modify: `Code/CMakeLists.txt`

**Interfaces:**
- Consumes: `Settings`, `parseSettingsJson()` and `formatSettingsJson()` (Task 3); `Code/settings.json`.
- Produces:
  - `Settings loadSettings();` returns the defaults from `settings.json`, overridden by valid JSON stored in flash.
  - `bool saveSettings(const Settings &settings);` writes the JSON to the last flash sector and returns false on failure. It pauses interrupts for roughly 50 ms, so **never call it while the motor is winding to a target**.
  - The generated header `settings_default.h` provides `constexpr char kDefaultSettingsJson[]`.

- [ ] **Step 1: Generate `settings_default.h` from `settings.json` in CMake**

In `Code/CMakeLists.txt`, append `settings_store.cpp` to `add_executable(Code ...)`. Then insert this block right **after** the `add_executable(...)` call:

```cmake
# settings.json is compiled in as the default settings (settings_default.h).
# Editing it re-runs CMake; the header is rewritten only when the text changed.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/settings.json)
file(READ ${CMAKE_CURRENT_LIST_DIR}/settings.json SETTINGS_JSON)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/settings_default.h.tmp
    "#pragma once\n// Generated from settings.json by CMakeLists.txt. Do not edit.\nconstexpr char kDefaultSettingsJson[] = R\"json(${SETTINGS_JSON})json\";\n")
configure_file(${CMAKE_CURRENT_BINARY_DIR}/generated/settings_default.h.tmp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/settings_default.h COPYONLY)
```

Replace `target_link_libraries(Code ...)` with:

```cmake
target_link_libraries(Code
        pico_stdlib
        hardware_spi
        hardware_pwm
        hardware_flash
        pico_flash)
```

Replace `target_include_directories(Code PRIVATE ...)` with:

```cmake
target_include_directories(Code PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)
```

- [ ] **Step 2: Implement the store**

`Code/settings_store.h`:

```cpp
#pragma once

#include "settings.h"

// Settings persisted as JSON text in the last 4 KB flash sector.

// Defaults from settings.json (compiled in), overridden by whatever valid
// JSON is stored in flash. Erased or corrupt flash just gives the defaults.
Settings loadSettings();

// Erases the settings sector and writes `settings` as JSON. Interrupts are
// off for the erase (~50 ms): encoder steps and motor ramp steps are
// missed meanwhile, so do not call it while wire is being measured.
// Returns false if the text did not fit or the flash could not be locked.
bool saveSettings(const Settings &settings);
```

`Code/settings_store.cpp`:

```cpp
#include "settings_store.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"
#include "settings_default.h"  // generated from settings.json

namespace {

// Last sector of the flash chip, far past the end of the program
constexpr uint32_t kSettingsOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
constexpr uint32_t kFlashLockTimeoutMs = 1000;

// Runs with interrupts off (via flash_safe_execute); `param` is one page.
void writeSettingsSector(void *param) {
    flash_range_erase(kSettingsOffset, FLASH_SECTOR_SIZE);
    flash_range_program(kSettingsOffset, static_cast<const uint8_t *>(param), FLASH_PAGE_SIZE);
}

}  // namespace

Settings loadSettings() {
    Settings settings;
    parseSettingsJson(kDefaultSettingsJson, settings);  // host test guarantees it parses

    // Erased flash is all 0xFF, so it has no NUL and is not parsed
    const char *stored = reinterpret_cast<const char *>(XIP_BASE + kSettingsOffset);
    if (memchr(stored, '\0', FLASH_PAGE_SIZE) != nullptr) parseSettingsJson(stored, settings);
    return settings;
}

bool saveSettings(const Settings &settings) {
    static uint8_t page[FLASH_PAGE_SIZE];  // static: not on the stack while interrupts are off
    memset(page, 0, sizeof(page));         // zero padding also terminates the text
    if (formatSettingsJson(settings, reinterpret_cast<char *>(page), sizeof(page)) < 0) return false;
    return flash_safe_execute(writeSettingsSector, page, kFlashLockTimeoutMs) == PICO_OK;
}
```

- [ ] **Step 3: Build the firmware**

Run: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build`
Expected: the build succeeds (CMake re-runs automatically because `CMakeLists.txt` changed). `Code/build/generated/settings_default.h` exists and contains `"mm_per_tick": 1.0`.
Check: `grep mm_per_tick Code/build/generated/settings_default.h`

- [ ] **Step 4: Run the host tests (nothing may regress)**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`.

- [ ] **Step 5: Commit**

```bash
git add Code/settings_store.h Code/settings_store.cpp
git commit -m "feat(firmware): persist settings as JSON in the last flash sector

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/settings_store.h Code/settings_store.cpp Code/CMakeLists.txt
```

---

### Task 6: Calibration pages + wiring into `Code.cpp`

**Files:**
- Create: `Code/calibration_menu.h`, `Code/calibration_menu.cpp`
- Modify: `Code/Code.cpp`, `Code/CMakeLists.txt`

**Interfaces:**
- Consumes: `MenuTab::isSkipped()` (Task 1); `KnobAccel` (Task 2); `Settings` (Task 3); `adjustLength`, `computeMmPerTick`, `ticksToMm`, `fitsDisplay`, `kDefaultCalLengthMm` (Task 4); `loadSettings`, `saveSettings` (Task 5); `Max7219::printNumber/printText`, `Motor::start/stop`, `RotaryEncoder::reset/getCount`.
- Produces: `enum class CalibrationMode`, `struct CalibrationContext`, and `class CalibrationMenu final : public Menu` with `explicit CalibrationMenu(CalibrationContext &ctx)`. The future Working menu reads `settings.mm_per_tick` from the global `Settings settings` in `Code.cpp`.

Page behaviour, which the code below implements:

| # | Tab | Enter | Knob turn | Every pass | Leave |
|---|---|---|---|---|---|
| 1 | `CalLengthTab` | show length | ± `KnobAccel` step, clamped | remember time | — |
| 2 | `CalModeTab` | mode = Motor, show `Auto` | odd detents toggle `Auto`/`HAnd` | — | — |
| 2.5 | `CalAttachTab` (skipped in Hand) | reset ticks, show `tIE End` | — | — | — |
| 3 | `CalMoveTab` | Hand: reset ticks. Motor: `motor.start()` | — | show old-ratio mm, `--------` if > 8 digits | `motor.stop()`, store \|ticks\| |
| 4 | `CalResultTab` | show ticks, compute, save (or `Err` / `FLSH Err`) | — | — | — |

A click on any page goes to the next page. The click on page 4 closes the menu, and the idle screen comes back. `CalibrationMenu::onClose()` always stops the motor.

- [ ] **Step 1: Write the header**

`Code/calibration_menu.h`:

```cpp
#pragma once

#include <stdint.h>

#include "calibration.h"
#include "knob_accel.h"
#include "menu.h"
#include "settings.h"

class Max7219;
class Motor;
class RotaryEncoder;

enum class CalibrationMode : uint8_t {
    Motor,  // the motor pulls the wire (shown as "Auto": no M on 7 segments)
    Hand,   // the user pulls the wire; the attach page is skipped
};

// Hardware and state shared by the calibration pages.
struct CalibrationContext {
    Max7219 &display;
    Motor &motor;
    RotaryEncoder &measure;  // measuring-roll encoder
    Settings &settings;      // live settings; the new ratio is written here
    bool (*save)(const Settings &);

    int32_t length_mm = kDefaultCalLengthMm;  // kept between calibrations
    CalibrationMode mode = CalibrationMode::Motor;
    int32_t ticks = 0;  // |count| when the move page was left
};

// Page 1: length of the test wire in mm, speed-dependent knob step.
class CalLengthTab final : public MenuTab {
public:
    explicit CalLengthTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    void onTurn(int32_t detents) override;
    TabAction update(uint32_t now_ms) override;

private:
    void draw();
    CalibrationContext &ctx_;
    KnobAccel accel_;
    uint32_t now_ms_ = 0;  // time of the latest pass, for KnobAccel
};

// Page 2: by motor ("Auto") or by hand ("HAnd"); each detent toggles.
class CalModeTab final : public MenuTab {
public:
    explicit CalModeTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    void onTurn(int32_t detents) override;

private:
    void draw();
    CalibrationContext &ctx_;
};

// Page 2.5 (motor mode only): counting starts, motor still off, the user
// ties the wire end to the motor.
class CalAttachTab final : public MenuTab {
public:
    explicit CalAttachTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    bool isSkipped() const override;

private:
    CalibrationContext &ctx_;
};

// Page 3: the wire goes through; shows the length by the old ratio.
class CalMoveTab final : public MenuTab {
public:
    explicit CalMoveTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;
    TabAction update(uint32_t now_ms) override;
    void onExit() override;

private:
    void draw();
    CalibrationContext &ctx_;
    int64_t shown_mm_ = -1;  // last value drawn, to redraw only on change
};

// Page 4: shows the tick count; computes and saves the new ratio.
class CalResultTab final : public MenuTab {
public:
    explicit CalResultTab(CalibrationContext &ctx) : ctx_(ctx) {}
    void onEnter() override;

private:
    CalibrationContext &ctx_;
};

// The calibration menu: pages 1, 2, 2.5, 3, 4 in order.
class CalibrationMenu final : public Menu {
public:
    explicit CalibrationMenu(CalibrationContext &ctx);

protected:
    void onClose() override;

private:
    CalibrationContext &ctx_;
    CalLengthTab length_;
    CalModeTab mode_;
    CalAttachTab attach_;
    CalMoveTab move_;
    CalResultTab result_;
};
```

- [ ] **Step 2: Implement the pages**

`Code/calibration_menu.cpp`:

```cpp
#include "calibration_menu.h"

#include "max7219.h"
#include "motor.h"
#include "rotary_encoder.h"

// --- Page 1: length ---

void CalLengthTab::onEnter() {
    accel_.reset();
    draw();
}

void CalLengthTab::onTurn(int32_t detents) {
    // now_ms_ is from the previous pass (<= one loop period old): turns are
    // delivered before update() within a pass
    ctx_.length_mm = adjustLength(ctx_.length_mm, accel_.scale(detents, now_ms_));
    draw();
}

TabAction CalLengthTab::update(uint32_t now_ms) {
    now_ms_ = now_ms;
    return TabAction::Stay;
}

void CalLengthTab::draw() {
    ctx_.display.printNumber(ctx_.length_mm);
}

// --- Page 2: mode ---

void CalModeTab::onEnter() {
    ctx_.mode = CalibrationMode::Motor;  // "by motor" is always offered first
    draw();
}

void CalModeTab::onTurn(int32_t detents) {
    if (detents % 2 != 0) {
        ctx_.mode = ctx_.mode == CalibrationMode::Motor ? CalibrationMode::Hand
                                                        : CalibrationMode::Motor;
    }
    draw();
}

void CalModeTab::draw() {
    ctx_.display.printText(ctx_.mode == CalibrationMode::Motor ? "Auto" : "HAnd");
}

// --- Page 2.5: attach the wire to the motor ---

void CalAttachTab::onEnter() {
    ctx_.measure.reset();  // counting starts now; the motor stays off
    ctx_.display.printText("tIE End");
}

bool CalAttachTab::isSkipped() const {
    return ctx_.mode == CalibrationMode::Hand;
}

// --- Page 3: move the wire ---

void CalMoveTab::onEnter() {
    if (ctx_.mode == CalibrationMode::Hand) {
        ctx_.measure.reset();  // motor mode has counted since the attach page
    } else {
        ctx_.motor.start();
    }
    shown_mm_ = -1;
    draw();
}

TabAction CalMoveTab::update(uint32_t) {
    draw();
    return TabAction::Stay;
}

void CalMoveTab::onExit() {
    ctx_.motor.stop();  // harmless in hand mode
    int32_t count = ctx_.measure.getCount();
    ctx_.ticks = count < 0 ? -count : count;
}

void CalMoveTab::draw() {
    int64_t mm = ticksToMm(ctx_.measure.getCount(), ctx_.settings.mm_per_tick);
    if (mm == shown_mm_) return;
    shown_mm_ = mm;
    if (fitsDisplay(mm)) {
        ctx_.display.printNumber((int32_t)mm);
    } else {
        ctx_.display.printText("--------");  // old ratio too far off to be worth showing
    }
}

// --- Page 4: result ---

void CalResultTab::onEnter() {
    double mm_per_tick = 0.0;
    if (!computeMmPerTick(ctx_.length_mm, ctx_.ticks, mm_per_tick)) {
        ctx_.display.printText("Err");  // no ticks counted: nothing to save
        return;
    }
    ctx_.display.printNumber(ctx_.ticks);  // drawn before the flash write pauses interrupts
    ctx_.settings.mm_per_tick = mm_per_tick;
    if (!ctx_.save(ctx_.settings)) ctx_.display.printText("FLSH Err");
}

// --- Menu ---

CalibrationMenu::CalibrationMenu(CalibrationContext &ctx)
    : ctx_(ctx), length_(ctx), mode_(ctx), attach_(ctx), move_(ctx), result_(ctx) {
    addTab(length_);
    addTab(mode_);
    addTab(attach_);
    addTab(move_);
    addTab(result_);
}

void CalibrationMenu::onClose() {
    ctx_.motor.stop();  // safety net, whichever way the menu closes
}
```

In `Code/CMakeLists.txt`, append `calibration_menu.cpp` to `add_executable(Code ...)`.

- [ ] **Step 3: Wire it into `Code.cpp`**

In `Code/Code.cpp`, add these includes after `#include "menu_manager.h"`:

```cpp
#include "settings_store.h"
#include "calibration_menu.h"
```

In the anonymous namespace:
- delete `LabelTab calTab1("CAL 1");`, `LabelTab calTab2("CAL 2");` and `Menu calibrationMenu;`;
- update the `LabelTab` comment to say that it is the placeholder for the **working** tabs only;
- after `Menu workingMenu;`, add:

```cpp
Settings settings;  // loaded from flash in main(); the calibration menu updates it

CalibrationContext calibration{display, motor, measureEncoder, settings, saveSettings};
CalibrationMenu calibrationMenu(calibration);
```

In `main()`, right after `stdio_init_all();`, add:

```cpp
    settings = loadSettings();  // settings.json defaults, overridden by the last calibration
```

and delete the two lines `calibrationMenu.addTab(calTab1);` and `calibrationMenu.addTab(calTab2);`. `CalibrationMenu` adds its own tabs. The `menus.addMenu(MenuTrigger::MeasureClick, calibrationMenu);` line stays.

- [ ] **Step 4: Build and run the host tests**

Run: `~/.pico-sdk/ninja/v1.13.2/ninja -C Code/build && Code/tests/run_tests.sh`
Expected: the build succeeds with no warnings from the new files, and the tests print `All tests passed`.

- [ ] **Step 5: Hardware check (flash with `picotool load Code/build/Code.elf -fx`)**

If the board does not appear over USB, hold BOOTSEL, reconnect, and reflash.

1. Power-up: all segments are lit. Click the measuring-encoder button: the display shows `1000`.
2. Turn the knob slowly: the value changes by 1. Spin it fast: it jumps by 10 or 100. Counter-clockwise lowers it, down to 1 at the lowest. Set it to the length of a test piece of wire (e.g. `1000`).
3. Click: the display shows `Auto`. Turn one detent: `HAnd`. Turn again: `Auto`.
4. **Hand path:** select `HAnd` and click. The display shows `0` right away (the attach page is skipped). Pull the wire through the roll: the number rises (it shows raw ticks, since the ratio is 1.0). Click: the tick count is shown. Click again: the idle screen comes back.
5. Run calibration again in hand mode with the same wire. Page 3 should now end close to the length set on page 1.
6. **Motor path:** select `Auto` and click. The display shows `tIE End` and the motor stays still. Click: the motor ramps up and the number rises. Click: the motor ramps down and the tick count is shown.
7. Power-cycle the board and run a hand calibration. Page 3 still shows mm, not raw ticks, which proves the ratio was loaded from flash.
8. Page 3 with no wire moved, then click: `Err` is shown and nothing is saved.

- [ ] **Step 6: Commit**

```bash
git add Code/calibration_menu.h Code/calibration_menu.cpp
git commit -m "feat(firmware): add the calibration menu pages and save the mm/tick ratio

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/calibration_menu.h Code/calibration_menu.cpp Code/Code.cpp Code/CMakeLists.txt
```

---

### Task 7: Documentation

**Files:**
- Modify: `Code/CLAUDE.md`

- [ ] **Step 1: Update `Code/CLAUDE.md`**

In the `Code.cpp` bullet, replace "The `LabelTab` menus in it are placeholders." with:
"It loads `Settings settings` from flash at boot. The Working menu's `LabelTab`s are still placeholders."

In the "Menu system" list, add after the `menu.{h,cpp}` item:
"  - A tab can override `isSkipped()` to be left out of the sequence. It is asked at every transition, so it can depend on an earlier page's choice (the calibration attach page uses this)."

Add these bullets after the menu-system list:

```markdown
- Settings: `settings.{h,cpp}` (`Settings`, a minimal flat-JSON parse/format, host-tested). `settings_store.{h,cpp}` keeps the JSON text in the last 4 KB flash sector (`loadSettings()` / `saveSettings()`). The defaults live in `Code/settings.json`, which CMake compiles in as the generated `build/generated/settings_default.h`. A new key needs a field in `Settings`, a line in `parseSettingsJson()` / `formatSettingsJson()`, and an entry in `settings.json`. `saveSettings()` turns interrupts off for about 50 ms (encoder steps are lost meanwhile), so never call it while wire is being measured.
- Calibration (`calibration.{h,cpp}` = pure maths, `calibration_menu.{h,cpp}` = pages): length (the `KnobAccel` step is 1/10/100 mm depending on turning speed) → mode `Auto`/`HAnd` → `tIE End` (motor mode only, counting starts) → move (shows mm by the old ratio, `--------` past 8 digits) → result (shows |ticks|, saves `mm_per_tick = length / |ticks|`). The 7-segment font has no `M`/`X`, which is why the texts look the way they do.
```

- [ ] **Step 2: Commit**

```bash
git add Code/CLAUDE.md
git commit -m "docs(firmware): document settings storage and the calibration menu

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/CLAUDE.md
```
