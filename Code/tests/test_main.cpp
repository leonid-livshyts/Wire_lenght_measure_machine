#include <cstdio>

#include "check.h"

void runClickDetectorTests();

int main() {
    runClickDetectorTests();

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("All tests passed\n");
    return 0;
}
