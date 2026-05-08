# Codebase Structure

**Analysis Date:** 2025-01-21

## Directory Layout

```
Superbike/
├── display/             # Dashboard firmware (ESP32-S3 + LVGL)
│   ├── lib/             # Board-specific hardware drivers
│   ├── src/
│   │   ├── CAN/         # CAN reception and DashboardState updates
│   │   ├── DashboardUI/ # UI screens and LVGL widget management
│   │   ├── Logger/      # SD card CSV logging
│   │   ├── lvgl_config/ # LVGL porting and HAL initialization
│   │   └── main.cpp     # Display entry point and task orchestration
├── mainboard/           # Core bike controller firmware
│   ├── src/
│   │   ├── CAN/         # Mainboard CAN task and decoding
│   │   ├── DataLogging/ # Local data logging logic
│   │   ├── HighVoltage/ # Precharge and HV safety logic
│   │   └── main.cpp     # Mainboard entry point
├── include/             # Shared headers (Decoder, Types, Constants)
├── hardware_in_loop/    # HIL testing framework
├── twai_transmit/       # TWAI (CAN) porting examples and tutorials
├── OLD_FIRMWARE/        # Legacy Teensy-based firmware (Reference only)
└── platformio.ini       # Root configuration (often overridden by subfolders)
```

## Directory Purposes

**display/src/DashboardUI/:**
- Purpose: Management of the visual interface.
- Contains: Screen-specific implementations (`DriveScreen.cpp`, `ChargingScreen.cpp`) and shared UI components.
- Key files: `DashboardUI.cpp`, `DashboardShared.h`.

**mainboard/src/HighVoltage/:**
- Purpose: Management of the high-voltage battery system.
- Contains: Precharge sequencing and HV contactor control.
- Key files: `Precharge.cpp`.

**include/:**
- Purpose: Single source of truth for definitions used across all bike hardware.
- Contains: Type definitions, CAN message IDs, and shared parsing logic.
- Key files: `CANDecoder.h`, `Constants.h`, `Types.h`, `Context.h`.

**display/src/lvgl_config/:**
- Purpose: Bridges LVGL to the specific ESP32-S3 hardware used by the dashboard.
- Contains: Display/Touch drivers and frame buffer management.
- Key files: `lvgl_v8_port.cpp`.

## Key File Locations

**Entry Points:**
- `display/src/main.cpp`: Orchestrates dashboard tasks.
- `mainboard/src/main.cpp`: Orchestrates core bike controller tasks.

**Configuration:**
- `include/Config.h`: System-wide configuration toggles.
- `include/Constants.h`: Physical constants (temp limits, voltages).
- `display/platformio.ini`: Library and build settings for the display board.

**Core Logic:**
- `include/CANDecoder.h`: Shared CAN message dispatch and decoding.
- `display/src/CAN/CAN_Receive.cpp`: Updates `DashboardState` from bus traffic.
- `mainboard/src/HighVoltage/Precharge.cpp`: HV safety state machine.

**Testing:**
- `hardware_in_loop/src/TestFiles/`: Logic for simulating bike hardware for firmware validation.

## Naming Conventions

**Files:**
- PascalCase: For UI classes and components (e.g., `DriveScreen.cpp`).
- snake_case: For utility and driver files (e.g., `sd_card.cpp`).

**Directories:**
- PascalCase: For logical modules (e.g., `DashboardUI`).
- snake_case: For system-level or driver modules (e.g., `lvgl_config`).

## Where to Add New Code

**New UI Screen:**
- Implementation: Add `.cpp`/`.h` to `display/src/DashboardUI/`.
- Integration: Update `dashboard_create()` in `display/src/DashboardUI/DashboardUI.cpp`.

**New CAN Message:**
- ID Definition: Add to `include/Constants.h`.
- Decoder: Add `decodeX` function and update `dispatch` in `include/CANDecoder.h`.
- State: Add field to `Context` in `include/Context.h` or `DashboardState` in `display/src/DashboardUI/DashboardUI.h`.

**New Sensor/Actuator Logic:**
- Implementation: Create new subdirectory in `mainboard/src/`.
- Integration: Start a FreeRTOS task in `mainboard/src/main.cpp`.

## Special Directories

**.pio/:**
- Purpose: PlatformIO build artifacts and library downloads.
- Generated: Yes
- Committed: No

**OLD_FIRMWARE/:**
- Purpose: Archive of previous firmware versions and reference material.
- Committed: Yes

---

*Structure analysis: 2025-01-21*
