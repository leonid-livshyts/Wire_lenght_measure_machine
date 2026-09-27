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
    // Also runs on the pass that opens the menu, right after onEnter().
    virtual TabAction update(uint32_t now_ms) { (void)now_ms; return TabAction::Stay; }

    // The tab is about to be hidden (next tab or menu closing).
    virtual void onExit() {}

    // Return true to leave this tab out of the sequence. Asked every time
    // the menu moves to it, so it can depend on an earlier tab's choice.
    virtual bool isSkipped() const { return false; }

protected:
    // Tabs are never deleted through a MenuTab pointer (no heap).
    ~MenuTab() = default;
};

// An ordered sequence of tabs: open -> tab 0 -> tab 1 -> ... -> close.
// Subclass and override onOpen() / onClose() for menu-wide setup and
// cleanup (e.g. stopping the motor when the working menu closes).
// Menus are statics, never deleted through a Menu pointer (no heap), so the destructor is not virtual.
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

    // Calls onExit() on the shown tab. A close() from inside the hook is
    // deferred to the caller via close_requested_, so no hook fires twice.
    void leaveCurrent();

    // Finishes closing the menu: clears current_ and fires onClose().
    void finishClose();

    // Index of the first tab at or after `index` that is not skipped, -1 if none.
    int firstShownFrom(int index) const;

    MenuTab *tabs_[kMaxTabs] = {};
    int tab_count_ = 0;
    int current_ = -1;
    bool leaving_ = false;          // inside leaveCurrent(): a call chain is exiting the shown tab
    bool close_requested_ = false;  // close() was re-entered from onExit(); finish it when leaveCurrent() returns
};
