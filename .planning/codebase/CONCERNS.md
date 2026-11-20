# Codebase Concerns

**Analysis Date:** 2025-02-13

## Tech Debt

**Mainboard Firmware Implementation:**
- Issue: `mainboard/src/main.cpp` is a stub and the firmware logic seems incomplete or fragmented.
- Files: `mainboard/src/main.cpp`
- Impact: Mainboard functionality is not yet fully functional or integrated.
- Fix approach: Port relevant logic from `OLD_FIRMWARE` or implement the missing state machine and task orchestration in `mainboard/src/main.cpp`.

**Unverified Pin Configurations:**
- Issue: Numerous TODOs indicate that GPIO pin numbers for the ESP32-S3 board have not been verified and still reference legacy Teensy pins.
- Files: `mainboard/src/GPIO/Pins.h`
- Impact: High risk of hardware malfunction or damage if incorrect pins are toggled.
- Fix approach: Audit the PCB schematic and update `Pins.h` with the correct ESP32-S3 GPIO mappings.

**Legacy Codebase (OLD_FIRMWARE):**
- Issue: The repository contains a large amount of legacy Teensy-based code and bundled libraries that are no longer actively used but add noise to searches and exploration.
- Files: `OLD_FIRMWARE/`
- Impact: Increased maintenance burden, confusing for new contributors, and bloats the repository.
- Fix approach: Archive the legacy code into a separate branch or repository and remove it from the main branch.

**Bundled External Libraries:**
- Issue: Multiple external libraries (LVGL, ESP32_Display_Panel, etc.) are bundled directly in the `display/lib` directory instead of being managed via a package manager like PlatformIO's library manager.
- Files: `display/lib/lvgl/`, `display/lib/ESP32_Display_Panel/`, `display/lib/ESP32_IO_Expander/`
- Impact: Difficult to update libraries and leads to potential version conflicts.
- Fix approach: Move library management to `platformio.ini` where possible.

## Known Bugs

**CAN Bus Initialization Hack:**
- Issue: `canTask` contains a check for `CAN_NODES != 0` because sending messages with zero nodes on the bus "breaks" things.
- Files: `mainboard/src/CAN/CAN.cpp`
- Impact: Fragile initialization logic that depends on a manual configuration constant.
- Fix approach: Implement robust TWAI error handling that can detect and recover from "No ACK" errors without crashing the task.

**Persistent Debug Flags:**
- Issue: `DEBUG_CHARGING_SCREEN_ONLY` is currently enabled by default, which overrides normal dashboard behavior and forces the charging screen to display.
- Files: `include/Config.h`
- Impact: Unexpected behavior in production builds if the developer forgets to comment it out.
- Fix approach: Move debug flags to a non-committed `local_config.h` or use build environment variables.

## Security Considerations

**Unprotected Shared State:**
- Issue: While some shared state uses spinlocks (e.g., `g_cell_voltages_mux`), other parts of `DashboardState` might be accessed concurrently without protection.
- Files: `display/src/main.cpp`, `include/Types.h`
- Current mitigation: Some use of `std::atomic` for `can_status`, and a mutex for cell voltages.
- Recommendations: Perform a thread-safety audit on `DashboardState` and ensure all multi-task access is protected by mutexes or atomics.

## Performance Bottlenecks

**O(N) Cell Voltage Summation:**
- Issue: The `decipherCellsVoltage` function recalculates the sum of all 24 cells every time a message containing just 4 cells arrives.
- Files: `include/CANDecoder.h`
- Cause: Repeated $O(N)$ calculation in a high-frequency CAN interrupt/polling context.
- Improvement path: Maintain a running sum or only update the total `hv_series_voltage` once all cells have been updated (`cell_voltages_ready`).

**LVGL Refresh Rate:**
- Issue: `dashboardTask` runs at a fixed `REFRESH_RATE_HZ` (30Hz) and calls `dashboard_refresh` which performs string formatting and UI updates regardless of whether data has changed.
- Files: `display/src/DashboardUI/DashboardUI.cpp`
- Cause: High CPU usage from unnecessary UI redraws and string operations.
- Improvement path: Implement "dirty checking" or only refresh specific UI elements when their underlying data actually changes.

## Fragile Areas

**CAN Error Recovery:**
- Issue: The display board's TWAI recovery for `ERR_PASS` involves a full driver uninstall and reinstall.
- Files: `display/src/CAN/CAN_Receive.cpp`
- Why fragile: This is a heavy-handed recovery mechanism that might drop messages or cause instability during the re-initialization phase.
- Safe modification: Investigate if `twai_initiate_recovery()` is sufficient for `ERR_PASS` or if a more surgical recovery is possible.

**SD Card Logging Reliability:**
- Issue: The CSV logger flushes every 10 seconds.
- Files: `display/src/Logger/logger.cpp`
- Why fragile: Power loss or card removal can lead to losing up to 10 seconds of critical telemetry data.
- Safe modification: Reduce flush interval or implement a more robust circular buffer/emergency flush on power-down (if hardware supports it).

## Missing Critical Features

**Unit Testing Framework:**
- Problem: There are no unit tests for core logic like `CANDecoder` or data filters.
- Blocks: Verification of decoding logic without physical hardware or complex HIL setups.
- Fix: Integrate a testing framework (e.g., Unity, which is standard for PlatformIO) and add tests for `include/CANDecoder.h`.

**Consistent Logging Framework:**
- Problem: The codebase uses a mix of `Serial.printf`, `printf`, and `Serial.println` for debugging.
- Blocks: Easy redirection of logs to SD card, Serial, or a remote server.
- Fix: Implement a unified logger with severity levels (INFO, WARN, ERROR).

## Test Coverage Gaps

**CAN Decoding:**
- What's not tested: Parsing logic for various CAN messages.
- Files: `include/CANDecoder.h`
- Risk: Incorrectly parsed battery or motor data could lead to wrong dashboard displays or unsafe control decisions.
- Priority: High

---

*Concerns audit: 2025-02-13*
