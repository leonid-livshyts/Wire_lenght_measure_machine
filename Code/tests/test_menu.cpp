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

void testCloseFromOnExitDuringNext() {
    FakeTab a, b;
    HookMenu m;
    m.addTab(a);
    m.addTab(b);
    a.close_on_exit = &m;
    m.open();
    m.click();
    CHECK(!m.isOpen());
    CHECK(m.currentTab() == -1);
    CHECK(a.exits == 1);
    CHECK(b.enters == 0);
    CHECK(m.closes == 1);
}

void testCloseFromOnExitDuringClose() {
    FakeTab a, b;
    HookMenu m;
    m.addTab(a);
    m.addTab(b);
    a.close_on_exit = &m;
    m.open();
    m.close();
    CHECK(!m.isOpen());
    CHECK(a.exits == 1);
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
    testCloseFromOnExitDuringNext();
    testCloseFromOnExitDuringClose();
}
