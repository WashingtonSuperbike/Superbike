/**
 * Gyroscope.cpp - MPU6050 gyroscope/accelerometer implementation
 *
 * Uses the Adafruit MPU6050 library to read gyroscope and accelerometer data.
 */

#include "Gyroscope.h"
#include <Arduino.h>

namespace superbike {

// Constructor
Gyroscope::Gyroscope() :
  _gyro_offset_x(0.0f),
  _gyro_offset_y(0.0f),
  _gyro_offset_z(0.0f)
{
  // Initialize GyroData to zero
  _gyro_data.roll_angle = 0.0f;
  _gyro_data.pitch_angle = 0.0f;
  _gyro_data.yaw_angle = 0.0f;
  _gyro_data.roll_rate = 0.0f;
  _gyro_data.pitch_rate = 0.0f;
  _gyro_data.yaw_rate = 0.0f;
  _gyro_data.roll_accel = 0.0f;
  _gyro_data.pitch_accel = 0.0f;
  _gyro_data.yaw_accel = 0.0f;
}

bool Gyroscope::InitGyroscope() {
  Serial.println("\n[GYRO INIT] ========================================");
  Serial.println("[GYRO INIT] Starting MPU6050 initialization...");

  // Try to initialize MPU6050
  if (!_mpu.begin()) {
    Serial.println("[GYRO INIT] ERROR: Failed to find MPU6050 chip!");
    return false;
  }

  Serial.println("[GYRO INIT] MPU6050 found!");

  // Set accelerometer range to ±8g
  _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.println("[GYRO INIT] Accelerometer range set to ±8G");

  // Set gyroscope range to ±500 degrees/sec
  _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.println("[GYRO INIT] Gyroscope range set to ±500 deg/s");

  // Set filter bandwidth to 21 Hz
  _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("[GYRO INIT] Filter bandwidth set to 21 Hz");

  delay(100);

  Serial.println("[GYRO INIT] Starting calibration (2 seconds)...");
  Serial.println("[GYRO INIT] Keep the sensor still!");

  // Calibrate the gyroscope
  calibrate();

  Serial.println("[GYRO INIT] Calibration complete!");
  Serial.print("[GYRO INIT] Gyro offsets: X=");
  Serial.print(_gyro_offset_x);
  Serial.print(", Y=");
  Serial.print(_gyro_offset_y);
  Serial.print(", Z=");
  Serial.println(_gyro_offset_z);
  Serial.println("[GYRO INIT] ======================================== \n");

  return true;
}

void Gyroscope::calibrate() {
  const int num_samples = 2000;
  float sum_x = 0.0f;
  float sum_y = 0.0f;
  float sum_z = 0.0f;

  // Collect samples
  for (int i = 0; i < num_samples; i++) {
    sensors_event_t accel, gyro, temp;
    _mpu.getEvent(&accel, &gyro, &temp);

    sum_x += gyro.gyro.x;
    sum_y += gyro.gyro.y;
    sum_z += gyro.gyro.z;

    delay(1);
  }

  // Calculate average offsets
  _gyro_offset_x = sum_x / num_samples;
  _gyro_offset_y = sum_y / num_samples;
  _gyro_offset_z = sum_z / num_samples;
}

void Gyroscope::UpdateGyroData() {
  // Get new sensor events
  sensors_event_t accel, gyro, temp;
  _mpu.getEvent(&accel, &gyro, &temp);

  // Update gyroscope rates (in rad/s, apply calibration offsets)
  _gyro_data.roll_rate = gyro.gyro.x - _gyro_offset_x;
  _gyro_data.pitch_rate = gyro.gyro.y - _gyro_offset_y;
  _gyro_data.yaw_rate = gyro.gyro.z - _gyro_offset_z;

  // Update accelerometer data (in m/s^2)
  // Note: MPU6050 acceleration is in m/s^2, we store as g's (divide by 9.81)
  _gyro_data.roll_accel = accel.acceleration.x / 9.81f;
  _gyro_data.pitch_accel = accel.acceleration.y / 9.81f;
  _gyro_data.yaw_accel = accel.acceleration.z / 9.81f;

  // Calculate roll and pitch angles from accelerometer (in degrees)
  // Roll: rotation around X axis (uses Y and Z accelerations)
  // Pitch: rotation around Y axis (uses X and Z accelerations)

  float accel_x = accel.acceleration.x;
  float accel_y = accel.acceleration.y;
  float accel_z = accel.acceleration.z;

  // Calculate roll angle (rotation around X-axis)
  // Range: -180 to +180 degrees
  _gyro_data.roll_angle = atan2(accel_y, accel_z) * 57.2958f; // 180/PI = 57.2958

  // Calculate pitch angle (rotation around Y-axis)
  // Range: -90 to +90 degrees
  float accel_magnitude = sqrt(accel_y * accel_y + accel_z * accel_z);
  _gyro_data.pitch_angle = atan2(-accel_x, accel_magnitude) * 57.2958f;

  // Yaw angle cannot be determined from accelerometer alone (requires magnetometer)
  // For now, integrate yaw rate (simple integration, will drift over time)
  // dt = 0.01 seconds (assuming 100 Hz update rate)
  static unsigned long last_update = 0;
  unsigned long now = millis();
  if (last_update > 0) {
    float dt = (now - last_update) / 1000.0f;  // Convert ms to seconds
    _gyro_data.yaw_angle += _gyro_data.yaw_rate * 57.2958f * dt;  // Convert rad/s to deg/s

    // Keep yaw angle in -180 to +180 range
    if (_gyro_data.yaw_angle > 180.0f) {
      _gyro_data.yaw_angle -= 360.0f;
    } else if (_gyro_data.yaw_angle < -180.0f) {
      _gyro_data.yaw_angle += 360.0f;
    }
  }
  last_update = now;
}

} // namespace superbike
