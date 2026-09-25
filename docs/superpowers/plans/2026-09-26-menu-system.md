# Menu System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a menu framework for the firmware. The two menus it has to support are Working and Calibration. Each menu is a fixed sequence of tabs, a short click on the user encoder advances to the next tab, only one menu can be open at a time, and after power-up the display lights all segments without opening a menu.

**Architecture:** There are three small classes, and none of them depends on the hardware:
- `ClickDetector` turns the debounced button level into "short click" events. A press counts only if it is released within 2 s.
- `Menu` holds an ordered list of `MenuTab` pages and walks through them.
- `MenuManager` binds each menu to the event that opens it. It lets only one menu be active, routes the user-encoder input to the active menu, and shows the idle screen when that menu closes.

`Code.cpp` wires these classes to the real encoders and the display. The real menus are not part of this plan. It registers only two placeholder menus, each with two label tabs, so the flow can be checked on the hardware.

**Tech Stack:** C++17, Raspberry Pi Pico SDK 2.3.1 (RP2350B, Ninja build), host-side unit tests compiled with the system `g++` (no test framework, a 10-line `CHECK` macro).

## Global Constraints

- The menu core (`click_detector.*`, `menu.*`, `menu_manager.*`) must not include any Pico SDK header. Only `<stdint.h>` is allowed, so the same files compile for the host tests and for the firmware.
- No heap in the firmware path: no `new`, `std::function`, `std::vector` or `std::string`. Fixed-size arrays and plain function pointers only.
- A short click is a press released **≤ 2000 ms** after it started (`ClickDetector::kShortClickMaxMs = 2000`). A longer press does nothing.
- In **both** menus, the next tab is selected by a short click on the **user** encoder.
- The Working menu opens on a short click of the user encoder. The Calibration menu opens on a short click of the measuring-roll encoder.
- Only one menu is open at a time. While a menu is open, the event that opens the other menu is **ignored**. A click never closes one menu and opens another.
- After power-up the display shows every segment and dot lit (idle screen), and no menu opens until the matching click. The display returns to this idle screen whenever a menu closes.
- The code style matches the existing firmware: `snake_case_` members, `camelCase()` methods, `kConstant` constants, 4-space indent, `#pragma once`, and the `// comment` density used in `motor.cpp` and `rotary_encoder.cpp`.
- Every new firmware `.cpp` file must be added to `add_executable(Code ...)` in `Code/CMakeLists.txt`.
- Do not edit the `DO NOT EDIT` block in `Code/CMakeLists.txt`.

## File Structure

All paths are relative to the repository root. The firmware keeps its existing flat layout in `Code/`.

| File | Responsibility |
|---|---|
| `Code/click_detector.{h,cpp}` (create) | Short-click detection from the button level and a millisecond timestamp |
| `Code/menu.{h,cpp}` (create) | `MenuTab` interface, `TabAction`, and `Menu`, which walks the tabs of one menu |
| `Code/menu_manager.{h,cpp}` (create) | `MenuInput`, `MenuTrigger`, and `MenuManager`, which opens menus on their trigger, keeps them mutually exclusive, and shows the idle screen |
| `Code/tests/check.h` (create) | `CHECK` macro and failure counter |
| `Code/tests/test_main.cpp` (create) | Runs all test suites and returns non-zero on failure |
| `Code/tests/test_click_detector.cpp`, `test_menu.cpp`, `test_menu_manager.cpp` (create) | One suite per class |
| `Code/tests/fake_tab.h` (create) | `FakeTab`, which records calls for the menu tests |
| `Code/tests/run_tests.sh` (create) | Compiles and runs the host tests |
| `Code/Code.cpp` (modify) | Replaces the motor test loop with the menu manager, the placeholder menus and the idle screen |
| `Code/CMakeLists.txt` (modify) | Adds the new sources |
| `Code/CLAUDE.md` (modify) | Documents the menu system, how to add a menu, and how to run the tests |
| `.gitignore` (modify) | Ignores `Code/tests/build/` |

---

### Task 1: Test harness + `ClickDetector`

**Files:**
- Create: `Code/tests/check.h`, `Code/tests/test_main.cpp`, `Code/tests/run_tests.sh`, `Code/tests/test_click_detector.cpp`
- Create: `Code/click_detector.h`, `Code/click_detector.cpp`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: none.
- Produces:
  - `class ClickDetector { static constexpr uint32_t kShortClickMaxMs = 2000; explicit ClickDetector(uint32_t max_short_ms = kShortClickMaxMs); bool update(bool pressed, uint32_t now_ms); }`
  - `Code/tests/check.h` provides `CHECK(cond)` and `inline int g_failures`.
  - `Code/tests/run_tests.sh` compiles `Code/tests/test_*.cpp` plus the core sources listed in its `SOURCES` variable. Later tasks append to `SOURCES`.

- [ ] **Step 0: Commit the existing firmware baseline (only if `Code/` is still untracked)**

The whole `Code/` directory is untracked at the moment. Commit it first so that the menu work shows up as clean diffs. The pathspec commits only `Code/` and leaves the unrelated staged changes (the Case/PCB files) alone:

