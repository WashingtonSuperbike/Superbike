/**
 * Constants.h - Cross-board constants
 *
 * This file contains constants that are shared across multiple hardware
 * boards, primarily CAN message IDs and protocol-level definitions.
 * Board-specific constants (pins, task sizes, etc.) are in each board's
 * folder (e.g., mainboard/src/Pins.h).
 */

#pragma once

#include <stdint.h>

// ============================================================================
// CAN MESSAGE IDs
// ============================================================================

/// Motor controller message - CAN
#define MOTOR_STATS_MSG 0x0CF11E05
/// Motor controller message - CAN
#define MOTOR_TEMPS_MSG 0x0CF11F05
/// Charge controller status (current,volt...)
#define EVCC_STATS 0x18e54024
/// Thunderstruck Charger status (current,volt...)
#define CHARGER_STATS 0x18eb2440
/// BMS cell data message (overvolt,undervolt...)
#define DD_BMS_STATUS_IND 0x01dd0001
/// Themistor values message
#define DD_BMSC_TH_STATUS_IND 0x01df0e00
/// Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC1_CELLS_04  0x01df0900
/// Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC1_CELLS_58  0x01df0a00
/// Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC1_CELLS_912 0x01df0b00
/// Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC2_CELLS_04  0x01df0901
/// Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC2_CELLS_58  0x01df0a01
/// Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC2_CELLS_912 0x01df0b01
/* BMS Request Cell Voltages LTC 1 */
#define BMSC1_LTC1_REQUEST_CELLS 0x01de0800
/* BMS Request Cell Voltages LTC 2 */
#define BMSC1_LTC2_REQUEST_CELLS 0x01de0801
