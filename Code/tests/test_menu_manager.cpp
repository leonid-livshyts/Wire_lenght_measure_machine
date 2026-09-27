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
MenuInput userLongClick() { MenuInput in; in.user_long_click = true; return in; }
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
    testLongClickDoesNotOpenMenu();
    testLongClickReachesOpenTab();
}
