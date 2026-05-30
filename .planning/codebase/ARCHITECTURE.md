# Architecture

**Analysis Date:** 2025-01-21

## Pattern Overview

**Overall:** Distributed, Multi-Tasking Embedded System

**Key Characteristics:**
- **Decentralized Control:** Multiple ESP32-S3 microcontrollers (Mainboard, Display) communicating via a shared CAN bus (TWAI).
- **Task-Based Concurrency:** Heavily utilizes FreeRTOS for non-blocking execution of independent subsystems (CAN, UI, SD, Sensors).
- **Shared State Model:** Uses central "Context" structures to pass data between asynchronous tasks within a single node.

## Layers

**Application Layer:**
- Purpose: High-level bike logic and user interface.
- Location: `display/src/DashboardUI/`, `mainboard/src/`
- Contains: Screen implementations, bike state machines, logging logic.
- Depends on: Subsystem Layer, Shared Logic Layer.
- Used by: N/A

**Subsystem Layer:**
- Purpose: Management of specific hardware functions or communication protocols.
- Location: `display/src/CAN/`, `display/src/SDCard/`, `mainboard/src/CAN/`, `mainboard/src/HighVoltage/`
- Contains: TWAI drivers, SD card pollers, precharge logic.
- Depends on: Shared Logic Layer, Hardware Abstraction Layer.
- Used by: Application Layer.

**Shared Logic Layer:**
- Purpose: Consistent data definitions and parsing logic used by multiple boards.
- Location: `include/`
- Contains: `CANDecoder.h` (shared parsing), `Context.h` (state structs), `Constants.h`.
- Depends on: Hardware Abstraction Layer (types only).
- Used by: Subsystem Layer, Application Layer.

**Hardware Abstraction Layer (HAL):**
- Purpose: Interaction with physical hardware.
- Location: `display/src/lvgl_config/`, `display/lib/`
- Contains: LVGL porting, ESP-IDF drivers (`twai.h`, `gpio.h`), Board-specific initialization.
- Depends on: Vendor libraries (ESP-IDF, Arduino).
- Used by: Subsystem Layer.

## Data Flow

**CAN Bus Telemetry Flow:**

1. **Reception:** The `twai_recv` task (on Display) or `canTask` (on Mainboard) receives a raw frame from the CAN bus.
2. **Decoding:** The frame is passed to `CANDecoder::dispatch`, which uses shared static inline functions to parse the payload into engineering units.
3. **State Update:** Decoded values are written to the central state struct (`DashboardState` or `Context`). Thread safety for multi-byte values (like cell voltages) is managed via `portMUX_TYPE` spinlocks.
4. **Consumption:**
   - **UI:** The `dash_refresh` task reads `DashboardState` and updates LVGL widgets.
   - **Logging:** The `logger_task` reads the state and writes CSV rows to the SD card.

**State Management:**
- **Centralized State:** Data is held in a single struct per board (`DashboardState` for display, `Context` for mainboard).
- **Thread Safety:** Uses `std::atomic` for simple status flags and FreeRTOS critical sections/spinlocks for arrays (e.g., `BatteryVoltages.cell_voltages`).

## Key Abstractions

**CANDecoder:**
- Purpose: Shared namespace for consistent decoding of CAN messages across different hardware nodes.
- Examples: `include/CANDecoder.h`
- Pattern: Strategy/Static Dispatch.

**DashboardUI:**
- Purpose: Separates UI layout from data logic.
- Examples: `display/src/DashboardUI/DriveScreen.cpp`, `display/src/DashboardUI/ChargingScreen.cpp`
- Pattern: Screen-based separation of concerns.

## Entry Points

**Display Mainboard:**
- Location: `display/src/main.cpp`
- Triggers: Power on / Reset.
- Responsibilities: Initializes LCD, Touch, SD, RTC, LVGL, and starts the FreeRTOS task scheduler for UI and communication.

**Main Board:**
- Location: `mainboard/src/main.cpp` (Implementation mostly in `mainboard/src/CAN/CAN.cpp` tasks)
- Triggers: Power on / Reset.
- Responsibilities: Core bike control, HV precharge management, and sensor aggregation.

## Error Handling

**Strategy:** Severity-based Escalation

**Patterns:**
- **CAN Watchdogs:** Timestamps (`motor_last_rx_ms`, etc.) are checked by the UI task to detect lost communication.
- **Global Error List:** The display maintains an `ErrorList` that aggregates active faults from BMS, Motor Controller, and Mainboard.

## Cross-Cutting Concerns

**Logging:** Handled by a dedicated `logger_task` in `display/src/Logger/` writing to SD card.
**Validation:** Shared constants in `include/Constants.h` define safety limits used by both boards.
**Authentication:** Not applicable (Physical bus access assumed).

---

*Architecture analysis: 2025-01-21*
