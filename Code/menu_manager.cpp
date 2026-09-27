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
        if (input.user_long_click) active_->longClick();
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
