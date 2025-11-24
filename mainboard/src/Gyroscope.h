#pragma once

#include <Adafruit_MPU6050.h>

namespace superbike {
  class Gyroscope {
    public:
      typedef struct {
        float roll_angle;
        float pitch_angle;
        float yaw_angle;

        float roll_rate;
        float pitch_rate;
        float yaw_rate;
      
        float roll_accel;
        float pitch_accel;
        float yaw_accel;
      } GyroData;
    
      void InitGyroscope();
      GyroData GetGyroData() const { return _gyro_data; }
      void SetGyroData();

    private:
      GyroData _gyro_data;
  }
}

