---
phase: 03-data-model-logging-task
reviewed: 2026-04-16T00:00:00Z
depth: standard
files_reviewed: 6
files_reviewed_list:
  - src/Logger/logger.h
  - src/Logger/logger.cpp
  - src/SDCard/sd_card.h
  - src/SDCard/sd_card.cpp
  - src/main.cpp
  - src/RTC/rtc.cpp
findings:
  critical: 0
  warning: 1
  info: 4
  total: 5
status: issues_found
---

# Phase 03: Code Review Report (Iteration 2)

**Reviewed:** 2026-04-16
**Depth:** standard
**Files Reviewed:** 6
**Status:** issues_found

## Summary

Six files were reviewed: the Logger module (`logger.h`, `logger.cpp`), SD card driver
(`sd_card.h`, `sd_card.cpp`), `main.cpp`, and the newly added RTC driver (`rtc.cpp`).
This is the second review pass on this phase. All four fixes from the prior review
(CR-01 atomic `sd_started`, WR-02 column count comments, WR-03 duplicate macros,
WR-04 mutex timeout) are confirmed applied and correct.

One new warning was found in `logger.cpp`: the cell-voltage local array is hardcoded
to 24 elements rather than using `CONFIG_HV_CELL_COUNT`, creating a silent mismatch
risk if the pack size ever changes. Four info items cover the new `rtc.cpp` file and
two carry-over items from the prior review that remain unfixed and are still relevant.

No critical issues were found.

## Warnings

### WR-01: Hardcoded Cell Count `24` in `logger.cpp` Should Use `CONFIG_HV_CELL_COUNT`

**File:** `src/Logger/logger.cpp:43`
**Issue:** The local snapshot array and `memcpy` are hardcoded to 24 elements:

```cpp
float cells[24];
taskENTER_CRITICAL(&g_cell_voltages_mux);
memcpy(cells, state.battery.hv_cell_voltages, sizeof(cells));
taskEXIT_CRITICAL(&g_cell_voltages_mux);
```

`hv_cell_voltages` is declared as `float hv_cell_voltages[CONFIG_HV_CELL_COUNT]`
in `DashboardUI.h`. `CONFIG_HV_CELL_COUNT` is currently 24, so the values match
today. However, if `CONFIG_HV_CELL_COUNT` is increased (e.g., to a larger pack),
`sizeof(cells)` will be smaller than `sizeof(state.battery.hv_cell_voltages)` —
the `memcpy` silently under-copies and the trailing cells are not logged. If it is
decreased, the `memcpy` reads past the array end (undefined behaviour). The mismatch
will produce no compile error or warning.

**Fix:** Replace the literal with the constant and update the `snprintf` format
string to match:

```cpp
float cells[CONFIG_HV_CELL_COUNT];
taskENTER_CRITICAL(&g_cell_voltages_mux);
memcpy(cells, state.battery.hv_cell_voltages, sizeof(cells));
taskEXIT_CRITICAL(&g_cell_voltages_mux);
```

Note: the `snprintf` format string on lines 52-54 also hardcodes 24 `%.2f`
specifiers and the header string hardcodes 24 column names. Both would also
need updating if `CONFIG_HV_CELL_COUNT` changes. Given this coupling, consider
generating the cell columns in a loop rather than unrolling them statically if
the cell count is expected to vary across pack configurations.

---

## Info

### IN-01: `RTC_I2C_TIMEOUT_MS` Macro Name Implies Milliseconds but Holds Ticks

**File:** `src/RTC/rtc.cpp:20`
**Issue:**
```cpp
#define RTC_I2C_TIMEOUT_MS  pdMS_TO_TICKS(50)
```
The `_MS` suffix implies the value is in milliseconds, but `pdMS_TO_TICKS(50)` has
already converted to FreeRTOS ticks. A reader passing this macro to a function that
also converts ms-to-ticks would double-convert. All four I2C helper functions pass
it directly to `i2c_master_write_to_device` / `i2c_master_write_read_device`, which
accept `TickType_t` — so the usage is currently correct. The hazard is future callers
misreading the name.