```bash
cd /home/leonid/Documents/hardware_projects/wire_counting_machine
git status --short Code | head -3        # "?? Code/" means untracked, so run the next two commands
git add Code
git commit -m "chore(firmware): add existing firmware (display, encoders, motor)" -- Code
```

`Code/build/` and `.vscode` are already listed in `.gitignore`, so they stay out of the commit. Confirm with `git show --stat HEAD | grep -c build/`, which should print `0`.

- [ ] **Step 1: Create the test harness**

`Code/tests/check.h`:

```cpp
#pragma once

#include <cstdio>

// Minimal test helper: counts failures and keeps going, so one run reports
// every broken check.
inline int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)
```

`Code/tests/test_main.cpp`:

```cpp
#include <cstdio>

#include "check.h"

void runClickDetectorTests();

int main() {
    runClickDetectorTests();

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("All tests passed\n");
    return 0;
}
```

`Code/tests/run_tests.sh`:

```sh
#!/bin/sh
# Host-side unit tests for the hardware-independent firmware logic.
set -e
cd "$(dirname "$0")"
mkdir -p build

SOURCES="../click_detector.cpp"

g++ -std=c++17 -Wall -Wextra -Werror -I.. -o build/tests test_*.cpp $SOURCES
./build/tests
```

Then make it executable and ignore the build output:

```bash
chmod +x Code/tests/run_tests.sh
echo '/Code/tests/build/' >> .gitignore
```

- [ ] **Step 2: Write the failing tests**

`Code/tests/test_click_detector.cpp`:

```cpp
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
```

- [ ] **Step 3: Run the tests to verify they fail**

Run: `Code/tests/run_tests.sh`
Expected: the compile fails with `click_detector.h: No such file or directory`.

- [ ] **Step 4: Write the implementation**

`Code/click_detector.h`:

```cpp
#pragma once

#include <stdint.h>

// Turns a debounced button level into "short click" events.
//
// A click is reported once, on release, and only if the button was held for
// at most max_short_ms. Longer presses are ignored, so leaning on the knob
// does not flip menu pages. Pure logic (no SDK calls), so it is unit-tested
// on the host; the caller passes the time in.
class ClickDetector {
public:
    static constexpr uint32_t kShortClickMaxMs = 2000;

    explicit ClickDetector(uint32_t max_short_ms = kShortClickMaxMs);

    // Call regularly with the current button state and time. Returns true
    // exactly once per short click. now_ms may wrap around.
    bool update(bool pressed, uint32_t now_ms);

private:
    uint32_t max_short_ms_;
    bool was_pressed_ = false;
    uint32_t press_start_ms_ = 0;
};
```

`Code/click_detector.cpp`:

```cpp
#include "click_detector.h"

ClickDetector::ClickDetector(uint32_t max_short_ms) : max_short_ms_(max_short_ms) {}

bool ClickDetector::update(bool pressed, uint32_t now_ms) {
    if (pressed && !was_pressed_) press_start_ms_ = now_ms;

    // Unsigned subtraction keeps the duration right across a counter wrap
    bool click = !pressed && was_pressed_ &&
                 (uint32_t)(now_ms - press_start_ms_) <= max_short_ms_;
    was_pressed_ = pressed;
    return click;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`, exit code 0.

- [ ] **Step 6: Commit**

```bash
git add .gitignore Code/click_detector.h Code/click_detector.cpp Code/tests
git commit -m "feat(firmware): add short-click detector and host test harness

Constraint: menu pages must only turn on a short click (<= 2 s)
Confidence: high
Scope-risk: narrow

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- .gitignore Code/click_detector.h Code/click_detector.cpp Code/tests
```

---

### Task 2: `MenuTab` + `Menu`

**Files:**
- Create: `Code/menu.h`, `Code/menu.cpp`
- Create: `Code/tests/fake_tab.h`, `Code/tests/test_menu.cpp`
- Modify: `Code/tests/test_main.cpp`, `Code/tests/run_tests.sh`

**Interfaces:**
- Consumes: the `CHECK` harness from Task 1.
- Produces:
  - `enum class TabAction : uint8_t { Stay, Next, CloseMenu };`
  - `class MenuTab`, with virtual hooks and these defaults: `void onEnter()`, `void onTurn(int32_t detents)`, `TabAction onClick()` (default `Next`), `TabAction update(uint32_t now_ms)` (default `Stay`), `void onExit()`. Its destructor is protected and non-virtual.
  - `class Menu { static constexpr int kMaxTabs = 8; bool addTab(MenuTab &tab); int tabCount() const; void open(); void close(); bool isOpen() const; int currentTab() const /* -1 when closed */; void turn(int32_t detents); void click(); void update(uint32_t now_ms); protected: virtual void onOpen(); virtual void onClose(); }`
  - `Code/tests/fake_tab.h` provides `FakeTab` with public counters `enters`, `exits`, `clicks`, `turned` and the settable fields `click_action` and `update_action`.

- [ ] **Step 1: Write the fake tab and the failing tests**

`Code/tests/fake_tab.h`:

