# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD weact_studio_rp2350b_core CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Code C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Code Code.cpp max7219.cpp rotary_encoder.cpp motor.cpp
        click_detector.cpp menu.cpp menu_manager.cpp knob_accel.cpp settings.cpp calibration.cpp settings_store.cpp calibration_menu.cpp
        number_edit_tab.cpp run_logic.cpp working_menu.cpp)

# settings.json is compiled in as the default settings (settings_default.h).
# Editing it re-runs CMake; the header is rewritten only when the text changed.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/settings.json)
file(READ ${CMAKE_CURRENT_LIST_DIR}/settings.json SETTINGS_JSON)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/settings_default.h.tmp
    "#pragma once\n// Generated from settings.json by CMakeLists.txt. Do not edit.\nconstexpr char kDefaultSettingsJson[] = R\"json(${SETTINGS_JSON})json\";\n")
configure_file(${CMAKE_CURRENT_BINARY_DIR}/generated/settings_default.h.tmp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/settings_default.h COPYONLY)

pico_set_program_name(Code "Code")
pico_set_program_version(Code "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Code 0)
pico_enable_stdio_usb(Code 1)

# Add the standard library to the build
target_link_libraries(Code
        pico_stdlib
        hardware_spi
        hardware_pwm
        hardware_flash
        pico_flash)

# Add the standard include files to the build
target_include_directories(Code PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

pico_add_extra_outputs(Code)

