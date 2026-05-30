#pragma once

#include "CAN.h"
#include "DataLogging.h"
#include "Precharge.h"

typedef struct _Context {
    MotorStats motor_stats;
    MotorTemps motor_temps;
    ChargeControllerStats charge_controller_stats;
    ChargerStats charger_stats;
    BMSStatus bms_status;
    ThermistorTemps thermistor_temps;
    BatteryVoltages battery_voltages;
    CSVWriter logs[CONFIG_LOG_COUNT];
    GyroKalman gyro_kalman;
    bool sd_started;
    // Tick count of the last received DD_BMS_STATUS_IND message. 0 = never received.
    uint32_t last_bms_rx_tick;
    // Tick count of the last received CAN frame of ANY id. 0 = never received.
    // Drives the CAN-bus liveness watchdog (isHVSafe) and recovery (canTask).
    uint32_t last_can_rx_tick;
} Context;
