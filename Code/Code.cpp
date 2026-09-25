#include <stdio.h>
#include "pico/stdlib.h"
#include "max7219.h"
#include "rotary_encoder.h"
#include "motor.h"

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

Max7219 display(spi0, DISPLAY_SCK_PIN, DISPLAY_MOSI_PIN, DISPLAY_CS_PIN);
RotaryEncoder measureEncoder(MEASURE_ENC_CLK_PIN, MEASURE_ENC_DT_PIN, MEASURE_ENC_SW_PIN);
RotaryEncoder userEncoder(USER_ENC_CLK_PIN, USER_ENC_DT_PIN, USER_ENC_SW_PIN);
Motor motor(MOTOR_PWM_PIN);

int main()
{
    motor.init();  // first: until then the driver sees a low pin = full speed
    stdio_init_all();
    display.init(15);
    measureEncoder.init();
    userEncoder.init();

    // Motor test: the user encoder sets the max speed (%), its button
    // starts / stops the motor. Speed changes while running are ramped too.
    int32_t maxSpeed = 100;
    motor.setMaxSpeed(maxSpeed);
    display.printNumber(maxSpeed);
    int32_t shownPercent = -1;

    while (true) {
        int32_t delta = userEncoder.readDelta();
        if (delta != 0) {
            maxSpeed += delta * 5;
            if (maxSpeed < 0) maxSpeed = 0;
            if (maxSpeed > 100) maxSpeed = 100;
            motor.setMaxSpeed(maxSpeed);
            display.printNumber(maxSpeed);
        }

        if (userEncoder.wasClicked()) {
            if (motor.isRunning()) motor.stop();
            else motor.start();
        }

        int32_t percent = (int32_t)(motor.getSpeed() * 100.0f + 0.5f);
        if (percent != shownPercent) {
            printf("max %ld%%  speed %ld%%\n", (long)maxSpeed, (long)percent);
            shownPercent = percent;
        }
        sleep_ms(5);
    }
}
