/**
 * DashboardShared.h — Shared infrastructure for all dashboard screen files.
 *
 * Provides: color palette macros, temperature arc helpers, spinlock externs.
 * Any screen file (DriveScreen, ChargingScreen, future screens) includes this
 * directly. DashboardUI.h also includes it for backward compatibility.
 *
 * Rules: No widget handles. No screen-specific state. No DashboardState fields.
 */

#pragma once

#include <lvgl.h>
#include "freertos/FreeRTOS.h"

// ============================================================================
// COLOUR PALETTE (matches NewUI.cpp hex values, converted to LVGL 32-bit)
// ============================================================================

#define CLR_BG           lv_color_black()
#define CLR_WHITE        lv_color_white()
#define CLR_SPEED        lv_color_white()
#define CLR_STATUS_GREEN lv_color_make(0x24, 0xBE, 0x24)  // green-ish
#define CLR_POWER_POS    lv_color_make(0x24, 0xBE, 0x48)  // ~0x24BE
#define CLR_POWER_NEG    lv_color_make(0x4D, 0x6A, 0x4D)  // ~0x4D6A (regen)
#define CLR_BATT_TEMP    lv_color_make(0xF8, 0x71, 0x10)  // ~0xF8E2 warm
#define CLR_MOTOR_TEMP   lv_color_make(0x3E, 0xCA, 0x50)  // ~0x3F2A cool
#define CLR_MC_TEMP      lv_color_make(0xFE, 0x80, 0x00)  // ~0xFE40 orange
#define CLR_GYRO         lv_color_make(0xBD, 0xBD, 0xBD)  // ~0xBDF7 grey
#define CLR_WARN_RED     lv_color_make(0xF8, 0x20, 0x00)
#define CLR_WARN_YELLOW  lv_color_make(0xFE, 0xE0, 0x00)
#define CLR_DISABLED     lv_color_make(0x80, 0x80, 0x80)  // gray for boot/inactive
#define CLR_LOGO         lv_color_make(0x4B, 0x2C, 0x92)  // purple
#define CLR_MOTORING     lv_color_make(0x00, 0x80, 0xFF)  // blue  (positive current)
#define CLR_REGEN        lv_color_make(0x00, 0xC8, 0x00)  // green (negative current)
#define CLR_NEEDLE       lv_color_make(0xFF, 0x20, 0x00)  // red needle

// ============================================================================
// SPINLOCKS
// ============================================================================

// Definitions live in their respective .cpp files (DashboardUI.cpp and CAN_Receive.cpp).
extern portMUX_TYPE g_error_list_mux;    // Protects DashboardState.error_list updates/reads
extern portMUX_TYPE g_cell_voltages_mux; // Protects BatteryVoltages.hv_cell_voltages writes/reads

// ============================================================================
// HELPER: pick arc indicator colour based on temperature vs thresholds
// ============================================================================

static inline lv_color_t temp_arc_color(float temp, float warn, float crit)
{
    if (temp >= crit) return CLR_WARN_RED;
    if (temp >= warn) return CLR_WARN_YELLOW;
    return CLR_MOTOR_TEMP;
}

// ============================================================================
// HELPER: create a temperature arc (right column)
// ============================================================================

inline void create_temp_arc(lv_obj_t *parent, lv_obj_t **arc, lv_obj_t **label,
                            const char *title, lv_color_t color,
                            int min_val, int max_val,
                            lv_coord_t x, lv_coord_t y)
{
    *arc = lv_arc_create(parent);
    lv_obj_set_size(*arc, 110, 110);
    lv_arc_set_range(*arc, min_val, max_val);
    lv_arc_set_value(*arc, min_val);
    lv_arc_set_bg_angles(*arc, 135, 45);
    lv_obj_set_style_arc_color(*arc, lv_color_make(0x30, 0x30, 0x30), LV_PART_MAIN);
    lv_obj_set_style_arc_color(*arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(*arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(*arc, 10, LV_PART_INDICATOR);
    lv_obj_remove_style(*arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(*arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(*arc, x, y);

    // Value label centred inside the arc
    *label = lv_label_create(*arc);
    lv_label_set_text(*label, "--");
    lv_obj_set_style_text_color(*label, CLR_WHITE, 0);
    lv_obj_set_style_text_font(*label, &lv_font_montserrat_16, 0);
    lv_obj_center(*label);

    // Title label below the arc
    lv_obj_t *title_lbl = lv_label_create(parent);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, CLR_WHITE, 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align_to(title_lbl, *arc, LV_ALIGN_BOTTOM_MID, 0, 5);
}
