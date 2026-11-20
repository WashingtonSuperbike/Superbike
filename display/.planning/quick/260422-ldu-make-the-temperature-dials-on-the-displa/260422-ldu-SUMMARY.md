---
quick_id: 260422-ldu
status: complete
date: 2026-04-22
commit: d3b570e
---

# Quick Task 260422-ldu: Temperature Arc Color Changes

## What Was Built

Three temperature arcs (BATT, MOTOR, MTR CTRL) now change indicator color dynamically based on temperature thresholds:

- **Green** (`CLR_MOTOR_TEMP`) — normal, below warn threshold
- **Yellow** (`CLR_WARN_YELLOW`) — at or above warn threshold
- **Red** (`CLR_WARN_RED`) — at or above crit threshold

## Changes Made

### `src/DashboardUI/DashboardUI.h`
- Added `BATT_TEMP_WARN_CELSIUS 50.0f` and `BATT_TEMP_CRIT_CELSIUS 60.0f` to the SAFETY THRESHOLDS section

### `src/DashboardUI/DashboardUI.cpp`
- Added `temp_arc_color(float temp, float warn, float crit)` static inline helper
- All three `create_temp_arc()` calls in `dashboard_create()` now use `CLR_MOTOR_TEMP` (green) as initial color
- Each `if (temp_int != prev_temp)` dirty-check block in `dashboard_refresh()` now calls `lv_obj_set_style_arc_color()` with the appropriate color tier

## Thresholds

| Arc | Warn | Crit |
|-----|------|------|
| BATT | 50°C | 60°C |
| MOTOR | 95°C (MOTOR_TEMP_WARN_CELSIUS) | 110°C (MOTOR_TEMP_CRIT_CELSIUS) |
| MTR CTRL | 70°C (MC_TEMP_WARN_CELSIUS) | 95°C (MC_TEMP_CRIT_CELSIUS) |

## Human Checkpoint

Flash and sweep temperatures through each band to verify color transitions. Label text should remain white throughout.
