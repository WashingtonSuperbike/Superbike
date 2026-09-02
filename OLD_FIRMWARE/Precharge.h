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

// How long without a valid MHS edge before the IMD is considered unavailable
// (disconnected, unpowered, wiring fault). Comfortably exceeds the IMD's own
// worst-case response/self-test timing (~20s response + 10s self-test).
#define IMD_STALE_TIMEOUT_MS  30000

// Response value threshold (kOhm) - insulation resistance below this trips a
// fault. Matches the IMD's factory-configured R_an (see ordering info).
#define IMD_RESPONSE_VALUE_KOHM  100.0f

// Frequency tolerance bands (Hz) for classifying MHS carrier frequency.
// Centered on the IMD's five signaling frequencies (10/20/30/40/50 Hz per
// datasheet) with +/-2.5 Hz margin for capture jitter.
#define IMD_FREQ_10HZ_MIN   7.5f
#define IMD_FREQ_10HZ_MAX  12.5f
#define IMD_FREQ_20HZ_MIN  17.5f
#define IMD_FREQ_20HZ_MAX  22.5f
#define IMD_FREQ_30HZ_MIN  27.5f
#define IMD_FREQ_30HZ_MAX  32.5f
#define IMD_FREQ_40HZ_MIN  37.5f
#define IMD_FREQ_40HZ_MAX  42.5f
#define IMD_FREQ_50HZ_MIN  47.5f
#define IMD_FREQ_50HZ_MAX  52.5f

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

// Raw MHS capture data.
typedef struct {
  volatile uint32_t last_rise_us;
  volatile uint32_t period_us;       // time between rising edges (-> frequency)
  volatile uint32_t pulse_width_us;  // time high within the period (-> duty cycle)
  volatile uint32_t last_edge_tick;  // xTaskGetTickCount() at last edge, for staleness check
} IMD_RawCapture;

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

// IMD Functions

// Call once during setup, after GPIO/interrupt subsystems are intialized
void imd_init();

// Returns true if the IMD currently reports no fault (OKHS high, confirmed by
// MHS decode agreeing). False if faulted, stale, or mismatched. Mirrors
// get_hv_fault_reason()/get_precharge_pct() as a read-only status accessor.
bool get_imd_ok();

#endif // _PRECHARGE_H
