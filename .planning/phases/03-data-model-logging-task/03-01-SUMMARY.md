---
phase: 03-data-model-logging-task
plan: 01
subsystem: data-model
tags: [freertos, sdfat, can, esp32s3, spinlock]

requires: []
provides:
  - hv_cell_voltages[24] array in DashboardBatteryVoltages with thread-safe CAN population
  - g_cell_voltages_mux spinlock (defined in CAN_Receive.cpp, extern in DashboardUI.h)
  - Narrow SD file API: sd_open_log_file, sd_write_log_line, sd_sync_log_file, sd_close_log_file
affects: [03-02-logging-task]

tech-stack:
  added: []
  patterns: [narrow-api, spinlock-critical-section, private-static-file-handle]

key-files:
  created: []
  modified:
    - display/src/DashboardUI/DashboardUI.h
    - display/src/CAN/CAN_Receive.cpp
    - display/src/SDCard/sd_card.h
    - display/src/SDCard/sd_card.cpp

key-decisions:
  - "Added bounds check (i + totalOffset < CONFIG_HV_CELL_COUNT) per T-03-03 — not in original plan but required for safety"
  - "Added freertos/FreeRTOS.h include to DashboardUI.h to resolve portMUX_TYPE"
  - "Sum loop left outside critical section — CAN task owns both write and sum, no race"

requirements-completed: [MODEL-01, LOG-01, LOG-02, LOG-05]

duration: 10min
completed: 2026-04-15
---

# Phase 03 Plan 01: Data Model Extension + Narrow SD File API Summary

**hv_cell_voltages[24] added to DashboardBatteryVoltages with g_cell_voltages_mux spinlock, and four-function narrow file API (sd_open/write/sync/close_log_file) added to sd_card module with private FsFile**

## Performance

- **Duration:** ~10 min
- **Started:** 2026-04-15T00:00:00Z
- **Completed:** 2026-04-15T00:10:00Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Extended `DashboardBatteryVoltages` with `float hv_cell_voltages[CONFIG_HV_CELL_COUNT]`
- Fixed `decipherCellsVoltage` to write into `battery->hv_cell_voltages[]` under `g_cell_voltages_mux` critical section (removed local static array)
- Exposed `g_cell_voltages_mux` spinlock via extern in `DashboardUI.h`, defined in `CAN_Receive.cpp`
- Added four narrow file API functions to `sd_card.h`/`sd_card.cpp` with private `static FsFile s_log_file`

## Task Commits

1. **Task 1: Extend DashboardBatteryVoltages + fix decipherCellsVoltage** - `62e3ef8` (feat)
2. **Task 2: Add narrow file API to sd_card module** - `86ea458` (feat)

## Files Created/Modified
- [`display/src/DashboardUI/DashboardUI.h`](../../../../display/src/DashboardUI/DashboardUI.h) - Added Config.h/FreeRTOS.h includes, hv_cell_voltages field, extern g_cell_voltages_mux
- [`display/src/CAN/CAN_Receive.cpp`](../../../../display/src/CAN/CAN_Receive.cpp) - Defined g_cell_voltages_mux, rewrote decipherCellsVoltage to use battery struct + critical section
- [`display/src/SDCard/sd_card.h`](../../../../display/src/SDCard/sd_card.h) - Declared four narrow file API functions
- [`display/src/SDCard/sd_card.cpp`](../../../../display/src/SDCard/sd_card.cpp) - Added static FsFile s_log_file, implemented four narrow API functions

## Decisions Made
- Added bounds check `i + totalOffset < CONFIG_HV_CELL_COUNT` inside the critical section (T-03-03 mitigation — not explicitly in original task action but required per threat model)
- Added `#include "freertos/FreeRTOS.h"` to DashboardUI.h as the plan anticipated (portMUX_TYPE requires it)
- Sum loop kept outside critical section — correct because sum runs in same CAN task context as the write

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Bounds check added per T-03-03 threat model**
- **Found during:** Task 1 (decipherCellsVoltage rewrite)
- **Issue:** Plan's task action didn't include bounds check in the code snippet, but the plan's threat model (T-03-03) explicitly calls for `i + totalOffset < CONFIG_HV_CELL_COUNT` guard
- **Fix:** Added bounds check inside critical section before writing to battery->hv_cell_voltages
- **Files modified:** display/src/CAN/CAN_Receive.cpp
- **Verification:** Build passes; grep confirms guard present
- **Committed in:** 62e3ef8

---

**Total deviations:** 1 auto-fixed (1 bug/safety)
**Impact on plan:** Required per threat model T-03-03. No scope creep.

## Issues Encountered
None

## Next Phase Readiness
- Plan 03-02 can proceed: `hv_cell_voltages[24]`, `g_cell_voltages_mux`, and all four narrow file API functions are available
- `SdFs sd` and `FsFile s_log_file` remain private to `sd_card.cpp` per D-02

---
*Phase: 03-data-model-logging-task*
*Completed: 2026-04-15*

## Self-Check: PASSED
