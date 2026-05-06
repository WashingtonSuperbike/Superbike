# Requirements: Milestone v1.7 — Screen Separation

## Milestone Goal

Validate the charging and drive screen implementations (including debug override defines), then refactor the monolithic `DashboardUI.cpp` into per-screen source files with clean separation of concerns.

---

## In Scope

### Validation

- [ ] **VAL-01**: Charging screen activates when EVCC reports `en > 0` within `CAN_TIMEOUT_MS`
- [ ] **VAL-02**: Charging screen activates when charger `output_current > 0.5A` within `CAN_TIMEOUT_MS`
- [ ] **VAL-03**: Drive screen restores when neither EVCC nor charger data is live
- [ ] **VAL-04**: Screen transition plays `LV_SCR_LOAD_ANIM_FADE_ON` (500ms) on switch
- [ ] **VAL-05**: `DEBUG_CHARGING_SCREEN_ONLY` defined → charging screen shown regardless of live data
- [ ] **VAL-06**: `DEBUG_SPEEDOMETER_SCREEN_ONLY` defined → drive screen shown regardless of live data
- [ ] **VAL-07**: Charging screen displays correct voltage, current, power, charger temp, and status label
- [ ] **VAL-08**: Drive screen smoke test: speed needle, temp arcs, current arcs, and error overlays render correctly after refactor

### Refactor

- [ ] **REF-01**: `DriveScreen.h/.cpp` created; contains `drive_screen_build`, `drive_screen_refresh`, EMA filter instances, and all dirty-check statics
- [ ] **REF-02**: `ChargingScreen.h/.cpp` created; contains `charging_screen_build`, `charging_screen_refresh`, and charging widget handle fields
- [ ] **REF-03**: Shared infra header (color palette macros, `create_temp_arc` helper, `g_error_list_mux` declaration) extracted so both screen files can include it without circular dependencies
- [ ] **REF-04**: `DashboardWidgets` struct split: drive widget fields move to `DriveScreen.h`, charging widget fields to `ChargingScreen.h`
- [ ] **REF-05**: `DashboardUI.cpp` reduced to coordinator: `dashboard_create`, `dashboard_refresh` (screen routing logic), `dashboardTask` — no widget construction or data refresh logic remains
- [ ] **REF-06**: Binary behavior unchanged after refactor — same screen logic, same data paths, same rendered output

---

## Future Requirements (deferred)

- UI layout redesign for drive screen — revisit after more riding data
- Charging screen layout improvements (e.g. cell voltage bar, time-to-full estimate)
- Touch input — hardware has no touch panel; re-evaluate if board changes

## Out of Scope

- New charging screen features — v1.7 validates and refactors the existing screen only
- Gyro/IMU integration — tabled by user
- Log rotation / max file size — not needed for current session lengths

---

## Traceability

| REQ-ID | Phase | Status |
|--------|-------|--------|
| VAL-01 | Phase 12 | Passed-with-notes (tested via debug constant; live EVCC path code-verified) |
| VAL-02 | Phase 12→15 | Deferred — active charger not available; code verified by inspection |
| VAL-03 | Phase 12→15 | Deferred — live CAN required; code verified by inspection |
| VAL-04 | Phase 12 | Passed-with-notes (fade confirmed; appears choppy at 30 Hz) |
| VAL-05 | Phase 12 | Passed-with-gap (1s drive screen on boot before task fires) |
| VAL-06 | Phase 12 | Passed (drive screen locked; live charger path not tested) |
| VAL-07 | Phase 12→15 | Partial — zero-data layout explained; live data confirmation deferred to Phase 15 |
| VAL-08 | Phase 15 | Pending |
| REF-01 | Phase 14 | Pending |
| REF-02 | Phase 14 | Pending |
| REF-03 | Phase 13 | Pending |
| REF-04 | Phase 14 | Pending |
| REF-05 | Phase 14 | Pending |
| REF-06 | Phase 15 | Pending |
