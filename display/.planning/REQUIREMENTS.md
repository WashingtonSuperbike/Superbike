# Requirements: Milestone v1.8 — UI Enhancements

## Milestone Goal

Implement regenerative braking amperage estimation and custom transition animations between drive and charging states.


---

## In Scope

### Regen Estimation

- [ ] **REGEN-01**: Implement a heuristic to estimate `regen_current` when `motor_current == 0` and `speed > 5 mph`.
- [ ] **REGEN-02**: Update `drive_screen_refresh` to use the estimated regen value for the green recovery bar.

---

## Future Requirements (deferred)

- **ANIM-01..04**: Custom Plug-in Animation Framework (GIF/media transition)
- UI layout redesign for drive screen — revisit after more riding data
- Charging screen layout improvements (e.g. cell voltage bar, time-to-full estimate)
- Touch input — hardware has no touch panel; re-evaluate if board changes

## Out of Scope

- Live charger hardware verification (Deferred from Phase 15)
- New motor controller firmware (regen must be estimated in display firmware)


---

## Traceability

| REQ-ID | Phase | Status |
|--------|-------|--------|
| REGEN-01 | Phase 16 | Shipped |
| REGEN-02 | Phase 16 | Shipped |
| ANIM-01 | Phase 17 | Deferred |
| ANIM-02 | Phase 17 | Deferred |
| ANIM-03 | Phase 17 | Deferred |
| ANIM-04 | Phase 17 | Deferred |
| REF-01..06 | Phase 14-15 | Shipped |
| VAL-01..08 | Phase 12-15 | Shipped (deferred VAL-02/03/07) |

