/**
 * Contains all precharge + HV related state machine controls.
 * Estimates gyro angle using Kalman filter.
*/
#include "Precharge.h"
#include <Wire.h>
#include "GPIO.h"

// I2C is incredibly unstable? Or perhaps not using proper wiring causes this,
// but the reading in precharge data can often bug out and output
// "nan" because of randomness? I would personally recommend
// having some sort of true or false based
// indicator at the bottom right of the speedometer screen
// that outputs "true" or something to indicate
// that the gyro is not bugging out. Or perhaps
// output the gyro data on the bottom right
// to indicate danger? Something like that.

// OKAY, MAKE SURE YOU READ THIS IF YOU SEE ISSUES WITH THE GYRO.
// By my *limited* understanding, I think the problem is that
// the gyro needs to be consistently powered, hence why proper
// gyro setup code has a significant delay between
// turning the thing on and actually reading data from it.
// An easy way to work around it, is to power on the Teensy
// and then once everything is up and running,
// reprogram it by using the button on the board.
// In the case of the actual race, I would turn on low-voltage
// and then wait a second and then turn it off and then
// turn it back on.

/* Current HV state */
static HV_STATE hv_state = HV_OFF;

// Returns true if the motor controller is done precharging.
// Returns false otherwise.
bool isPrecharged(PreChargeTaskData preChargeData) {

  BatteryVoltages battery_voltages = preChargeData.context->battery_voltages;
  MotorStats motor_stats = preChargeData.context->motor_stats;

  // Ret false if we haven't received all BMS cell voltages yet
  if (!(battery_voltages.hv_cell_voltages_ready)) {
    return false;
  }

  // Ret true if the difference between the main-accumulator-series-voltage and the
  // motorcontroller-voltage is less than 10% of the main-accumulator-series-voltage
  return ((battery_voltages.hv_series_voltage - motor_stats.motor_controller_battery_voltage) <=
          (battery_voltages.hv_series_voltage * 0.1))
          //and main-accumulator-voltage is greater than 80V (but this should be changed later as the bike voltage may be as low as ~60V).
          && battery_voltages.hv_series_voltage > 80;
}

// this function returns true if there are no HV errors detected on the bike
bool isHVSafe(PreChargeTaskData preChargeData) {
  //BMSStatus bmsStatus = preChargeData.bmsStatus;
  MotorTemps motor_temps = preChargeData.context->motor_temps;

  /* !!!! these are commented out now but all of this should be checked when using the real bike !!!!
    though you will have to determine which of these are emergency HV states. i.e. Which ones should turn off the contactor instantly
    and which ones should you simply alert the rider?
    As of now, they all immediately turn off the contactor which may be dangerous for the rider
  */

  //if (*bmsStatus.ltc_fault == 1) return 0;
  //if (*bmsStatus.ltc_count != NUMBER_OF_LTCS) return 0;
  //    the below if can be reduced to if (*bmsStatus.bms_c_fault) which returns true for any non-zero bms_c_fault value
  //if (*bmsStatus.bms_c_fault == 1 || *bmsStatus.bms_c_fault == 2 || *bmsStatus.bms_c_fault == 4 ||    //checks BMS fault error codes
  //    *bmsStatus.bms_c_fault == 8) return 0;
  //if (*bmsStatus.bms_status_flag == 1 || *bmsStatus.bms_status_flag == 2) return 0;  //check if cells are above or below the voltage cutoffs
  if (motor_temps.motor_controller_temperature >= MOTORCONTROLLER_TEMP_MAX
      || motor_temps.motor_temperature >= MOTOR_TEMP_MAX)       return 0;
  return 1;
}

const char* state_name(HV_STATE state) {
  switch (state) {
    case HV_OFF: return "HV_OFF";
    case HV_PRECHARGING: return "HV_PRECHARGING";
    case HV_ON: return "HV_ON";
    case HV_ERROR: return "HV_ERROR";
    default: return "UNKNOWN_STATE";
  }
}

// NOTE: "input" needs to change to the GPIO value for the on-button for the bike
void preChargeCircuitFSMTransitions (PreChargeTaskData preChargeData) {
  HV_STATE old_state = hv_state;
  GyroKalman *gyro_kalman = &preChargeData.context->gyro_kalman;
  switch (hv_state) { // transitions
    case HV_OFF:
      if (check_HV_toggle()) {
        hv_state = HV_PRECHARGING;
      }
      break;
    case HV_PRECHARGING:
      if (!check_HV_toggle()) {
        // kill-switch activated or HV switch turned off
        hv_state = HV_OFF;
      }
      else if (!isHVSafe(preChargeData)) {
        // HV error detected
        hv_state = HV_ERROR;
      }
      else if (isPrecharged(preChargeData)) {
        // finished precharging
        hv_state = HV_ON;
      }
      else {
        // no updates, keep precharging
        hv_state = HV_PRECHARGING;
      }
      break;
    case HV_ON:
      if (!check_HV_toggle()) {
        // kill-switch activated or HV switch turned off
        hv_state = HV_OFF;
      }
      else if (!isHVSafe(preChargeData) || gyro_kalman->yaw_angle > 45 || gyro_kalman->yaw_angle < -45 || gyro_kalman->roll_angle > 45 || gyro_kalman->roll_angle < -45) {
        // HV error detected
        hv_state = HV_ERROR;
      }
      else {
        // no updates, keep HV on
        hv_state = HV_ON;
      }
      break;
    case HV_ERROR:
      if (!check_HV_toggle()) {
        // kill-switch activated or HV switch turned off
        hv_state = HV_OFF;
      } else {
        // otherwise stay here
        hv_state = HV_ERROR;
      }
      break;
    default:
      hv_state = HV_OFF;
      break;
  } // transitions

  if (hv_state != old_state) {
    Serial.printf("HV transitioned from %s to %s state\n", state_name(old_state), state_name(hv_state));
  }
}

void preChargeCircuitFSMStateActions () {
  switch (hv_state) { // state actions
    case HV_OFF:
      open_contactor();
      open_precharge();
      break;
    case HV_PRECHARGING:
      open_contactor();
      close_precharge();
      break;
    case HV_ON:
      close_contactor();
      open_precharge();
      break;
    case HV_ERROR:
      open_contactor();
      open_precharge();
    default:
      break;
  } // state actions
}
