# Requirements: Superbike Display Module

**Defined:** 2026-04-19
**Core Value:** A self-contained display module that compiles, runs on hardware, and logs all telemetry to SD — the foundation everything else builds on.

## v1.3 Requirements (COMPLETE)

Requirements for the CAN Bus Integration milestone. CAN receive decoders and the twai_recv task are already wired; these requirements close the operational gaps.

### CAN Status UI

- [x] **CANU-01**: DashboardState has a can_status field with values: RECEIVING, NO_DATA, ERR_PASSIVE, BUS_OFF
- [x] **CANU-02**: Dashboard displays a can_icon: green when RECEIVING, yellow when NO_DATA, red when ERR_PASSIVE or BUS_OFF
- [x] **CANU-03**: can_status transitions to RECEIVING when any valid CAN message is decoded
- [x] **CANU-04**: can_status transitions to NO_DATA when no CAN message received within 3 seconds

### CAN Recovery

- [x] **CANR-01**: TWAI driver initiates bus-off recovery automatically on TWAI_ALERT_BUS_OFF
- [x] **CANR-02**: TWAI driver restarts automatically on TWAI_ALERT_ERR_PASS
- [x] **CANR-03**: can_status reflects the fault state (ERR_PASSIVE / BUS_OFF) before and during recovery attempt

## Future Requirements

### CAN Enhancements

- **CANU-F01**: No-data watchdog triggers TWAI driver restart (not just icon state)
- **CANU-F02**: CAN message rate counter / frames-per-second debug display
- **CANU-F03**: Gyro/lean angle columns added to CSV log (roll, pitch, yaw)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Gyro/IMU integration | Hardware not currently connected; separate milestone |
| Log rotation / max file size | Not needed for short sessions; revisit if card fills |
| PC log viewer / parser | Raw CSV works fine with spreadsheet tools |
| Bidirectional CAN (display → mainboard) | Display is listen-only; no transmit use case identified |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| CANU-01 | Phase 4 | SATISFIED |
| CANU-02 | Phase 4 | SATISFIED |
| CANU-03 | Phase 4 | SATISFIED |
| CANU-04 | Phase 4 | SATISFIED |
| CANR-01 | Phase 4 | SATISFIED |
| CANR-02 | Phase 4 | SATISFIED |
| CANR-03 | Phase 4 | SATISFIED |

**Coverage:**
- v1.3 requirements: 7 total
- Mapped to phases: 7
- Unmapped: 0 ✓

---
*Requirements defined: 2026-04-19*
*Last updated: 2026-04-20 after v1.3 completion*
