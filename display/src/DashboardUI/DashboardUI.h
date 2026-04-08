/**
 * DashboardUI.h - LVGL dashboard for 800x480 display
 *
 * Replaces the old Adafruit_ILI9341 display code with an LVGL-based
 * speedometer dashboard. All data fields from the original Display.h
 * are preserved and driven from a DashboardState struct.
 *
 * This header is self-contained — it does not depend on mainboard headers
 * (Context.h, Types.h, Config.h). The struct layouts mirror the mainboard's
 * superbike::Context so that CAN-received data can be copied directly once
 * inter-board communication is wired up.
 */

#pragma once

#include <lvgl.h>
#include <stdint.h>

// ============================================================================
// DRIVE-TRAIN CONSTANTS (from old firmware, used for speed calculation)
// ============================================================================

#define GEAR_RATIO    (48.0f / 16.0f)
#define WHEEL_DIAM_M  0.522f           // wheel diameter in metres
#define MPH_CONVERT   2.2369362920544f // m/s -> mph

// ============================================================================
// TEMPERATURE GAUGE RANGES
// ============================================================================

#define BATT_TEMP_MIN    0
#define BATT_TEMP_MAX   70
#define MOTOR_TEMP_MIN   0
#define MOTOR_TEMP_MAX 110
#define MC_TEMP_MIN      0
#define MC_TEMP_MAX     90

// ============================================================================
// THERMISTOR COUNT (matches mainboard Config.h)
// ============================================================================

#ifndef DASHBOARD_THERMISTOR_COUNT
#define DASHBOARD_THERMISTOR_COUNT 10
#endif

// ============================================================================
// DASHBOARD DATA MODEL
// ============================================================================
// These structs mirror the mainboard's superbike::Context sub-structs so
// that data received over CAN can be memcpy'd in without conversion.

struct DashboardMotorStats {
    float RPM;
    float motor_current;
    float motor_controller_battery_voltage;
    int   error_message;
};

struct DashboardMotorTemps {
    float throttle;
    float motor_controller_temperature;
    float motor_temperature;
    uint8_t controller_status;
};

struct DashboardBMSStatus {
    float bms_status_flag;
    int   bms_c_id;
    int   bms_c_fault;
    int   ltc_fault;
    int   ltc_count;
};

struct DashboardBatteryVoltages {
    float hv_series_voltage;
    float aux_battery_voltage;
};

struct DashboardGyroData {
    float roll_angle;
    float pitch_angle;
    float yaw_angle;
};

struct DashboardThermistorTemps {
    float temps[DASHBOARD_THERMISTOR_COUNT];
};

/**
 * Single struct holding every value the dashboard can display.
 * Populate from CAN, serial, or simulation — the UI doesn't care.
 */
struct DashboardState {
    DashboardMotorStats       motor;
    DashboardMotorTemps       temps;
    DashboardBMSStatus        bms;
    DashboardBatteryVoltages  battery;
    DashboardGyroData         gyro;
    DashboardThermistorTemps  thermistors;
    bool sd_started;
};

// ============================================================================
// WIDGET HANDLE STRUCT
// ============================================================================

/**
 * Holds every LVGL object pointer created by the dashboard so they can
 * be updated later without recreating them each frame.
 */
struct DashboardWidgets {
    // Speedometer meter
    lv_obj_t              *meter;
    lv_obj_t              *meter_speed_label;
    lv_meter_indicator_t  *current_motoring_indic;
    lv_meter_indicator_t  *current_regen_indic;

    // Needle as a standalone lv_line (avoids full-meter redraw on every update)
    lv_obj_t              *needle_line;
    lv_point_t             needle_pts[2];  // must persist — LVGL holds pointer

    // Temperature arcs (right column)
    lv_obj_t *batt_temp_arc;
    lv_obj_t *batt_temp_label;
    lv_obj_t *motor_temp_arc;
    lv_obj_t *motor_temp_label;
    lv_obj_t *mc_temp_arc;
    lv_obj_t *mc_temp_label;

    // Gyro arc
    lv_obj_t *gyro_arc;
    lv_obj_t *gyro_label;

    // Battery info (bottom-right)
    lv_obj_t *batt_voltage_label;
    lv_obj_t *batt_percent_label;
    lv_obj_t *batt_icon;

    // Stopwatch
    lv_obj_t *stopwatch_label;

    // Status icons (bottom strip)
    lv_obj_t *sd_icon;
    lv_obj_t *wifi_icon;
    lv_obj_t *warning_icon;
    lv_obj_t *info_icon;
    lv_obj_t *temp_warning_icon;
    lv_obj_t *logo_icon;

    // Error message
    lv_obj_t *error_label;

    // BMS status
    lv_obj_t *bms_status_label;
};

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * Build the full dashboard UI on the active screen.
 * Call once after lvgl_port_init(), inside lvgl_port_lock().
 */
void dashboard_create(void);

/**
 * Push the latest DashboardState values into the LVGL widgets.
 * Caller MUST hold the LVGL mutex (lvgl_port_lock).
 */
void dashboard_refresh(const DashboardState &state);

/**
 * FreeRTOS task entry point: reads DashboardState and refreshes the
 * dashboard at ~10 Hz. Pass a DashboardState* as the task parameter.
 */
void dashboardTask(void *param);
