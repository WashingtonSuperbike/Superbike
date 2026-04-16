---
phase: 03-data-model-logging-task
plan: 01
subsystem: display/Logger, display/SDCard, display/main
tags: [logging, sd-card, freertos, csv, data-model]
requirements_satisfied: [MODEL-01, LOG-01, LOG-02, LOG-03, LOG-04, LOG-05, LOG-06]

dependency_graph:
  requires:
    - display/src/SDCard/sd_card.h (SdFs instance, SPI mutex)
    - display/src/RTC/rtc.h (rtc_get_filename)
    - display/src/DashboardUI/DashboardUI.h (DashboardState, hv_cell_voltages)
  provides:
    - display/src/Logger/logger.h (logger_task entry point)
    - display/src/Logger/logger.cpp (full CSV logging implementation)
    - sd_get_fs() / sd_get_spi_mutex() accessors in sd_card.h/cpp
  affects:
    - display/src/main.cpp (simulation task enabled, logger_task spawned)

tech_stack:
  added:
    - SdFat FsFile direct access via sd_get_fs() accessor
    - FreeRTOS SemaphoreHandle_t SPI mutex in sd_card.cpp
  patterns:
    - Mutex-guarded SdFat I/O (xSemaphoreTake/Give around every SdFat call)
    - State-machine transition detection (prev_sd_started / sd_now)
    - Stack-buffer snprintf row formatting (512 bytes)

key_files:
  created:
    - display/src/Logger/logger.h
    - display/src/Logger/logger.cpp
  modified:
    - display/src/SDCard/sd_card.h (sd_get_fs, sd_get_spi_mutex declarations)
    - display/src/SDCard/sd_card.cpp (spi_mutex static, do_mount mutex guards, accessors)
    - display/src/main.cpp (simulationTask uncommented + cell voltages, logger_task spawn)

decisions:
  - "sd_get_fs() returns SdFs& reference — logger opens FsFile directly without duplicating SdFat object"
  - "spi_mutex created in sd_init() before any SPI ops; do_mount() acquires with 50ms timeout"
  - "loggingTask (Logging/) replaced by logger_task (Logger/) in main.cpp spawn"
  - "simulationTask uncommented and cell voltage loop added (24 cells, 3.3-4.1V sine sweep)"

metrics:
  duration: ~1 session
  completed: 2026-04-15
  tasks_completed: 3
  tasks_total: 3
  files_created: 2
  files_modified: 3
  files_deleted: 2
---

# Phase 03 Plan 01: Data Model Extension + Logger Module Summary

**One-liner:** SPI mutex added to sd_card, sd_get_fs/sd_get_spi_mutex accessors exposed, Logger module (logger.h/cpp) implements RTC-named CSV files with 34-column header and 20 Hz writes using direct SdFat FsFile access.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Extend data model + SD card accessors | 5c43baf | sd_card.h, sd_card.cpp, main.cpp |
| 2 | Implement Logger module | 8ffc453 | Logger/logger.h, Logger/logger.cpp, main.cpp |

## Task 3: Hardware Verification — PASSED

Verified on physical hardware by user (2026-04-15):

## What Was Built

### Task 1: Extend data model + SD card accessors

- `DashboardBatteryVoltages.hv_cell_voltages[24]` already existed via `CONFIG_HV_CELL_COUNT` (MODEL-01 pre-satisfied from prior work).
- Added `static SemaphoreHandle_t spi_mutex` to `sd_card.cpp`; created in `sd_init()` before any SPI operations.
- Wrapped all `do_mount()` SdFat I/O with `xSemaphoreTake(spi_mutex, 50ms)` / `xSemaphoreGive` (D-04, T-03-01).
- Added `SdFs& sd_get_fs()` and `SemaphoreHandle_t sd_get_spi_mutex()` implementations and declarations.
- Uncommented `simulationTask` spawn in `main.cpp`; added 24-cell voltage animation loop: `3.7f + 0.4f * fabsf(sinf(t + i * 0.25f))` (D-03, MODEL-01).

### Task 2: Logger module

- Created `display/src/Logger/logger.h` — declares `logger_task(void *param)`.
- Created `display/src/Logger/logger.cpp` — `writeHeader()` writes 34-column header row matching LOG-03; `writeRow()` formats all telemetry fields via `snprintf` into a 512-byte stack buffer; `logger_task()` detects `sd_started` transitions, acquires SPI mutex, opens RTC-named file on mount, writes at 20 Hz, flushes every 10 s, closes cleanly on removal.
- Updated `main.cpp`: replaced `#include "Logging/logging.h"` with `#include "Logger/logger.h"`, replaced `loggingTask` spawn with `logger_task` spawn on Core 0.
- Build passes with zero errors (Flash: 16.8%, RAM: 28.2%).

## Post-Verification Improvements

### Logging module consolidation
`Logging/logging.h` + `logging.cpp` (camelCase `loggingTask`, narrow file API) deleted. Dead narrow API functions (`sd_open_log_file`, `sd_write_log_line`, `sd_sync_log_file`, `sd_close_log_file`, `s_log_file`) removed from `sd_card`. `csv_log` task stack corrected 4096→8192 (snprintf+SdFat cache headroom). Commit: `a35850b`.

### SD removal detection lag fix (root cause analysis)
**Symptom:** Icon took ~2s to turn red after card removal.
**Root cause:** `logger_task`'s failed write (~300ms SPI timeout) sets `sd.card()->errorCode()` non-zero before `sd_poll_task` runs. `do_mount()` saw `errorCode != 0`, skipped the fast `readCID` path, and fell into `sd.begin()` which blocks ~2s with no card present.
**Fix 1 — `do_mount()`:** When `errorCode != 0` on an already-mounted card, return `false` immediately and reset `s_sd_ever_began` (avoids `sd.begin()` timeout).
**Fix 2 — `writeRow()`:** Returns `bool`; on write failure `logger_task` closes the file immediately and releases the SPI mutex so `sd_poll_task` can acquire it within the next poll cycle.
**Result:** Removal detection ~500ms instead of ~2s. Commit: `afbc910`.

## Deviations from Plan

### Pre-existing: hv_cell_voltages[24] already in DashboardUI.h

- **Found during:** Task 1 assessment
- **Issue:** The struct already had `float hv_cell_voltages[CONFIG_HV_CELL_COUNT]` (CONFIG_HV_CELL_COUNT=24) from the CAN receive path added in a prior phase.
- **Fix:** No change needed to struct. Proceeded directly to adding simulation animation and SD accessors.

## Known Stubs

None — all data flows from simulationTask (simulation mode) to logger_task to SD card. No placeholder values in the logging path.

## Threat Surface Scan

All mitigations in the plan's threat register were applied:

| Threat | Mitigation Applied |
|--------|--------------------|
| T-03-01 (SPI bus DoS) | `spi_mutex` guards all SdFat calls in both `do_mount()` and `logger_task` |
| T-03-03 (card removal mid-write) | `prev_sd_started -> !sd_now` transition closes `logFile` cleanly |

## Self-Check: PASSED

- `display/src/Logger/logger.h` — FOUND
- `display/src/Logger/logger.cpp` — FOUND
- `display/src/SDCard/sd_card.h` — contains `sd_get_fs`, `sd_get_spi_mutex` — FOUND
- `display/src/SDCard/sd_card.cpp` — contains `spi_mutex` — FOUND
- Commit 5c43baf — FOUND (feat(03-01): extend data model + SD card accessors)
- Commit 8ffc453 — FOUND (feat(03-01): implement Logger module)
- Build: SUCCESS (platformio run -e display)
