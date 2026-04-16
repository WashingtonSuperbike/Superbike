---
phase: 03-data-model-logging-task
reviewed: 2026-04-15T00:00:00Z
depth: standard
files_reviewed: 5
files_reviewed_list:
  - display/src/Logger/logger.h
  - display/src/Logger/logger.cpp
  - display/src/SDCard/sd_card.h
  - display/src/SDCard/sd_card.cpp
  - display/src/main.cpp
findings:
  critical: 1
  warning: 4
  info: 3
  total: 8
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-04-15
**Depth:** standard
**Files Reviewed:** 5
**Status:** issues_found

## Summary

Five files were reviewed: the new Logger module (`logger.h`, `logger.cpp`), the SD card driver (`sd_card.h`, `sd_card.cpp`), and `main.cpp`. The implementation is well-structured with clear mutex discipline and a correct state-machine approach to card insertion/removal. One critical race condition was found on `DashboardState::sd_started` — the field is declared `volatile bool` but is written from `sd_poll_task` (Core 0) and read from `logger_task` (Core 0) without any memory barrier or lock, which is insufficient on a multicore system where both tasks may run on the same core but the compiler can still reorder or cache reads. More importantly, `simulationTask` runs on Core 0 and explicitly does NOT touch `sd_started`, but `dashboardTask` runs on Core 1 and reads the same struct — so `sd_started` crosses core boundaries in practice. Four warnings cover: a missing mutex-take failure handling that leaves `file_open = true` on card-removal detection, a header column count mismatch, a macro redefinition risk, and a stale comment in the mutex guard on `do_mount`. Three info items cover minor style and dead-code observations.

## Critical Issues

### CR-01: `sd_started` Written and Read Across Cores Without Atomic Protection

**File:** `display/src/SDCard/sd_card.cpp:127`
**Issue:** `state->sd_started = mounted` is written from `sd_poll_task` pinned to Core 0. `logger_task` reads `state->sd_started` (line 108 of `logger.cpp`) also pinned to Core 0, but `dashboardTask` reads `DashboardState` on Core 1 (main.cpp line 137-145). `volatile bool` prevents compiler optimization but does NOT provide a memory barrier between ESP32-S3 cores. A Core 1 read of `sd_started` may see a stale value due to cache incoherence, and the write from Core 0 is not guaranteed to be visible to Core 1 without an explicit memory barrier or FreeRTOS primitive.

Even within Core 0, `volatile` is not the right tool for inter-task communication — FreeRTOS documents that `volatile` alone is not sufficient and recommends using `atomic` operations or FreeRTOS queues/event groups for shared flags.

**Fix:** Replace the raw `volatile bool` with either an `atomic<bool>` (C++17, already enabled) or a FreeRTOS event group bit. The minimal fix using C++ atomics:

```cpp
// In DashboardState (DashboardUI.h):
#include <atomic>
std::atomic<bool> sd_started{false};

// In sd_card.cpp — unchanged, atomic<bool> assignment is safe:
state->sd_started = mounted;

// In logger.cpp — unchanged, atomic<bool> load is safe:
bool sd_now = state->sd_started.load();
```

`std::atomic<bool>` on Xtensa generates a memory barrier that ensures both cores see a consistent value without any performance cost for a single bool.

---

## Warnings

### WR-01: Card-Removal Branch Does Not Set `file_open = false` When Mutex Take Fails

**File:** `display/src/Logger/logger.cpp:130-140`
**Issue:** When the removal transition (`prev_sd_started && !sd_now`) is detected and `file_open` is true, the code attempts `xSemaphoreTake(mtx, pdMS_TO_TICKS(50))`. If the take fails (returns `pdFALSE`), the code falls through without closing `logFile` and without setting `file_open = false`. On the next iteration `sd_now` is still false and `prev_sd_started` is now false, so the removal branch will not fire again. The subsequent write-row branch checks `file_open && sd_now` — since `sd_now` is false the write won't run either. The file handle is effectively leaked until the next insertion event (which calls `logFile = sd_get_fs().open(...)`, implicitly abandoning the old handle without closing it).