```cpp
#pragma once

#include "menu.h"

// Records every call from the menu and returns configurable actions.
class FakeTab final : public MenuTab {
public:
    int enters = 0;
    int exits = 0;
    int clicks = 0;
    int32_t turned = 0;
    TabAction click_action = TabAction::Next;
    TabAction update_action = TabAction::Stay;

    void onEnter() override { enters++; }
    void onTurn(int32_t detents) override { turned += detents; }
    TabAction onClick() override { clicks++; return click_action; }
    TabAction update(uint32_t) override { return update_action; }
    void onExit() override { exits++; }
};
```

`Code/tests/test_menu.cpp`:

```cpp
#include "check.h"
#include "fake_tab.h"
#include "menu.h"

namespace {

class HookMenu final : public Menu {
public:
    int opens = 0;
    int closes = 0;

protected:
    void onOpen() override { opens++; }
    void onClose() override { closes++; }
};

void testOpenEntersFirstTab() {
    FakeTab a, b;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    CHECK(!m.isOpen());
    CHECK(m.currentTab() == -1);

    m.open();
    CHECK(m.isOpen());
    CHECK(m.currentTab() == 0);
    CHECK(a.enters == 1);
    CHECK(b.enters == 0);

    m.open();  // already open: no restart
    CHECK(a.enters == 1);
}

void testClickNextAdvances() {
    FakeTab a, b;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.click();
    CHECK(a.clicks == 1);
    CHECK(a.exits == 1);
    CHECK(b.enters == 1);
    CHECK(m.currentTab() == 1);
}

void testClickOnLastTabCloses() {
    FakeTab a, b;
    HookMenu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.click();
    m.click();
    CHECK(!m.isOpen());
    CHECK(m.currentTab() == -1);
    CHECK(b.exits == 1);
    CHECK(m.opens == 1);
    CHECK(m.closes == 1);
}

void testStayKeepsTab() {
    FakeTab a, b;
    a.click_action = TabAction::Stay;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.click();
    CHECK(m.currentTab() == 0);
    CHECK(a.exits == 0);
    CHECK(b.enters == 0);
}

void testCloseMenuFromMiddleTab() {
    FakeTab a, b;
    a.click_action = TabAction::CloseMenu;
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

void testUpdateCanAdvance() {
    FakeTab a, b;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.update(10);
    CHECK(m.currentTab() == 0);  // default update() stays
    a.update_action = TabAction::Next;
    m.update(20);
    CHECK(m.currentTab() == 1);
}

void testTurnGoesToCurrentTabOnly() {
    FakeTab a, b;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.turn(3);
    m.turn(-1);
    CHECK(a.turned == 2);
    CHECK(b.turned == 0);
    m.click();
    m.turn(5);
    CHECK(a.turned == 2);
    CHECK(b.turned == 5);
}

void testClosedMenuIgnoresInput() {
    FakeTab a;
    HookMenu m;
    m.addTab(a);
    m.click();
    m.turn(2);
    m.update(0);
    m.close();
    CHECK(a.clicks == 0);
    CHECK(a.turned == 0);
    CHECK(a.enters == 0);
    CHECK(a.exits == 0);
    CHECK(m.closes == 0);
}

void testExplicitClose() {
    FakeTab a, b;
    HookMenu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.close();
    CHECK(!m.isOpen());
    CHECK(a.exits == 1);
    CHECK(m.closes == 1);
}

void testReopenStartsAtFirstTab() {
    FakeTab a, b;
    Menu m;
    m.addTab(a);
    m.addTab(b);
    m.open();
    m.click();
    m.close();
    m.open();
    CHECK(m.currentTab() == 0);
    CHECK(a.enters == 2);
}

void testTabLimit() {
    FakeTab tabs[Menu::kMaxTabs + 1];
    Menu m;
    for (int i = 0; i < Menu::kMaxTabs; i++) CHECK(m.addTab(tabs[i]));
    CHECK(!m.addTab(tabs[Menu::kMaxTabs]));
    CHECK(m.tabCount() == Menu::kMaxTabs);
}

void testEmptyMenuClosesImmediately() {
    HookMenu m;
    m.open();
    CHECK(!m.isOpen());
    CHECK(m.opens == 1);
    CHECK(m.closes == 1);
}

}  // namespace

void runMenuTests() {
    testOpenEntersFirstTab();
    testClickNextAdvances();
    testClickOnLastTabCloses();
    testStayKeepsTab();
    testCloseMenuFromMiddleTab();
    testUpdateCanAdvance();
    testTurnGoesToCurrentTabOnly();
    testClosedMenuIgnoresInput();
    testExplicitClose();
    testReopenStartsAtFirstTab();
    testTabLimit();
    testEmptyMenuClosesImmediately();
}
```

Replace `Code/tests/test_main.cpp` with:

```cpp
#include <cstdio>

#include "check.h"

void runClickDetectorTests();
void runMenuTests();

int main() {
    runClickDetectorTests();
    runMenuTests();

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("All tests passed\n");
    return 0;
}
```

In `Code/tests/run_tests.sh`, change the `SOURCES` line to:

```sh
SOURCES="../click_detector.cpp ../menu.cpp"
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `Code/tests/run_tests.sh`
Expected: the compile fails with `menu.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`Code/menu.h`:

