/**
 * Controls writing of variable values to CSV files stored on the SD card.
*/

#include <SdFat.h>
#include "DataLogging.h"
#include "config.h"
#include "context.h"
#include "arduino_freertos.h"
#include "avr/pgmspace.h"

const size_t BUF_DIM = 4096;

/// Represents the time that recording started
unsigned long epochTime;

//SdFat file system
SdFs sd;

void dataLoggingTask(void *dlData) {
  /// The dataLoggingTask() processes the data, for each of the logs.
  /// Just uses sdFat methods within the helper methods addRecord(), saveFiles() as
  /// required to process the neatly organized CSVWriter data and then save it.
  DataLoggingTaskData *dl = (DataLoggingTaskData *)dlData;
  Context *context = dl->context;
  CSVWriter *writers = context->logs;

  // SD init must happen after the scheduler starts — FIFO_SDIO uses FreeRTOS yield internally
  Serial.print("Starting SD: ");
  if (startSD()) {
    Serial.println("SD successfully started");
    context->sd_started = 1;
  } else {
    context->sd_started = 0;
    Serial.println("Error starting SD card");
  }

  const TickType_t epoch = xTaskGetTickCount();
  TickType_t last_save = xTaskGetTickCount();
  TickType_t now = xTaskGetTickCount();
  while (1) {
    if (context->sd_started) {
      now = xTaskGetTickCount();
      for (int i = 0; i < CONFIG_LOG_COUNT; i++) {
        addRecord(&writers[i], now - epoch);
      }
      if ((now - last_save) > 10000) {
        Serial.print("Saving files...");
        saveFiles(writers);
        Serial.println("saved");
        last_save = now;
      }
        // delay 50ms (change to modify how fast records are saved
      // in the future, this 'writeRate' could be specific to each data log
      // ex: motor current should write every 5ms but battery voltage only every 1 second
      // as battery voltage doesn't change very fast but current can
      vTaskDelay((50 * configTICK_RATE_HZ) / 1000);
    } else {
      Serial.println("SD card missing");
      // only print every 10 seconds, or until sd_card is fixed (not done currently)
      vTaskDelay((10000 * configTICK_RATE_HZ) / 1000);
    }
  }
}

bool startSD() {
  // Raise priority to prevent preemption during SDIO hardware init.
  // FIFO_SDIO init programs the SDHC peripheral state machine — a context
  // switch mid-init corrupts it and produces a bus fault on the next access.
  vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
  bool r = sd.begin(SdioConfig(FIFO_SDIO));
  vTaskPrioritySet(NULL, 2);  // restore DATA LOGGING TASK priority
  return r;
}

bool openFile(CSVWriter *writer) {
  writer->file = sd.open(writer->filename, O_RDWR | O_CREAT | O_TRUNC);
  writer->open = (bool)writer->file;
  if (!writer->open) {
    Serial.printf("ERROR: Failed to open file %s\n", writer->filename);
  }
  return writer->open;
}

void closeFile(CSVWriter *writer) {
  /// closeFile() closes the file inside the CSVWriter
  writer->file.close();
}

void saveFile(CSVWriter *writer) {
  /// saveFile() saves the file inside the CSVWriter
  writer->file.sync();
}

void saveFiles(CSVWriter *writers) {
  /// saveFiles() calls on saveFile() multiple times
  /// until all the files are saved.
  for (int i = 0; i < CONFIG_LOG_COUNT; i++) {
    saveFile(&writers[i]);
  }
}

void printFile(CSVWriter *writer) {
  /// Opens the file using openFile() and then prints out what is
  /// read in from the openFile() method to the Serial monitor
  if (!writer->open) {
    openFile(writer);
  }
  int data;
  while ((data = writer->file.read()) >= 0) Serial.write(data);
}

void addRecord(CSVWriter *writer, int sTime) {
  if (!writer->open) {
    if (!openFile(writer)) {
      return;
    }
  }

  writer->file.print(sTime);
  for (int i = 0; i < writer->dataValuesLen; i++) {
    writer->file.print(',');
    if (writer->D_TYPE == FLOAT) {
      writer->file.print(writer->dataValues[i]);
    } else {
      writer->file.print((int)writer->dataValues[i]);
    }
  }
  writer->file.println();
}
