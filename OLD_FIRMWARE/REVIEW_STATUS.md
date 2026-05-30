# OLD_FIRMWARE Code Review — Fix Status

**Goal:** Validate firmware for live bike test. Focus: reliability, crash recovery, bug fixes.
**Build:** Compiles clean (one `-fpermissive` warning in FlexCAN_T4FD.tpp — benign, in system library).

---

## Applied Fixes

| # | Area | Description | Status |
|---|------|-------------|--------|
| 1 | GPIO.ino | Removed Display.h include + TFT/TS pin setup (display now a separate CAN node) | Done |
| 2 | CAN.h | Changed `bms_status_flag` from `float` to `int` (was silently truncating byte values) | Done |
| 3 | config.h | Added `CAN_TIMEOUT 2000` define; removed `USE_DEBUGGING_SCREEN` | Done |
| 4 | context.h | Added `last_bms_rx_tick` field to `Context` struct | Done |
| 5 | PreCharge.cpp `isHVSafe` | Replaced `==` equality with `&` bitwise AND for all fault flag checks | Done |
| 5 | PreCharge.cpp `isHVSafe` | Added BMS CAN staleness check (returns false if >2s since last BMS message) | Done |
| 5 | PreCharge.cpp `isHVSafe` | Added motor and motor controller overtemp checks | Done |
| 5 | CAN.cpp | Fixed thermistor index OOB write: `5 + ltcID` → `5 * ltcID` | Done |
| 5 | CAN.cpp | Added `last_bms_rx_tick` update in all BMS/cell-voltage/thermistor CAN cases | Done |
| 7 | OLD_FIRMWARE.ino | Added WDT_T4 hardware watchdog (5s timeout); `wdt_kick()` called from preChargeTask | Done |
| 8 | CAN.cpp `decipherCellsVoltage` | Fixed strict aliasing UB: replaced `uint16_t*` cast with `memcpy` | Done |
| 9 | CAN.cpp `canTask` | Fixed tick rollover: `xTaskGetTickCount() - last_request > pdMS_TO_TICKS(2000)` | Done |
| 10 | PreCharge.cpp FSM state actions | Added missing `break` in `HV_ERROR` case | Done |
| 11 | PreCharge.cpp FSM transitions | Added 10s precharge timeout → HV_ERROR if voltage not reached | Done |
| 12 | PreCharge.cpp `isHVSafe` | Added `Serial.printf` to each safety check to log which condition triggered HV_ERROR | Done |
| 13 | PreCharge.cpp | Added `debounced_HV_toggle()`: requires 3 stable reads (30ms) before accepting switch change | Done |
| 14 | OLD_FIRMWARE.ino | Removed uninitialized `TaskHandle_t*` pointer array; pass `NULL` for unused handles in `xTaskCreate` | Done |
| 15 | DataLogging.cpp `startSD` | Save/restore actual task priority instead of hardcoding `2` after SD init | Done |
| — | Compile | Fixed Wire namespace conflict: `#include <Wire.h>` first in Precharge.h + `extern TwoWire Wire` in PreCharge.cpp | Done |
| — | Compile | Added `constrain` macro guard in CAN.cpp before FlexCAN_T4.h include | Done |

---

## Skipped

| # | Area | Description | Reason |
|---|------|-------------|--------|
| 6 | PreCharge.cpp `initI2C` | 0x69 fallback: try both addresses before giving up | Skipped by user |

---

## All critical fixes complete. Ready for hardware validation.
