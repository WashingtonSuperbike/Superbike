/**
 * Contains all precharge + HV related state machine controls.
 * Estimates gyro angle using Kalman filter.
*/
#include "Precharge.h"
#include "GPIO.h"
#include "Main.h"
#include "MainboardProtocol.h"

extern TwoWire Wire;

/* Current HV state */
static HV_STATE hv_state = HV_OFF;

/* Why HV last became unsafe / errored. Written only by the FSM (isHVSafe and the
 * precharge-timeout branch); read by the CAN task via get_hv_fault_reason() to
 * populate the MAINBOARD_STATUS frame. */
static MB_FAULT_REASON hv_fault_reason = MB_FAULT_NONE;

/* Precharge progress 0..100, recomputed each loop; read by the CAN task. */
static volatile uint8_t mb_precharge_pct = 0;

/* Consistent, FSM-private copy of the CAN-written safety fields. Filled once per
 * loop under a critical section so isHVSafe()/isPrecharged() never read a struct
 * the CAN task was halfway through updating. File-scope (not on the task stack)
 * to keep the precharge stack small; only plain-old-data sub-structs are copied
 * into it (never logs[]/FsFile). Bug #3 fix. */
static Context precharge_snapshot;

/* Raw IMD MHS edge capture. Written only by mhsISR(); read only by
 * imd_check(), called from isHVSafe(). Not part of Context - the IMD is two
 * local GPIO/PWM pins, not CAN-sourced data. */
static IMD_RawCapture imd_raw = {0};

/* Debounced OKHS state - mirrors debounced_HV_toggle()'s pattern. Written by
 * debounced_okhs(), sampled once per precharge loop. Defaults to "ok" so an
 * unconfigured/not-yet-sampled state doesn't spuriously fault before first read. */
static bool imd_okhs_ok = true;

/* Last computed fault status, exposed via get_imd_ok(). */
static bool imd_currently_ok = true;

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

