/**
 * MainboardProtocol.h - Mainboard -> Display CAN contract (CANONICAL COPY)
 *
 * Single source of truth for the status frame the Teensy mainboard broadcasts
 * for the ESP display. Define it ONCE here; the display (which has include/ on
 * its build path) includes this file directly.
 *
 * NOTE: the Arduino-IDE mainboard tree (OLD_FIRMWARE/) cannot see include/, so
 * it keeps a byte-identical synced copy at OLD_FIRMWARE/MainboardProtocol.h.
 * Any change here MUST be mirrored there until OLD_FIRMWARE moves to PlatformIO.
 */

#pragma once

#include <stdint.h>

// ============================================================================
// CAN MESSAGE ID  (29-bit extended; mainboard is the only transmitter)
// ============================================================================
#define MAINBOARD_STATUS_IND  0x01AD0001UL

// Transmit/refresh period. The display watchdogs the mainboard against ~3x this.
#define MB_STATUS_PERIOD_MS   100   // 10 Hz

// ============================================================================
// FRAME LAYOUT  (DLC = 4)
//   buf[0] hv_state        : HV_STATE ordinal (see below)
//   buf[1] fault_reason    : MB_FAULT_REASON
//   buf[2] rolling_counter : ++ every frame, wraps at 256 (liveness / freeze det.)
//   buf[3] precharge_pct   : 0..100
// ============================================================================
#define MB_STATUS_DLC             4
#define MB_STATUS_OFF_HV_STATE    0
#define MB_STATUS_OFF_FAULT       1
#define MB_STATUS_OFF_COUNTER     2
#define MB_STATUS_OFF_PRECHARGE   3

// HV_STATE ordinals carried in buf[0]. These MUST match enum HV_STATE on both
// boards (mainboard OLD_FIRMWARE/Precharge.h, display include/Types.h):
//   0 = HV_OFF, 1 = HV_PRECHARGING, 2 = HV_ON, 3 = HV_ERROR

// ============================================================================
// FAULT REASON  (buf[1]) — why the mainboard is in HV_ERROR.
// Codes on the wire; the display maps (hv_state, fault_reason) -> rider tip text.
// ============================================================================
enum MB_FAULT_REASON {
    MB_FAULT_NONE              = 0,  // no active fault
    MB_FAULT_BMS_TIMEOUT       = 1,  // no BMS status within CAN_BMS_TIMEOUT_MS
    MB_FAULT_BMS_OVERTEMP      = 2,  // BMS c_fault: thermistor overtemp / cell census
    MB_FAULT_BMS_VOLTAGE       = 3,  // BMS status flag: over/undervoltage cutoff
    MB_FAULT_LTC_FAULT         = 4,  // BMS reported an LTC fault
    MB_FAULT_LTC_COUNT         = 5,  // detected LTC count != configured
    MB_FAULT_MC_HV_FAULT       = 6,  // motor controller high-voltage fault
    MB_FAULT_MOTOR_OVERTEMP    = 7,  // motor over temperature
    MB_FAULT_MC_OVERTEMP       = 8,  // motor controller over temperature
    MB_FAULT_PRECHARGE_TIMEOUT = 9,  // precharge did not complete within timeout
    MB_FAULT_CAN_BUS           = 10, // CAN bus silent / bus-off (loss of bus)
};
