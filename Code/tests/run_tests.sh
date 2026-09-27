#!/bin/sh
# Host-side unit tests for the hardware-independent firmware logic.
set -e
cd "$(dirname "$0")"
mkdir -p build

SOURCES="../click_detector.cpp ../menu.cpp ../menu_manager.cpp ../knob_accel.cpp ../settings.cpp"

g++ -std=c++17 -Wall -Wextra -Werror -I.. -o build/tests test_*.cpp $SOURCES
./build/tests
