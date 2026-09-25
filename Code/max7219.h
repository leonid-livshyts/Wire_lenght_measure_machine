#pragma once

#include <stdint.h>
#include "hardware/spi.h"

// Driver for an 8-digit 7-segment module built on MAX7219.
// Positions are counted from the LEFT: 0 = leftmost digit, 7 = rightmost.
class Max7219 {
public:
    static constexpr int kDigits = 8;

    // Raw segment bits (no-decode mode): DP A B C D E F G
    static constexpr uint8_t SEG_DP = 0x80;
    static constexpr uint8_t SEG_A  = 0x40;
    static constexpr uint8_t SEG_B  = 0x20;
    static constexpr uint8_t SEG_C  = 0x10;
    static constexpr uint8_t SEG_D  = 0x08;
    static constexpr uint8_t SEG_E  = 0x04;
    static constexpr uint8_t SEG_F  = 0x02;
    static constexpr uint8_t SEG_G  = 0x01;

    Max7219(spi_inst_t *spi, uint sck_pin, uint mosi_pin, uint cs_pin);

    void init(uint8_t brightness = 8);
    void clear();
    void setBrightness(uint8_t level);  // 0..15
    void setPower(bool on);             // false = shutdown mode (display off, state kept)
    void setTest(bool on);              // true = all segments lit

    // Single digit control. `dot` sets the decimal point of that digit.
    void setDigit(int pos, int value, bool dot = false);  // value 0..15 (hex), -1 = blank
    void setChar(int pos, char c, bool dot = false);      // 0-9, A-F and some letters, '-', '_', ' '
    void setSegments(int pos, uint8_t segments);          // raw bits, see SEG_*
    uint8_t getSegments(int pos) const;

    // Decimal point control without touching the digit itself
    void setDot(int pos, bool on);
    bool getDot(int pos) const;

    // Right-aligned number. `decimals` places a dot, e.g. (1250, 2) -> "12.50".
    // Shows "Err" if the number does not fit.
    void printNumber(int32_t value, int decimals = 0);

    // Left-aligned text; a '.' is merged into the previous character.
    void printText(const char *text);

    static uint8_t charToSegments(char c);

private:
    void write(uint8_t reg, uint8_t data);
    void writeDigit(int pos);

    spi_inst_t *spi_;
    uint sck_pin_;
    uint mosi_pin_;
    uint cs_pin_;
    uint8_t buffer_[kDigits] = {};
};