// Returns true if HV System is safe, false otherwise.
// Validated 05/29/2026 during Hardware In Loop (HIL) Testing.
// Any changes to below method require repeat of testing procedure.
// Do not remove comments relating to HIL testing identifiers.
bool isHVSafe(PreChargeTaskData preChargeData) {
  // Clear the reason on entry; each failing branch below records its own cause.
  hv_fault_reason = MB_FAULT_NONE;

  // The CAN bus must be alive. If no frame of any id has arrived within
  // CAN_BUS_TIMEOUT_MS (bus-off, transceiver/wiring fault), HV cannot be trusted.
  // Only enforced after the first frame ever received (boot is covered by the
  // ltc_count check below, which blocks HV until the BMS comes online).
  uint32_t last_can = preChargeData.context->last_can_rx_tick;
  if (last_can != 0 && (xTaskGetTickCount() - last_can) > pdMS_TO_TICKS(CAN_BUS_TIMEOUT_MS)) {
    hv_fault_reason = MB_FAULT_CAN_BUS;
    Serial.println("HV unsafe: CAN bus silent (possible bus-off)");
    return 0;
  }

  MB_FAULT_REASON imd_reason;
  bool imd_faulted = imd_check(&imd_reason);
  imd_currently_ok = !imd_faulted;
  if (imd_faulted) {
    hv_fault_reason = imd_reason;
    Serial.printf("HV unsafe: IMD fault (reason %d)\n", imd_reason);
    return 0;
  }

  // BMS must be actively sending messages. Only checked after first message received
  // (at boot, ltc_count == 0 != NUMBER_OF_LTCS already blocks HV until BMS comes online).
  uint32_t last_bms_rx = preChargeData.context->last_bms_rx_tick;
  if (last_bms_rx != 0 && (xTaskGetTickCount() - last_bms_rx) > pdMS_TO_TICKS(CAN_BMS_TIMEOUT_MS)) {
    hv_fault_reason = MB_FAULT_BMS_TIMEOUT;
    Serial.println("BMS timeout: no status message received");
    return 0;
  }

  if (preChargeData.context->bms_status.bms_c_fault & 0x06) {
    // BMS-1.2: Thermistor overtemp fault (0x02) or
    // BMS-1.8: Cell census fault (cell(s) not detected) (0x04)
    hv_fault_reason = MB_FAULT_BMS_OVERTEMP;
    Serial.printf("HV unsafe: BMS c_fault 0x%02X (overtemp/census)\n", preChargeData.context->bms_status.bms_c_fault);
    return 0;
  }

  if (preChargeData.context->bms_status.bms_status_flag & 0x03) {
    // BMS-1.3: High Voltage Cutoff (0x01) or
    // BMS-1.4: Low Voltage Cutoff reached (0x02)
    hv_fault_reason = MB_FAULT_BMS_VOLTAGE;
    Serial.printf("HV unsafe: voltage flag 0x%02X (over/undervoltage)\n", preChargeData.context->bms_status.bms_status_flag);
    return 0;
  }

  if (preChargeData.context->bms_status.ltc_fault != 0) {
    // BMS-1.9: LTC Fault detected
    hv_fault_reason = MB_FAULT_LTC_FAULT;
    Serial.println("HV unsafe: LTC fault");
    return 0;
  }

  if (preChargeData.context->bms_status.ltc_count != NUMBER_OF_LTCS) {
    // BMS-1.10: LTC Count does not match configuration
    hv_fault_reason = MB_FAULT_LTC_COUNT;
    Serial.printf("HV unsafe: LTC count %d (expected %d)\n", preChargeData.context->bms_status.ltc_count, NUMBER_OF_LTCS);
    return 0;
  }

  if (preChargeData.context->motor_stats.error_message & 0x02) {
    // MC-1.2: High Voltage Fault from Motor Controller
    hv_fault_reason = MB_FAULT_MC_HV_FAULT;
    Serial.println("HV unsafe: motor controller HV fault");
    return 0;
  }

  if (preChargeData.context->motor_temps.motor_temperature > MOTOR_TEMP_MAX) {
    // MC-1.5: Motor overtemperature
    hv_fault_reason = MB_FAULT_MOTOR_OVERTEMP;
    Serial.printf("HV unsafe: motor temp %.1f > %d C\n", preChargeData.context->motor_temps.motor_temperature, MOTOR_TEMP_MAX);
    return 0;
  }

  if (preChargeData.context->motor_temps.motor_controller_temperature > MOTORCONTROLLER_TEMP_MAX) {
    // MC-1.6: Motor controller overtemperature
    hv_fault_reason = MB_FAULT_MC_OVERTEMP;
    Serial.printf("HV unsafe: MC temp %.1f > %d C\n", preChargeData.context->motor_temps.motor_controller_temperature, MOTORCONTROLLER_TEMP_MAX);
    return 0;
  }

  // HV system passed safety checks
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

// --- Status accessors for the CAN task (MAINBOARD_STATUS frame) ---
HV_STATE get_hv_state()        { return hv_state; }
uint8_t  get_hv_fault_reason() { return (uint8_t)hv_fault_reason; }
uint8_t  get_precharge_pct()   { return mb_precharge_pct; }

// Estimate precharge progress as motor-controller-bus voltage / pack series
// voltage. 100% once HV is on, 0% when off/errored. Uses the FSM snapshot.
static uint8_t compute_precharge_pct(const Context *c) {
  if (hv_state == HV_ON) return 100;
  if (hv_state != HV_PRECHARGING) return 0;
  float series = c->battery_voltages.hv_series_voltage;
  if (series <= 0.0f) return 0;
  float pct = (c->motor_stats.motor_controller_battery_voltage / series) * 100.0f;
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return (uint8_t)pct;
}

// Debounces the HV toggle switch. Requires 3 consecutive stable reads (30ms at 10ms task rate)
// before accepting a state change, filtering out contact bounce and electrical noise.
static bool debounced_HV_toggle() {
  static uint8_t count = 0;
  static bool confirmed = false;
  bool raw = check_HV_toggle();
  if (raw != confirmed) {
    if (++count >= 3) {
      confirmed = raw;
      count = 0;
    }
  } else {
    count = 0;
  }
  return confirmed;
}

// NOTE: "input" needs to change to the GPIO value for the on-button for the bike
void preChargeCircuitFSMTransitions (PreChargeTaskData preChargeData) {
  HV_STATE old_state = hv_state;
  GyroKalman *gyro_kalman = &preChargeData.context->gyro_kalman;
  static TickType_t precharge_start_tick = 0;
  bool hv_on = debounced_HV_toggle();
  imd_okhs_ok = debounced_okhs();
  switch (hv_state) { // transitions
    case HV_OFF:
      if (hv_on) {
        hv_state = HV_PRECHARGING;
        precharge_start_tick = xTaskGetTickCount();
      }
      break;
    case HV_PRECHARGING:
      if (!hv_on) {
        // kill-switch activated or HV switch turned off
        hv_state = HV_OFF;
      }
      else if (xTaskGetTickCount() - precharge_start_tick > pdMS_TO_TICKS(10000)) {
        Serial.println("Precharge timeout: HV did not reach target voltage within 10s");
        hv_fault_reason = MB_FAULT_PRECHARGE_TIMEOUT;
        hv_state = HV_ERROR;
      }
      else if (!isHVSafe(preChargeData)) {
        hv_state = HV_ERROR;
      }
      else if (isPrecharged(preChargeData)) {
        hv_state = HV_ON;
      }
      else {
        hv_state = HV_PRECHARGING;
      }
      break;
    case HV_ON:
      if (!hv_on) {
        // kill-switch activated or HV switch turned off
        hv_state = HV_OFF;
      }
      else if (!isHVSafe(preChargeData)) {
        hv_state = HV_ERROR;
      }
      else {
        hv_state = HV_ON;
      }
      break;
    case HV_ERROR:
      if (!hv_on) {
        // kill-switch activated or HV switch turned off
        hv_state = HV_OFF;
      } else {
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
      break;
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

/* ISR for MHS pin CHANGE. Keep minimal: timestamp and store, nothing
 * else. All decoding happens later in imd_check(), on the precharge task's
 * own timeline, never inside the ISR. Keeps interrupt as short as possible */
static void mhsISR() {
  uint32_t now = micros();
  if (digitalReadFast(IMD_MHS_PIN) == HIGH) {
    imd_raw.period_us = now - imd_raw.last_rise_us;
    imd_raw.last_rise_us = now;
  } else {
    imd_raw.pulse_width_us = now - imd_raw.last_rise_us;
  }
  imd_raw.last_edge_tick = xTaskGetTickCountFromISR();
}

/* Debounces OKHS the same way debounced_HV_toggle() debounces the HV switch:
 * requires 3 consecutive stable reads (30ms at 10ms task rate) before
 * accepting a state change. */
static bool debounced_okhs() {
  static uint8_t count = 0;
  static bool confirmed = true;  // matches imd_okhs_ok's fail-open-until-sampled default
  bool raw = digitalReadFast(IMD_OKHS_PIN);
  if (raw != confirmed) {
    if (++count >= 3) {
      confirmed = raw;
      count = 0;
    }
  } else {
    count = 0;
  }
  return confirmed;
}

/* Classifies current IMD state into a fault reason, cross-checking OKHS
 * against the MHS frequency/duty decode (redundant signals per datasheet -
 * see design discussion). Returns true (and writes *reason_out) if unsafe;
 * false if the IMD confirms normal operation on both signals. */
static bool imd_check(MB_FAULT_REASON *reason_out) {
  uint32_t now_tick = xTaskGetTickCount();

  // Staleness: no MHS edge recently -> IMD unavailable regardless of OKHS.
  if (imd_raw.period_us == 0 ||
      (now_tick - imd_raw.last_edge_tick) > pdMS_TO_TICKS(IMD_STALE_TIMEOUT_MS)) {
    *reason_out = MB_FAULT_IMD_TIMEOUT;
    return true;
  }

  float freq_hz = 1000000.0f / (float)imd_raw.period_us;
  float dc_meas = (float)imd_raw.pulse_width_us / (float)imd_raw.period_us * 100.0f;

  bool freq_is_fault_band =
      (freq_hz >= IMD_FREQ_20HZ_MIN && freq_hz <= IMD_FREQ_20HZ_MAX) ||
      (freq_hz >= IMD_FREQ_40HZ_MIN && freq_hz <= IMD_FREQ_40HZ_MAX) ||
      (freq_hz >= IMD_FREQ_50HZ_MIN && freq_hz <= IMD_FREQ_50HZ_MAX);

  // At 10 Hz (normal), resistance can still breach the response threshold
  // via duty cycle even though the frequency band itself says "normal".
  bool insulation_low = false;
  if (freq_hz >= IMD_FREQ_10HZ_MIN && freq_hz <= IMD_FREQ_10HZ_MAX) {
    float r_f_kohm = (dc_meas - 5.0f) / 90.0f * 1200.0f;
    insulation_low = (r_f_kohm <= IMD_RESPONSE_VALUE_KOHM);
  }

  bool mhs_says_fault = freq_is_fault_band || insulation_low;

  if (imd_okhs_ok == mhs_says_fault) {
    // Disagreement between OKHS and MHS decode. Treat conservatively as
    // unsafe - this should not happen in normal operation; if it fires,
    // investigate wiring/decode logic rather than trusting either signal.
    Serial.printf("IMD mismatch: OKHS_ok=%d freq=%.1fHz dc=%.1f%%\n",
                  imd_okhs_ok, freq_hz, dc_meas);
    *reason_out = MB_FAULT_IMD_MISMATCH;
    return true;
  }

  if (!imd_okhs_ok) {
    // Both agree: real fault. Classify by frequency band.
    if (freq_hz >= IMD_FREQ_50HZ_MIN && freq_hz <= IMD_FREQ_50HZ_MAX) {
      *reason_out = MB_FAULT_IMD_EARTH;
    } else if (freq_hz >= IMD_FREQ_40HZ_MIN && freq_hz <= IMD_FREQ_40HZ_MAX) {
      *reason_out = MB_FAULT_IMD_DEVICE;
    } else if (freq_hz >= IMD_FREQ_20HZ_MIN && freq_hz <= IMD_FREQ_20HZ_MAX) {
      *reason_out = MB_FAULT_IMD_UNDERVOLTAGE;
    } else {
      *reason_out = MB_FAULT_IMD_INSULATION;
    }
    return true;
  }

  return false;  // both agree: no fault
}

bool get_imd_ok() { return imd_currently_ok; }

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

void imd_init() {
  pinMode(IMD_OKHS_PIN, INPUT);
  pinMode(IMD_MHS_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(IMD_MHS_PIN), mhsISR, CHANGE);
}

void preChargeTask(void *taskData) {
  PreChargeTaskData preChargeData = *(PreChargeTaskData *)taskData;
  Context *shared = preChargeData.context;
  GyroKalman *gyro_kalman = &shared->gyro_kalman;

  // The FSM evaluates safety against the snapshot, not the live shared Context.
  PreChargeTaskData snapData = { &precharge_snapshot };

  while (1) {
    // Reset the watchdog timer
    wdt_kick();

    // Atomically copy the CAN-written safety fields into the snapshot. The CAN
    // task (lower priority) updates these field-by-field and can be preempted
    // mid-write by this higher-priority task; the critical section guarantees a
    // self-consistent view for the safety checks below. The copies are small,
    // trivially-copyable PODs, so the section is only a few hundred ns.
    taskENTER_CRITICAL();
    precharge_snapshot.motor_stats      = shared->motor_stats;
    precharge_snapshot.motor_temps      = shared->motor_temps;
    precharge_snapshot.bms_status       = shared->bms_status;
    precharge_snapshot.battery_voltages = shared->battery_voltages;
    precharge_snapshot.last_bms_rx_tick = shared->last_bms_rx_tick;
    precharge_snapshot.last_can_rx_tick = shared->last_can_rx_tick;
    taskEXIT_CRITICAL();

    // Gyro is owned by this task (written below), so no race — copy outside the
    // critical section to keep snapData self-consistent for any tilt logic.
    precharge_snapshot.gyro_kalman = shared->gyro_kalman;

    preChargeCircuitFSMStateActions();
    preChargeCircuitFSMTransitions(snapData);

    // Refresh precharge progress for the CAN status frame (after the transition
    // so hv_state and the snapshot voltages are current this cycle).
    mb_precharge_pct = compute_precharge_pct(&precharge_snapshot);

    updateGyroData(gyro_kalman);

    // 100 ms should be unnoticeable compared to other task updates
    // but should be fast to pick up errors / switch updates
    vTaskDelay((10 * configTICK_RATE_HZ) / 1000);
  }
}
