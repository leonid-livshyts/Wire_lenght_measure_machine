#include "max7219.h"

#include "pico/stdlib.h"

namespace {

enum Register : uint8_t {
    REG_DIGIT0      = 0x01,
    REG_DECODE_MODE = 0x09,
    REG_INTENSITY   = 0x0A,
    REG_SCAN_LIMIT  = 0x0B,
    REG_SHUTDOWN    = 0x0C,
    REG_TEST        = 0x0F,
};

// 0-9, A-F in no-decode segment format
const uint8_t kHexFont[16] = {
    0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70,
    0x7F, 0x7B, 0x77, 0x1F, 0x4E, 0x3D, 0x4F, 0x47,
};

}  // namespace

Max7219::Max7219(spi_inst_t *spi, uint sck_pin, uint mosi_pin, uint cs_pin)
    : spi_(spi), sck_pin_(sck_pin), mosi_pin_(mosi_pin), cs_pin_(cs_pin) {}

void Max7219::init(uint8_t brightness) {
    spi_init(spi_, 1000 * 1000);  // MAX7219 allows 10 MHz; 1 MHz is safer with 3.3 V logic
    spi_set_format(spi_, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(sck_pin_, GPIO_FUNC_SPI);
    gpio_set_function(mosi_pin_, GPIO_FUNC_SPI);

    gpio_init(cs_pin_);
    gpio_set_dir(cs_pin_, GPIO_OUT);
    gpio_put(cs_pin_, 1);

    test_on_ = false;
    brightness_ = brightness > 15 ? 15 : brightness;
    power_on_ = true;
    for (uint8_t &segments : buffer_) segments = 0;
    refresh();
}

void Max7219::refresh() {
    write(REG_TEST, test_on_ ? 1 : 0);
    write(REG_DECODE_MODE, 0);          // raw segments for every digit
    write(REG_SCAN_LIMIT, kDigits - 1); // scan all 8 digits
    write(REG_INTENSITY, brightness_);
    for (int pos = 0; pos < kDigits; pos++) writeDigit(pos);
    write(REG_SHUTDOWN, power_on_ ? 1 : 0);  // last: switch on with the digits already set
}

void Max7219::clear() {
    for (int pos = 0; pos < kDigits; pos++) {
        buffer_[pos] = 0;
        writeDigit(pos);
    }
}

void Max7219::setBrightness(uint8_t level) {
    brightness_ = level > 15 ? 15 : level;
    write(REG_INTENSITY, brightness_);
}

void Max7219::setPower(bool on) {
    power_on_ = on;
    write(REG_SHUTDOWN, on ? 1 : 0);
}

void Max7219::setTest(bool on) {
    test_on_ = on;
    write(REG_TEST, on ? 1 : 0);
}

void Max7219::setDigit(int pos, int value, bool dot) {
    uint8_t segments = (value >= 0 && value <= 15) ? kHexFont[value] : 0;
    setSegments(pos, segments | (dot ? SEG_DP : 0));
}

void Max7219::setChar(int pos, char c, bool dot) {
    setSegments(pos, charToSegments(c) | (dot ? SEG_DP : 0));
}

void Max7219::setSegments(int pos, uint8_t segments) {
    if (pos < 0 || pos >= kDigits) return;
    buffer_[pos] = segments;
    writeDigit(pos);
}

uint8_t Max7219::getSegments(int pos) const {
    return (pos >= 0 && pos < kDigits) ? buffer_[pos] : 0;
}

void Max7219::setDot(int pos, bool on) {
    if (pos < 0 || pos >= kDigits) return;
    uint8_t segments = on ? (buffer_[pos] | SEG_DP) : (buffer_[pos] & ~SEG_DP);
    setSegments(pos, segments);
}

bool Max7219::getDot(int pos) const {
    return getSegments(pos) & SEG_DP;
}

void Max7219::printNumber(int32_t value, int decimals) {
    if (decimals < 0) decimals = 0;
    if (decimals > kDigits - 1) decimals = kDigits - 1;

    bool negative = value < 0;
    uint32_t magnitude = negative ? 0u - (uint32_t)value : (uint32_t)value;

    uint8_t out[kDigits] = {};
    int pos = kDigits - 1;
    int written = 0;
    // Always print at least the integer "0" before the dot
    while ((magnitude > 0 || written <= decimals) && pos >= 0) {
        out[pos] = kHexFont[magnitude % 10];
        if (written == decimals && decimals > 0) out[pos] |= SEG_DP;
        magnitude /= 10;
        written++;
        pos--;
    }

    bool fits = magnitude == 0 && (!negative || pos >= 0);
    if (!fits) {
        printText("Err");
        return;
    }
    if (negative) out[pos] = SEG_G;

    for (int i = 0; i < kDigits; i++) {
        buffer_[i] = out[i];
        writeDigit(i);
    }
}

void Max7219::printText(const char *text) {
    uint8_t out[kDigits] = {};
    int pos = 0;
    for (const char *p = text; *p && pos <= kDigits; p++) {
        if (*p == '.' && pos > 0 && !(out[pos - 1] & SEG_DP)) {
            out[pos - 1] |= SEG_DP;
            continue;
        }
        if (pos == kDigits) break;
        out[pos++] = *p == '.' ? SEG_DP : charToSegments(*p);
    }

    for (int i = 0; i < kDigits; i++) {
        buffer_[i] = out[i];
        writeDigit(i);
    }
}

uint8_t Max7219::charToSegments(char c) {
    if (c >= '0' && c <= '9') return kHexFont[c - '0'];
    switch (c) {
        case 'A': case 'a': return 0x77;
        case 'B': case 'b': return 0x1F;
        case 'C':           return 0x4E;
        case 'c':           return 0x0D;
        case 'D': case 'd': return 0x3D;
        case 'E': case 'e': return 0x4F;
        case 'F': case 'f': return 0x47;
        case 'G': case 'g': return 0x5E;
        case 'H':           return 0x37;
        case 'h':           return 0x17;
        case 'I':           return 0x30;
        case 'i':           return 0x10;
        case 'J': case 'j': return 0x3C;
        case 'L': case 'l': return 0x0E;
        case 'N': case 'n': return 0x15;
        case 'O':           return 0x7E;
        case 'o':           return 0x1D;
        case 'P': case 'p': return 0x67;
        case 'Q': case 'q': return 0x73;
        case 'R': case 'r': return 0x05;
        case 'S': case 's': return 0x5B;
        case 'T': case 't': return 0x0F;
        case 'U':           return 0x3E;
        case 'u':           return 0x1C;
        case 'Y': case 'y': return 0x3B;
        case '-':           return SEG_G;
        case '_':           return SEG_D;
        case '=':           return SEG_G | SEG_D;
        default:            return 0;  // space and unsupported characters are blank
    }
}

void Max7219::write(uint8_t reg, uint8_t data) {
    uint8_t frame[2] = {reg, data};
    gpio_put(cs_pin_, 0);
    spi_write_blocking(spi_, frame, 2);
    gpio_put(cs_pin_, 1);  // data is latched on the rising edge of CS
}

void Max7219::writeDigit(int pos) {
    // On common 8-digit modules DIG0 is the rightmost digit
    write(REG_DIGIT0 + (kDigits - 1 - pos), buffer_[pos]);
}
