# Roadmap: Superbike Display Module

## Milestones

- ✅ **v1.0 Build & Flash** — Phase 1 (hardware bringup complete, 2026-04-03)
- ✅ **v1.1 Speedometer Dial** — Phase 1 (lv_meter speedometer complete, 2026-04-03)
- ✅ **v1.2 SD Card Support** — Phases 2-3 (shipped 2026-04-16)
- ✅ **v1.3 CAN Bus Integration** — Phase 4 (shipped 2026-04-20)

---

## Phases

<details>
<summary>✅ v1.1 Speedometer Dial (Phase 1) — SHIPPED 2026-04-03</summary>

- [x] Phase 1: Speedometer Meter (1/1 plans) — completed 2026-04-03

</details>

<details>
<summary>✅ v1.2 SD Card Support (Phases 2-3) — SHIPPED 2026-04-16</summary>

- [x] Phase 2: SD + RTC Hardware Bringup (2/2 plans) — completed 2026-04-15
- [x] Phase 3: Data Model + Logging Task (2/2 plans) — completed 2026-04-16

Full details: `.planning/milestones/v1.2-ROADMAP.md`

</details>

<details>
<summary>✅ v1.3 CAN Bus Integration (Phase 4) — SHIPPED 2026-04-20</summary>

- [x] Phase 4: CAN Health Status + Auto-Recovery (3/3 plans) — completed 2026-04-20

</details>

## Phase Details

### Phase 4: CAN Health Status + Auto-Recovery
**Goal**: The dashboard reflects live CAN bus health and the TWAI driver recovers automatically from error-passive and bus-off faults
**Depends on**: Phase 3 (DashboardState data model, LVGL dashboard widget infrastructure)
**Requirements**: CANU-01, CANU-02, CANU-03, CANU-04, CANR-01, CANR-02, CANR-03
**Success Criteria** (what must be TRUE):
  1. The dashboard displays a can_icon that is green when CAN messages are actively arriving, yellow when no message has arrived in 3 seconds, and red when the bus is in error-passive or bus-off state
  2. Riding with a live CAN bus: can_icon turns green within one message interval and stays green while traffic flows
  3. Disconnecting the CAN bus while running: can_icon turns yellow after 3 seconds of silence, then red if TWAI escalates to error-passive or bus-off
  4. On TWAI_ALERT_BUS_OFF: the driver calls twai_initiate_recovery() + twai_start() and resumes receiving frames without a firmware restart
  5. On TWAI_ALERT_ERR_PASS: the driver calls twai_stop() / uninstall / install / start and resumes receiving frames without a firmware restart
**Plans**: 3 plans
Plans:
- [x] 04-01-PLAN.md — DashboardUI.h data model: CanStatus enum, can_status atomic field, can_icon widget rename
- [x] 04-02-PLAN.md — DashboardUI.cpp can_icon creation and 3-way color refresh logic
- [x] 04-03-PLAN.md — CAN_Receive.cpp recovery sequences, watchdog, alert mask; main.cpp stack bump
**UI hint**: yes

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Speedometer Meter | v1.1 | 1/1 | Complete | 2026-04-03 |
| 2. SD + RTC Hardware Bringup | v1.2 | 2/2 | Complete | 2026-04-15 |
| 3. Data Model + Logging Task | v1.2 | 2/2 | Complete | 2026-04-16 |
| 4. CAN Health Status + Auto-Recovery | v1.3 | 3/3 | Complete | 2026-04-20 |
