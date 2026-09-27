#pragma once

#include "settings.h"

// Settings persisted as JSON text in the last 4 KB flash sector.

// Defaults from settings.json (compiled in), overridden by whatever valid
// JSON is stored in flash. Erased or corrupt flash just gives the defaults.
Settings loadSettings();

// Erases the settings sector and writes `settings` as JSON. Interrupts are
// off for the erase (~50 ms typically, up to a few hundred ms worst case):
// encoder steps and motor ramp steps are missed meanwhile, so do not call
// it while wire is being measured.
// Returns false if the text did not fit or the flash could not be locked.
bool saveSettings(const Settings &settings);
