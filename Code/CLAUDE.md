# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Firmware for a wire/rope counting machine: a measuring roll geared to an encoder counts how much wire passes through, and a 12 V motor (via a small motor driver, for speed control and smooth stopping) pulls the wire so the machine can wind a hank of an exact length. An 8-digit 7-segment display plus a second (user) encoder are used to set the target length and calibrate the mechanism.

This directory is only the firmware. The wider repo (one level up) also holds `Case/` (FreeCAD), `PCB, Schematic and Components/` (KiCad — the source of truth for pin assignments), and `images/`.

## Toolchain & build

- Raspberry Pi Pico SDK 2.3.1 (C++17), managed by the Raspberry Pi Pico VS Code extension under `~/.pico-sdk/`. Do not edit the block marked `DO NOT EDIT` in `CMakeLists.txt`.
- Target board: `weact_studio_rp2350b_core` (RP2350B), set via `PICO_BOARD` in `CMakeLists.txt`.
- Build (Ninja, `build/` is already configured):
  ```
  ~/.pico-sdk/ninja/v1.13.2/ninja -C build
  ```
  Reconfigure from scratch: `cmake -G Ninja -B build` (use the cmake under `~/.pico-sdk/cmake/`).
- Outputs: `build/Code.uf2` (drag-and-drop to BOOTSEL drive), `build/Code.elf`.
- Flash over USB: `picotool load build/Code.elf -fx` (`.vscode/tasks.json` expects picotool 2.3.1; only 2.2.0-a4 is currently installed in `~/.pico-sdk/picotool/`). SWD flashing/debug uses OpenOCD with a CMSIS-DAP probe (see `.vscode/tasks.json` / `launch.json`).
- stdio goes over USB CDC (UART stdio disabled).
- There are no tests or linters.

New `.cpp` files must be added to `add_executable(Code ...)`, and any new SDK hardware library (e.g. `hardware_pwm`, `hardware_pio`) to `target_link_libraries`.

## Code structure

- `Code.cpp` — `main()`, pin constants (`constexpr uint ..._PIN`), and global peripheral objects. Currently only a display test.
- `max7219.{h,cpp}` — `Max7219` driver for an 8-digit 7-segment module on SPI0 (1 MHz, mode 0, CS toggled manually as a GPIO). Key conventions:
  - No-decode mode; the driver keeps a segment `buffer_` and rewrites digits from it. Segment bit layout is `DP A B C D E F G` (`SEG_*` constants).
  - Public positions are left-to-right (0 = leftmost), but on the module DIG0 is the rightmost digit — `writeDigit()` does the reversal. Keep that mapping in one place.
  - `printNumber(value, decimals)` gives right-aligned fixed-point output (e.g. `(1250, 2)` → `12.50`), falling back to `Err` if it does not fit; `printText()` merges `.` into the previous character.

- `rotary_encoder.{h,cpp}` — `RotaryEncoder` driver for KY-040 encoders, multi-instance (up to `kMaxEncoders` = 4). Rotation is decoded in a per-instance raw GPIO IRQ handler (quadrature lookup table, every edge of CLK/DT); the button is debounced by one shared 1 ms repeating timer. `getCount()` = raw quadrature steps (4 per detent, for the measuring roll), `readDelta()` = detents since last call (for UI), `wasClicked()` = one-shot press flag. Uses `gpio_add_raw_irq_handler_masked64`, so it coexists with other GPIO IRQ users — do not switch it to `gpio_set_irq_enabled_with_callback` (single global callback).

- `motor.{h,cpp}` — `Motor`, PWM speed control for the pulling motor. The driver (BC547 → IRF3205) is inverting (pin high = stopped), handled by inverted PWM output polarity, so the API uses 0.0 = stopped … 1.0 = full. `start()` / `stop()` / `setMaxSpeed()` are non-blocking: a 5 ms repeating timer ramps the speed along an S-curve over `ramp_ms` (default 2.5 s). A new command always ramps from the current speed, so it never jumps. `motor.init()` must run first in `main()`, because a low pin means full speed.

Current display wiring (SPI0): SCK=GP18, MOSI/DIN=GP19, CS/LOAD=GP17.
Encoders (KY-040, powered from 3.3 V): measuring roll CLK=GP2, DT=GP3, SW=GP4; user knob CLK=GP6, DT=GP7, SW=GP8.

`.great_cto/` and `shared/` are tooling files from a Claude plugin, not firmware.
