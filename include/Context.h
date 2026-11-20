/**
 * Context.h - Bike system context and task data structures
 *
 * This file defines the main Context struct (formerly BikeStatus) that
 * contains all system state, as well as the task data structures used
 * by FreeRTOS tasks.
 */

#pragma once

#include "Types.h"
#include "Constants.h"
namespace superbike {
// ============================================================================
// MAIN SYSTEM CONTEXT
// ============================================================================

/**
 * Context (formerly BikeStatus)
 *
 * Central data structure containing all bike system state including
 * motor stats, charger stats, BMS data, gyroscope data, and logging state.
 * This is passed between FreeRTOS tasks via task data structures.
 */
struct Context {
  MotorStats motor_stats;
  MotorTemps motor_temps;
  ChargeControllerStats charge_controller_stats;
  ChargerStats charger_stats;
  BMSStatus bms_status;
  ThermistorTemps thermistor_temps;
  BatteryVoltages battery_voltages;
  GyroData gyro_data;
  bool sd_started;

  // Watchdog Timestamps (ms)
  uint32_t motor_last_rx_ms;
  uint32_t bms_last_rx_ms;
  uint32_t charger_last_rx_ms;
  uint32_t evcc_last_rx_ms;
};

// ============================================================================
// FREERTOS TASK DATA STRUCTURES
// ============================================================================
// Note: All task data structs are defined here to break circular dependencies
// between CAN.h, DataLogging.h, and Precharge.h

/**
 * CAN task data
 */
struct CANTaskData {
  Context *bike_context;
};

/**
 * Data logging task data
 */
struct DataLoggingTaskData {
  Context *context;
};

/**
 * Precharge task data
 */
struct PreChargeTaskData {
  Context *context;
};

/**
 * Display task data
 */
struct DisplayTaskData {
  Context *context;
};

}
