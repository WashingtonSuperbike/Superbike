# Roadmap: Superbike Display Module

## Milestones

- ✅ **v1.0 Build & Flash** — Phase 1 (hardware bringup complete, 2026-04-03)
- ✅ **v1.1 Speedometer Dial** — Phase 1 (lv_meter speedometer complete, 2026-04-03)
- ✅ **v1.2 SD Card Support** — Phases 2-3 (shipped 2026-04-16)
- ✅ **v1.3 CAN Bus Integration** — Phase 4 (shipped 2026-04-20)
- ✅ **v1.5 Errors and Warnings** — Phases 05-08 (completed 2026-04-20)
- ✅ **v1.6 Data Smoothing** — Phases 09-11 (shipped 2026-05-05)
- 🔄 **v1.7 Screen Separation** — Phases 12-15 (in progress)

---

## Phases

<details>
<summary>✅ v1.6 Data Smoothing (Phases 09-11) — SHIPPED 2026-05-05</summary>

- [x] Phase 09: Filtering Utility (3/3 plans) — completed 2026-04-20
- [x] Phase 10: UI Integration (3/3 plans) — completed 2026-04-20
- [x] Phase 11: Validation & Performance (3/3 plans) — completed 2026-05-05

Full details: `.planning/milestones/v1.6-ROADMAP.md`

</details>

### v1.7 Screen Separation (Phases 12-15)

- [x] **Phase 12: Screen Transition Validation** — Verify charging/drive switching and debug override defines on hardware (completed 2026-05-05, VAL-02/03 deferred to Phase 15)
- [x] **Phase 13: Shared Infrastructure Extraction** — Extract shared header (color macros, arc helper, error mux) that both screen files can include (completed 2026-05-06)
- [x] **Phase 14: Per-Screen File Extraction** — Create DriveScreen and ChargingScreen files; split DashboardWidgets; reduce DashboardUI to coordinator (completed 2026-05-07)
- [ ] **Phase 15: Post-Refactor Verification** — Compile clean and smoke test on hardware; confirm behavior unchanged

## Phase Details

### Phase 12: Screen Transition Validation
**Goal**: The charging screen and drive screen switch correctly on live hardware, and debug defines lock the display to the expected screen
**Depends on**: Phase 11 (v1.6 complete)
**Requirements**: VAL-01, VAL-02, VAL-03, VAL-04, VAL-05, VAL-06, VAL-07
**Success Criteria** (what must be TRUE):
  1. Connecting EVCC with `en > 0` within `CAN_TIMEOUT_MS` causes the charging screen to appear
  2. Connecting a charger with `output_current > 0.5A` within `CAN_TIMEOUT_MS` causes the charging screen to appear
  3. Removing live EVCC/charger data causes the drive screen to restore
  4. Every screen switch plays a visible `LV_SCR_LOAD_ANIM_FADE_ON` animation lasting approximately 500ms
  5. Defining `DEBUG_CHARGING_SCREEN_ONLY` shows the charging screen regardless of whether live EVCC/charger data is present; defining `DEBUG_SPEEDOMETER_SCREEN_ONLY` shows the drive screen regardless of live charger data; charging screen displays correct voltage, current, power, charger temp, and status label values
**Plans**: 2 plans
Plans:
- [x] 12-01-PLAN.md — Live CAN hardware validation (EVCC path, charger path, watchdog restore, animation, label values)
- [x] 12-02-PLAN.md — Debug define validation (DEBUG_CHARGING_SCREEN_ONLY and DEBUG_SPEEDOMETER_SCREEN_ONLY)

### Phase 13: Shared Infrastructure Extraction
**Goal**: Color macros, `create_temp_arc`, and `g_error_list_mux` live in a shared header that either screen file can include without circular dependencies
**Depends on**: Phase 12
**Requirements**: REF-03
**Success Criteria** (what must be TRUE):
  1. A single shared header (e.g. `DashboardShared.h`) defines color palette macros, the `create_temp_arc` helper declaration, and the `g_error_list_mux` extern
  2. The project compiles without duplicate-symbol or include-cycle errors after the extraction
  3. No widget construction or screen-specific logic is present in the shared header
**Plans**: 1 plan
Plans:
- [x] 13-01-PLAN.md — Create DashboardShared.h and wire DashboardUI.h/DashboardUI.cpp to use it