**Fix:** Set `file_open = false` unconditionally after the removal is detected, even if the mutex take fails. If the close cannot be done immediately, skip it (the card is already gone) rather than leaving `file_open` true:

```cpp
if (prev_sd_started && !sd_now) {
    if (file_open) {
        SemaphoreHandle_t mtx = sd_get_spi_mutex();
        if (xSemaphoreTake(mtx, pdMS_TO_TICKS(50)) == pdTRUE) {
            logFile.close();
            xSemaphoreGive(mtx);
        }
        // Always clear file_open — card is gone regardless of mutex result
        file_open = false;
        Serial.println("CSV log: card removed, file closed");
    }
}
```

---

### WR-02: CSV Header Has 42 Columns But Format String Writes Different Count

**File:** `display/src/Logger/logger.cpp:33-43` and `62-67`
**Issue:** The `HEADER[]` string (lines 33-42) contains the following columns when counted:

- Row 1: `elapsed_ms, speed_mph, motor_rpm, motor_current_A, motor_temp_C, mc_temp_C` = 6
- Row 2: `bms_voltage_V, aux_voltage_V` = 2
- Cell rows (3 rows of 8): `cell_01_V` through `cell_24_V` = 24
- Thermistor rows: `thermistor_01_C` through `thermistor_10_C` = 10

Total header columns: **42**.

The file comment says "34-column" (line 29 and header comment in `logger.h`). The `writeRow` format string in lines 62-67 has: 1 (`%lu`) + 7 (`%.2f` × 6 + `%.2f`) + 24 cell floats + 10 thermistor floats = **42 format specifiers**, which matches the actual header. The "34-column" comment is stale from an earlier design and is misleading.

This is a documentation/comment mismatch, not a runtime bug — the data written is self-consistent. But the wrong column count in comments could cause confusion when the CSV is parsed by external tooling that relies on the documentation.

**Fix:** Update the comments to "42-column":

```cpp
// writeHeader — 42-column CSV header row (LOG-02, LOG-03)
// writeRow — format one 42-column CSV data row (LOG-03, LOG-04)
```

And update the corresponding doc comment in `logger.h`.

---

### WR-03: Macro Redefinition of `GEAR_RATIO`, `WHEEL_DIAM_M`, `MPH_CONVERT` Is Fragile

**File:** `display/src/Logger/logger.cpp:18-26`
**Issue:** `logger.cpp` uses `#ifndef GEAR_RATIO / #define GEAR_RATIO` guards to re-define three constants that are already defined as macros in `DashboardUI.h` (lines 25-27 of that file), which is included transitively via `DashboardUI/DashboardUI.h`. Because `DashboardUI.h` is included earlier in the translation unit, the `#ifndef` guards will silently discard the re-definitions in `logger.cpp` — so the values are actually coming from `DashboardUI.h`. This is correct behavior today, but the duplicated definitions create a maintenance hazard: if the values in `DashboardUI.h` change but `logger.cpp`'s copy is not updated, the `#ifndef` guards will hide the divergence without any compile error or warning.

**Fix:** Remove the duplicate definitions from `logger.cpp` entirely and rely solely on the values from `DashboardUI.h`:

```cpp
// Remove lines 15-26 from logger.cpp:
// #ifndef GEAR_RATIO
// #define GEAR_RATIO   (48.0f / 16.0f)
// ...
// #endif
// The constants are already defined in DashboardUI.h which is included above.
```

---

### WR-04: `do_mount()` Takes the SPI Mutex Internally, But Callers Cannot Tell If Failure Was Due to Mutex or Card Absence

**File:** `display/src/SDCard/sd_card.cpp:56-112`
**Issue:** `do_mount()` returns `false` both when the mutex cannot be acquired (line 63) and when the card is genuinely absent. The caller `sd_poll_task` (line 124) treats both cases identically — it sets `state->sd_started = false`. This means a transient mutex contention event (e.g., logger_task holding the mutex slightly longer than 50ms) will cause `sd_poll_task` to report the card as unmounted even if it is physically present. On the next `sd_poll_task` iteration, `do_mount()` will be called again and the state will correct itself, but in between there is a spurious `sd_started = false` which triggers `logger_task` to close the current log file (via the removal branch), losing that file handle permanently.

