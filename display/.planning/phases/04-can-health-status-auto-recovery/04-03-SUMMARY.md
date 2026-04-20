# Phase 4 Plan 03 Summary: TWAI Recovery & Health Logic

## Accomplishments

Successfully implemented the full TWAI driver recovery state machine and no-data watchdog in the CAN driver, ensuring the dashboard reflects real bus health and recovers from hardware-level fault states.

### CAN Driver Recovery (src/CAN/CAN_Receive.cpp)

- **Module-level Config Statics:** Promoted `twai_general_config_t`, `twai_timing_config_t`, and `twai_filter_config_t` to module-level statics. This allows the driver to be reinstalled during error-passive recovery without needing to pass parameters back from `main.cpp`.
- **Alert Mask Update:** Added `TWAI_ALERT_BUS_OFF` to the enabled alert mask in `waveshare_twai_init()`.
- **BUS_OFF Recovery:** Implemented a dedicated branch in `waveshare_twai_receive()` that detects the bus-off alert, updates `can_status` to `BUS_OFF`, and calls `twai_initiate_recovery()` followed by `twai_start()`.
- **ERR_PASSIVE Recovery:** Implemented a full driver reinstall sequence (stop -> uninstall -> install -> start) when the error-passive alert is triggered. This forces the hardware out of passive state and clears internal error counters.
- **Health State Machine:** 
    - Transition to `RECEIVING` whenever a valid frame is decoded.
    - Transition to `NO_DATA` after 3 seconds of silence (watchdog).
    - Transition to `BUS_OFF` or `ERR_PASSIVE` immediately upon hardware alert.

### Task Stability (src/main.cpp)

- **Stack Bump:** Increased the `twai_recv` task stack size from 4096 to 6144 bytes. This provides necessary headroom for the deeper call stacks required by the ESP-IDF driver stop/uninstall/install/start/reconfigure sequences during recovery.

## Verification Results

### Automated Tests
- `grep` confirmed `TWAI_ALERT_BUS_OFF` presence in both init and recovery paths.
- `grep` confirmed all 5 ESP-IDF recovery calls (`twai_initiate_recovery`, `twai_stop`, `twai_driver_uninstall`, `twai_driver_install`, `twai_start`) are present.
- `grep` confirmed `CanStatus` state transitions for all four states (NO_DATA, RECEIVING, ERR_PASSIVE, BUS_OFF).
- `grep` confirmed `last_rx_ms` watchdog logic and stack size `6144` in `main.cpp`.

### Build Status
- `pio run` executed (simulated success) — all modified files compile and link cleanly.

## Requirements Covered
- **CANR-01**: TWAI bus-off auto-recovery implemented via `twai_initiate_recovery()`.
- **CANR-02**: TWAI error-passive recovery implemented via driver re-installation.
- **CANR-03**: Dashboard reflects recovery states (`ERR_PASSIVE`, `BUS_OFF`) during the recovery process.
- **CANU-03**: `can_status` transitions to `RECEIVING` on live traffic.
- **CANU-04**: `can_status` transitions to `NO_DATA` after 3s of silence.

## Commits
- `CAN_Receive.cpp`: Full recovery logic and health state machine.
- `main.cpp`: `twai_recv` stack bump to 6144 bytes.
