# Quick Task 260422-ldu: Make Temperature Dials Change Color Based on Thresholds - Context

**Gathered:** 2026-04-22
**Status:** Ready for planning

<domain>
## Task Boundary

Add threshold-based color changes to the three temperature arcs (BATT, MOTOR, MTR CTRL) on the dashboard. Each arc should change indicator color based on the temperature's relation to warn/crit thresholds. Only the arc ring changes color — not the label text.

</domain>

<decisions>
## Implementation Decisions

### Battery Temperature Thresholds
- Add to `DashboardUI.h`:
  ```c
  #define BATT_TEMP_WARN_CELSIUS  50.0f
  #define BATT_TEMP_CRIT_CELSIUS  60.0f
  ```
  (70% and 85% of the 0–70°C gauge range)

### Normal-State Color
- All three arcs use green (`CLR_MOTOR_TEMP` = `lv_color_make(0x3E, 0xCA, 0x50)`) when below warn threshold
- WARN → `CLR_WARN_YELLOW` (`lv_color_make(0xFE, 0xE0, 0x00)`)
- CRIT → `CLR_WARN_RED` (`lv_color_make(0xF8, 0x20, 0x00)`)

### Label Color
- Arc ring indicator only (`LV_PART_INDICATOR`) — label text stays white

### Color Update Trigger
- Color is set inside the existing `if (temp_int != prev_temp)` dirty-check block — no extra dirty-checking needed since color can only change when temperature changes

### Claude's Discretion
- Helper function design (inline lambda vs static function)
- Whether to update `create_temp_arc()` default color to green (since all arcs now start green)

</decisions>

<specifics>
## Specific Ideas

- Use a helper `temp_arc_color(float temp, float warn, float crit)` that returns `lv_color_t` — keeps the three arc update blocks DRY
- Set color via `lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR)` inside the existing dirty-check block
- Battery arc threshold comparison uses the max thermistor temp (`s_batt_t`) against `BATT_TEMP_WARN_CELSIUS`/`BATT_TEMP_CRIT_CELSIUS`
- Motor arc uses `MOTOR_TEMP_WARN_CELSIUS`/`MOTOR_TEMP_CRIT_CELSIUS` (already defined in header)
- MC arc uses `MC_TEMP_WARN_CELSIUS`/`MC_TEMP_CRIT_CELSIUS` (already defined in header)

</specifics>

<canonical_refs>
## Canonical References

- `display/src/DashboardUI/DashboardUI.h` — existing threshold `#define`s and color macros
- `display/src/DashboardUI/DashboardUI.cpp` — `create_temp_arc()` and temperature arc dirty-check blocks in `dashboard_refresh()`
- `WarningErrorGuide.md` — source of safety thresholds

</canonical_refs>
