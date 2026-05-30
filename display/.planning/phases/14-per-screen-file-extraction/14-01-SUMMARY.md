---
phase: 14-per-screen-file-extraction
plan: "01"
subsystem: DashboardUI
tags: [refactor, drive-screen, extraction, lvgl]
dependency_graph:
  requires: []
  provides: [DriveScreen.h, DriveScreen.cpp, drive_screen_build, drive_screen_refresh, update_error_state]
  affects: [DashboardUI.cpp, DashboardShared.h]
tech_stack:
  added: []
  patterns: [per-screen-file extraction, static widget handle struct, EMA filter statics, portMUX spinlock ownership]
key_files:
  created:
    - display/src/DashboardUI/DriveScreen.h
    - display/src/DashboardUI/DriveScreen.cpp
  modified: []
decisions:
  - "DriveWidgets struct declared in DriveScreen.h (not reused from DashboardWidgets) — drive-only fields, no charging fields"
  - "g_error_list_mux definition moves to DriveScreen.cpp; extern declaration remains in DashboardShared.h"
  - "No build verification in Wave 1 — DashboardUI.cpp still has old definitions (ODR intentional until Wave 2 thin)"
metrics:
  duration: "203s"
  completed: "2026-05-07"
  tasks_completed: 2
  tasks_total: 2
  files_created: 2
  files_modified: 0
---

# Phase 14 Plan 01: Drive Screen Extraction (Wave 1) Summary

Drive screen extracted into self-contained DriveScreen.h/.cpp translation units — DriveWidgets struct (29 fields), drive_screen_build, drive_screen_refresh, update_error_state, g_error_list_mux definition, EMA filters, and all dirty-check statics copied verbatim from DashboardUI.cpp.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Create DriveScreen.h | 6349c54 | display/src/DashboardUI/DriveScreen.h |
| 2 | Create DriveScreen.cpp | 3ef08dc | display/src/DashboardUI/DriveScreen.cpp |

## Decisions Made

- **DriveWidgets struct**: New struct scoped to drive screen only. Excludes `main_drive_screen`, `charging_screen`, and all `chg_*` charging fields. Screen-root objects stay in DashboardUI.cpp for Wave 2.
- **g_error_list_mux ownership**: Definition moved to DriveScreen.cpp. DashboardShared.h already carries the `extern` declaration, satisfying T-14-01 (one definition, extern everywhere else).
- **No build validation in Wave 1**: Both DashboardUI.cpp and DriveScreen.cpp define the same spinlock and old/new function variants simultaneously. This is an intentional ODR state — the build is not run until Wave 2 removes the originals from DashboardUI.cpp.
- **Function rename only at signature**: `build_drive_ui` → `drive_screen_build` and `refresh_drive_ui` → `drive_screen_refresh`. Bodies copied verbatim — zero logic changes.

## Deviations from Plan

None — plan executed exactly as written.

## Threat Surface Scan

No new network endpoints, auth paths, file access patterns, or schema changes introduced. All code is MCU-internal. T-14-01 (g_error_list_mux ODR) and T-14-02 (include cycle guard) are both satisfied:
- T-14-01: Exactly one definition in DriveScreen.cpp; extern in DashboardShared.h.
- T-14-02: DashboardUI.h does not include DriveScreen.h; DriveScreen.h includes DashboardUI.h; #pragma once guards both.

## Self-Check: PASSED

- `display/src/DashboardUI/DriveScreen.h` — FOUND
- `display/src/DashboardUI/DriveScreen.cpp` — FOUND
- Commit 6349c54 (DriveScreen.h) — FOUND
- Commit 3ef08dc (DriveScreen.cpp) — FOUND
- `portMUX_TYPE g_error_list_mux = portMUX_INITIALIZER_UNLOCKED` in DriveScreen.cpp — FOUND (1 match)
- `void drive_screen_build` in DriveScreen.cpp — FOUND (1 match)
- `void drive_screen_refresh` in DriveScreen.cpp — FOUND (1 match)
- `void update_error_state` in DriveScreen.cpp — FOUND (1 match)
- `static DriveWidgets w;` in DriveScreen.cpp — FOUND (1 match)
- `chg_` references in DriveScreen.cpp — NOT FOUND (correct — 0 matches)
- `static void build_drive_ui` in DriveScreen.cpp — NOT FOUND (correct — old name absent)
- `static void refresh_drive_ui` in DriveScreen.cpp — NOT FOUND (correct — old name absent)
