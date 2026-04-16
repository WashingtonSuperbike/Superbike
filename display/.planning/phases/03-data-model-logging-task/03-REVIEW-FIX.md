---
phase: 03-data-model-logging-task
fixed_at: 2026-04-15T00:00:00Z
review_path: .planning/phases/03-data-model-logging-task/03-REVIEW.md
iteration: 1
findings_in_scope: 5
fixed: 4
skipped: 1
status: partial
---

# Phase 03: Code Review Fix Report

**Fixed at:** 2026-04-15
**Source review:** .planning/phases/03-data-model-logging-task/03-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 5 (CR-01, WR-01, WR-02, WR-03, WR-04)
- Fixed: 4
- Skipped: 1

## Fixed Issues

### CR-01: `sd_started` Written and Read Across Cores Without Atomic Protection

**Files modified:** `src/DashboardUI/DashboardUI.h`
**Commit:** e4eb211
**Applied fix:** Added `#include <atomic>` and changed `volatile bool sd_started;` to `std::atomic<bool> sd_started{};` in `DashboardState`. All existing read sites (implicit bool conversion in logger.cpp and sd_card.cpp) and write sites (atomic assignment) remain valid with no further changes required.

---

### WR-02: CSV Header Has 42 Columns But Comments Say 34

**Files modified:** `src/Logger/logger.h`, `src/Logger/logger.cpp`
**Commit:** 974931b
**Applied fix:** Updated three stale "34-column" comments to "42-column" — the docblock in logger.h, the `writeHeader` section comment in logger.cpp, and the `writeRow` section comment in logger.cpp.

---

### WR-03: Duplicate `GEAR_RATIO`/`WHEEL_DIAM_M`/`MPH_CONVERT` Macros in logger.cpp

**Files modified:** `src/Logger/logger.cpp`
**Commit:** d6336ff
**Applied fix:** Removed the entire `#ifndef`-guarded duplicate macro block (lines 15–26 in the pre-fix file). The three constants are already defined in `DashboardUI/DashboardUI.h` which is included above, making the local copies dead code. `DashboardUI.h` is now the single source of truth.

---

### WR-04: `do_mount()` Mutex Timeout Too Short — Transient Contention Triggers Spurious Unmount

**Files modified:** `src/SDCard/sd_card.cpp`
**Commit:** e690745
**Applied fix:** Increased the `xSemaphoreTake` timeout in `do_mount()` from `pdMS_TO_TICKS(50)` to `pdMS_TO_TICKS(200)`. This ensures transient SdFat write activity by logger_task (which can hold the mutex for the duration of a sector write) does not cause `do_mount` to return false and trigger a spurious `sd_started = false` in `sd_poll_task`.

---

## Skipped Issues

### WR-01: Card-Removal Branch Does Not Set `file_open = false` When Mutex Take Fails

**File:** `src/Logger/logger.cpp:130-140`
**Reason:** Code already correct — the current source already places `file_open = false` unconditionally outside the `xSemaphoreTake` success block. The reviewer described a version where it was inside the block, but in the actual file `file_open = false` appears at the same indentation level as the `if (xSemaphoreTake(...))` block, executing regardless of mutex acquisition outcome. No change needed.
**Original issue:** If mutex take fails during card-removal detection, `file_open` would remain true, leaking the file handle until the next insertion event.

---

_Fixed: 2026-04-15_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
