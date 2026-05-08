# Technology Stack

**Analysis Date:** 2025-01-24

## Languages

**Primary:**
- C++ (C++17) - Core firmware logic, UI components, and hardware drivers.

**Secondary:**
- C - Low-level drivers and LVGL configurations.
- Python - Tooling for asset generation (e.g., `display/tools/generate_dial_face.py`).

## Runtime

**Environment:**
- ESP32-S3 (Xtensa® Dual-core 32-bit LX7)

**Package Manager:**
- PlatformIO
- Lockfile: Not explicitly present in root (managed by `.pio/` folders).

## Frameworks

**Core:**
- Arduino ESP32 (v3.x) - Primary framework for all modules.
- ESP-IDF (v5.1+) - Underlying framework used for TWAI, display drivers, and FreeRTOS.

**Testing:**
- Custom Hardware-in-loop (HIL) - `hardware_in_loop/src/main.cpp`.

**Build/Dev:**
- PlatformIO - Build system and environment management.

## Key Dependencies

**Critical:**
- LVGL (v8.3.11) - Graphics library for the dashboard UI.
- ESP32_Display_Panel - Hardware abstraction for the Waveshare 5" LCD.
- ESP32_IO_Expander - Driver for CH422G I/O expander.
- ESP32-TWAI-CAN - CAN/TWAI bus communication.

**Infrastructure:**
- SdFat (v2.3.1) - High-performance SD card filesystem support.
- Adafruit MPU6050 - IMU driver for tilt/roll sensing.
- Adafruit GFX / SSD1306 - Used for secondary or legacy displays.

## Configuration

**Environment:**
- Configured via `platformio.ini` in root and subdirectories (`mainboard`, `display`, `twai_transmit`).
- `include/Config.h` - Global firmware flags and rates.

**Build:**
- `platformio.ini`: Defines board, platform, framework, and build flags.
- `display/lib/lv_conf.h`: LVGL configuration.

## Platform Requirements

**Development:**
- VS Code with PlatformIO extension.
- Python 3.x for asset scripts.

**Production:**
- Waveshare ESP32-S3-Touch-LCD-5 (Display Board).
- Custom Mainboard based on ESP32-S3.

---

*Stack analysis: 2025-01-24*
