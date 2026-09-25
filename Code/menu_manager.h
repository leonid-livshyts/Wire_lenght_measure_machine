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
