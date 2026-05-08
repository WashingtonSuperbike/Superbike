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
  if (!(battery_voltages.cell_voltages_ready)) {
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
      else if (!isHVSafe(preChargeData) || gyro_kalman->angle_Y > 45 || gyro_kalman->angle_Y < -45 || gyro_kalman->angle_X > 45 || gyro_kalman->angle_X < -45) {
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

void gyro_signals(GyroKalman *gyro_kalman) {
  // Read accelerometer and gyroscope data from MPU6050
  Wire.beginTransmission(0x68);
  Wire.write(0x3B); // starting register for accelerometer data
  byte error = Wire.endTransmission(false); // Keep connection active with restart
  if (error != 0) {
    return;
  }
  
  // Request 14 bytes: 6 accel + 2 temp + 6 gyro
  byte bytesReceived = Wire.requestFrom((uint8_t)0x68, (uint8_t)14, true);
  if (bytesReceived != 14) {
    return;
  }

  // Read accelerometer measurements (6 bytes)
  int16_t AccXLSB = Wire.read() << 8 | Wire.read();
  int16_t AccYLSB = Wire.read() << 8 | Wire.read();
  int16_t AccZLSB = Wire.read() << 8 | Wire.read();
  
  // Skip temperature readings (2 bytes)
  Wire.read();
  Wire.read();
  
  // Read gyro measurements (6 bytes)
  int16_t GyroX = Wire.read() << 8 | Wire.read();
  int16_t GyroY = Wire.read() << 8 | Wire.read();
  int16_t GyroZ = Wire.read() << 8 | Wire.read();

  // Convert measurement units to degree/second
  gyro_kalman->RateRoll = (float)GyroX / 65.5;
  gyro_kalman->RatePitch = (float)GyroY / 65.5;
  gyro_kalman->RateYaw = (float)GyroZ / 65.5;

  // Convert measurements from LSB to g (4096 LSB/g for +/-8g range)
  gyro_kalman->AccX = (float)AccXLSB / 4096;
  gyro_kalman->AccY = (float)AccYLSB / 4096;
  gyro_kalman->AccZ = (float)AccZLSB / 4096;

  // Calculate absolute angles
  float AccX = gyro_kalman->AccX;
  float AccY = gyro_kalman->AccY;
  float AccZ = gyro_kalman->AccZ;
  
  float denomRoll = sqrt(AccX * AccX + AccZ * AccZ);
  float denomPitch = sqrt(AccY * AccY + AccZ * AccZ);
  
  if (denomRoll > 0.001) {
    gyro_kalman->AngleRoll = (atan(AccY / denomRoll) * 57.2958) + 5;
  } else {
    gyro_kalman->AngleRoll = 0.0;
  }
  
  if (denomPitch > 0.001) {
    gyro_kalman->AnglePitch = -atan(AccX / denomPitch) * 57.2958;
  } else {
    gyro_kalman->AnglePitch = 0.0;
  }
}

void kalman_1d(float KalmanState, float KalmanUncertainty, float KalmanInput, float KalmanMeasurement, GyroKalman *gyro_kalman) {
  KalmanState = KalmanState + 0.004 * KalmanInput;
  KalmanUncertainty = KalmanUncertainty + 0.004 * 0.004 * 4 * 4;
  float KalmanGain = KalmanUncertainty * 1 / (1 * KalmanUncertainty + 3 * 3);
  KalmanState = KalmanState + KalmanGain * (KalmanMeasurement - KalmanState);
  KalmanUncertainty = (1 - KalmanGain) * KalmanUncertainty;
  // Output of filter
  gyro_kalman->Kalman1DOutput[0] = KalmanState;
  gyro_kalman->Kalman1DOutput[1] = KalmanUncertainty;
}

void updateGyroData(GyroKalman *gyro_kalman) {
  gyro_signals(gyro_kalman);
  
  gyro_kalman->RateRoll -= gyro_kalman->RateCalibrationRoll;
  gyro_kalman->RatePitch -= gyro_kalman->RateCalibrationPitch;
  gyro_kalman->RateYaw -= gyro_kalman->RateCalibrationYaw;

  // Calculate Roll angle (around x axis)
  kalman_1d(gyro_kalman->angle_X, gyro_kalman->KalmanUncertaintyAngleRoll, gyro_kalman->RateRoll, gyro_kalman->AngleRoll, gyro_kalman);
  gyro_kalman->angle_X = gyro_kalman->Kalman1DOutput[0];
  gyro_kalman->KalmanUncertaintyAngleRoll = gyro_kalman->Kalman1DOutput[1];

  // Calculate Pitch angle (around y-axis)
  kalman_1d(gyro_kalman->angle_Y, gyro_kalman->KalmanUncertaintyAnglePitch, gyro_kalman->RatePitch, gyro_kalman->AnglePitch, gyro_kalman);
  gyro_kalman->angle_Y = gyro_kalman->Kalman1DOutput[0];
  gyro_kalman->KalmanUncertaintyAnglePitch = gyro_kalman->Kalman1DOutput[1];
}

void initI2C(GyroKalman *gyro_kalman) {
  // Initialize all variables to 0
  gyro_kalman->angle_X = 0.0;
  gyro_kalman->angle_Y = 0.0;
  gyro_kalman->KalmanUncertaintyAngleRoll = 2.0;  // Start with some uncertainty
  gyro_kalman->KalmanUncertaintyAnglePitch = 2.0;

  Serial.println("\n[GYRO INIT] ========================================");
  Serial.println("[GYRO INIT] Starting I2C initialization...");
  
  Wire.begin();
  Wire.setClock(100000); // Use 100kHz (standard mode) for more reliability
  Serial.println("[GYRO INIT] I2C clock set to 100kHz");
  delay(250); // give delay for device to start

  // Scan I2C bus for devices
  Serial.println("[GYRO INIT] Scanning I2C bus...");
  int devicesFound = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("[GYRO INIT] I2C device found at address 0x%02X\n", address);
      devicesFound++;
    }
  }
  
  if (devicesFound == 0) {
    Serial.println("[GYRO INIT] ERROR: No I2C devices found!");
    Serial.println("[GYRO INIT] Check:");
    Serial.println("[GYRO INIT]   - MPU6050 is powered (VCC to 3.3V)");
    Serial.println("[GYRO INIT]   - SDA and SCL connections");
    Serial.println("[GYRO INIT]   - Pull-up resistors on SDA/SCL (4.7k)");
    return;
  }
  
  Serial.printf("[GYRO INIT] Found %d I2C device(s)\n", devicesFound);
  
  // Try address 0x68 first (AD0 = LOW)
  Serial.println("[GYRO INIT] Attempting to wake MPU6050 at 0x68...");
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0x00); // Set to 0 to wake up the MPU6050
  byte error = Wire.endTransmission(true);
  
  if (error != 0) {
    // Try address 0x69 (AD0 = HIGH)
    Serial.printf("[GYRO INIT] Address 0x68 failed (error %d), trying 0x69...\n", error);
    Wire.beginTransmission(0x69);
    Wire.write(0x6B);
    Wire.write(0x00);
    error = Wire.endTransmission(true);
    
    if (error != 0) {
      Serial.printf("[GYRO INIT] ERROR: MPU6050 not responding at 0x68 or 0x69 (error %d)\n", error);
      Serial.println("[GYRO INIT] Error codes: 0=success, 1=too long, 2=NACK address, 3=NACK data, 4=other");
      return;
    }
    
    // Success at 0x69 - update the address for all future operations
    Serial.println("[GYRO INIT] SUCCESS: MPU6050 found at address 0x69!");
    Serial.println("[GYRO INIT] NOTE: You need to change all 0x68 to 0x69 in the code!");
    return; // Exit for now so user can update the code
  }
  
  Serial.println("[GYRO INIT] MPU6050 awake at address 0x68");
  
  // Configure low pass filter (once at startup)
  Wire.beginTransmission(0x68);
  Wire.write(0x1A); // CONFIG register
  Wire.write(0x05); // Set DLPF to 10Hz cutoff
  Wire.endTransmission(true);
  Serial.println("[GYRO INIT] Low-pass filter configured");
  
  // Configure accelerometer range (once at startup)
  Wire.beginTransmission(0x68);
  Wire.write(0x1C); // ACCEL_CONFIG register
  Wire.write(0x10); // Set to +/-8g range
  Wire.endTransmission(true);
  Serial.println("[GYRO INIT] Accelerometer range set to +/-8g");
  
  // Configure gyroscope range (once at startup)
  Wire.beginTransmission(0x68);
  Wire.write(0x1B); // GYRO_CONFIG register
  Wire.write(0x08); // Set to +/-500 deg/s range
  Wire.endTransmission(true);
  Serial.println("[GYRO INIT] Gyroscope range set to +/-500 deg/s");
  
  // Give MPU6050 time to stabilize after configuration
  Serial.println("[GYRO INIT] Waiting for sensor to stabilize...");
  delay(500);
  
  // Re-wake sensor (some MPU6050 clones go back to sleep)
  Serial.println("[GYRO INIT] Re-waking sensor...");
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0x00); // Ensure awake
  Wire.endTransmission(true);
  delay(50);
  
  // Test read to verify sensor is readable
  Serial.println("[GYRO INIT] Testing sensor read...");
  Wire.beginTransmission(0x68);
  Wire.write(0x75); // WHO_AM_I register
  byte testError = Wire.endTransmission(false);
  if (testError != 0) {
    Serial.printf("[GYRO INIT] ERROR: Cannot communicate with sensor after config (error %d)\n", testError);
    Serial.println("[GYRO INIT] This may indicate:");
    Serial.println("[GYRO INIT]   - Faulty/clone MPU6050 chip");
    Serial.println("[GYRO INIT]   - Inadequate power supply");
    Serial.println("[GYRO INIT]   - Missing/weak pull-up resistors");
    return;
  }
  
  byte bytesRcvd = Wire.requestFrom((uint8_t)0x68, (uint8_t)1, true);
  if (bytesRcvd == 1) {
    byte whoami = Wire.read();
    Serial.printf("[GYRO INIT] WHO_AM_I register = 0x%02X (should be 0x68)\n", whoami);
    if (whoami != 0x68) {
      Serial.println("[GYRO INIT] WARNING: Unexpected WHO_AM_I value!");
    }
  } else {
    Serial.printf("[GYRO INIT] ERROR: Failed to read WHO_AM_I (got %d bytes)\n", bytesRcvd);
    return;
  }
  
  Serial.println("[GYRO INIT] Sensor read test successful!");
  Serial.println("[GYRO INIT] Starting calibration (2 seconds)...");

  // Perform gyroscope calibration measurements
  // 2000 milliseconds = 2 seconds to add all measured variables to calibration variables
  // This is important because this solves the issue of a non-zero rotation rate when stationary
  for (gyro_kalman->RateCalibrationNumber = 0; gyro_kalman->RateCalibrationNumber < 2000; gyro_kalman->RateCalibrationNumber++) {
    gyro_signals(gyro_kalman);
    gyro_kalman->RateCalibrationRoll += gyro_kalman->RateRoll;
    gyro_kalman->RateCalibrationPitch += gyro_kalman->RatePitch;
    gyro_kalman->RateCalibrationYaw += gyro_kalman->RateYaw;
    delay(1);
  }

  // Take average of calibrated rotation rate values from each direction
  gyro_kalman->RateCalibrationRoll /= 2000;
  gyro_kalman->RateCalibrationPitch /= 2000;
  gyro_kalman->RateCalibrationYaw /= 2000;
  //  *preChargeData.LoopTimer = micros();
}

void preChargeTask(void *taskData) {
  PreChargeTaskData preChargeData = *(PreChargeTaskData *)taskData;
  GyroKalman *gyro_kalman = &preChargeData.context->gyro_kalman;

  while (1) {
    preChargeCircuitFSMStateActions();
    preChargeCircuitFSMTransitions(preChargeData);

    updateGyroData(gyro_kalman);

    // 100 ms should be unnoticeable compared to other task updates
    // but should be fast to pick up errors / switch updates
    vTaskDelay((10 * configTICK_RATE_HZ) / 1000);
  }
}