### Phase 14: Per-Screen File Extraction
**Goal**: `DriveScreen.h/.cpp` and `ChargingScreen.h/.cpp` exist as self-contained translation units; `DashboardWidgets` is split; `DashboardUI.cpp` contains only coordinator logic
**Depends on**: Phase 13
**Requirements**: REF-01, REF-02, REF-04, REF-05
**Success Criteria** (what must be TRUE):
  1. `DriveScreen.h/.cpp` contains `drive_screen_build`, `drive_screen_refresh`, all EMA filter instances, and all drive dirty-check statics — nothing else
  2. `ChargingScreen.h/.cpp` contains `charging_screen_build`, `charging_screen_refresh`, and all charging widget handle fields — nothing else
  3. Drive widget fields are declared in `DriveScreen.h` and charging widget fields in `ChargingScreen.h`; neither struct field set appears in `DashboardUI.h`
  4. `DashboardUI.cpp` contains only `dashboard_create`, `dashboard_refresh` (screen routing), and `dashboardTask` — no widget construction or signal refresh logic
**Plans**: 3 plans
Plans:
- [x] 14-01-PLAN.md — Create DriveScreen.h and DriveScreen.cpp (drive widgets, EMA filters, error logic)
- [x] 14-02-PLAN.md — Create ChargingScreen.h and ChargingScreen.cpp (charging widgets, builder, refresher)
- [x] 14-03-PLAN.md — Thin DashboardUI.h and DashboardUI.cpp to coordinator-only; verify build passes

### Phase 15: Post-Refactor Verification
**Goal**: The refactored codebase compiles without warnings and produces identical on-screen behavior to the pre-refactor baseline
**Depends on**: Phase 14
**Requirements**: REF-06, VAL-02, VAL-03, VAL-07, VAL-08
**Success Criteria** (what must be TRUE):
  1. `pio run` completes with zero errors and zero warnings (D-01: zero total warnings, not just zero new)
  2. Drive screen on hardware shows correct speed needle, temperature arcs, current arcs, and error overlays after flashing the refactored build
  3. Switching between drive and charging screen (via debug defines) behaves identically to the Phase 12 validated baseline
  4. Charging screen activates within 2s when charger output_current > 0.5A (VAL-02, requires live charger)
  5. Drive screen restores within 1.8-2.5s after charger CAN frames stop (VAL-03, requires live charger)
  6. Charging screen labels show plausible live values with active charger (VAL-07)
**Plans**: 3 plans
Plans:
- [ ] 15-01-PLAN.md — Restore Config.h clean state and run zero-warning build; fix all warnings inline
- [ ] 15-02-PLAN.md — Hardware smoke test: drive screen visuals, CAN timeout error overlay, debug define screen switching
- [ ] 15-03-PLAN.md — Live charger hardware tests: VAL-02 (charger activation), VAL-03 (watchdog restore), VAL-07 (label accuracy)

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Speedometer Meter | v1.1 | 1/1 | Complete | 2026-04-03 |
| 2. SD + RTC Hardware Bringup | v1.2 | 2/2 | Complete | 2026-04-15 |
| 3. Data Model + Logging Task | v1.2 | 2/2 | Complete | 2026-04-16 |
| 4. CAN Health Status + Auto-Recovery | v1.3 | 3/3 | Complete | 2026-04-20 |
| 5. Error Management Infrastructure | v1.5 | 3/3 | Complete | 2026-04-20 |
| 6. Status Icons & Warning Bar | v1.5 | 3/3 | Complete | 2026-04-20 |
| 7. Critical Error Pop-up | v1.5 | 2/2 | Complete | 2026-04-20 |
| 8. Validation & Simulation | v1.5 | 2/2 | Complete | 2026-04-20 |
| 9. Filtering Utility | v1.6 | 3/3 | Complete | 2026-04-20 |
| 10. UI Integration | v1.6 | 3/3 | Complete | 2026-04-20 |
| 11. Validation & Performance | v1.6 | 3/3 | Complete | 2026-05-05 |
| 12. Screen Transition Validation | v1.7 | 2/2 | Complete | 2026-05-05 |
| 13. Shared Infrastructure Extraction | v1.7 | 1/1 | Complete | 2026-05-06 |
| 14. Per-Screen File Extraction | v1.7 | 3/3 | Complete | 2026-05-07 |
| 15. Post-Refactor Verification | v1.7 | 0/3 | Not started | - |