```cpp
#pragma once

#include <stdint.h>

// What a tab asks the menu to do after handling an event.
enum class TabAction : uint8_t {
    Stay,       // keep showing this tab
    Next,       // go to the next tab (closes the menu after the last one)
    CloseMenu,  // close the whole menu now
};

// One page of a menu. Subclass it and override only what the page needs;
// every hook has a sensible default. Tabs draw themselves (they keep their
// own reference to the display), so the menu core stays hardware-free.
class MenuTab {
public:
    // The tab became visible: draw it.
    virtual void onEnter() {}

    // The user encoder turned by `detents` (positive = clockwise).
    virtual void onTurn(int32_t detents) { (void)detents; }

    // Short click on the user encoder. Default: go to the next tab.
    virtual TabAction onClick() { return TabAction::Next; }

    // Called every main-loop pass while the tab is shown, for tabs that
    // react to something other than the knob (e.g. wire length reached).
    virtual TabAction update(uint32_t now_ms) { (void)now_ms; return TabAction::Stay; }

    // The tab is about to be hidden (next tab or menu closing).
    virtual void onExit() {}

protected:
    // Tabs are never deleted through a MenuTab pointer (no heap).
    ~MenuTab() = default;
};

// An ordered sequence of tabs: open -> tab 0 -> tab 1 -> ... -> close.
// Subclass and override onOpen() / onClose() for menu-wide setup and
// cleanup (e.g. stopping the motor when the working menu closes).
class Menu {
public:
    static constexpr int kMaxTabs = 8;

    // Appends a tab. Tabs are shown in the order they are added.
    // Returns false if the menu already has kMaxTabs tabs.
    bool addTab(MenuTab &tab);
    int tabCount() const;

    // Shows the first tab. Does nothing if the menu is already open.
    void open();

    // Hides the current tab and closes the menu. Safe to call when closed.
    void close();

    bool isOpen() const;

    // Index of the shown tab, -1 when closed.
    int currentTab() const;

    // Input for the shown tab; ignored while closed.
    void turn(int32_t detents);
    void click();
    void update(uint32_t now_ms);

protected:
    virtual void onOpen() {}
    virtual void onClose() {}

private:
    void apply(TabAction action);

    MenuTab *tabs_[kMaxTabs] = {};
    int tab_count_ = 0;
    int current_ = -1;
};
```

`Code/menu.cpp`:

```cpp
#include "menu.h"

bool Menu::addTab(MenuTab &tab) {
    if (tab_count_ >= kMaxTabs) return false;
    tabs_[tab_count_++] = &tab;
    return true;
}

int Menu::tabCount() const {
    return tab_count_;
}

void Menu::open() {
    if (isOpen()) return;
    onOpen();
    if (tab_count_ == 0) {  // nothing to show: open and close right away
        onClose();
        return;
    }
    current_ = 0;
    tabs_[current_]->onEnter();
}

void Menu::close() {
    if (!isOpen()) return;
    tabs_[current_]->onExit();
    current_ = -1;
    onClose();
}

bool Menu::isOpen() const {
    return current_ >= 0;
}

int Menu::currentTab() const {
    return current_;
}

void Menu::turn(int32_t detents) {
    if (isOpen() && detents != 0) tabs_[current_]->onTurn(detents);
}

void Menu::click() {
    if (isOpen()) apply(tabs_[current_]->onClick());
}

void Menu::update(uint32_t now_ms) {
    if (isOpen()) apply(tabs_[current_]->update(now_ms));
}

void Menu::apply(TabAction action) {
    if (!isOpen()) return;  // the tab may have closed the menu itself
    switch (action) {
        case TabAction::Stay:
            break;
        case TabAction::Next:
            if (current_ + 1 >= tab_count_) {
                close();
                break;
            }
            tabs_[current_]->onExit();
            current_++;
            tabs_[current_]->onEnter();
            break;
        case TabAction::CloseMenu:
            close();
            break;
    }
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`, exit code 0.

- [ ] **Step 5: Commit**

```bash
git add Code/menu.h Code/menu.cpp Code/tests
git commit -m "feat(firmware): add Menu and MenuTab page sequence

Rejected: std::function callbacks per tab | pulls in heap on the RP2350
Confidence: high
Scope-risk: narrow

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/menu.h Code/menu.cpp Code/tests
```

---

### Task 3: `MenuManager` (triggers, mutual exclusion, idle screen)

**Files:**
- Create: `Code/menu_manager.h`, `Code/menu_manager.cpp`
- Create: `Code/tests/test_menu_manager.cpp`
- Modify: `Code/tests/test_main.cpp`, `Code/tests/run_tests.sh`

**Interfaces:**
- Consumes: `Menu` and `MenuTab` from Task 2, and `FakeTab` from `Code/tests/fake_tab.h`.
- Produces:
  - `enum class MenuTrigger : uint8_t { UserClick, MeasureClick };`
  - `struct MenuInput { int32_t user_turn = 0; bool user_click = false; bool measure_click = false; };`
  - `class MenuManager { static constexpr int kMaxMenus = 4; using IdleScreenFn = void (*)(); explicit MenuManager(IdleScreenFn show_idle); bool addMenu(MenuTrigger trigger, Menu &menu); void begin(); void process(const MenuInput &input, uint32_t now_ms); Menu *activeMenu() const; }`

