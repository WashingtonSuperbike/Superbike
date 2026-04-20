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
#include <atomic>
#include "Config.h"
#include "freertos/FreeRTOS.h"

// ============================================================================
// SYMBOLS
// ============================================================================
#define LV_SYMBOL_CAN_LINK "\xEF\x83\x81"

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
#define DASHBOARD_THERMISTOR_COUNT CONFIG_THERMISTOR_COUNT
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
    float hv_cell_voltages[CONFIG_HV_CELL_COUNT];  // populated by CAN receive path only
};

struct DashboardGyroData {
    float roll_angle;
    float pitch_angle;
    float yaw_angle;
};

struct DashboardThermistorTemps {
    float temps[DASHBOARD_THERMISTOR_COUNT];
};

// CAN bus health state — updated atomically by CAN_Receive task, read by dashboard task.
// Underlying type uint8_t guarantees lock-free std::atomic on Xtensa/ESP32-S3.
// BOOT = 0 so brace-initialisation of std::atomic<CanStatus>{} gives the correct default.
enum class CanStatus : uint8_t {
    BOOT        = 0,   // driver running, no decoded frame received yet (boot default)
    RECEIVING   = 1,   // at least one frame decoded within the last 1 s
    TIMEOUT     = 2,   // no frame decoded in last 1 s (after initial contact)
    ERR_PASSIVE = 3,   // TWAI entered error-passive; recovery in progress
    BUS_OFF     = 4,   // TWAI bus-off; recovery in progress
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
    std::atomic<bool>      sd_started{};
    std::atomic<CanStatus> can_status{};  // D-01: lock-free CAN health state; default NO_DATA (ordinal 0)
};

// Spinlock protecting hv_cell_voltages[CONFIG_HV_CELL_COUNT].
// Must be held by both the CAN write path and the logging read path.
// Defined in CAN_Receive.cpp; used in Logging/logging.cpp.
extern portMUX_TYPE g_cell_voltages_mux;

// ============================================================================
// WIDGET HANDLE STRUCT
// ============================================================================

/**
 * Holds every LVGL object pointer created by the dashboard so they can
 * be updated later without recreating them each frame.
 */
struct DashboardWidgets {
    // Speedometer dial (static image + dynamic overlays)
    lv_obj_t              *dial_img;
    lv_obj_t              *meter_speed_label;
    lv_obj_t              *current_motoring_arc;
    lv_obj_t              *current_regen_arc;

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
    lv_obj_t *can_icon;   // D-10: replaces wifi_icon; same x=50 position in status strip
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
