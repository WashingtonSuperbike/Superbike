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

#include "DashboardShared.h"
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

    // Watchdog Timestamps (ms)
    uint32_t motor_last_rx_ms;
    uint32_t bms_last_rx_ms;
    uint32_t charger_last_rx_ms;
    uint32_t evcc_last_rx_ms;

    // Mainboard status (MAINBOARD_STATUS_IND from the Teensy mainboard).
    // Atomics: written by the CAN task, read by the dashboard/error task.
    std::atomic<uint8_t> mb_hv_state{0};      // HV_STATE ordinal (0=OFF..3=ERROR)
    std::atomic<uint8_t> mb_fault_reason{0};  // MB_FAULT_REASON ordinal
    std::atomic<uint8_t> mb_precharge_pct{0}; // 0..100 (optional display)
    uint32_t mb_last_rx_ms;                   // watchdog: last MAINBOARD_STATUS rx

    // Error Management
    ErrorList     error_list;
    std::atomic<ErrorSeverity> bms_severity{ErrorSeverity::NONE};
    std::atomic<ErrorSeverity> mc_severity{ErrorSeverity::NONE};
    std::atomic<ErrorSeverity> mb_severity{ErrorSeverity::NONE};

    std::atomic<bool>      sd_started{};
    std::atomic<CanStatus> can_status{};  // D-01: lock-free CAN health state; default NO_DATA (ordinal 0)
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
