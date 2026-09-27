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
    int first = firstShownFrom(0);
    if (first < 0) {  // nothing to show: open and close right away
        onClose();
        return;
    }
    current_ = first;
    tabs_[current_]->onEnter();
}

void Menu::leaveCurrent() {
    leaving_ = true;
    tabs_[current_]->onExit();
    leaving_ = false;
}

void Menu::finishClose() {
    close_requested_ = false;
    current_ = -1;
    onClose();
}

int Menu::firstShownFrom(int index) const {
    for (int i = index; i < tab_count_; i++) {
        if (!tabs_[i]->isSkipped()) return i;
    }
    return -1;
}

void Menu::close() {
    if (!isOpen()) return;
    if (leaving_) {  // re-entered from onExit(): the running transition closes
        close_requested_ = true;
        return;
    }
    leaveCurrent();
    finishClose();
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

void Menu::longClick() {
    if (isOpen()) apply(tabs_[current_]->onLongClick());
}

void Menu::update(uint32_t now_ms) {
    if (isOpen()) apply(tabs_[current_]->update(now_ms));
}

void Menu::apply(TabAction action) {
    if (!isOpen()) return;  // the tab may have closed the menu itself
    switch (action) {
        case TabAction::Stay:
            break;
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
        case TabAction::CloseMenu:
            close();
            break;
    }
}
