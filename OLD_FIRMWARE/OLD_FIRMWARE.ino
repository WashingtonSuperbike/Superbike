/**
*/

#include "arduino_freertos.h"

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

static void initWriter(CSVWriter *w, const char *filename, int len, float *data, data_type dtype) {
  w->filename     = filename;
  w->dataValuesLen = len;
  w->dataValues   = data;
  w->D_TYPE       = dtype;
  w->open         = false;
  // w->file is default-constructed in-place; do not copy-assign SdFile objects
}

void initializeLogStructs() {
  initWriter(&context->logs[0], MOTOR_TEMPERATURE_LOG,           1,  &(context->motor_temps.motor_temperature),              FLOAT);
  initWriter(&context->logs[1], MOTOR_CONTROLLER_TEMPERATURE_LOG, 1, &(context->motor_temps.motor_controller_temperature),   FLOAT);
  initWriter(&context->logs[2], MOTOR_CONTROLLER_VOLTAGE_LOG,    1,  &(context->motor_stats.motor_controller_battery_voltage), FLOAT);
  initWriter(&context->logs[3], MOTOR_CURRENT_LOG,               1,  &(context->motor_stats.motor_current),                  FLOAT);
  initWriter(&context->logs[4], RPM_LOG,                         1,  &(context->motor_stats.RPM),                             FLOAT);
  initWriter(&context->logs[5], THERMISTOR_LOG,                  10, context->thermistor_temps.temps,                        FLOAT);
  initWriter(&context->logs[6], BMS_VOLTAGE_LOG,                 1,  &(context->battery_voltages.hv_series_voltage),         FLOAT);
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
