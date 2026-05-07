---
phase: 13-shared-infrastructure-extraction
reviewed: 2026-05-05T00:00:00Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - display/src/DashboardUI/DashboardShared.h
  - display/src/DashboardUI/DashboardUI.h
  - display/src/DashboardUI/DashboardUI.cpp
findings:
  critical: 2
  warning: 3
  info: 2
  total: 7
status: issues_found
---

# Phase 13: Code Review Report

**Reviewed:** 2026-05-05
**Depth:** standard
**Files Reviewed:** 3
**Status:** issues_found

## Summary

Phase 13 extracted colour macros, the `temp_arc_color` helper, `create_temp_arc`, and
spinlock `extern` declarations from `DashboardUI.cpp` into a new `DashboardShared.h`
header. The extraction itself is structurally sound: `#pragma once` guards are present,
ODR semantics for the `inline` helper are correct, and the `extern` placement in the
shared header is appropriate for future screen-split consumers.

Two blocking bugs were found in `DashboardUI.cpp` — both pre-existing but now surfaced
by review. They affect safety-critical display behaviour: one silently suppresses the
critical-error modal for a specific motor controller fault, and the other makes the
thermistor over-temperature warning unreachable. Three warnings cover uninitialized
widget pointers and unused computed state. Two info items address duplicate includes
and a magic-number duplication.

---

## Critical Issues

### CR-01: `mc_err & 0x400` logged as CRIT but `max_mc` only escalated to WARN — modal never fires

**File:** `display/src/DashboardUI/DashboardUI.cpp:174-176`

**Issue:** When motor-controller error bit `0x400` ("THROTTLE SHORT OR OPEN CIRCUIT") is
set, `add_error_to_list` is called with `ErrorSeverity::CRIT`. However, the very next
line only guards with `if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;`,
so `max_mc` is capped at `WARN`. `state->mc_severity` is stored as `WARN`, meaning
`crit_active` in `refresh_drive_ui` will never be `true` for this fault alone, and the
critical-error modal is suppressed. The error appears in the carousel (WARN level) instead
of the prominent full-screen modal, defeating its safety intent.

Compare the identical pattern at `mc_err & 0x800` (line 178-180) where `max_mc` is
correctly set to `ErrorSeverity::CRIT`.

**Fix:**
```cpp
// line 174-177 — fix the max_mc update to match the CRIT severity passed to add_error_to_list
if (mc_err & 0x400) {
    add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "THROTTLE SHORT OR OPEN CIRCUIT");
    max_mc = ErrorSeverity::CRIT;   // was: if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
}
```

---

### CR-02: Thermistor over-temperature warning triggers only ABOVE the critical threshold

**File:** `display/src/DashboardUI/DashboardUI.cpp:128`

**Issue:** The thermistor loop checks `state->thermistors.temps[i] >= BATT_TEMP_MAX`.
`BATT_TEMP_MAX` is defined as `BATT_TEMP_CRIT_CELSIUS + 5.0f` = **65 °C**, but
`BATT_TEMP_CRIT_CELSIUS` is **60 °C**. The WARN path is therefore unreachable: by the
time this check fires, the battery is already 5 °C past critical. The comment on the same
line admits this is wrong ("Using gauge max as warning threshold"). The warning generated
here (`ErrorSeverity::WARN`) will never appear before a higher-severity BMS fault has
already fired via `bms_c_fault & 0x04`.

**Fix:**
```cpp
// Use the actual warning threshold, not the gauge upper bound
if (state->thermistors.temps[i] >= BATT_TEMP_WARN_CELSIUS) {
    char t_buf[32];
    snprintf(t_buf, sizeof(t_buf), "BMS: THERMISTOR %d HOT", i);
    ErrorSeverity sev = (state->thermistors.temps[i] >= BATT_TEMP_CRIT_CELSIUS)
                        ? ErrorSeverity::CRIT : ErrorSeverity::WARN;
    add_error_to_list(state->error_list, ErrorSource::BMS, sev, t_buf);
    if (max_bms < sev) max_bms = sev;
}
```

---

## Warnings

### WR-01: `stopwatch_label` and `error_label` declared in `DashboardWidgets` but never initialized

**File:** `display/src/DashboardUI/DashboardUI.h:158,170`

**Issue:** `DashboardWidgets::stopwatch_label` (line 158) and
`DashboardWidgets::error_label` (line 170) are declared in the widget struct. Neither is
created in `build_drive_ui()` nor written anywhere in `DashboardUI.cpp`. The static `w`
instance is zero-initialized, so both pointers are `nullptr`. Any future code — including
in Phase 14 screen files that include `DashboardShared.h` — that calls LVGL APIs on these
pointers will fault. The layout comment in the file header (`│ ... error │ ...`) implies
`error_label` was intended to exist.