At 500ms poll intervals and a 50ms mutex timeout, this scenario is unlikely but not impossible under heavy SdFat write activity.

**Fix:** Either return a tri-state from `do_mount` (mounted / unmounted / busy), or use a longer mutex timeout in `do_mount` that exceeds the worst-case SdFat write time:

```cpp
// Option A: increase timeout to 200ms to survive a full SdFat sector write
if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
    return false;  // still busy; state unchanged this cycle
}
```

Alternatively, if `do_mount` cannot take the mutex, simply skip the poll and return the previous known state rather than returning false.

---

## Info

### IN-01: Stale Comment References `Logging/logging.cpp` (Wrong Path)

**File:** `display/src/DashboardUI/DashboardUI.h:108`
**Issue:** The comment reads "Defined in CAN_Receive.cpp; used in `Logging/logging.cpp`." The logging module now lives at `Logger/logger.cpp`, not `Logging/logging.cpp`. This is a leftover from the refactor described in commit `a35850b`.

**Fix:**
```cpp
// Defined in CAN_Receive.cpp; used in Logger/logger.cpp.
extern portMUX_TYPE g_cell_voltages_mux;
```

---

### IN-02: `SD_DUMMY_GPIO` and `SD_EXPANDER_CS_PIN` Share the Same Value — Risk of Future Collision

**File:** `display/src/SDCard/sd_card.cpp:12-20`
**Issue:** Both `SD_EXPANDER_CS_PIN` (expander logical I/O pin 4) and `SD_DUMMY_GPIO` (ESP32-S3 GPIO 4) are defined as `4`. The comment explains this is coincidental and that they refer to entirely different hardware resources. While this is technically correct, it creates a cognitive hazard for anyone modifying pin assignments — changing one may look like it changes the other.

**Fix:** Add a `static_assert` to catch any future divergence that would make `SD_DUMMY_GPIO` no longer a safe, unconnected GPIO:

```cpp
// Ensure these are numerically equal only by coincidence (different namespaces)
static_assert(SD_DUMMY_GPIO == 4, "Verify SD_DUMMY_GPIO is still unconnected on this board rev");
```

Or give them distinct names with a clear comment that the numeric equality is intentional and board-revision-specific.

---

### IN-03: `simulationTask` Modifies `dashState` Without Any Synchronization Against `dashboardTask` (Core 1)

**File:** `display/src/main.cpp:33-82`
**Issue:** `simulationTask` (Core 0) writes to `dashState` fields (e.g., `dashState.motor.RPM`, `dashState.battery.hv_cell_voltages[]`) without any mutex or atomic. `dashboardTask` (Core 1) reads `dashState`. For a simulation/demo context this is an acceptable trade-off (torn reads produce only visual glitches, not data corruption or safety issues), but the cell voltage array is protected by `g_cell_voltages_mux` on the CAN path while the simulation path bypasses that spinlock entirely.

This inconsistency means the spinlock in `logger.cpp:57-59` may not actually protect against races with the simulation task — only against the CAN task.

**Fix (for the sim path):** Either hold the spinlock when writing cell voltages from `simulationTask`, or document explicitly that the spinlock protects only CAN vs. logger contention and the simulation is assumed to be mutually exclusive with the CAN path at runtime:

```cpp
// In simulationTask, when writing hv_cell_voltages:
taskENTER_CRITICAL(&g_cell_voltages_mux);
for (int i = 0; i < 24; i++) {
    dashState.battery.hv_cell_voltages[i] = 3.7f + 0.4f * fabsf(sinf(t + i * 0.25f));
}
taskEXIT_CRITICAL(&g_cell_voltages_mux);
```

---

_Reviewed: 2026-04-15_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
