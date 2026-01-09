#pragma once

#include "Types.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

namespace superbike {
  class Gyroscope {
    public:
      Gyroscope();

      bool InitGyroscope();
      GyroData GetGyroData() const { return _gyro_data; }
      void UpdateGyroData();

    private:
      Adafruit_MPU6050 _mpu;
      GyroData _gyro_data;

      // Calibration offsets
      float _gyro_offset_x;
      float _gyro_offset_y;
      float _gyro_offset_z;

      void calibrate();
  };
}
