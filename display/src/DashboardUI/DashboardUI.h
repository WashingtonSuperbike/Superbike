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
#include "Constants.h"
#include "Types.h"
#include "freertos/FreeRTOS.h"

// ============================================================================
// SYMBOLS
// ============================================================================
#define LV_SYMBOL_CAN_LINK "\xEF\x83\x81"

// ============================================================================
// TEMPERATURE GAUGE RANGES
// ============================================================================

#define BATT_TEMP_MIN   (0)
#define BATT_TEMP_MAX   (BATT_TEMP_CRIT_CELSIUS + 5.0f)
#define MOTOR_TEMP_MIN  (0)
#define MOTOR_TEMP_MAX  (MOTOR_TEMP_CRIT_CELSIUS + 5.0f)
#define MC_TEMP_MIN     (0)
#define MC_TEMP_MAX     (MC_TEMP_CRIT_CELSIUS + 5.0f)

// ============================================================================
// ERROR MANAGEMENT
// ============================================================================

enum class ErrorSeverity : uint8_t {
    NONE = 0,
    INFO = 1,
    WARN = 2,
    CRIT = 3
};

enum class ErrorSource : uint8_t {
    BMS = 0,
    MOTOR_CONTROLLER = 1,
    MAINBOARD = 2
};

struct DashboardError {
    ErrorSource   source;
    ErrorSeverity severity;
    char          description[48];
    bool          is_active;
};

#define MAX_ACTIVE_ERRORS 8

struct ErrorList {
    DashboardError errors[MAX_ACTIVE_ERRORS];
    uint32_t       last_update_ms; // for carousel rotation
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
    MotorStats       motor;
    MotorTemps       temps;
    BMSStatus        bms;
    BatteryVoltages  battery;
    GyroData         gyro;
    ThermistorTemps  thermistors;
    ChargeControllerStats evcc;
    ChargerStats     charger;

    // Error Management
    ErrorList     error_list;
    std::atomic<ErrorSeverity> bms_severity{ErrorSeverity::NONE};
    std::atomic<ErrorSeverity> mc_severity{ErrorSeverity::NONE};
    std::atomic<ErrorSeverity> mb_severity{ErrorSeverity::NONE};

    std::atomic<bool>      sd_started{};
    std::atomic<CanStatus> can_status{};  // D-01: lock-free CAN health state; default NO_DATA (ordinal 0)
};

// Spinlocks
extern portMUX_TYPE g_cell_voltages_mux;
extern portMUX_TYPE g_error_list_mux; // Protects error_list updates/reads

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
    lv_obj_t *mc_voltage;
    lv_obj_t *mc_voltage_label;
    lv_obj_t *batt_voltage;
    lv_obj_t *batt_voltage_label;
    lv_obj_t *batt_percent;
    lv_obj_t *batt_icon;

    // Stopwatch
    lv_obj_t *stopwatch_label;

    // Status icons (bottom strip)
    lv_obj_t *sd_icon;
    lv_obj_t *can_icon;
    lv_obj_t *bms_status_label; // existing
    lv_obj_t *mc_status_icon;   // new for Phase 06

    // Superbike logo (bottom middle)
    lv_obj_t *logo_icon;

    // Error message
    lv_obj_t *error_label;

    // Warning carousel (bottom-right)
    lv_obj_t *warning_carousel_label;
    lv_obj_t *warning_carousel_icon;

    // Critical Error Pop-up
    lv_obj_t *error_modal;
    lv_obj_t *modal_title;
    lv_obj_t *modal_desc;
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
 * Scan the raw CAN error bits in DashboardState and update the 
 * internal error_list and system severities.
 */
void update_error_state(DashboardState *state);

/**
 * FreeRTOS task entry point: reads DashboardState and refreshes the
 * dashboard at ~30 Hz. Pass a DashboardState* as the task parameter.
 */
void dashboardTask(void *param);