**Fix:** Either add the LVGL object creation calls in `build_drive_ui()` for both widgets,
or remove the dead fields from `DashboardWidgets` to prevent misleading consumers.

---

### WR-02: `crit_count` computed but never used

**File:** `display/src/DashboardUI/DashboardUI.cpp:784-789`

**Issue:** Inside the critical-error modal block, a loop computes `crit_count` (the number
of active critical errors) but the variable is never referenced after the loop. The
carousel that follows scans independently for the first active CRIT error. The dead
variable wastes cycles inside a spinlock and misleads readers who expect it to gate
carousel behaviour (e.g., "show count in modal title").

**Fix:** Remove the dead loop, or use `crit_count` to display an error count in
`modal_title` (e.g., "POWER CUT (2)"):
```cpp
// Option A — remove entirely:
// (delete lines 784-789)

// Option B — use it:
char title_buf[32];
snprintf(title_buf, sizeof(title_buf), "POWER CUT (%d)", crit_count);
lv_label_set_text(w.modal_title, title_buf);
```

---

### WR-03: Battery percentage calculation hardcoded with magic numbers, duplicated in two functions

**File:** `display/src/DashboardUI/DashboardUI.cpp:668,820`

**Issue:** The pack voltage → percentage formula appears identically in both
`refresh_drive_ui` (line 668) and `refresh_charging_ui` (line 820):
```cpp
float pct = (voltage - 60.0f) / (100.8f - 60.0f) * 100.0f;
```
The constants `60.0f` (pack empty voltage) and `100.8f` (pack full voltage) are not
defined in `Config.h` or `Constants.h`. If the pack chemistry or cell count changes,
the update must be made in two places and is easy to miss. The duplication also means
the drive-screen and charging-screen can diverge silently.

**Fix:** Add named constants to `include/Config.h` and call a shared helper:
```cpp
// In Config.h
#define PACK_VOLTAGE_EMPTY_V  60.0f
#define PACK_VOLTAGE_FULL_V   100.8f

// In DashboardShared.h (or DashboardUI.cpp)
static inline float pack_voltage_to_pct(float v) {
    float pct = (v - PACK_VOLTAGE_EMPTY_V) / (PACK_VOLTAGE_FULL_V - PACK_VOLTAGE_EMPTY_V) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    return pct;
}
```

---

## Info

### IN-01: Redundant `#include <lvgl.h>` in `DashboardUI.h`

**File:** `display/src/DashboardUI/DashboardUI.h:17`

**Issue:** `DashboardUI.h` includes `DashboardShared.h` at line 16, which itself includes
`<lvgl.h>` at line 13. The second `#include <lvgl.h>` on line 17 of `DashboardUI.h` is
redundant. `#pragma once` in `lvgl.h` makes this harmless, but it is noise that could
confuse a reader about which include provides LVGL symbols.

**Fix:** Remove line 17 (`#include <lvgl.h>`) from `DashboardUI.h`.

---

### IN-02: All three temperature arcs created with `CLR_MOTOR_TEMP` as initial colour, ignoring dedicated palette entries

**File:** `display/src/DashboardUI/DashboardUI.cpp:333,338,343`

**Issue:** `build_drive_ui()` passes `CLR_MOTOR_TEMP` (green, `0x3ECA50`) as the `color`
argument to `create_temp_arc()` for all three arcs: battery, motor, and motor controller.
The colour palette in `DashboardShared.h` already defines `CLR_BATT_TEMP` (warm orange,
`0xF87110`) and `CLR_MC_TEMP` (orange, `0xFE8000`) which are semantically appropriate
for their respective arcs. Using the correct colours at creation time would make the
initial (boot) state visually distinguishable before any temperature data arrives.

**Fix:**
```cpp
create_temp_arc(scr, &w.batt_temp_arc,  &w.batt_temp_label,
                "BATT",     CLR_BATT_TEMP,   // was CLR_MOTOR_TEMP
                BATT_TEMP_MIN, BATT_TEMP_MAX, arc_x, arc_y);

create_temp_arc(scr, &w.motor_temp_arc, &w.motor_temp_label,
                "MOTOR",    CLR_MOTOR_TEMP,  // correct
                MOTOR_TEMP_MIN, MOTOR_TEMP_MAX, arc_x + arc_spacing, arc_y);

create_temp_arc(scr, &w.mc_temp_arc,   &w.mc_temp_label,
                "MTR CTRL", CLR_MC_TEMP,     // was CLR_MOTOR_TEMP
                MC_TEMP_MIN, MC_TEMP_MAX, arc_x, arc_y + arc_spacing);
```

---

_Reviewed: 2026-05-05_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
