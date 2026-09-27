#include "settings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr char kMmPerTickKey[] = "\"mm_per_tick\"";

}  // namespace

// A minimal reader for our own flat file, not a general JSON parser.
bool parseSettingsJson(const char *text, Settings &out) {
    if (text == nullptr) return false;
    const char *key = std::strstr(text, kMmPerTickKey);
    if (key == nullptr) return false;

    const char *p = key + sizeof(kMmPerTickKey) - 1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != ':') return false;
    p++;

    char *end = nullptr;
    double value = std::strtod(p, &end);  // skips leading whitespace itself
    if (end == p || !std::isfinite(value) || value <= 0.0) return false;

    out.mm_per_tick = value;
    return true;
}

int formatSettingsJson(const Settings &settings, char *buf, size_t size) {
    // 9 significant digits keep the ratio accurate to far below one tick per km
    int n = std::snprintf(buf, size, "{\"mm_per_tick\": %.9g}\n", settings.mm_per_tick);
    if (n < 0 || (size_t)n >= size) return -1;
    return n;
}
