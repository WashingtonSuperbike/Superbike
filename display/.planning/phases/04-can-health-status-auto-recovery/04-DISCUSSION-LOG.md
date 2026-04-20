# Phase 4: CAN Health Status + Auto-Recovery - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-19
**Phase:** 04-can-health-status-auto-recovery
**Areas discussed:** can_status field type, No-data watchdog, Post-recovery state, can_icon placement

---

## can_status Field Type

| Option | Description | Selected |
|--------|-------------|----------|
| std::atomic<CanStatus> | Lock-free, follows sd_started pattern, consistent with existing code | ✓ |
| Spinlock (portMUX_TYPE) | Follows g_cell_voltages_mux pattern, more explicit | |
| You decide | Claude picks | |

**User's choice:** `std::atomic<CanStatus>` (recommended)
**Notes:** Consistent with existing `sd_started` atomic pattern.

---

## No-Data Watchdog

| Option | Description | Selected |
|--------|-------------|----------|
| Static inside waveshare_twai_receive() | Self-contained in CAN_Receive.cpp, no struct changes | ✓ |
| Field on DashboardState | More visible but grows struct across module boundaries | |
| You decide | Claude picks | |

**User's choice:** Static inside `waveshare_twai_receive()` (recommended)
**Notes:** Least invasive approach; watchdog is internal CAN logic, not dashboard data.

---

## Post-Recovery State

| Option | Description | Selected |
|--------|-------------|----------|
| NO_DATA immediately | Recovery succeeded, driver running, waiting for first frame. Yellow icon. | ✓ |
| Keep fault state until first frame | Stay red until RECEIVING. More conservative. | |
| You decide | Claude picks | |

**User's choice:** `NO_DATA` immediately (recommended)
**Notes:** Consistent with CANU-03 — RECEIVING only on actual decoded frame. Yellow tells rider driver restarted but no traffic yet.

---

## can_icon Placement

| Option | Description | Selected |
|--------|-------------|----------|
| Replace wifi_icon (x=50) | Dead widget, no logic, clean repurpose. No layout shifts. | ✓ |
| New slot after sd_icon | Insert between sd and wifi, shift others right | |
| You decide | Claude picks position | |

**User's choice:** Replace `wifi_icon` at x=50 (recommended)
**Notes:** `wifi_icon` has existed since Phase 1 with no update logic — confirmed dead widget.

---

## Claude's Discretion

- Whether to inline recovery logic or factor into private helpers in CAN_Receive.cpp
- Config params storage strategy for ERR_PASS reinstall (module-level statics recommended)
- twai_recv task stack size adjustment if needed
- Icon symbol choice (LV_SYMBOL_WIFI inherited, or substitute if better option exists)

## Deferred Ideas

- CANU-F01: No-data watchdog triggers driver restart (future requirement, out of v1.3 scope)
- CANU-F02: CAN message rate / FPS debug display (future)
- wifi_icon future: if WiFi added, slot repurposing will need revisiting
