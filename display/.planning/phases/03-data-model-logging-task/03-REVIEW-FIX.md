---
phase: 03-data-model-logging-task
fixed_at: 2026-04-16T17:12:20Z
review_path: .planning/phases/03-data-model-logging-task/03-REVIEW.md
iteration: 3
findings_in_scope: 5
fixed: 4
skipped: 1
status: partial
---

# Phase 03: Code Review Fix Report (Iteration 3)

**Fixed at:** 2026-04-16T17:12:20Z
**Source review:** .planning/phases/03-data-model-logging-task/03-REVIEW.md
**Iteration:** 3

**Summary:**
- Findings in scope: 5
- Fixed: 4
- Skipped: 1 (WR-01 already applied in prior pass)

## Fixed Issues

### IN-01: `RTC_I2C_TIMEOUT_MS` Macro Name Implies Milliseconds but Holds Ticks

**Files modified:** `src/RTC/rtc.cpp`
**Commit:** `0c0a799`
**Applied fix:** Renamed `RTC_I2C_TIMEOUT_MS` to `RTC_I2C_TIMEOUT_TICKS` at the macro definition and updated all four call sites (`DEV_I2C_Write_Byte`, `DEV_I2C_Write_nByte`, `DEV_I2C_Read_Byte`, `DEV_I2C_Read_nByte`) to use the new name. The macro value (`pdMS_TO_TICKS(50)`) is unchanged.

---

### IN-02: Year-2000 Heuristic in `rtc_init()` Has a False-Positive Edge Case

**Files modified:** `src/RTC/rtc.cpp`
**Commit:** `bd00d02`
**Applied fix:** Replaced the brief comment above the `year == 2000` check with an expanded block that explicitly documents: (1) the PCF85063A has no battery backup so year==2000 always means a power-on reset, (2) the overwrite is unconditional by design, and (3) the theoretical false-positive on an actual 2000-01-01 boot is acceptable for this hardware. The code itself is unchanged.

---

### IN-03: `simulationTask` Writes `hv_cell_voltages` Without the Spinlock Used by Logger

**Files modified:** `src/main.cpp`
**Commit:** `67232cd`
**Applied fix:** Wrapped the cell voltage write loop in `taskENTER_CRITICAL(&g_cell_voltages_mux)` / `taskEXIT_CRITICAL(&g_cell_voltages_mux)`. Also updated the hardcoded loop bound `24` to `CONFIG_HV_CELL_COUNT` for consistency with the WR-01 fix already applied to logger.cpp.

---

### IN-04: `sd_get_spi_mutex()` Returns `nullptr` If Called Before `sd_init()`

**Files modified:** `src/SDCard/sd_card.cpp`
**Commit:** `4f01f08`
**Applied fix:** Expanded `sd_get_spi_mutex()` from a one-liner to a guarded body with `assert(spi_mutex != nullptr && "sd_get_spi_mutex() called before sd_init()")` before the return, so pre-init calls produce an immediate diagnosable crash rather than a silent FreeRTOS fault from `xSemaphoreTake(nullptr, ...)`.

---

## Skipped Issues

### WR-01: Hardcoded Cell Count `24` in `logger.cpp` Should Use `CONFIG_HV_CELL_COUNT`

**File:** `src/Logger/logger.cpp:43`
**Reason:** Already applied in a prior pass (iteration 2, commit a617f6a). Current code at line 43 reads `float cells[CONFIG_HV_CELL_COUNT];` — the fix is fully in place and no change was needed.
**Original issue:** Local snapshot array and `memcpy` were hardcoded to 24 elements instead of using `CONFIG_HV_CELL_COUNT`, creating a silent mismatch risk if pack size changes.

---

_Fixed: 2026-04-16T17:12:20Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 3_
