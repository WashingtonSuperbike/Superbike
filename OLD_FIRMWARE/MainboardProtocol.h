/**
 * MainboardProtocol.h - Mainboard -> Display CAN contract (SYNCED COPY)
 *
 * >>> CANONICAL SOURCE: include/MainboardProtocol.h <<<
 * This is a byte-identical copy kept in the Arduino-IDE sketch folder because
 * OLD_FIRMWARE cannot reference include/ on its build path. Any change to the
 * canonical file MUST be mirrored here (and vice-versa) until OLD_FIRMWARE
 * migrates to PlatformIO and can share include/ directly.
 */

#pragma once

#include <stdint.h>

// ============================================================================
// CAN MESSAGE ID  (29-bit extended; mainboard is the only transmitter)
// ============================================================================
#define MAINBOARD_STATUS_IND  0x01AD0001UL

// Transmit/refresh period. The display watchdogs the mainboard against ~3x this.
#define MB_STATUS_PERIOD_MS   100   // 10 Hz

// If the display sees no MAINBOARD_STATUS frame for this long, it declares the
// mainboard offline ("MAINBOARD OFFLINE"). 5 missed frames.
#define MB_OFFLINE_TIMEOUT_MS (5 * MB_STATUS_PERIOD_MS)   // 500 ms

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
    MB_FAULT_IMD_INSULATION    = 11, // IMD: insulation resistance below response threshold (OKHS low, MHS 10/20 Hz)
    MB_FAULT_IMD_EARTH         = 12, // IMD: earth/Kl.31 connection fault (MHS 50 Hz)
    MB_FAULT_IMD_DEVICE        = 13, // IMD: internal device error (MHS 40 Hz)
    MB_FAULT_IMD_UNDERVOLTAGE  = 14, // IMD: undervoltage condition detected (MHS 20 Hz)
    MB_FAULT_IMD_TIMEOUT       = 15, // IMD: no valid OKHS/MHS activity within expected window (IMD unpowered/disconnected/stale)
    MB_FAULT_IMD_MISMATCH      = 16, // IMD: OKHS and MHS decode disagree - treat as unsafe
};
