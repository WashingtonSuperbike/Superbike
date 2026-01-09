/**
 * DataLogging.h - SD card data logging functionality
 *
 * Header file for data logging task that writes sensor data to SD card
 * CSV files.
 */

#pragma once

#include "Config.h"
#include "Context.h"
#include "Constants.h"
#include "../GPIO/Pins.h"
#include "SD_MMC.h"

// ============================================================================
// LOG FILE NAMES
// ============================================================================

/// The name for the motor temperature log
#define MOTOR_TEMPERATURE_LOG "motor_temperature_log.csv"

/// The name for the motor controller temperature log
#define MOTOR_CONTROLLER_TEMPERATURE_LOG "mc_temperature_log.csv"

/// The name for the BMS voltage log
#define BMS_VOLTAGE_LOG "bms_voltage_log.csv"

/// The name for the motor controller voltage log
#define MOTOR_CONTROLLER_VOLTAGE_LOG "mc_voltage_log.csv"

/// The name for the motor controller current log
#define MOTOR_CURRENT_LOG "current_log.csv"

/// The name for the thermistor temperature log
#define THERMISTOR_LOG "thermistor_log.csv"

/// The name for the motor RPM log
#define RPM_LOG "rpm_log.csv"

// ============================================================================
// DATA LOGGING STRUCTURES
// ============================================================================

/**
 * CSV writer for SD card data logging
 *
 * Represents a writer to a CSV log file on the SD card.
 */
struct CSVWriter {
  const char *filename;
  int dataValuesLen;
  float *dataValues;  // Array of pointers to shared variables (data values)
  data_type D_TYPE;
  bool open;
  File file;
};

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Forward declaration (struct defined in Context.h)
struct DataLoggingTaskData;

void dataLoggingTask(void *dlData);
bool startSD();
bool openFile(CSVWriter *writer);
void closeFile(CSVWriter *writer);
void saveFile(CSVWriter *writer);
void saveFiles(CSVWriter *writers);
void printFile(CSVWriter *writer);
void addRecord(CSVWriter *writer, int sTime);