**Fix:** Rename to `RTC_I2C_TIMEOUT_TICKS` to accurately reflect the stored type:
```cpp
#define RTC_I2C_TIMEOUT_TICKS  pdMS_TO_TICKS(50)
```

---

### IN-02: Year-2000 Heuristic in `rtc_init()` Has a False-Positive Edge Case

**File:** `src/RTC/rtc.cpp:167`
**Issue:**
```cpp
if (err == ESP_OK && now.year == 2000) {
```
The PCF85063A resets to its power-on epoch (2000-01-01) when it loses power, so
detecting `year == 2000` is a reasonable proxy for "uninitialized." The edge case
is that a valid RTC time of exactly 2000-01-01 00:00:xx would be overwritten with
compile-time on every boot. On this hardware (no battery backup), the chip resets
on every power cycle, so the heuristic will always trigger and the RTC will always
be written from compile-time — which is the intended behaviour. The edge case is
theoretical for this design, but worth documenting in the comment to prevent a
future maintainer from misunderstanding the logic.

**Fix:** Expand the comment to make the design decision explicit:
```cpp
// PCF85063A has no battery backup, so year == 2000 always means the chip just
// reset to its power-on default. We overwrite it unconditionally with compile-time.
// (The false-positive if someone actually ran this on 2000-01-01 is acceptable.)
if (err == ESP_OK && now.year == 2000) {
```

---

### IN-03: `simulationTask` Writes `hv_cell_voltages` Without the Spinlock Used by Logger

**File:** `src/main.cpp:72-74`
**Issue:** The simulation task writes to `dashState.battery.hv_cell_voltages[]` on
Core 0 without holding `g_cell_voltages_mux`:
```cpp
for (int i = 0; i < 24; i++) {
    dashState.battery.hv_cell_voltages[i] = 3.7f + 0.4f * fabsf(sinf(t + i * 0.25f));
}
```
`logger_task` reads `hv_cell_voltages` while holding `g_cell_voltages_mux` (logger.cpp
lines 44-46). The CAN receive path (`CAN_Receive.cpp`) also acquires the spinlock when
writing. The simulation task bypasses it, so the spinlock only protects logger vs. CAN
— not logger vs. simulation. In a build where both `simulationTask` and `logger_task`
run concurrently (e.g., during bench testing), a torn read of the cell array is
possible.

**Fix:** Wrap the simulation's cell voltage writes in the spinlock, or add a comment
making the mutual-exclusion assumption explicit (CAN and sim are never both active):
```cpp
taskENTER_CRITICAL(&g_cell_voltages_mux);
for (int i = 0; i < 24; i++) {
    dashState.battery.hv_cell_voltages[i] = 3.7f + 0.4f * fabsf(sinf(t + i * 0.25f));
}
taskEXIT_CRITICAL(&g_cell_voltages_mux);
```

---

### IN-04: `sd_get_spi_mutex()` Returns `nullptr` If Called Before `sd_init()`

**File:** `src/SDCard/sd_card.cpp:115`
**Issue:**
```cpp
static SemaphoreHandle_t spi_mutex = nullptr;
SemaphoreHandle_t sd_get_spi_mutex() { return spi_mutex; }
```
If any code calls `sd_get_spi_mutex()` and then `xSemaphoreTake(nullptr, ...)` before
`sd_init()` runs, FreeRTOS will crash. In the current `setup()` ordering, `sd_init()`
is called before task creation — so the tasks that use the mutex are only started after
it is valid. The risk is that a future refactor changes the initialization order.

**Fix:** Add an assertion in `sd_get_spi_mutex()` to catch early use:
```cpp
SemaphoreHandle_t sd_get_spi_mutex() {
    assert(spi_mutex != nullptr && "sd_get_spi_mutex() called before sd_init()");
    return spi_mutex;
}
```

---

_Reviewed: 2026-04-16_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
