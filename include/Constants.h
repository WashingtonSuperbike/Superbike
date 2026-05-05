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
// MISC CONSTANTS
// ============================================================================
#define NUMBER_OF_LTCS 2   // Number of LTC (Linear Technology battery monitors) connected to the BMS

// ============================================================================
// CAN MESSAGE IDs
// ============================================================================

#define MOTOR_STATS_MSG           0x0CF11E05    // Motor controller message with stats
#define MOTOR_TEMPS_MSG           0x0CF11F05    // Motor controller message with temperatures
#define EVCC_STATS                0x18e54024    // Charge controller status (current,volt...)
#define CHARGER_STATS             0x18eb2440    // Thunderstruck Charger status (current,volt...)
#define DD_BMS_STATUS_IND         0x01dd0001    // BMS cell data message (overvolt,undervolt...)
#define DD_BMSC_TH_STATUS_IND     0x01df0e00    // Themistor values message
#define BMSC1_LTC1_CELLS_04       0x01df0900    // Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC1_CELLS_58       0x01df0a00    // Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC1_CELLS_912      0x01df0b00    // Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC2_CELLS_04       0x01df0901    // Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC2_CELLS_58       0x01df0a01    // Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC2_CELLS_912      0x01df0b01    // Convention: BMSC, LTC, CELL RANGE
#define BMSC1_LTC1_REQUEST_CELLS  0x01de0800    // BMS Request Cell Voltages LTC 1
#define BMSC1_LTC2_REQUEST_CELLS  0x01de0801    // BMS Request Cell Voltages LTC 2

// ============================================================================
// SAFETY THRESHOLDS
// ============================================================================

#define MC_TEMP_WARN_CELSIUS     70.0f
#define MC_TEMP_CRIT_CELSIUS     95.0f
#define MOTOR_TEMP_WARN_CELSIUS  95.0f
#define MOTOR_TEMP_CRIT_CELSIUS  110.0f
#define BATT_TEMP_WARN_CELSIUS   50.0f
#define BATT_TEMP_CRIT_CELSIUS   60.0f

// ============================================================================
// DRIVETRAIN CONSTANTS
// ============================================================================
#define GEAR_RATIO    (48.0 / 16.0)     // Gear ratio: 48 teeth in the back wheel sprocket, 16 on motor sprocket
#define WHEEL_DIAM_M  0.522f            // Wheel diameter in meters
#define MS_TO_MPH     2.2369362920544f  // Conversion factor for m/s to MPH