- [ ] **Step 1: Write the failing tests**

`Code/tests/test_menu_manager.cpp`:

```cpp
#include "check.h"
#include "fake_tab.h"
#include "menu.h"
#include "menu_manager.h"

namespace {

int g_idle_shown = 0;
void countIdle() { g_idle_shown++; }

MenuInput userClick() { MenuInput in; in.user_click = true; return in; }
MenuInput measureClick() { MenuInput in; in.measure_click = true; return in; }
MenuInput userTurn(int32_t detents) { MenuInput in; in.user_turn = detents; return in; }
const MenuInput kNoInput{};

// Same setup as the firmware: the working menu opens on a user encoder
// click, the calibration menu on a measuring encoder click.
struct Fixture {
    FakeTab work1, work2, cal1, cal2;
    Menu working, calibration;
    MenuManager manager{countIdle};

    Fixture() {
        g_idle_shown = 0;
        working.addTab(work1);
        working.addTab(work2);
        calibration.addTab(cal1);
        calibration.addTab(cal2);
        manager.addMenu(MenuTrigger::UserClick, working);
        manager.addMenu(MenuTrigger::MeasureClick, calibration);
        manager.begin();
    }
};

void testBeginShowsIdleWithoutMenu() {
    Fixture f;
    CHECK(g_idle_shown == 1);
    CHECK(f.manager.activeMenu() == nullptr);
    CHECK(!f.working.isOpen());
    CHECK(!f.calibration.isOpen());
}

void testNothingOpensWithoutClick() {
    Fixture f;
    f.manager.process(kNoInput, 0);
    f.manager.process(userTurn(3), 5);
    CHECK(f.manager.activeMenu() == nullptr);
    CHECK(f.work1.enters == 0);
    CHECK(f.cal1.enters == 0);
    CHECK(f.work1.turned == 0);
}

void testUserClickOpensWorking() {
    Fixture f;
    f.manager.process(userClick(), 0);
    CHECK(f.manager.activeMenu() == &f.working);
    CHECK(f.work1.enters == 1);
    CHECK(f.cal1.enters == 0);
}

void testMeasureClickOpensCalibration() {
    Fixture f;
    f.manager.process(measureClick(), 0);
    CHECK(f.manager.activeMenu() == &f.calibration);
    CHECK(f.cal1.enters == 1);
    CHECK(f.work1.enters == 0);
}

void testCalibrationBlockedWhileWorking() {
    Fixture f;
    f.manager.process(userClick(), 0);
    f.manager.process(measureClick(), 5);
    CHECK(f.manager.activeMenu() == &f.working);
    CHECK(f.working.currentTab() == 0);
    CHECK(f.cal1.enters == 0);
}

void testUserClickAdvancesCalibrationInsteadOfOpeningWorking() {
    Fixture f;
    f.manager.process(measureClick(), 0);
    f.manager.process(userClick(), 5);
    CHECK(f.manager.activeMenu() == &f.calibration);
    CHECK(f.calibration.currentTab() == 1);
    CHECK(f.work1.enters == 0);
}

void testClicksWalkTabsThenReturnToIdle() {
    Fixture f;
    f.manager.process(userClick(), 0);   // open -> tab 0
    f.manager.process(userClick(), 5);   // tab 1
    CHECK(f.working.currentTab() == 1);
    f.manager.process(userClick(), 10);  // past the last tab -> close
    CHECK(f.manager.activeMenu() == nullptr);
    CHECK(!f.working.isOpen());
    CHECK(g_idle_shown == 2);
    CHECK(f.work1.enters == 1);  // the closing click did not reopen the menu
}

void testNextClickReopensAtFirstTab() {
    Fixture f;
    for (int i = 0; i < 3; i++) f.manager.process(userClick(), 0);
    f.manager.process(userClick(), 20);
    CHECK(f.manager.activeMenu() == &f.working);
    CHECK(f.working.currentTab() == 0);
    CHECK(f.work1.enters == 2);
}

void testTurnGoesToActiveTab() {
    Fixture f;
    f.manager.process(userClick(), 0);
    f.manager.process(userTurn(4), 5);
    CHECK(f.work1.turned == 4);
    CHECK(f.cal1.turned == 0);
}

void testTabCanCloseMenuFromUpdate() {
    Fixture f;
    f.manager.process(userClick(), 0);
    f.work1.update_action = TabAction::CloseMenu;
    f.manager.process(kNoInput, 50);
    CHECK(f.manager.activeMenu() == nullptr);
    CHECK(g_idle_shown == 2);
}

void testMenuClosedFromOutsideReturnsToIdle() {
    Fixture f;
    f.manager.process(userClick(), 0);
    f.working.close();
    f.manager.process(kNoInput, 5);
    CHECK(f.manager.activeMenu() == nullptr);
    CHECK(g_idle_shown == 2);
}

void testTriggerCanBeBoundOnlyOnce() {
    Fixture f;
    Menu other;
    CHECK(!f.manager.addMenu(MenuTrigger::UserClick, other));
}

void testSimultaneousClicksOpenFirstRegisteredMenu() {
    Fixture f;
    MenuInput both;
    both.user_click = true;
    both.measure_click = true;
    f.manager.process(both, 0);
    CHECK(f.manager.activeMenu() == &f.working);
    CHECK(f.cal1.enters == 0);
}

}  // namespace

void runMenuManagerTests() {
    testBeginShowsIdleWithoutMenu();
    testNothingOpensWithoutClick();
    testUserClickOpensWorking();
    testMeasureClickOpensCalibration();
    testCalibrationBlockedWhileWorking();
    testUserClickAdvancesCalibrationInsteadOfOpeningWorking();
    testClicksWalkTabsThenReturnToIdle();
    testNextClickReopensAtFirstTab();
    testTurnGoesToActiveTab();
    testTabCanCloseMenuFromUpdate();
    testMenuClosedFromOutsideReturnsToIdle();
    testTriggerCanBeBoundOnlyOnce();
    testSimultaneousClicksOpenFirstRegisteredMenu();
}
```

