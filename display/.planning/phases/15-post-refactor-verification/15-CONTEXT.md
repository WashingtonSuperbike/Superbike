# Phase 15: Post-Refactor Verification - Context

**Gathered:** 2026-05-07
**Status:** Ready for planning

<domain>
## Phase Boundary

Validate that the v1.7 structural refactor (phases 12-14) produces a clean build and preserves on-screen behavior. Two deliverables: (1) `pio run` completes with zero warnings and zero errors; (2) a targeted hardware smoke test confirms drive screen visuals, charging screen visuals, and screen switching still work. No new features, no logic changes — verification only.

</domain>

<decisions>
## Implementation Decisions

### Build Clean Threshold
- **D-01:** Zero total warnings is the bar — not just "zero new." Any warning present in the refactored build is a defect for this phase.
- **D-02:** Fix warnings inline rather than report-and-block. Phase 15 is not complete until the build is clean; no deferring to a follow-up phase.

### Smoke Test Scope
- **D-03:** Targeted spot-check only — not a full Phase 12 replay. Phase 12 validated live CAN paths, watchdog timing, and animation timing; those haven't changed. Phase 15 verifies the things the refactor could have broken: drive screen visuals, charging screen visuals, and screen switching.
- **D-04:** Drive screen smoke test covers: speed needle, temperature arcs, current arcs, and error overlays (VAL-08). Error overlay triggered via CAN timeout — let the watchdog expire naturally with no CAN connected, errors appear within CAN_TIMEOUT_MS.
- **D-05:** Screen switching verified via debug defines only: `DEBUG_CHARGING_SCREEN_ONLY` and `DEBUG_SPEEDOMETER_SCREEN_ONLY`. Phase 12 already validated the live EVCC/charger switching paths — no need to re-run them here.

### Claude's Discretion
- Plan structure (one plan vs. two) — could split build/fix plan from hardware smoke plan or combine
- Order of build check vs. hardware test within the phase
- Exact warning categories to address if any are found (unused variable, sign comparison, etc.)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase Requirements
- `display/.planning/REQUIREMENTS.md` §Validation — VAL-08: drive screen smoke test (speed needle, temp arcs, current arcs, error overlays render correctly after refactor)
- `display/.planning/REQUIREMENTS.md` §Refactor — REF-06: binary behavior unchanged after refactor (same screen logic, same data paths, same rendered output)
- `display/.planning/ROADMAP.md` §Phase 15 — success criteria (3 conditions: zero-warning build, drive screen hardware smoke, screen switching via debug defines)

### Phase 12 Baseline
- `display/.planning/phases/12-screen-transition-validation/` — Phase 12 verification results are the behavioral baseline. Phase 15 spot-check must be consistent with these results.

### Refactored Source Files
- `display/src/DashboardUI/DriveScreen.h` — DriveWidgets struct + public API
- `display/src/DashboardUI/DriveScreen.cpp` — drive screen builder, refresher, EMA filters, error logic (724 lines)
- `display/src/DashboardUI/ChargingScreen.h` — ChargingWidgets struct + public API
- `display/src/DashboardUI/ChargingScreen.cpp` — charging screen builder and refresher (121 lines)
- `display/src/DashboardUI/DashboardUI.cpp` — coordinator-only (96 lines): dashboard_create, dashboard_refresh, dashboardTask
- `display/src/DashboardUI/DashboardShared.h` — color macros, temp_arc_color, create_temp_arc, spinlock externs

### Phase 14 Verification
- `display/.planning/phases/14-per-screen-file-extraction/14-VERIFICATION.md` — confirms structural correctness; explicitly defers REF-06/VAL-08 behavioral check to Phase 15

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `pio run -e display` — existing build command; zero-warning check runs against this target
- `DEBUG_CHARGING_SCREEN_ONLY` / `DEBUG_SPEEDOMETER_SCREEN_ONLY` — compile-time defines in DashboardUI.cpp; already validated in Phase 12-02; use for screen switching spot-check
- CAN watchdog timeout path — error state fires automatically when CAN data goes stale; no special injection needed for error overlay test

### Established Patterns
- Build validation: run `pio run -e display`, check output for "warning:" occurrences
- Hardware flash: `pio run -t upload -e display` followed by serial monitor observation
- Phase 12 used `-DDEBUG_CHARGING_SCREEN_ONLY` and `-DDEBUG_SPEEDOMETER_SCREEN_ONLY` as build flags for debug define testing

### Integration Points
- DashboardUI.cpp is the entry point for all screen logic; build passes here means all includes resolve
- Error overlay in DriveScreen.cpp: `add_error_to_list` and `update_error_state` — these trigger when `g_dashboard_state.can_status == CAN_ERROR`
- Screen routing in `dashboard_refresh` (DashboardUI.cpp:74-76) calls `charging_screen_refresh` or `drive_screen_refresh` based on state

</code_context>

<specifics>
## Specific Ideas

- The CAN timeout error path is the simplest way to verify error overlays: power on the board with no CAN connected, wait CAN_TIMEOUT_MS, confirm error overlay appears on drive screen. No extra tooling needed.
- "Zero total warnings" was chosen over "zero new warnings" to avoid needing a pre-refactor snapshot — simpler pass/fail criterion.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 15-Post-Refactor Verification*
*Context gathered: 2026-05-07*
