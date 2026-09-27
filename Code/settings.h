#pragma once

#include <stddef.h>

// Values that must survive a power cycle. Stored as JSON text (see
// settings_store.h); the defaults come from settings.json.
struct Settings {
    double mm_per_tick = 1.0;  // wire length per measuring-roll quadrature step
};

// Reads the settings from NUL-terminated JSON text. Only the keys it knows
// are read; returns false and leaves `out` unchanged if "mm_per_tick" is
// missing or not a finite number > 0.
bool parseSettingsJson(const char *text, Settings &out);

// Writes the settings as NUL-terminated JSON. Returns the length (without
// the NUL), or -1 if `size` is too small.
int formatSettingsJson(const Settings &settings, char *buf, size_t size);
