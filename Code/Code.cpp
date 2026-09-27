#include <stdio.h>
#include "pico/stdlib.h"
#include "max7219.h"
#include "rotary_encoder.h"
#include "motor.h"
#include "click_detector.h"
#include "menu.h"
#include "menu_manager.h"
#include "settings_store.h"
#include "calibration_menu.h"

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
constexpr uint32_t DISPLAY_REFRESH_MS = 250;  // heals a display corrupted by a noisy SPI frame

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
// flow on the hardware; replace it with the real working tabs.
class LabelTab final : public MenuTab {
public:
    explicit LabelTab(const char *label) : label_(label) {}
    void onEnter() override { display.printText(label_); }

private:
    const char *label_;
};

LabelTab workTab1("run 1");
LabelTab workTab2("run 2");

Menu workingMenu;

Settings settings;  // loaded from flash in main(); the calibration menu updates it

CalibrationContext calibration{display, motor, measureEncoder, settings, saveSettings};
CalibrationMenu calibrationMenu(calibration);

MenuManager menus(showIdleScreen);

}  // namespace

int main()
{
    motor.init();  // first: until then the driver sees a low pin = full speed
    stdio_init_all();
    settings = loadSettings();  // settings.json defaults, overridden by the last calibration
    display.init(15);
    measureEncoder.init();
    userEncoder.init();

    workingMenu.addTab(workTab1);
    workingMenu.addTab(workTab2);

    // Working menu: user knob click. Calibration: measuring encoder click.
    // Tabs of both menus are advanced by a short click on the user knob.
    menus.addMenu(MenuTrigger::UserClick, workingMenu);
    menus.addMenu(MenuTrigger::MeasureClick, calibrationMenu);
    menus.begin();  // idle: all segments lit, no menu until a click

    ClickDetector userClicks;
    ClickDetector measureClicks;
    uint32_t last_display_refresh = 0;

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        MenuInput input;
        input.user_turn = userEncoder.readDelta();  // read every pass so idle turns are dropped
        input.user_click = userClicks.update(userEncoder.isPressed(), now);
        input.measure_click = measureClicks.update(measureEncoder.isPressed(), now);
        // measureEncoder.readDelta() is deliberately not drained: the measuring roll uses getCount(),
        // so a later readDelta() would return everything since boot.
        menus.process(input, now);

        if (now - last_display_refresh >= DISPLAY_REFRESH_MS) {
            display.refresh();
            last_display_refresh = now;
        }

        sleep_ms(LOOP_PERIOD_MS);
    }
}
