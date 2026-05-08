/**
 * ChargingScreen.h — Charging screen widgets and public API.
 *
 * ChargingWidgets holds all LVGL object handles for the charging screen.
 * Include this in ChargingScreen.cpp and DashboardUI.cpp only.
 */

#pragma once

#include "DashboardShared.h"
#include "DashboardUI.h"

// ============================================================================
// CHARGING SCREEN WIDGET HANDLES
// ============================================================================

struct ChargingWidgets {
    lv_obj_t *chg_batt_pct_label;
    lv_obj_t *chg_batt_pct_symbol; // separate "%" label — 144px font lacks the glyph
    lv_obj_t *chg_voltage_label;
    lv_obj_t *chg_current_label;
    lv_obj_t *chg_power_label;
    lv_obj_t *chg_temp_label;
    lv_obj_t *chg_avg_cell_v_label;
    lv_obj_t *chg_cell_stddev_label;
    lv_obj_t *chg_bms_max_temp_label;
    lv_obj_t *chg_eta_label;
    lv_obj_t *chg_status_label;
    lv_obj_t *chg_bar;
};

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * Build all charging screen LVGL widgets on `scr`.
 * Call from dashboard_create() after creating the charging screen object.
 */
void charging_screen_build(lv_obj_t *scr);

/**
 * Refresh charging screen widgets from the latest DashboardState.
 * Caller MUST hold the LVGL mutex (lvgl_port_lock).
 */
void charging_screen_refresh(const DashboardState &state);
