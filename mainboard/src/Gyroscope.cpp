#include "Gyroscope.h"

void GyroSignals(GyroKalman *gyro_kalman) {
  // Read accelerometer and gyroscope data from MPU6050
  Wire.beginTransmission(0x68);
  Wire.write(0x3B); // starting register for accelerometer data
  byte error = Wire.endTransmission(false); // Keep connection active with restart
  if (error != 0) {
    return;
  }
  
  // Request 14 bytes: 6 accel + 2 temp + 6 gyro
  byte bytesReceived = Wire.requestFrom(0x68, 14, true);
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
  gyro_kalman->roll_rate = (float)GyroX / 65.5;
  gyro_kalman->pitch_rate = (float)GyroY / 65.5;
  gyro_kalman->yaw_rate = (float)GyroZ / 65.5;

  // Convert measurements from LSB to g (4096 LSB/g for +/-8g range)
  gyro_kalman->roll_accel = (float)AccXLSB / 4096;
  gyro_kalman->pitch_accel = (float)AccYLSB / 4096;
  gyro_kalman->yaw_rate = (float)AccZLSB / 4096;

  // Calculate absolute angles
  float roll_accel = gyro_kalman->roll_accel;
  float pitch_accel = gyro_kalman->pitch_accel;
  float yaw_rate = gyro_kalman->yaw_rate;
  
  float denomRoll = sqrt(roll_accel * roll_accel + yaw_rate * yaw_rate);
  float denomPitch = sqrt(pitch_accel * pitch_accel + yaw_rate * yaw_rate);
  
  if (denomRoll > 0.001) {
    gyro_kalman->AngleRoll = (atan(pitch_accel / denomRoll) * 57.2958) + 5;
  } else {
    gyro_kalman->AngleRoll = 0.0;
  }
  
  if (denomPitch > 0.001) {
    gyro_kalman->AnglePitch = -atan(roll_accel / denomPitch) * 57.2958;
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
  GyroSignals(gyro_kalman);
  
  gyro_kalman->roll_rate -= gyro_kalman->RateCalibrationRoll;
  gyro_kalman->pitch_rate -= gyro_kalman->RateCalibrationPitch;
  gyro_kalman->yaw_rate -= gyro_kalman->RateCalibrationYaw;

  // Calculate Roll angle (around x axis)
  kalman_1d(gyro_kalman->roll_angle, gyro_kalman->KalmanUncertaintyAngleRoll, gyro_kalman->roll_rate, gyro_kalman->AngleRoll, gyro_kalman);
  gyro_kalman->roll_angle = gyro_kalman->Kalman1DOutput[0];
  gyro_kalman->KalmanUncertaintyAngleRoll = gyro_kalman->Kalman1DOutput[1];

  // Calculate Pitch angle (around y-axis)
  kalman_1d(gyro_kalman->yaw_angle, gyro_kalman->KalmanUncertaintyAnglePitch, gyro_kalman->pitch_rate, gyro_kalman->AnglePitch, gyro_kalman);
  gyro_kalman->yaw_angle = gyro_kalman->Kalman1DOutput[0];
  gyro_kalman->KalmanUncertaintyAnglePitch = gyro_kalman->Kalman1DOutput[1];
}

void initI2C(GyroKalman *gyro_kalman) {
  // Initialize all variables to 0
  gyro_kalman->roll_angle = 0.0;
  gyro_kalman->yaw_angle = 0.0;
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
  
  byte bytesRcvd = Wire.requestFrom(0x68, 1, true);
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
    GyroSignals(gyro_kalman);
    gyro_kalman->RateCalibrationRoll += gyro_kalman->roll_rate;
    gyro_kalman->RateCalibrationPitch += gyro_kalman->pitch_rate;
    gyro_kalman->RateCalibrationYaw += gyro_kalman->yaw_rate;
    delay(1);
  }

  // Take average of calibrated rotation rate values from each direction
  gyro_kalman->RateCalibrationRoll /= 2000;
  gyro_kalman->RateCalibrationPitch /= 2000;
  gyro_kalman->RateCalibrationYaw /= 2000;
  //  *preChargeData.LoopTimer = micros();
}
