#include "settings_store.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"
#include "settings_default.h"  // generated from settings.json

namespace {

// Last sector of the flash chip, far past the end of the program
constexpr uint32_t kSettingsOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
constexpr uint32_t kFlashLockTimeoutMs = 1000;

// Runs with interrupts off (via flash_safe_execute); `param` is one page.
void writeSettingsSector(void *param) {
    flash_range_erase(kSettingsOffset, FLASH_SECTOR_SIZE);
    flash_range_program(kSettingsOffset, static_cast<const uint8_t *>(param), FLASH_PAGE_SIZE);
}

}  // namespace

Settings loadSettings() {
    Settings settings;
    parseSettingsJson(kDefaultSettingsJson, settings);  // host test guarantees it parses

    // Erased flash is all 0xFF, so it has no NUL and is not parsed
    const char *stored = reinterpret_cast<const char *>(XIP_BASE + kSettingsOffset);
    if (memchr(stored, '\0', FLASH_PAGE_SIZE) != nullptr) parseSettingsJson(stored, settings);
    return settings;
}

bool saveSettings(const Settings &settings) {
    static uint8_t page[FLASH_PAGE_SIZE];  // static: not on the stack while interrupts are off
    memset(page, 0, sizeof(page));         // zero padding also terminates the text
    if (formatSettingsJson(settings, reinterpret_cast<char *>(page), sizeof(page)) < 0) return false;
    return flash_safe_execute(writeSettingsSector, page, kFlashLockTimeoutMs) == PICO_OK;
}
