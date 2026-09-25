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
