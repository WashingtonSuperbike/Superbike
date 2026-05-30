/**
 * DriveScreen.h — Drive screen widgets and public API.
 *
 * DriveWidgets holds all LVGL object handles for the drive screen.
 * Include this in DriveScreen.cpp and DashboardUI.cpp only.
 */

#pragma once

#include "DashboardShared.h"
#include "DashboardUI.h"

// ============================================================================
// DRIVE SCREEN WIDGET HANDLES
// ============================================================================

struct DriveWidgets {
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

    // Status icons (bottom strip)
    lv_obj_t *sd_icon;
    lv_obj_t *can_icon;
    lv_obj_t *bms_status_label;
    lv_obj_t *mc_status_icon;

    // Superbike logo (bottom middle)
    lv_obj_t *logo_icon;

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
 * Build all drive screen LVGL widgets on `scr`.
 * Call from dashboard_create() after creating the drive screen object.
 */
void drive_screen_build(lv_obj_t *scr);

/**
 * Refresh drive screen widgets from the latest DashboardState.
 * Caller MUST hold the LVGL mutex (lvgl_port_lock).
 */
void drive_screen_refresh(const DashboardState &state);
