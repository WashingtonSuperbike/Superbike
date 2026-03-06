/**
*/

#ifndef DATALOG_H_
#define DATALOG_H_

#include <SdFat.h>
#include "config.h"

/// Uses the configMINIMAL_STACK_SIZE variable in Main.h to add up to the stack size used for the dataLoggingTask()
#define DATALOGGING_TASK_STACK_SIZE 10000

//Names for each of the log files
/// The name for the cell voltages log
#define CELL_VOLTAGES_LOG "cell_voltages_log.csv"
/// The name for the series voltage log
#define SERIES_VOLTAGE_LOG "series_voltage_log.csv"
/// The name for the BMS status log
#define BMS_STATUS_LOG "bms_status_log.csv"

/**
 * Basic enum for data types for logging. Nothing else really.
 */
enum data_type {
  INT,
  FLOAT
};


/**
 * Represents a writer to a CSV log file on the sd card. Complex struct.
 * Keep it as it is, doesn't require changing really. Just use it.
 */
typedef struct {
  const char *filename;
  int dataValuesLen;
  //array of pointers to shared variables (the data values in the csv log)
  float *dataValues;
  data_type D_TYPE;
  bool open;
  SdFile file;
} CSVWriter;

#include "CAN.h"
#include "Precharge.h"
#include "context.h"

/**
 * A struct for the overall dataLoggingTaskData. Stores a pointer
 * to the pointer (array) of CSVWriters. Thus to process the logs,
 * you have to dereference the pointer and then call on the array by
 * index. The length just represents the number of logs and log files to generate.
 * This would be of interest to teams that might want to add more info in the
 * log SD card.
 */
typedef struct {
  Context *context;
} DataLoggingTaskData;

void dataLoggingTask(void *dlData);
bool startSD();
bool openFile(CSVWriter *writer);
void closeFile(CSVWriter *writer);
void saveFile(CSVWriter *writer);
void saveFiles(CSVWriter *writers);
void printFile(CSVWriter *writer);
void addRecord(CSVWriter *writer, int sTime);
#endif
