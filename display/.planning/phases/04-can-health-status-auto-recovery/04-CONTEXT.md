# Phase 4: CAN Health Status + Auto-Recovery - Context

**Gathered:** 2026-04-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Add a `can_status` field (RECEIVING / NO_DATA / ERR_PASSIVE / BUS_OFF) to `DashboardState`, display a color-coded `can_icon` in the bottom status strip, implement a 3-second no-data watchdog, and add automatic TWAI driver recovery from error-passive and bus-off fault states — all without a firmware restart.

CAN decode logic (motor stats, BMS, cell voltages, thermistors) is already implemented and is out of scope for this phase.

</domain>

<decisions>
## Implementation Decisions

### can_status Field Type
- **D-01:** Add `std::atomic<CanStatus>` field to `DashboardState` (in `DashboardUI.h`). Follows the `sd_started` atomic pattern — lock-free, no spinlock overhead, consistent with existing cross-task state sharing.
- **D-02:** Define `CanStatus` as an `enum class` (RECEIVING, NO_DATA, ERR_PASSIVE, BUS_OFF) in `DashboardUI.h`. Default value: `NO_DATA` (not yet receiving at boot).

### No-Data Watchdog
- **D-03:** `last_rx_ms` lives as a `static` local inside `waveshare_twai_receive()`. Self-contained in `CAN_Receive.cpp` — no DashboardState struct changes needed beyond the `can_status` field. On each call, compare `millis() - last_rx_ms > 3000` to detect silence. Update `last_rx_ms` when any message is successfully decoded (inside `handle_rx_message`).
- **D-04:** The POLLING_RATE_MS alert wait (currently 1000ms) means the watchdog check fires at most once per second — acceptable; 3s threshold is well above the wake interval.

### TWAI Alert Set
- **D-05:** Add `TWAI_ALERT_BUS_OFF` to the `twai_reconfigure_alerts` call in `waveshare_twai_init()`. Current set is missing BUS_OFF (needed for CANR-01). Keep existing: `TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL`.

### Recovery Sequences (locked by requirements)
- **D-06:** BUS_OFF recovery: set `can_status = BUS_OFF`, then call `twai_initiate_recovery()` + `twai_start()`. On success → `can_status = NO_DATA`.
- **D-07:** ERR_PASS recovery: set `can_status = ERR_PASSIVE`, then call `twai_stop()` → `twai_driver_uninstall()` → `twai_driver_install()` → `twai_start()`. On success → `can_status = NO_DATA`.
- **D-08:** `can_status` reflects the fault state (ERR_PASSIVE / BUS_OFF) before and during the recovery attempt (CANR-03). After successful `twai_start()`, status transitions to `NO_DATA` immediately — RECEIVING only fires when a real decoded message arrives (CANU-03).

### Post-Recovery State
- **D-09:** After either recovery path completes successfully, `can_status` becomes `NO_DATA` immediately. The yellow icon tells the rider the driver restarted but no traffic has arrived yet. Consistent with CANU-03 (RECEIVING requires an actual decoded frame).

### can_icon UI
- **D-10:** Replace the unused `wifi_icon` widget (currently at x=50, always white, no update logic) with `can_icon`. Same x/y position. Rename field in `DashboardWidgets` from `wifi_icon` to `can_icon`.
- **D-11:** Color mapping: RECEIVING → `CLR_RPM_ARC` (green, same green used for motor current arc); NO_DATA → `CLR_WARN_YELLOW`; ERR_PASSIVE or BUS_OFF → `CLR_WARN_RED`. Consistent with established STATUS-01 color convention.
- **D-12:** Icon symbol: `LV_SYMBOL_WIFI` (inherited from wifi_icon slot) is acceptable if no better CAN symbol exists in LVGL v8. Claude may substitute a more appropriate symbol (e.g., a custom label "CAN") if it renders cleanly at the icon font size.

