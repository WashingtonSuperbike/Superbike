/**
 * Header file for the precharge FW + task
*/
#ifndef _PRECHARGE_H_
#define _PRECHARGE_H_

#include <Wire.h>
#include "arduino_freertos.h"
#include "avr/pgmspace.h"

#define PRECHARGE_TASK_STACK_SIZE configMINIMAL_STACK_SIZE + 2000

/// THIS NEEDS TO BE CHANGED TO OUR ACTUAL NUMBER OF LTCs
#define NUMBER_OF_LTCS 2
/// HV trip thresholds. MUST stay equal to the display's crit constants in
/// include/Constants.h (MC_TEMP_CRIT_CELSIUS / MOTOR_TEMP_CRIT_CELSIUS) so the
/// dashboard cannot show "green" while the mainboard trips HV. Raised from the
/// old 65/80 placeholders to the real hardware limits (HIL re-validation req'd).
#define MOTORCONTROLLER_TEMP_MAX 95   // == MC_TEMP_CRIT_CELSIUS
#define MOTOR_TEMP_MAX 110            // == MOTOR_TEMP_CRIT_CELSIUS

/// An enum for all the states. OFF, Precharge, ON, Error
enum HV_STATE {HV_OFF , HV_PRECHARGING, HV_ON, HV_ERROR};

/**
 * Just too many things in here. The packaged struct for processing preCharge data
 * This contains the BMS data, the motorData nad the cellVoltages, all good for
 * processing. Then there's about 15 variables for processing gyroscope data.
 * This can be reduced down to two: angle_X and angle_Y.
 */

typedef struct {
  float angle_X;
  float angle_Y;
  float RateRoll;
  float RatePitch;
  float RateYaw;

  float RateCalibrationRoll;
  float RateCalibrationPitch;
  float RateCalibrationYaw;

  int RateCalibrationNumber;
  float AccX;
  float AccY;
  float AccZ;
  float AngleRoll;
  float AnglePitch;

  float KalmanAngleRoll;
  float KalmanUncertaintyAngleRoll;
  float KalmanAnglePitch;
  float KalmanUncertaintyAnglePitch;
  float Kalman1DOutput[2];
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

// Accessors used by the CAN task to build the MAINBOARD_STATUS frame. Each
// returns a single word written only by preChargeTask, so reads are atomic on
// the M7 and need no lock.
HV_STATE get_hv_state();
uint8_t  get_hv_fault_reason();   // MB_FAULT_REASON ordinal
uint8_t  get_precharge_pct();     // 0..100
// I2C Accelerometer/Gyroscope access methods
void initI2C(GyroKalman *gyro_kalman);
void gyro_signals(GyroKalman *gyro_kalman);
void updateGyroData(GyroKalman *gyro_kalman);
void kalman_1d(float KalmanState, float KalmanUncertainty, float KalmanInput, float KalmanMeasurement, GyroKalman *gyro_kalman);

#endif // _PRECHARGE_H
