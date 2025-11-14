/**
 * Header file for the precharge FW + task
*/
#ifndef _PRECHARGE_H_
#define _PRECHARGE_H_

//ESP32 already has FreeRTOS built in by default, so no need to include it separately
//#include "arduino_freertos.h"
//This header is for AVR microcontrollers,ESP32 doesn’t use AVR memory mapping(reads flash data natively)
//#include "avr/pgmspace.h"

#define PRECHARGE_TASK_STACK_SIZE configMINIMAL_STACK_SIZE + 2000

/// THIS NEEDS TO BE CHANGED TO OUR ACTUAL NUMBER OF LTCs
#define NUMBER_OF_LTCS 2
/// Motor controller temperature max. This might need to be changed
/// depending on how high the MCU hits during operation.
#define MOTORCONTROLLER_TEMP_MAX 65
/// Motor temperature max. This might need to be changed
/// depending on how high the motor hits during operation.
#define MOTOR_TEMP_MAX 80

/// An enum for all the states. OFF, Precharge, ON, Error
enum HV_STATE {HV_OFF , HV_PRECHARGING, HV_ON, HV_ERROR};

/**
 * Just too many things in here. The packaged struct for processing preCharge data
 * This contains the BMS data, the motorData nad the cellVoltages, all good for
 * processing. Then there's about 15 variables for processing gyroscope data.
 * This can be reduced down to two: angle_X and angle_Y.
 */

//3 accel values, 3 rate values
//Matches MPU6050 library data exactly, easy to maintain and readable
typedef struct {
  float angle_X;
  float angle_Y;
  float RateRoll;
  float RatePitch;
  float RateYaw;

  float AccX;
  float AccY;
  float AccZ;
  float AngleRoll;
  float AnglePitch;
} GyroKalman;

#include "CAN.h"
#include "DataLogging.h"
#include "context.h"

typedef struct {
  Context *context;
} PreChargeTaskData;

void preChargeTask(void *taskData);
bool isPrecharged(PreChargeTaskData preChargeData);
bool isHVSafe(PreChargeTaskData preChargeData);
const char* state_name(HV_STATE state);
// I2C Accelerometer/Gyroscope access methods
void initI2C(GyroKalman *gyro_kalman);
void gyro_signals(GyroKalman *gyro_kalman);
void updateGyroData(GyroKalman *gyro_kalman);
void kalman_1d(float KalmanState, float KalmanUncertainty, float KalmanInput, float KalmanMeasurement, GyroKalman *gyro_kalman);

#endif // _PRECHARGE_H