### Claude's Discretion
- Whether to inline recovery logic directly in `waveshare_twai_receive()` or factor it into private helpers in `CAN_Receive.cpp`.
- Exact `twai_driver_install` config parameters reused during ERR_PASS restart (same g_config / t_config / f_config as initial init — or stored as module-level statics).
- Stack size for `twai_recv` task if recovery logic increases depth.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Data Model
- `src/DashboardUI/DashboardUI.h` — `DashboardState` (add `std::atomic<CanStatus> can_status`), `DashboardWidgets` (rename `wifi_icon` → `can_icon`), color constants

### CAN Driver
- `src/CAN/CAN_Receive.h` — public API (`waveshare_twai_init`, `waveshare_twai_receive`)
- `src/CAN/CAN_Receive.cpp` — alert handling, recovery sequences, no-data watchdog location
- ESP-IDF TWAI API: `twai_initiate_recovery()`, `twai_stop()`, `twai_driver_uninstall()`, `twai_driver_install()`, `twai_start()`, `twai_reconfigure_alerts()`

### Dashboard UI
- `src/DashboardUI/DashboardUI.cpp` — `dashboard_create()` (icon creation, `wifi_icon` slot), `dashboard_refresh()` (status icon update logic)

### Requirements
- `.planning/REQUIREMENTS.md` §v1.3 — CANU-01 through CANU-04, CANR-01 through CANR-03 (all 7 requirements for this phase)

### Entry Point
- `src/main.cpp` — `twai_recv` task spawn, task stack size

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `std::atomic<bool> sd_started` in `DashboardState`: direct template for `std::atomic<CanStatus> can_status`
- `create_icon_label()` in `DashboardUI.cpp`: existing helper for creating bottom-strip icon widgets
- `CLR_WARN_RED` / `CLR_WARN_YELLOW` / `CLR_RPM_ARC`: color constants already defined for green/yellow/red
- `twai_reconfigure_alerts()` call in `waveshare_twai_init()`: add `TWAI_ALERT_BUS_OFF` here
- `twai_read_alerts()` + alert flag checks in `waveshare_twai_receive()`: recovery logic slots in alongside existing ERR_PASS / BUS_ERROR handling

### Established Patterns
- Cross-task state: `std::atomic<T>` for single-field flags (sd_started model)
- Cross-task state: `portMUX_TYPE` spinlock for multi-field writes (g_cell_voltages_mux model) — not needed here since can_status is a single atomic field
- Icon update pattern: `lv_obj_set_style_text_color(w.sd_icon, CLR_*, 0)` inside `dashboard_refresh()`
- Tasks on Core 0 for hardware I/O; `twai_recv` already pinned to Core 0

### Integration Points
- `DashboardUI.h`: add `CanStatus` enum + `std::atomic<CanStatus> can_status` field
- `DashboardUI.cpp`: replace wifi_icon creation with can_icon; add can_status → color mapping in `dashboard_refresh()`
- `CAN_Receive.cpp`: add BUS_OFF to alert set; add recovery sequences; add watchdog; update can_status field on state pointer
- `main.cpp`: no structural changes expected; may need stack size bump for twai_recv if recovery adds depth

</code_context>

<specifics>
## Specific Ideas

- The `wifi_icon` widget has existed since Phase 1 with no update logic — confirmed dead weight. Repurposing it as `can_icon` is a clean rename, not a behavioral change.
- `POLLING_RATE_MS` is currently 1000ms (1-second alert wait timeout). The watchdog fires on each `waveshare_twai_receive()` wake — worst-case latency to detect NO_DATA is 1s beyond the 3s threshold (acceptable).
- ERR_PASS recovery requires full driver reinstall (stop/uninstall/install/start) per ESP-IDF TWAI docs — not just `twai_start()`. Config params for reinstall should be stored as module-level statics so they're available without re-passing from main.

</specifics>

<deferred>
## Deferred Ideas

- **CANU-F01** (future): No-data watchdog triggers TWAI driver restart (not just icon state) — out of scope for v1.3, noted in REQUIREMENTS.md future requirements.
- **CANU-F02** (future): CAN message rate counter / FPS debug display.
- **wifi_icon future use**: If WiFi is ever added, a new slot or widget rename would be needed — for now the slot is being repurposed for CAN.

</deferred>

---

*Phase: 04-can-health-status-auto-recovery*
*Context gathered: 2026-04-19*
