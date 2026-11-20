---
phase: quick-260422-ldu
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - src/DashboardUI/DashboardUI.h
  - src/DashboardUI/DashboardUI.cpp
autonomous: true
requirements: [QUICK-260422-LDU]
must_haves:
  truths:
    - "Battery temp arc indicator is green below 50°C, yellow between 50–60°C, red at/above 60°C"
    - "Motor temp arc indicator is green below MOTOR_TEMP_WARN_CELSIUS, yellow between WARN and CRIT, red at/above CRIT"
    - "MC temp arc indicator is green below MC_TEMP_WARN_CELSIUS, yellow between WARN and CRIT, red at/above CRIT"
    - "Arc label text remains white regardless of temperature"
    - "Arc color only updates when the displayed integer temperature changes"
  artifacts:
    - path: "src/DashboardUI/DashboardUI.h"
      provides: "BATT_TEMP_WARN_CELSIUS and BATT_TEMP_CRIT_CELSIUS defines"
      contains: "BATT_TEMP_WARN_CELSIUS"
    - path: "src/DashboardUI/DashboardUI.cpp"
      provides: "temp_arc_color() helper and per-arc color updates in dashboard_refresh()"
      contains: "lv_obj_set_style_arc_color"
  key_links:
    - from: "dashboard_refresh() temp dirty-check blocks"
      to: "temp_arc_color() helper"
      via: "direct call inside each if (temp_int != prev_temp) block"
      pattern: "temp_arc_color\\("
---

<objective>
Make the three temperature arcs (BATT, MOTOR, MTR CTRL) on the dashboard change
indicator color based on warn/crit thresholds: green (normal) → yellow (warn)
→ red (crit). Only the arc indicator changes; label text stays white.

Purpose: Give the rider immediate visual feedback when any tracked thermal
domain crosses safety thresholds.
Output: Updated DashboardUI.h (new battery thresholds) and DashboardUI.cpp
(color helper + per-arc color updates).
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/quick/260422-ldu-make-the-temperature-dials-on-the-displa/260422-ldu-CONTEXT.md
@src/DashboardUI/DashboardUI.h
@src/DashboardUI/DashboardUI.cpp

<interfaces>
Existing color macros (DashboardUI.h):
- CLR_MOTOR_TEMP = lv_color_make(0x3E, 0xCA, 0x50)  // green (normal)
- CLR_WARN_YELLOW = lv_color_make(0xFE, 0xE0, 0x00)  // warn
- CLR_WARN_RED = lv_color_make(0xF8, 0x20, 0x00)    // crit

Existing thresholds (DashboardUI.h):
- MOTOR_TEMP_WARN_CELSIUS, MOTOR_TEMP_CRIT_CELSIUS
- MC_TEMP_WARN_CELSIUS, MC_TEMP_CRIT_CELSIUS

New thresholds to add:
- BATT_TEMP_WARN_CELSIUS  50.0f
- BATT_TEMP_CRIT_CELSIUS  60.0f

LVGL API:
- lv_obj_set_style_arc_color(lv_obj_t* obj, lv_color_t color, lv_style_selector_t selector)
  Use selector = LV_PART_INDICATOR to target the arc ring only.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Add battery temp thresholds and color helper</name>
  <files>src/DashboardUI/DashboardUI.h, src/DashboardUI/DashboardUI.cpp</files>
  <action>
    In DashboardUI.h, add next to the existing temperature threshold defines:
      #define BATT_TEMP_WARN_CELSIUS  50.0f
      #define BATT_TEMP_CRIT_CELSIUS  60.0f

    In DashboardUI.cpp (file-local, near the top of the anonymous/static
    helper area above dashboard_refresh()), add a static inline helper:

      static inline lv_color_t temp_arc_color(float temp, float warn, float crit) {
          if (temp >= crit) return CLR_WARN_RED;
          if (temp >= warn) return CLR_WARN_YELLOW;
          return CLR_MOTOR_TEMP;
      }

    Do NOT alter any other logic in this task. Keep create_temp_arc() behavior
    unchanged for now (Task 2 handles per-refresh color updates).
  </action>
  <verify>
    <automated>pio run -e display 2>&amp;1 | tail -20</automated>
    Build succeeds; grep confirms the defines and helper exist:
      grep -n "BATT_TEMP_WARN_CELSIUS" src/DashboardUI/DashboardUI.h
      grep -n "temp_arc_color" src/DashboardUI/DashboardUI.cpp
  </verify>
  <done>
    Header exposes BATT_TEMP_WARN_CELSIUS and BATT_TEMP_CRIT_CELSIUS; .cpp
    defines temp_arc_color() helper returning the correct color tier; project
    still builds cleanly.
  </done>
