/**
*/

#include <arduino_freertos.h>

using namespace arduino;

#include "avr/pgmspace.h"
#include "Main.h"
#include "CAN.h"
#include "DataLogging.h"
#include "config.h"
#include "Display.h"
#include "Precharge.h"
#include "GPIO.h"
#include <TimeLib.h>


static Context bike_context;
static Context *context = &bike_context;


TaskHandle_t *displayTaskHandle;
TaskHandle_t *canTaskHandle;
TaskHandle_t *dataloggingTaskHandle;
TaskHandle_t *prechargeTaskHandle;

TaskHandle_t *taskHandles[] = {displayTaskHandle, canTaskHandle, dataloggingTaskHandle, prechargeTaskHandle};

void setup() {

  initGPIO();

  Serial.begin(115200);

  initializeLogStructs();
  /* on brand new board, run File->Examples->Time->TimeTeensy3 and open the serial port. That will set the internal time to the real world clock */
  setTime(Teensy3Clock.get());

  /// Then this method starts the SD Card and prints the status if that works.
  Serial.print("Starting SD: ");
  if (startSD()) {
    Serial.println("SD successfully started");
    context->sd_started = 1;
  } else {
    context->sd_started = 0;
    Serial.println("Error starting SD card");
  }

  initDisplay(context);

  initCAN();

  /// Then this method calls on the setupI2C() method which just initializes the I2C communication protocol,
  /// setting the clock to 40KHz, reading in the initial values of the gyroscope, taking ~2 seconds
  /// worth of data to calibrate.
  initI2C(&context->gyro_kalman);

  /* Create each task (not started until scheduler starts).                                *
   * Each task has a priority, and higher priority tasks will preempt lower priority tasks */
  portBASE_TYPE s1, s2, s3, s4, s5;

  s1 = xTaskCreate(preChargeTask, "PRECHARGE TASK", PRECHARGE_TASK_STACK_SIZE, (void *)&context, 5, prechargeTaskHandle);
  // make sure to set CAN_NODES in config.h
  s2 = xTaskCreate(canTask, "CAN TASK", CAN_TASK_STACK_SIZE, (void *)&context, 4, canTaskHandle);
  s3 = xTaskCreate(idleTask, "IDLE_TASK", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  s4 = xTaskCreate(displayTask, "DISPLAY TASK", DISPLAY_TASK_STACK_SIZE, (void*)&context, 3, displayTaskHandle);
  s5 = xTaskCreate(dataLoggingTask, "DATA LOGGING TASK", DATALOGGING_TASK_STACK_SIZE, (void*)&context, 2, dataloggingTaskHandle);

  /* If any tasks failed to create, don't continue. */
  if (s1 != pdPASS || s2 != pdPASS || s3 != pdPASS || s4 != pdPASS || s5 != pdPASS) {
    Serial.printf("Failed to create tasks: %d %d %d %d %d", s1, s2, s3, s4, s5);
    while (1);
  }

  Serial.println("Starting the scheduler");
  vTaskStartScheduler();

  /* We should never hit this since scheduler is running tasks */
  Serial.println("Insufficient RAM");
}

void initializeLogStructs() {
  // Log all cell voltages in one file (CONFIG_DISPLAY_CELL_COUNT values per record)
  context->logs[0] = {CELL_VOLTAGES_LOG, CONFIG_DISPLAY_CELL_COUNT, context->battery_voltages.hv_cell_voltages, FLOAT};
  // Log total series voltage
  context->logs[1] = {SERIES_VOLTAGE_LOG, 1, &(context->battery_voltages.hv_series_voltage), FLOAT};
  // Log BMS status flag
  context->logs[2] = {BMS_STATUS_LOG, 1, &(context->bms_status.bms_status_flag), FLOAT};
}

time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

void idleTask(void *taskData) {
  while (1) {
    vTaskDelay((50 * configTICK_RATE_HZ) / 1000);
  }
}

void loop() {
}