Replace `Code/tests/test_main.cpp` with:

```cpp
#include <cstdio>

#include "check.h"

void runClickDetectorTests();
void runMenuTests();
void runMenuManagerTests();

int main() {
    runClickDetectorTests();
    runMenuTests();
    runMenuManagerTests();

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("All tests passed\n");
    return 0;
}
```

In `Code/tests/run_tests.sh`, change the `SOURCES` line to:

```sh
SOURCES="../click_detector.cpp ../menu.cpp ../menu_manager.cpp"
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `Code/tests/run_tests.sh`
Expected: the compile fails with `menu_manager.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`Code/menu_manager.h`:

```cpp
#pragma once

#include <stdint.h>

#include "menu.h"

// Events that can open a menu while no menu is shown.
enum class MenuTrigger : uint8_t {
    UserClick,     // short click on the user knob
    MeasureClick,  // short click on the measuring-roll encoder
};

// Input gathered by the main loop for one pass.
struct MenuInput {
    int32_t user_turn = 0;       // user knob detents since the last pass
    bool user_click = false;     // short click on the user knob
    bool measure_click = false;  // short click on the measuring-roll encoder
};

// Owns the "which menu is on screen" decision:
//  - idle (no menu): a menu opens when its trigger fires;
//  - a menu is open: the user knob drives it and every trigger is ignored,
//    so e.g. calibration can never start while wire is being measured;
//  - when the menu closes, the idle screen is shown again.
//
// Adding a menu: build a Menu from MenuTabs and bind it with addMenu().
// A new kind of trigger needs a MenuTrigger value, a MenuInput field and a
// case in menu_manager.cpp's isTriggered().
class MenuManager {
public:
    static constexpr int kMaxMenus = 4;
    using IdleScreenFn = void (*)();

    explicit MenuManager(IdleScreenFn show_idle);

    // Binds `menu` to open on `trigger`. If several triggers fire in the same
    // pass, the menu added first wins. Returns false if the trigger is
    // already bound or kMaxMenus menus are registered.
    bool addMenu(MenuTrigger trigger, Menu &menu);

    // Shows the idle screen. Call once after the display is initialised.
    void begin();

    // Call every main-loop pass.
    void process(const MenuInput &input, uint32_t now_ms);

    // The open menu, or nullptr while idle.
    Menu *activeMenu() const;

private:
    struct Binding {
        MenuTrigger trigger;
        Menu *menu;
    };

    void showIdle();

    Binding bindings_[kMaxMenus] = {};
    int menu_count_ = 0;
    Menu *active_ = nullptr;
    IdleScreenFn show_idle_;
};
```

`Code/menu_manager.cpp`:

```cpp
#include "menu_manager.h"

namespace {

bool isTriggered(MenuTrigger trigger, const MenuInput &input) {
    switch (trigger) {
        case MenuTrigger::UserClick:    return input.user_click;
        case MenuTrigger::MeasureClick: return input.measure_click;
    }
    return false;
}

}  // namespace

MenuManager::MenuManager(IdleScreenFn show_idle) : show_idle_(show_idle) {}

bool MenuManager::addMenu(MenuTrigger trigger, Menu &menu) {
    if (menu_count_ >= kMaxMenus) return false;
    for (int i = 0; i < menu_count_; i++) {
        if (bindings_[i].trigger == trigger) return false;
    }
    bindings_[menu_count_++] = {trigger, &menu};
    return true;
}

void MenuManager::begin() {
    active_ = nullptr;
    showIdle();
}

void MenuManager::process(const MenuInput &input, uint32_t now_ms) {
    if (active_ == nullptr) {
        // Idle: the first matching trigger opens its menu; the knob is ignored
        for (int i = 0; i < menu_count_; i++) {
            if (isTriggered(bindings_[i].trigger, input)) {
                active_ = bindings_[i].menu;
                active_->open();
                break;
            }
        }
    } else {
        // A menu is open: only the user knob drives it. Triggers (including
        // the measuring-encoder click) are dropped, so menus never overlap.
        active_->turn(input.user_turn);
        if (input.user_click) active_->click();
    }

    if (active_ != nullptr && active_->isOpen()) active_->update(now_ms);

    // Closed by a click, by a tab, or from outside via Menu::close()
    if (active_ != nullptr && !active_->isOpen()) {
        active_ = nullptr;
        showIdle();
    }
}

Menu *MenuManager::activeMenu() const {
    return active_;
}

void MenuManager::showIdle() {
    if (show_idle_ != nullptr) show_idle_();
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`, exit code 0.

