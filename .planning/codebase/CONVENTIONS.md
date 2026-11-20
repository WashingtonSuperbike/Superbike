# Coding Conventions

**Analysis Date:** 2025-07-22

## Naming Patterns

**Files:**
- Header files: `PascalCase.h` (e.g., `CANDecoder.h`, `DashboardUI.h`) or `snake_case.h` (e.g., `sd_card.h`).
- Implementation files: `PascalCase.cpp` (e.g., `DashboardUI.cpp`) or `snake_case.cpp` (e.g., `rtc.cpp`).
- Some platform-specific files use `snake_case` (e.g., `waveshare_twai_port.cpp`).

**Functions:**
- Generally `camelCase` for actions: `initCAN()`, `decodeMotorStats()`, `drivingSimulationTask()`.
- Static/Internal helpers often use `camelCase`: `canSniff()`, `checkCAN()`.

**Variables:**
- State variables and members: `snake_case` (e.g., `motor_current`, `bms_last_rx_ms`, `stage_start_ms`).
- Local variables: Often `camelCase` or `snake_case` (e.g., `cellVoltageRxMsg`, `sin_t`).
- Instance pointers: `camelCase` (e.g., `canData`, `dashState`).

**Types:**
- Classes and Structs: `PascalCase` (e.g., `DashboardState`, `BMSStatus`, `MotorStats`).
- Enums: `PascalCase` (e.g., `CanStatus`).
- Enum Members: `UPPER_SNAKE_CASE` (e.g., `RECEIVING`).

**Constants and Macros:**
- `UPPER_SNAKE_CASE` (e.g., `CAN_TX_PIN`, `THERMISTOR_COUNT`, `BATT_TEMP_CRIT_CELSIUS`).

## Code Style

**Formatting:**
- Indentation: 4 spaces.
- Braces: Generally on a new line for functions, but can be same line for control structures (inconsistent but leans towards new line for functions).
- Pointer alignment: Space before the asterisk `Type *var`.

**Linting:**
- No automated linting configuration (e.g., `.clang-format`) detected. Standards are maintained manually.

## Import Organization

**Order:**
1. System/Standard libraries (`<Arduino.h>`, `<esp_display_panel.hpp>`).
2. Third-party libraries (`<lvgl.h>`, `<FlexCAN_T4.h>`).
3. Internal headers with relative paths (`"lvgl_config/lvgl_v8_port.h"`, `"CAN/CAN_Receive.h"`).

**Path Aliases:**
- Use of `-I` flags in `platformio.ini` allows flat imports for some directories (e.g., `#include "CANDecoder.h"` from `include/`).

## Error Handling

**Patterns:**
- Return `ESP_OK` or `bool` for success/failure in low-level drivers.
- `assert()` for critical board initialization in `setup()`.
- `Serial.printf()` or `printf()` for logging errors to the console.
- Visual feedback on the display (icons change color/state based on CAN message presence or fault flags).

## Logging

**Framework:** `Serial` or `printf`.

**Patterns:**
- Initialization status: `Serial.println("Initializing RTC")`.
- Error reporting: `printf("Failed to install TWAI driver\n")`.
- Runtime debugging: `Serial.printf("RX ID: 0x%08X ...")`.

## Comments

**When to Comment:**
- Header file documentation for shared logic.
- Complex state transitions or simulation logic.
- TODOs for future improvements.

**JSDoc/TSDoc:**
- Doxygen-style comments used for function documentation in some files:
  ```cpp
  /**
   * Checks the CAN bus for any buffered messages and decodes one into the bike context.
   * 
   * @param canData struct containing pointer to bike context
   */
  ```

## Function Design

**Size:** Tasks are modularized into FreeRTOS tasks (e.g., `dashboardTask`, `logger_task`).

**Parameters:** Pointers to state structures (`DashboardState *state`) are common to allow shared access across tasks.

**Return Values:** Usually `void` for tasks, `bool` or `esp_err_t` for initialization/decoding functions.

## Module Design

**Exports:** Static inline functions in headers for shared decoding logic (`include/CANDecoder.h`).

**Barrel Files:** Not used; direct header inclusion is preferred.

---

*Convention analysis: 2025-07-22*
