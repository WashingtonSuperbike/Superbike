/**
 * Types.h - Cross-board type definitions
 *
 * This file contains type definitions that can be shared across multiple
 * hardware boards (mainboard, display board, hardware-in-loop, etc.).
 * These are primarily CAN data structures and other common types used
 * throughout the superbike firmware.
 */

#pragma once

#include <stdint.h>
#include "Config.h"

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// Forward declarations for task data structs (defined in Context.h)
struct Context;
struct CANTaskData;
struct DataLoggingTaskData;
struct PreChargeTaskData;
struct DisplayTaskData;

// ============================================================================
// BASIC TYPE ALIASES
// ============================================================================

typedef uint8_t byte;

// ============================================================================
// ENUMS
// ============================================================================

/**
 * Data type enum for data logging
 */
enum data_type {
  INT,
  FLOAT
};

/**
 * High voltage system state enum
 */
enum HV_STATE {
  HV_OFF,
  HV_PRECHARGING,
  HV_ON,
  HV_ERROR
};

/**
 * Print type enum for display data formatting
 */
enum PRINT_TYPE {
  NUMBER,
  ARRAY,
  BOOL
};

// ============================================================================
// MOTOR CONTROLLER DATA STRUCTURES (CAN)
// ============================================================================

/**
 * Motor controller statistics from CAN bus
 *
 * RPM: RPM reported by motor controller
 * motor_current: current used by motor (verify if motor or controller current)
 * motor_controller_battery_voltage: voltage applied to HV input terminals
 * error_message: error status bits reported by controller
 */
struct MotorStats {
  float RPM;
  float motor_current;
  float motor_controller_battery_voltage;
  int error_message;
};

/**
 * Motor and controller temperatures from CAN bus
 *
 * throttle: throttle applied (range TBD after testing)
 * motor_controller_temperature: temperature of motor controller in Celsius
 * motor_temperature: temperature of motor in Celsius
 * controller_status: status bits reported by controller (not errors)
 */
struct MotorTemps {
  float throttle;
  float motor_controller_temperature;
  float motor_temperature;
  byte controller_status;
};

// ============================================================================
// CHARGER DATA STRUCTURES (CAN)
// ============================================================================

/**
 * Charge controller statistics from CAN bus
 *
 * en: if charge controller has charging enabled
 * charge_voltage: voltage the charge controller is telling charger to use
 * charge_current: current the charge controller is telling charger to use
 */
struct ChargeControllerStats {
  byte en;
  float charge_voltage;
  float charge_current;
};

/**
 * Thunderstruck charger statistics from CAN bus
 *
 * status_flag + charge_flag: see Thunderstruck engineers' documentation
 * output_voltage: voltage applied by charger
 * output_current: current applied by charger
 * charger_temp: charger temperature
 */
struct ChargerStats {
  byte status_flag;
  byte charge_flag;
  float output_voltage;
  float output_current;
  int8_t charger_temp;
};

// ============================================================================
// BMS (BATTERY MANAGEMENT SYSTEM) DATA STRUCTURES (CAN)
// ============================================================================

/**
 * BMS status from CAN bus
 *
 * bms_status_flag: each bit represents an error (check datasheet)
 * bms_c_id: cell ID that is reporting a fault
 * bms_c_fault: fault related to cell mentioned above
 * ltc_fault: LTC error status (check datasheet)
 * ltc_count: how many LTCs detected (should be 2 for 20s pack)
 */
struct BMSStatus {
  // TODO: Change bms_status_flag to int after display feature is completed
  float bms_status_flag;
  int bms_c_id;
  int bms_c_fault;
  int ltc_fault;
  int ltc_count;
};

/**
 * Thermistor temperatures from BMS
 *
 * temps_valid: if temp has been received over CAN by BMS
 * temps: temperature of thermistor reported by BMS
 */
struct ThermistorTemps {
  bool temps_valid[CONFIG_THERMISTOR_COUNT];
  float temps[CONFIG_THERMISTOR_COUNT];
};

/**
 * Battery cell voltages from BMS
 *
 * hv_cell_voltages_ready: true if cell voltage reported by BMS since boot
 * hv_cell_voltages: HV cell voltages sent by BMS over CAN
 * hv_series_voltage: sum of hv_cell_voltages, updated on CAN receive
 * aux_battery_voltage: auxiliary (LV) battery voltage
 */
struct BatteryVoltages {
  bool hv_cell_voltages_ready;
  float hv_cell_voltages[CONFIG_HV_CELL_COUNT];
  float hv_series_voltage;
  float aux_battery_voltage;
};

// ============================================================================
// DISPLAY DATA STRUCTURES
// ============================================================================

/**
 * Printed data structure for display
 *
 * Stores all info for a specific type of data being displayed including
 * position, type, value pointer, old value, and label.
 */
struct PrintedDataStruct {
  int labelX, y, dataX;       // y is same for data and label, X differs
  volatile float oldData;     // volatile for some printed data
  PRINT_TYPE type;
  volatile float* currData;   // volatile for some printed data
  int dataLen;
  const char* labelPtr;       // Label text
};

/**
 * Printed data structure for thermistor array display
 *
 * Similar to PrintedDataStruct but adjusted for thermistor array
 * using a float array as oldData instead of single float.
 */
struct PrintedDataThermStruct {
  int labelX, y, dataX;         // y is same for data and label, X differs
  volatile float oldData[16];   // volatile for some printed data
  volatile float* currData;     // volatile for some printed data
  const char* labelPtr;         // Label text
};

/**
 * Printed data structure for time display
 *
 * Work in progress - to be better integrated with time update system.
 */
struct PrintedDataTimeStruct {
  int labelX, y, dataX;         // y is same for data and label, X differs
  volatile float oldData[10];   // volatile for some printed data
  volatile float* currData;     // volatile for some printed data
  const char* labelPtr;         // Label text
};

/**
 * Gyroscope Data Structure
 */
typedef struct {
  float roll_angle;
  float pitch_angle;
  float yaw_angle;

  float roll_rate;
  float pitch_rate;
  float yaw_rate;

  float roll_accel;
  float pitch_accel;
  float yaw_accel;
} GyroData;
