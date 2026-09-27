#include <cmath>
#include <cstdio>
#include <cstring>

#include "check.h"
#include "settings.h"

namespace {

void testParsesValue() {
    Settings s;
    CHECK(parseSettingsJson("{\"mm_per_tick\": 0.125}", s));
    CHECK(s.mm_per_tick == 0.125);
}

void testParsesWithWhitespace() {
    Settings s;
    CHECK(parseSettingsJson("{\n    \"mm_per_tick\" :\t2.5\n}\n", s));
    CHECK(s.mm_per_tick == 2.5);
}

void testRejectsBadValues() {
    const char *bad[] = {
        "",
        "{}",
        "{\"mm_per_tick\": }",
        "{\"mm_per_tick\": \"abc\"}",
        "{\"mm_per_tick\" 1.0}",
        "{\"mm_per_tick\": 0}",
        "{\"mm_per_tick\": -1.5}",
        "{\"mm_per_tick\": nan}",
        "{\"mm_per_tick\": inf}",
    };
    for (const char *text : bad) {
        Settings s;
        s.mm_per_tick = 3.0;
        CHECK(!parseSettingsJson(text, s));
        CHECK(s.mm_per_tick == 3.0);  // unchanged on failure
    }
    Settings s;
    CHECK(!parseSettingsJson(nullptr, s));
}

void testFormatRoundTrip() {
    Settings s;
    s.mm_per_tick = 0.0123456789;
    char buf[64];
    int n = formatSettingsJson(s, buf, sizeof(buf));
    CHECK(n > 0);
    CHECK((size_t)n == std::strlen(buf));
    Settings back;
    CHECK(parseSettingsJson(buf, back));
    CHECK(std::fabs(back.mm_per_tick - s.mm_per_tick) < s.mm_per_tick * 1e-8);
}

void testFormatBufferTooSmall() {
    Settings s;
    char buf[8];
    CHECK(formatSettingsJson(s, buf, sizeof(buf)) == -1);
}

void testRepoDefaultsFileIsValid() {
    // run_tests.sh runs from Code/tests
    std::FILE *f = std::fopen("../settings.json", "r");
    CHECK(f != nullptr);
    if (f == nullptr) return;
    char buf[512] = {};
    CHECK(std::fread(buf, 1, sizeof(buf) - 1, f) > 0);
    std::fclose(f);
    Settings s;
    s.mm_per_tick = 99.0;
    CHECK(parseSettingsJson(buf, s));
    CHECK(s.mm_per_tick == 1.0);
}

}  // namespace

void runSettingsTests() {
    testParsesValue();
    testParsesWithWhitespace();
    testRejectsBadValues();
    testFormatRoundTrip();
    testFormatBufferTooSmall();
    testRepoDefaultsFileIsValid();
}