- [ ] **Step 5: Commit**

```bash
git add Code/menu_manager.h Code/menu_manager.cpp Code/tests
git commit -m "feat(firmware): add MenuManager with exclusive menus and idle screen

Constraint: calibration must never start while the working menu is open
Rejected: opening a menu closes the other | lets a stray click abort a run
Directive: triggers are ignored while any menu is open - keep it that way
Confidence: high
Scope-risk: narrow

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/menu_manager.h Code/menu_manager.cpp Code/tests
```

---

### Task 4: Wire the menu system into the firmware

**Files:**
- Modify: `Code/Code.cpp` (replace everything below the pin constants and peripheral objects, i.e. the whole motor-test `main()`)
- Modify: `Code/CMakeLists.txt` (the `add_executable` line)
- Modify: `Code/CLAUDE.md`

**Interfaces:**
- Consumes: `ClickDetector::update(bool, uint32_t)`, `MenuTab`, `Menu::addTab`, `MenuManager(IdleScreenFn)`, `MenuManager::addMenu`, `MenuManager::begin`, `MenuManager::process`, `MenuInput`, `MenuTrigger`, plus the existing `RotaryEncoder::readDelta()`, `RotaryEncoder::isPressed()` and `Max7219::setSegments()` / `printText()`.
- Produces: the firmware image `Code/build/Code.uf2`. It boots to the all-segments idle screen and has two placeholder menus. Real tabs replace the `LabelTab`s in later work.

- [ ] **Step 1: Add the new sources to the build**

In `Code/CMakeLists.txt`, replace:

```cmake
add_executable(Code Code.cpp max7219.cpp rotary_encoder.cpp motor.cpp)
```

with:

```cmake
add_executable(Code Code.cpp max7219.cpp rotary_encoder.cpp motor.cpp
        click_detector.cpp menu.cpp menu_manager.cpp)
```

- [ ] **Step 2: Replace `Code/Code.cpp`**

Write the whole file. The pin constants and peripheral objects are unchanged. `main()` no longer runs the motor test:

```cpp
#include <stdio.h>
#include "pico/stdlib.h"
#include "max7219.h"
#include "rotary_encoder.h"
#include "motor.h"
#include "click_detector.h"
#include "menu.h"
#include "menu_manager.h"

// MAX7219 wiring (SPI0)
constexpr uint DISPLAY_SCK_PIN  = 18;  // CLK
constexpr uint DISPLAY_MOSI_PIN = 19;  // DIN
constexpr uint DISPLAY_CS_PIN   = 17;  // CS / LOAD

// KY-040 encoders (powered from 3.3 V)
constexpr uint MEASURE_ENC_CLK_PIN = 2;  // encoder geared to the measuring roll
constexpr uint MEASURE_ENC_DT_PIN  = 3;
constexpr uint MEASURE_ENC_SW_PIN  = 4;
constexpr uint USER_ENC_CLK_PIN    = 6;  // encoder for setting length / calibration
constexpr uint USER_ENC_DT_PIN     = 7;
constexpr uint USER_ENC_SW_PIN     = 8;

// Motor driver input (BC547 -> IRF3205, inverting: high = stopped)
constexpr uint MOTOR_PWM_PIN = 10;

constexpr uint32_t LOOP_PERIOD_MS = 5;

Max7219 display(spi0, DISPLAY_SCK_PIN, DISPLAY_MOSI_PIN, DISPLAY_CS_PIN);
RotaryEncoder measureEncoder(MEASURE_ENC_CLK_PIN, MEASURE_ENC_DT_PIN, MEASURE_ENC_SW_PIN);
RotaryEncoder userEncoder(USER_ENC_CLK_PIN, USER_ENC_DT_PIN, USER_ENC_SW_PIN);
Motor motor(MOTOR_PWM_PIN);

namespace {

// Idle screen (after power-up and whenever a menu closes): every segment
// and dot lit, which also shows at a glance if a segment is dead.
void showIdleScreen() {
    for (int pos = 0; pos < Max7219::kDigits; pos++) display.setSegments(pos, 0xFF);
}

// PLACEHOLDER tab: only shows a fixed label. It exists to check the menu
// flow on the hardware; replace it with the real working / calibration tabs.
class LabelTab final : public MenuTab {
public:
    explicit LabelTab(const char *label) : label_(label) {}
    void onEnter() override { display.printText(label_); }

private:
    const char *label_;
};

LabelTab workTab1("run 1");
LabelTab workTab2("run 2");
LabelTab calTab1("CAL 1");
LabelTab calTab2("CAL 2");

Menu workingMenu;
Menu calibrationMenu;
MenuManager menus(showIdleScreen);

}  // namespace

int main()
{
    motor.init();  // first: until then the driver sees a low pin = full speed
    stdio_init_all();
    display.init(15);
    measureEncoder.init();
    userEncoder.init();

    workingMenu.addTab(workTab1);
    workingMenu.addTab(workTab2);
    calibrationMenu.addTab(calTab1);
    calibrationMenu.addTab(calTab2);

    // Working menu: user knob click. Calibration: measuring encoder click.
    // Tabs of both menus are advanced by a short click on the user knob.
    menus.addMenu(MenuTrigger::UserClick, workingMenu);
    menus.addMenu(MenuTrigger::MeasureClick, calibrationMenu);
    menus.begin();  // idle: all segments lit, no menu until a click

    ClickDetector userClicks;
    ClickDetector measureClicks;

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        MenuInput input;
        input.user_turn = userEncoder.readDelta();  // read every pass so idle turns are dropped
        input.user_click = userClicks.update(userEncoder.isPressed(), now);
        input.measure_click = measureClicks.update(measureEncoder.isPressed(), now);
        menus.process(input, now);

        sleep_ms(LOOP_PERIOD_MS);
    }
}
```

