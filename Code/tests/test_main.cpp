#include <cstdio>

#include "check.h"

void runClickDetectorTests();
void runMenuTests();
void runMenuManagerTests();
void runKnobAccelTests();
void runSettingsTests();
void runCalibrationTests();
void runRunLogicTests();

int main() {
    runClickDetectorTests();
    runMenuTests();
    runMenuManagerTests();
    runKnobAccelTests();
    runSettingsTests();
    runCalibrationTests();
    runRunLogicTests();

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("All tests passed\n");
    return 0;
}