</task>

<task type="auto">
  <name>Task 2: Wire color updates into the three arc dirty-check blocks</name>
  <files>src/DashboardUI/DashboardUI.cpp</files>
  <action>
    In dashboard_refresh(), inside each of the three existing
    `if (temp_int != prev_temp)` blocks (batt_temp, motor_temp, mc_temp),
    add a call to update the arc indicator color using the helper from Task 1:

      // Battery block (uses s_batt_t against BATT_TEMP_WARN/CRIT_CELSIUS)
      lv_obj_set_style_arc_color(
          batt_temp_arc,
          temp_arc_color(s_batt_t, BATT_TEMP_WARN_CELSIUS, BATT_TEMP_CRIT_CELSIUS),
          LV_PART_INDICATOR);

      // Motor block (uses the motor temp var against MOTOR_TEMP_WARN/CRIT_CELSIUS)
      lv_obj_set_style_arc_color(
          motor_temp_arc,
          temp_arc_color(<motor_temp_var>, MOTOR_TEMP_WARN_CELSIUS, MOTOR_TEMP_CRIT_CELSIUS),
          LV_PART_INDICATOR);

      // MC block (uses the mc temp var against MC_TEMP_WARN/CRIT_CELSIUS)
      lv_obj_set_style_arc_color(
          mc_temp_arc,
          temp_arc_color(<mc_temp_var>, MC_TEMP_WARN_CELSIUS, MC_TEMP_CRIT_CELSIUS),
          LV_PART_INDICATOR);

    Use whatever the current local float variables are named in each block
    (match existing code — do not introduce new variables). Place the color
    call alongside the existing label/arc-value update inside the same
    dirty-check branch so it only runs when temp_int changed.

    Also: in dashboard_create() (or wherever create_temp_arc() is invoked),
    ensure all three arcs are created with CLR_MOTOR_TEMP as the initial
    indicator color so they start green before the first refresh tick.
    Do NOT change the label text color.
  </action>
  <verify>
    <automated>pio run -e display 2>&amp;1 | tail -20</automated>
    Build succeeds. Manually grep to confirm three color-update sites:
      grep -n "lv_obj_set_style_arc_color" src/DashboardUI/DashboardUI.cpp
    Expect at least three matches inside dashboard_refresh() (plus any
    existing calls in create_temp_arc()).
  </verify>
  <done>
    All three temp arcs update indicator color on each temperature change:
    green below warn, yellow between warn and crit, red at/above crit.
    Label text remains white. Build is clean.
  </done>
</task>

<task type="checkpoint:human-verify" gate="blocking">
  <what-built>
    Threshold-based color changes on the three dashboard temperature arcs.
  </what-built>
  <how-to-verify>
    1. Flash the build to the display board (or run the simulator).
    2. Using the HIL or simulation, sweep each temperature through its bands:
       - Battery: 40°C (green) → 55°C (yellow) → 65°C (red)
       - Motor:   below WARN (green) → between WARN/CRIT (yellow) → above CRIT (red)
       - MC:      below WARN (green) → between WARN/CRIT (yellow) → above CRIT (red)
    3. Confirm each arc ring changes color at the expected threshold and
       that the numeric label text stays white throughout.
  </how-to-verify>
  <resume-signal>Type "approved" or describe any color/threshold issues.</resume-signal>
</task>

</tasks>

<verification>
- Build succeeds for the display env.
- grep shows BATT_TEMP_WARN_CELSIUS / BATT_TEMP_CRIT_CELSIUS in the header.
- grep shows temp_arc_color() defined once and called from all three
  per-arc dirty-check blocks in dashboard_refresh().
- Human checkpoint confirms visual behavior on hardware/sim.
</verification>

<success_criteria>
- All three temperature arcs reflect green/yellow/red based on their
  respective WARN/CRIT thresholds.
- Color only updates on the existing temp_int change trigger (no extra
  refresh overhead).
- Label text color is unchanged.
- No regressions in dashboard rendering.
</success_criteria>

<output>
After completion, create `.planning/quick/260422-ldu-make-the-temperature-dials-on-the-displa/260422-ldu-SUMMARY.md`
</output>