- [ ] **Step 3: Build the firmware**

Run: `cd Code && ~/.pico-sdk/ninja/v1.13.2/ninja -C build`
Expected: Ninja re-runs CMake (because `CMakeLists.txt` changed), compiles `click_detector.cpp`, `menu.cpp` and `menu_manager.cpp`, and links `Code.elf`. The build finishes with no errors, and `ls -l build/Code.uf2` shows a fresh timestamp.

If the build fails because CMake cannot be found, reconfigure with the cmake under `~/.pico-sdk/cmake/` and build again: `~/.pico-sdk/cmake/*/bin/cmake -G Ninja -B build`.

- [ ] **Step 4: Re-run the host tests**

Run: `Code/tests/run_tests.sh`
Expected: `All tests passed`.

- [ ] **Step 5: Check on the hardware (manual)**

Flash with `picotool load Code/build/Code.elf -fx`, or copy `Code.uf2` to the BOOTSEL drive. Then check each of these:

1. After power-up the display shows `8.8.8.8.8.8.8.8.` and stays like that. Turning either knob does nothing.
2. A short click on the user knob shows `run 1`, the next short click shows `run 2`, and the next returns to all segments lit.
3. Holding the user knob for more than 2 s and then releasing it changes nothing.
4. With `run 1` or `run 2` shown, clicking the measuring encoder changes nothing.
5. From idle, clicking the measuring encoder shows `CAL 1`. Short clicks on the user knob then show `CAL 2` and return to idle. Clicking the measuring encoder while `CAL 1` or `CAL 2` is shown changes nothing.

- [ ] **Step 6: Update `Code/CLAUDE.md`**

Replace the line `- There are no tests or linters.` with:

```markdown
- Host unit tests for the hardware-independent logic: `Code/tests/run_tests.sh` (system `g++`, no framework). Add a new core `.cpp` to `SOURCES` in that script. No linters.
```

Replace the line `- \`Code.cpp\` — \`main()\`, pin constants (\`constexpr uint ..._PIN\`), and global peripheral objects. Currently only a display test.` with:

```markdown
- `Code.cpp` — `main()`, pin constants (`constexpr uint ..._PIN`), global peripheral objects, the idle screen, and menu registration. The main loop only collects input (`MenuInput`) and calls `MenuManager::process()`. The `LabelTab` menus in it are placeholders.
```

Then add this section after the `motor.{h,cpp}` bullet:

```markdown
- Menu system (hardware-free, host-tested, so no Pico headers in these files):
  - `click_detector.{h,cpp}` — `ClickDetector`: reports a click on release only if the press lasted ≤ 2 s (`kShortClickMaxMs`); longer presses are ignored.
  - `menu.{h,cpp}` — `MenuTab` (one page: `onEnter` / `onTurn` / `onClick` / `update` / `onExit`, returning `TabAction::Stay | Next | CloseMenu`) and `Menu` (ordered tabs; `Next` past the last tab closes the menu; override `onOpen()` / `onClose()` for menu-wide setup and cleanup).
  - `menu_manager.{h,cpp}` — `MenuManager`: while idle, opens the menu whose `MenuTrigger` fired; while a menu is open, only the user knob drives it and every trigger is ignored (menus never overlap); shows the idle screen (all segments lit) when the menu closes.
  - Flow: user knob short click opens the Working menu, measuring-encoder short click opens Calibration; in both, a user-knob short click goes to the next tab.
  - Adding a menu: subclass `MenuTab` per page (tabs draw on `display` themselves), `addTab()` them in order to a `Menu`, and `menus.addMenu(trigger, menu)` in `main()`. A new trigger source needs a `MenuTrigger` value, a `MenuInput` field, a case in `isTriggered()` (`menu_manager.cpp`), and filling that field in the main loop.
```

- [ ] **Step 7: Commit**

```bash
git add Code/Code.cpp Code/CMakeLists.txt Code/CLAUDE.md
git commit -m "feat(firmware): run the main loop through the menu manager

Boot shows all segments lit; the user knob opens the working menu and the
measuring encoder opens calibration. Both menus hold placeholder label tabs
until the real pages are written.

Directive: LabelTab menus in Code.cpp are placeholders - replace, do not extend
Confidence: medium
Scope-risk: moderate
Not-tested: behaviour with the motor running (motor test loop removed)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -- Code/Code.cpp Code/CMakeLists.txt Code/CLAUDE.md
```
