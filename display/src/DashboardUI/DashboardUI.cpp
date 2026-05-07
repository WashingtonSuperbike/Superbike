/**
 * DashboardUI.cpp - LVGL dashboard for 800x480 (Waveshare ESP32-S3-Touch-LCD-5)
 *
 * Layout modelled after NewUI.cpp (320x240 TFT mockup) but expanded for the
 * larger screen and using native LVGL widgets (arcs, bars, labels).
 *
 * ┌──────────────────────────────────────────────────────────┐
 * │  SPEED (big)   │   RPM arc + value   │  temp arcs col   │
 * │  mph           │                     │  BATT / MOTOR/MC │
 * │                │                     │  GYRO arc        │
 * ├──────────────────────────────────────────────────────────┤
 * │  power bar (full width, positive + regen)                │
 * ├──────────────────────────────────────────────────────────┤
 * │  stopwatch  │ icons │ logo │ error │ batt V / %  │ batt │
 * └──────────────────────────────────────────────────────────┘
 */

#include "DashboardUI.h"
#include "../lvgl_config/lvgl_v8_port.h"
#include "../Utils/EMAFilter.h"
#include "Config.h"
#include <cstdio>
#include <cmath>

// ============================================================================
// STATIC WIDGET STATE
// ============================================================================

static DashboardWidgets w;

// Anti-flicker EMA filters (tau=0.1s @ 30 Hz: alpha≈0.28, 95% settle≈0.3s)
// Temperatures are raw — physical inertia prevents inter-frame flicker.
static EMAFilter f_rpm(0.1f);
static EMAFilter f_current(0.1f);
static EMAFilter f_batt_voltage(0.1f);
static EMAFilter f_mc_voltage(0.1f);
static EMAFilter f_gyro(0.1f);

// Previous values for dirty-checking (only redraw when changed)
static int prev_speed = -1;
static int32_t prev_current_value = 0;   // absolute amps, clamped to 100
static int     prev_current_dir   = 0;   // -1 regen, 0 idle, +1 motoring
static int     prev_batt_temp     = -1;
static int     prev_motor_temp    = -1;
static int     prev_mc_temp       = -1;
static int     prev_gyro_roll     = 0;
static int     prev_batt_mv       = -1; // (int)(voltage * 10)
static int     prev_mc_mv         = -1; // (int)(mc_voltage * 10)
static int     prev_batt_pct      = -1;
static bool    prev_sd_started    = false;
static CanStatus prev_can_status  = CanStatus::BOOT;

// Spinlock protecting DashboardState.error_list
portMUX_TYPE g_error_list_mux = portMUX_INITIALIZER_UNLOCKED;

// ============================================================================
// ERROR MAPPING HELPERS
// ============================================================================

static void add_error_to_list(ErrorList &list, ErrorSource source, ErrorSeverity severity, const char *desc)
{
    if (list.errors[MAX_ACTIVE_ERRORS - 1].is_active) return; // list full

    // Find first empty slot or update existing (though with clear-and-rebuild it's always empty)
    for (int i = 0; i < MAX_ACTIVE_ERRORS; i++) {
        if (!list.errors[i].is_active) {
            list.errors[i].source = source;
            list.errors[i].severity = severity;
            strncpy(list.errors[i].description, desc, sizeof(list.errors[i].description) - 1);
            list.errors[i].description[sizeof(list.errors[i].description) - 1] = '\0';
            list.errors[i].is_active = true;
            break;
        }
    }
}

void update_error_state(DashboardState *state)
{
    taskENTER_CRITICAL(&g_error_list_mux);

    // Clear active flags (rebuild list every update)
    for (int i = 0; i < MAX_ACTIVE_ERRORS; i++) {
        state->error_list.errors[i].is_active = false;
    }

    ErrorSeverity max_bms = ErrorSeverity::NONE;
    ErrorSeverity max_mc  = ErrorSeverity::NONE;

    // --- BMS Errors (from @WarningErrorGuide.md) ---
    uint32_t bms_flag = (uint32_t)state->bms.bms_status_flag;
    if (bms_flag & 0x01) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::CRIT, "BMS: HIGH CELL VOLTAGE");
        max_bms = ErrorSeverity::CRIT;
    }
    if (bms_flag & 0x02) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::CRIT, "BMS: LOW CELL VOLTAGE");
        max_bms = ErrorSeverity::CRIT;
    }
    if (bms_flag & 0x04) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::WARN, "BMS: BALANCING VOLTAGE");
        if (max_bms < ErrorSeverity::WARN) max_bms = ErrorSeverity::WARN;
    }

    if (state->bms.bms_c_fault & 0x04) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::CRIT, "BMS: OVER-TEMPERATURE");
        max_bms = ErrorSeverity::CRIT;
    }
    if (state->bms.bms_c_fault & 0x08) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::WARN, "BMS: THERMISTOR CENSUS");
        if (max_bms < ErrorSeverity::WARN) max_bms = ErrorSeverity::WARN;
    }
    if (state->bms.bms_c_fault & 0x01) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::WARN, "BMS: CONFIG UNLOCKED");
        if (max_bms < ErrorSeverity::WARN) max_bms = ErrorSeverity::WARN;
    }
    if (state->bms.bms_c_fault & 0x02) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::CRIT, "BMS: CENSUS FAULT");
        max_bms = ErrorSeverity::CRIT;
    }

    if (state->bms.ltc_fault & 0x01) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::CRIT, "BMS: LTC CHIP FAULT");
        max_bms = ErrorSeverity::CRIT;
    }
    
    // Check thermistor array for overheating
    for (int i = 0; i < CONFIG_THERMISTOR_COUNT; i++) {
        if (state->thermistors.temps[i] >= BATT_TEMP_WARN_CELSIUS) {
            char t_buf[32];
            snprintf(t_buf, sizeof(t_buf), "BMS: THERMISTOR %d HOT", i);
            ErrorSeverity sev = (state->thermistors.temps[i] >= BATT_TEMP_CRIT_CELSIUS)
                                ? ErrorSeverity::CRIT : ErrorSeverity::WARN;
            add_error_to_list(state->error_list, ErrorSource::BMS, sev, t_buf);
            if (max_bms < sev) max_bms = sev;
        }
    }

    // Only check LTC count if we are actually receiving CAN data
    // if (state->can_status.load() == CanStatus::RECEIVING && state->bms.ltc_count == 0) {
    //     add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::WARN, "BMS: LTCS NOT DETECTED");
    //     if (max_bms < ErrorSeverity::WARN) max_bms = ErrorSeverity::WARN;
    // }

    // --- Motor Controller Errors ---
    uint16_t mc_err = (uint16_t)state->motor.error_message;
    // Byte 7 (low byte of error_message)
    if (mc_err & 0x01) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC ID ANGLE FAULT NEEDS CALIBRATION");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x02) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "MC OVER VOLTAGE PROTECTION");
        max_mc = ErrorSeverity::CRIT;
    }
    if (mc_err & 0x04) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC LOW VOLTAGE");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x10) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "MOTOR STALL");
        max_mc = ErrorSeverity::CRIT;
    }
    if (mc_err & 0x20) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC INTERNAL VOLTAGE FAULT");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    // Over-temp error handled below (0x40)
    if (mc_err & 0x80) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "THROTTLE DISABLED. RELEASE TO RESET.");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x200) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC INTERNAL RESET CONTINUE DRIVING");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x400) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "THROTTLE SHORT OR OPEN CIRCUIT");
        max_mc = ErrorSeverity::CRIT;
    }
    if (mc_err & 0x800) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "ANGLE SENSOR ERROR");
        max_mc = ErrorSeverity::CRIT;
    }
    // Motor over temperature handled below (0x4000)
    if (mc_err & 0x8000) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "HALL GALVANOMETER ERROR");
        max_mc = ErrorSeverity::CRIT;
    }

    // Temperature Escalations (using #defined thresholds)
    float mc_temp = state->temps.motor_controller_temperature;
    if (mc_temp >= MC_TEMP_CRIT_CELSIUS || (mc_err & 0x40)) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "MC OVER TEMP SHUTDOWN");
        max_mc = ErrorSeverity::CRIT;
    } else if (mc_temp >= MC_TEMP_WARN_CELSIUS) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC HIGH TEMP");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }

    float motor_temp = state->temps.motor_temperature;
    if (motor_temp >= MOTOR_TEMP_CRIT_CELSIUS || (mc_err & 0x4000)) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "MOTOR OVER TEMP SHUTDOWN");
        max_mc = ErrorSeverity::CRIT;
    } else if (motor_temp >= MOTOR_TEMP_WARN_CELSIUS) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MOTOR HIGH TEMP");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }

    state->bms_severity.store(max_bms);
    state->mc_severity.store(max_mc);

    taskEXIT_CRITICAL(&g_error_list_mux);
}

// ============================================================================
// PLACEHOLDER IMAGES
// ============================================================================

/*
 * LVGL requires images in its own format (lv_img_dsc_t). To add real icons:
 *
 * 1. Export your icon as a C array using the LVGL Online Image Converter:
 *       https://lvgl.io/tools/imageconverter
 *    Choose "C array", colour format CF_TRUE_COLOR_ALPHA, and the target size.
 *
 * 2. Place the generated .c file in this directory (e.g. icon_battery.c).
 *    It will define a const lv_img_dsc_t (e.g. icon_battery).
 *
 * 3. Declare it here with LV_IMG_DECLARE(icon_battery); and pass it to
 *    lv_img_set_src(w.batt_icon, &icon_battery);
 *
 * Until real assets are ready, we use LVGL's built-in symbol font as
 * placeholder text in labels styled to look like icons.
 */

// Superbike logo
LV_IMG_DECLARE(logo55);

// Speedometer dial face
LV_IMG_DECLARE(dial_face);

// Include the large font for digital speed readout
LV_FONT_DECLARE(lv_font_montserrat_144);

// Add a link symbol for CAN status (0xF381 in can_link.c)
LV_FONT_DECLARE(can_link);

// ============================================================================
// HELPER: create a small "icon" label using LV_SYMBOL_* placeholder text
// ============================================================================

static lv_obj_t *create_icon_label(lv_obj_t *parent, const char *symbol,
                                   lv_color_t color, lv_coord_t x, lv_coord_t y,
                                   const lv_font_t *font = &lv_font_montserrat_20)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

// ============================================================================
// UI BUILDERS
// ============================================================================

static void build_drive_ui(lv_obj_t *scr)
{
    // -- SPEEDOMETER DIAL (static pre-rendered image) --
    w.dial_img = lv_img_create(scr);
    lv_img_set_src(w.dial_img, &dial_face);
    lv_obj_set_pos(w.dial_img, 35, 15);
    lv_obj_clear_flag(w.dial_img, LV_OBJ_FLAG_CLICKABLE);

    // --- Current arcs ---
    // Motoring arc (blue): shows 0A to +current, clockwise from 0 mph origin
    w.current_motoring_arc = lv_arc_create(scr);
    lv_obj_set_size(w.current_motoring_arc, 450, 450);
    lv_obj_set_pos(w.current_motoring_arc, 25, 5);
    lv_arc_set_mode(w.current_motoring_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(w.current_motoring_arc, 0, 100);
    lv_arc_set_value(w.current_motoring_arc, 0);
    lv_arc_set_bg_angles(w.current_motoring_arc, 165, 375);
    lv_obj_set_style_arc_color(w.current_motoring_arc, CLR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(w.current_motoring_arc, CLR_MOTORING, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(w.current_motoring_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(w.current_motoring_arc, 10, LV_PART_INDICATOR);
    lv_obj_remove_style(w.current_motoring_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w.current_motoring_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(w.current_motoring_arc, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(w.current_motoring_arc, LV_OBJ_FLAG_HIDDEN);

    // Regen arc (green): shows -current to 0A, counter-clockwise from 0 mph origin
    w.current_regen_arc = lv_arc_create(scr);
    lv_obj_set_size(w.current_regen_arc, 450, 450);
    lv_obj_set_pos(w.current_regen_arc, 25, 5);
    lv_arc_set_mode(w.current_regen_arc, LV_ARC_MODE_REVERSE);
    lv_arc_set_range(w.current_regen_arc, 0, 100);
    lv_arc_set_value(w.current_regen_arc, 0);
    lv_arc_set_bg_angles(w.current_regen_arc, 115, 165);
    lv_obj_set_style_arc_color(w.current_regen_arc, CLR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(w.current_regen_arc, CLR_REGEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(w.current_regen_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(w.current_regen_arc, 10, LV_PART_INDICATOR);
    lv_obj_remove_style(w.current_regen_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w.current_regen_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(w.current_regen_arc, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(w.current_regen_arc, LV_OBJ_FLAG_HIDDEN);

    // --- Speed needle ---
    w.needle_line = lv_line_create(scr);
    lv_obj_set_style_line_width(w.needle_line, 5, 0);
    lv_obj_set_style_line_color(w.needle_line, CLR_NEEDLE, 0);
    lv_obj_set_style_line_rounded(w.needle_line, true, 0);
    w.needle_pts[0] = {250, 230};
    w.needle_pts[1] = {(lv_coord_t)(250 + 195.0f * sinf(255.0f * M_PI / 180.0f)),
                       (lv_coord_t)(230 - 195.0f * cosf(255.0f * M_PI / 180.0f))};
    lv_line_set_points(w.needle_line, w.needle_pts, 2);

    // --- Digital speed ---
    w.meter_speed_label = lv_label_create(scr);
    lv_label_set_text(w.meter_speed_label, "0");
    lv_obj_set_style_text_color(w.meter_speed_label, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w.meter_speed_label, &lv_font_montserrat_144, 0);
    lv_obj_set_style_text_align(w.meter_speed_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(w.meter_speed_label, 300);
    lv_obj_set_pos(w.meter_speed_label, 100, 300);

    // -- TEMPERATURE ARCS (right column) --
    const lv_coord_t arc_x = 520;
    const lv_coord_t arc_y = 25;
    const lv_coord_t arc_spacing = 135;
    create_temp_arc(scr, &w.batt_temp_arc, &w.batt_temp_label,
                    "BATT", CLR_MOTOR_TEMP,
                    BATT_TEMP_MIN, BATT_TEMP_MAX,
                    arc_x, arc_y);

    create_temp_arc(scr, &w.motor_temp_arc, &w.motor_temp_label,
                    "MOTOR", CLR_MOTOR_TEMP,
                    MOTOR_TEMP_MIN, MOTOR_TEMP_MAX,
                    arc_x + arc_spacing, arc_y);

    create_temp_arc(scr, &w.mc_temp_arc, &w.mc_temp_label,
                    "MTR CTRL", CLR_MOTOR_TEMP,
                    MC_TEMP_MIN, MC_TEMP_MAX,
                    arc_x, arc_y + arc_spacing);

    // -- GYRO ARC --
    w.gyro_arc = lv_arc_create(scr);
    lv_obj_set_size(w.gyro_arc, 110, 110);
    lv_arc_set_range(w.gyro_arc, -90, 90);
    lv_arc_set_value(w.gyro_arc, 0);
    lv_arc_set_bg_angles(w.gyro_arc, 135, 45);
    lv_obj_set_style_arc_color(w.gyro_arc, lv_color_make(0x30, 0x30, 0x30), LV_PART_MAIN);
    lv_obj_set_style_arc_color(w.gyro_arc, CLR_GYRO, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(w.gyro_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(w.gyro_arc, 10, LV_PART_INDICATOR);
    lv_obj_remove_style(w.gyro_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w.gyro_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(w.gyro_arc, arc_x + arc_spacing, arc_y + arc_spacing);

    w.gyro_label = lv_label_create(w.gyro_arc);
    lv_label_set_text(w.gyro_label, "0 deg");
    lv_obj_set_style_text_color(w.gyro_label, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w.gyro_label, &lv_font_montserrat_16, 0);
    lv_obj_center(w.gyro_label);

    lv_obj_t *gyro_title = lv_label_create(scr);
    lv_label_set_text(gyro_title, "GYRO");
    lv_obj_set_style_text_color(gyro_title, CLR_WHITE, 0);
    lv_obj_set_style_text_font(gyro_title, &lv_font_montserrat_12, 0);
    lv_obj_align_to(gyro_title, w.gyro_arc, LV_ALIGN_BOTTOM_MID, 0, 5);

    // -- BOTTOM STRIP --

    // Status icons row
    lv_coord_t icon_y = 445;
    w.sd_icon           = create_icon_label(scr, LV_SYMBOL_SD_CARD,  CLR_WHITE,        15,  icon_y);
    w.can_icon          = create_icon_label(scr, LV_SYMBOL_CAN_LINK, CLR_DISABLED,     50,  icon_y, &can_link);
    w.bms_status_label  = create_icon_label(scr, LV_SYMBOL_CHARGE,   CLR_STATUS_GREEN, 90,  icon_y);
    w.mc_status_icon    = create_icon_label(scr, LV_SYMBOL_SETTINGS, CLR_STATUS_GREEN, 120, icon_y);

    // Superbike logo (center bottom)
    w.logo_icon = lv_img_create(scr);
    lv_img_set_src(w.logo_icon, &logo55);
    lv_obj_set_style_img_recolor(w.logo_icon, CLR_LOGO, 0);
    lv_obj_set_style_img_recolor_opa(w.logo_icon, LV_OPA_COVER, 0);
    lv_obj_align(w.logo_icon, LV_ALIGN_BOTTOM_MID, 0, -15);

    // Battery info group
    // Create the battery icon as the anchor
    w.batt_icon = create_icon_label(scr, LV_SYMBOL_BATTERY_FULL, CLR_WHITE, 660, icon_y-5, &lv_font_montserrat_30);
    lv_obj_set_width(w.batt_icon, 50);
    lv_obj_set_style_text_align(w.batt_icon, LV_TEXT_ALIGN_CENTER, 0);

    // Pack Voltage (Left of icon)
    w.batt_voltage = lv_label_create(scr);
    lv_obj_set_width(w.batt_voltage, 100);
    lv_obj_set_style_text_align(w.batt_voltage, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(w.batt_voltage, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w.batt_voltage, &lv_font_montserrat_24, 0);
    lv_obj_align_to(w.batt_voltage, w.batt_icon, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_label_set_text(w.batt_voltage, "0.0 V");

    // Battery % (Right of icon)
    w.batt_percent = lv_label_create(scr);
    lv_obj_set_width(w.batt_percent, 80);
    lv_obj_set_style_text_align(w.batt_percent, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(w.batt_percent, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w.batt_percent, &lv_font_montserrat_24, 0);
    lv_obj_align_to(w.batt_percent, w.batt_icon, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_label_set_text(w.batt_percent, "-- %");

    // Battery label (above battery icon)
    w.batt_voltage_label = lv_label_create(scr);
    lv_obj_set_width(w.batt_voltage_label, 100);
    lv_obj_set_style_text_align(w.batt_voltage_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(w.batt_voltage_label, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w.batt_voltage_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(w.batt_voltage_label, w.batt_voltage, LV_ALIGN_OUT_TOP_MID, 10, -1);
    lv_label_set_text(w.batt_voltage_label, "-- BATT --");

    // MC Voltage (Left of Pack Voltage)
    w.mc_voltage = lv_label_create(scr);
    lv_obj_set_width(w.mc_voltage, 100);
    lv_obj_set_style_text_align(w.mc_voltage, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(w.mc_voltage, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w.mc_voltage, &lv_font_montserrat_24, 0);
    lv_obj_align_to(w.mc_voltage, w.batt_voltage, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    lv_label_set_text(w.mc_voltage, "0.0 V");

    // MC Voltage Label (above MC Voltage)
    w.mc_voltage_label = lv_label_create(scr);
    lv_obj_set_width(w.mc_voltage_label, 100);
    lv_obj_set_style_text_align(w.mc_voltage_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(w.mc_voltage_label, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w.mc_voltage_label, &lv_font_montserrat_12, 0);
    lv_obj_align_to(w.mc_voltage_label, w.mc_voltage, LV_ALIGN_OUT_TOP_MID, 10, -1);
    lv_label_set_text(w.mc_voltage_label, "-- MC --");

    // -- Warning carousel (bottom-right above battery info) --
    int warning_y = icon_y - 110;
    w.warning_carousel_icon = create_icon_label(scr, LV_SYMBOL_WARNING, CLR_WARN_YELLOW, 460, warning_y);
    lv_obj_add_flag(w.warning_carousel_icon, LV_OBJ_FLAG_HIDDEN);

    w.warning_carousel_label = lv_label_create(scr);
    lv_label_set_text(w.warning_carousel_label, "");
    lv_obj_set_style_text_color(w.warning_carousel_label, CLR_WARN_YELLOW, 0);
    lv_obj_set_style_text_font(w.warning_carousel_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(w.warning_carousel_label, 490, warning_y);
    lv_obj_set_width(w.warning_carousel_label, 300);

    // -- CRITICAL ERROR MODAL --
    w.error_modal = lv_obj_create(scr);
    lv_obj_set_size(w.error_modal, 640, 360);
    lv_obj_center(w.error_modal);
    lv_obj_set_style_bg_color(w.error_modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(w.error_modal, LV_OPA_90, 0);
    lv_obj_set_style_border_color(w.error_modal, CLR_WARN_RED, 0);
    lv_obj_set_style_border_width(w.error_modal, 4, 0);
    lv_obj_set_style_radius(w.error_modal, 20, 0);
    lv_obj_add_flag(w.error_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(w.error_modal, LV_OBJ_FLAG_SCROLLABLE);

    w.modal_title = lv_label_create(w.error_modal);
    lv_label_set_text(w.modal_title, "POWER CUT");
    lv_obj_set_style_text_color(w.modal_title, CLR_WARN_RED, 0);
    lv_obj_set_style_text_font(w.modal_title, &lv_font_montserrat_48, 0);
    lv_obj_align(w.modal_title, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *modal_sub = lv_label_create(w.error_modal);
    lv_label_set_text(modal_sub, "CRITICAL FAULT DETECTED");
    lv_obj_set_style_text_color(modal_sub, CLR_WHITE, 0);
    lv_obj_set_style_text_font(modal_sub, &lv_font_montserrat_20, 0);
    lv_obj_align(modal_sub, LV_ALIGN_TOP_MID, 0, 110);

    w.modal_desc = lv_label_create(w.error_modal);
    lv_label_set_text(w.modal_desc, "NO DATA");
    lv_obj_set_style_text_color(w.modal_desc, CLR_WARN_YELLOW, 0);
    lv_obj_set_style_text_font(w.modal_desc, &lv_font_montserrat_30, 0);
    lv_obj_set_width(w.modal_desc, 580);
    lv_obj_set_style_text_align(w.modal_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(w.modal_desc, LV_ALIGN_CENTER, 0, 40);
}

static void build_charging_ui(lv_obj_t *scr)
{
    // Big Battery Percentage
    w.chg_batt_pct_label = lv_label_create(scr);
    lv_obj_set_style_text_font(w.chg_batt_pct_label, &lv_font_montserrat_144, 0);
    lv_obj_set_style_text_color(w.chg_batt_pct_label, CLR_WHITE, 0);
    lv_obj_align(w.chg_batt_pct_label, LV_ALIGN_TOP_MID, 0, 40);
    lv_label_set_text(w.chg_batt_pct_label, "0%");

    // Charging Bar
    w.chg_bar = lv_bar_create(scr);
    lv_obj_set_size(w.chg_bar, 600, 40);
    lv_obj_align(w.chg_bar, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_bg_color(w.chg_bar, lv_color_make(0x30, 0x30, 0x30), LV_PART_MAIN);
    lv_obj_set_style_bg_color(w.chg_bar, CLR_STATUS_GREEN, LV_PART_INDICATOR);

    // Stats Grid
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 700, 150);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto create_stat = [&](lv_obj_t **lbl, const char *title) {
        lv_obj_t *stat_cont = lv_obj_create(cont);
        lv_obj_set_size(stat_cont, 160, 100);
        lv_obj_set_style_bg_color(stat_cont, lv_color_make(0x20, 0x20, 0x20), 0);
        lv_obj_set_style_border_color(stat_cont, lv_color_make(0x40, 0x40, 0x40), 0);
        
        lv_obj_t *t = lv_label_create(stat_cont);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 5);

        *lbl = lv_label_create(stat_cont);
        lv_label_set_text(*lbl, "--");
        lv_obj_set_style_text_font(*lbl, &lv_font_montserrat_24, 0);
        lv_obj_align(*lbl, LV_ALIGN_CENTER, 0, 10);
    };

    create_stat(&w.chg_voltage_label, "VOLTAGE");
    create_stat(&w.chg_current_label, "CURRENT");
    create_stat(&w.chg_power_label, "POWER");
    create_stat(&w.chg_temp_label, "CHG TEMP");

    w.chg_status_label = lv_label_create(scr);
    lv_obj_align(w.chg_status_label, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_style_text_color(w.chg_status_label, CLR_STATUS_GREEN, 0);
    lv_label_set_text(w.chg_status_label, "CHARGING ACTIVE");
}

// ============================================================================
// UI REFRESHERS
// ============================================================================

static int rpm_to_mph(float rpm)
{
    return (int)(rpm / GEAR_RATIO * (float)M_PI * WHEEL_DIAM_M / 60.0f * MS_TO_MPH);
}

static void refresh_drive_ui(const DashboardState &state)
{
    char buf[32];

    // Snap to values on first run to avoid 0 -> value ramp transients
    if (prev_speed == -1) {
        f_rpm.reset(state.motor.RPM);
        f_current.reset(state.motor.motor_current);
        f_batt_voltage.reset(state.battery.hv_series_voltage);
        f_mc_voltage.reset(state.motor.motor_controller_battery_voltage);
        f_gyro.reset(state.gyro.roll_angle);
    }

    float s_rpm     = f_rpm.update(state.motor.RPM);
    float s_current = f_current.update(state.motor.motor_current);
    float s_voltage = f_batt_voltage.update(state.battery.hv_series_voltage);
    float s_mc_v    = f_mc_voltage.update(state.motor.motor_controller_battery_voltage);
    float s_gyro    = f_gyro.update(state.gyro.roll_angle);
    float s_motor_t = state.temps.motor_temperature;
    float s_mc_t    = state.temps.motor_controller_temperature;

    float s_batt_t = 0;
    for (int i = 0; i < CONFIG_THERMISTOR_COUNT; i++) {
        if (state.thermistors.temps[i] > s_batt_t)
            s_batt_t = state.thermistors.temps[i];
    }

    // -- Speed (needle + digital label) --
    int speed = rpm_to_mph(s_rpm);
    if (speed < 0)   speed = 0;
    if (speed > 140) speed = 140;
    if (speed != prev_speed) {
        float angle_deg = 255.0f + (float)speed * (210.0f / 140.0f);
        float angle_rad = angle_deg * (float)M_PI / 180.0f;
        w.needle_pts[0] = {250, 230};
        w.needle_pts[1] = {(lv_coord_t)(250 + 195.0f * sinf(angle_rad)),
                           (lv_coord_t)(230 - 195.0f * cosf(angle_rad))};
        lv_line_set_points(w.needle_line, w.needle_pts, 2);
        snprintf(buf, sizeof(buf), "%d", speed);
        lv_label_set_text(w.meter_speed_label, buf);
        prev_speed = speed;
    }

    // -- Temperature arcs --
    int batt_temp_int = (int)s_batt_t;
    if (batt_temp_int != prev_batt_temp) {
        lv_arc_set_value(w.batt_temp_arc, batt_temp_int);
        lv_obj_set_style_arc_color(w.batt_temp_arc, temp_arc_color(s_batt_t, BATT_TEMP_WARN_CELSIUS, BATT_TEMP_CRIT_CELSIUS), LV_PART_INDICATOR);
        snprintf(buf, sizeof(buf), "%d C", batt_temp_int);
        lv_label_set_text(w.batt_temp_label, buf);
        prev_batt_temp = batt_temp_int;
    }

    // Motor temperature
    int motor_temp_int = (int)s_motor_t;
    if (motor_temp_int != prev_motor_temp) {
        lv_arc_set_value(w.motor_temp_arc, motor_temp_int);
        lv_obj_set_style_arc_color(w.motor_temp_arc, temp_arc_color(s_motor_t, MOTOR_TEMP_WARN_CELSIUS, MOTOR_TEMP_CRIT_CELSIUS), LV_PART_INDICATOR);
        snprintf(buf, sizeof(buf), "%d C", motor_temp_int);
        lv_label_set_text(w.motor_temp_label, buf);
        prev_motor_temp = motor_temp_int;
    }

    // Motor controller temperature
    int mc_temp_int = (int)s_mc_t;
    if (mc_temp_int != prev_mc_temp) {
        lv_arc_set_value(w.mc_temp_arc, mc_temp_int);
        lv_obj_set_style_arc_color(w.mc_temp_arc, temp_arc_color(s_mc_t, MC_TEMP_WARN_CELSIUS, MC_TEMP_CRIT_CELSIUS), LV_PART_INDICATOR);
        snprintf(buf, sizeof(buf), "%d C", mc_temp_int);
        lv_label_set_text(w.mc_temp_label, buf);
        prev_mc_temp = mc_temp_int;
    }

    // -- Gyro (roll angle) --
    int roll = (int)s_gyro;
    if (roll != prev_gyro_roll) {
        lv_arc_set_value(w.gyro_arc, roll);
        snprintf(buf, sizeof(buf), "%d deg", roll);
        lv_label_set_text(w.gyro_label, buf);
        prev_gyro_roll = roll;
    }

    // -- Motor current arcs --
    float abs_current = fabsf(s_current);
    if (abs_current > 100.0f) abs_current = 100.0f;
    int cur_dir = (s_current > 0.0f) ? 1 : (s_current < 0.0f) ? -1 : 0;
    int32_t cur_val = (int32_t)abs_current;
    if (cur_dir != prev_current_dir || cur_val != prev_current_value) {
        if (cur_dir > 0) {
            lv_arc_set_value(w.current_motoring_arc, cur_val);
            lv_obj_clear_flag(w.current_motoring_arc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(w.current_regen_arc, LV_OBJ_FLAG_HIDDEN);
        } else if (cur_dir < 0) {
            lv_arc_set_value(w.current_regen_arc, cur_val);
            lv_obj_clear_flag(w.current_regen_arc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(w.current_motoring_arc, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(w.current_motoring_arc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(w.current_regen_arc, LV_OBJ_FLAG_HIDDEN);
        }
        prev_current_dir   = cur_dir;
        prev_current_value = cur_val;
    }

    // -- Battery and MC voltage --
    int batt_mv = (int)(s_voltage * 10.0f);
    if (batt_mv != prev_batt_mv) {
        snprintf(buf, sizeof(buf), "%.1f V", s_voltage);
        lv_label_set_text(w.batt_voltage, buf);
        prev_batt_mv = batt_mv;
    }

    int mc_mv = (int)(s_mc_v * 10.0f);
    if (mc_mv != prev_mc_mv) {
        snprintf(buf, sizeof(buf), "%.1f V", s_mc_v);
        lv_label_set_text(w.mc_voltage, buf);
        prev_mc_mv = mc_mv;
    }        

    // Battery %
    float pct = (s_voltage - 60.0f) / (100.8f - 60.0f) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    int pct_int = (int)pct;

    if (pct_int != prev_batt_pct) {
        snprintf(buf, sizeof(buf), "%d %%", pct_int);
        lv_label_set_text(w.batt_percent, buf);

        // Update icon symbol and color based on percentage thresholds
        const char *symbol = LV_SYMBOL_BATTERY_FULL;
        lv_color_t color = CLR_WHITE;

        if (pct < 10.0f) {
            symbol = LV_SYMBOL_BATTERY_EMPTY;
            color = CLR_WARN_RED;
        } else if (pct < 25.0f) {
            symbol = LV_SYMBOL_BATTERY_1;
            color = CLR_WARN_RED;
        } else if (pct < 50.0f) {
            symbol = LV_SYMBOL_BATTERY_2;
            color = CLR_WARN_YELLOW;
        } else if (pct < 75.0f) {
            symbol = LV_SYMBOL_BATTERY_3;
            color = CLR_WHITE;
        }

        lv_label_set_text(w.batt_icon, symbol);
        lv_obj_set_style_text_color(w.batt_icon, color, 0);
        prev_batt_pct = pct_int;
    }

    // -- SD card --
    if (state.sd_started != prev_sd_started) {
        lv_obj_set_style_text_color(w.sd_icon, state.sd_started ? CLR_WHITE : CLR_WARN_RED, 0);
        prev_sd_started = state.sd_started;
    }

    // -- CAN status --
    CanStatus can_st = state.can_status.load();
    if (can_st != prev_can_status) {
        lv_color_t can_color;
        if (can_st == CanStatus::RECEIVING) can_color = CLR_STATUS_GREEN;
        else if (can_st == CanStatus::TIMEOUT) can_color = CLR_WARN_YELLOW;
        else if (can_st == CanStatus::BOOT) can_color = CLR_DISABLED;
        else can_color = CLR_WARN_RED;
        lv_obj_set_style_text_color(w.can_icon, can_color, 0);
        prev_can_status = can_st;
    }

    // -- BMS/MC Status Icons --
    auto get_sev_color = [](ErrorSeverity sev) {
        if (sev == ErrorSeverity::CRIT) return CLR_WARN_RED;
        if (sev == ErrorSeverity::WARN) return CLR_WARN_YELLOW;
        if (sev == ErrorSeverity::INFO) return CLR_WHITE;
        return CLR_STATUS_GREEN;
    };
    lv_obj_set_style_text_color(w.bms_status_label, get_sev_color(state.bms_severity.load()), 0);
    lv_obj_set_style_text_color(w.mc_status_icon,   get_sev_color(state.mc_severity.load()), 0);

    // -- Warning Carousel --
    static int carousel_idx = 0;
    static uint32_t last_carousel_ms = 0;
    const uint32_t carousel_interval_ms = 1500;

    taskENTER_CRITICAL(&g_error_list_mux);
    int warning_count = 0;
    int first_warning_idx = -1;
    for (int i = 0; i < MAX_ACTIVE_ERRORS; i++) {
        if (state.error_list.errors[i].is_active && state.error_list.errors[i].severity != ErrorSeverity::CRIT) {
            if (first_warning_idx == -1) first_warning_idx = i;
            warning_count++;
        }
    }

    if (warning_count > 0) {
        lv_obj_clear_flag(w.warning_carousel_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(w.warning_carousel_label, LV_OBJ_FLAG_HIDDEN);

        // Rotate index every interval
        if (millis() - last_carousel_ms > carousel_interval_ms) {
            carousel_idx++;
            last_carousel_ms = millis();
        }

        // Find the next active warning
        int found_idx = -1;
        for (int i = 0; i < MAX_ACTIVE_ERRORS; i++) {
            int check_idx = (carousel_idx + i) % MAX_ACTIVE_ERRORS;
            if (state.error_list.errors[check_idx].is_active && state.error_list.errors[check_idx].severity != ErrorSeverity::CRIT) {
                found_idx = check_idx;
                break;
            }
        }

        if (found_idx != -1) {
            lv_label_set_text(w.warning_carousel_label, state.error_list.errors[found_idx].description);
        }
    } else {
        lv_obj_add_flag(w.warning_carousel_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(w.warning_carousel_label, LV_OBJ_FLAG_HIDDEN);
    }
    taskEXIT_CRITICAL(&g_error_list_mux);

    // -- Critical Error Modal (v1.5) --
    // Show instantly if any CRIT error exists. Priority: CRIT > Carousel.
    bool crit_active = (state.bms_severity.load() == ErrorSeverity::CRIT || 
                        state.mc_severity.load() == ErrorSeverity::CRIT);
    
    if (crit_active) {
        lv_obj_clear_flag(w.error_modal, LV_OBJ_FLAG_HIDDEN);
        static int crit_carousel_idx = 0;
        static uint32_t last_crit_carousel_ms = 0;
        taskENTER_CRITICAL(&g_error_list_mux);

        // Count active critical errors
        int crit_count = 0;
        for (int i = 0; i < MAX_ACTIVE_ERRORS; i++) {
            if (state.error_list.errors[i].is_active && state.error_list.errors[i].severity == ErrorSeverity::CRIT) {
                crit_count++;
            }
        }

        // Rotate index every interval
        if (millis() - last_crit_carousel_ms > carousel_interval_ms) {
            crit_carousel_idx++;
            last_crit_carousel_ms = millis();
        }

        // Find the next active critical error
        int found_crit_idx = -1;
        for (int i = 0; i < MAX_ACTIVE_ERRORS; i++) {
            int check_idx = (crit_carousel_idx + i) % MAX_ACTIVE_ERRORS;
            if (state.error_list.errors[check_idx].is_active && state.error_list.errors[check_idx].severity == ErrorSeverity::CRIT) {
                found_crit_idx = check_idx;
                break;
            }
        }
        if (found_crit_idx != -1) lv_label_set_text(w.modal_desc, state.error_list.errors[found_crit_idx].description);
        else lv_label_set_text(w.modal_desc, "CRITICAL FAULT");
        taskEXIT_CRITICAL(&g_error_list_mux);
    } else {
        lv_obj_add_flag(w.error_modal, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_charging_ui(const DashboardState &state)
{
    char buf[32];
    
    // Battery %
    float voltage = state.battery.hv_series_voltage;
    float pct = (voltage - 60.0f) / (100.8f - 60.0f) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f)   pct = 0.0f;

    snprintf(buf, sizeof(buf), "%d%%", (int)pct);
    lv_label_set_text(w.chg_batt_pct_label, buf);
    lv_bar_set_value(w.chg_bar, (int)pct, LV_ANIM_ON);

    // Voltage
    snprintf(buf, sizeof(buf), "%.1f V", voltage);
    lv_label_set_text(w.chg_voltage_label, buf);

    // Current
    snprintf(buf, sizeof(buf), "%.1f A", state.charger.output_current);
    lv_label_set_text(w.chg_current_label, buf);

    // Power (kW)
    float power_kw = (voltage * state.charger.output_current) / 1000.0f;
    snprintf(buf, sizeof(buf), "%.2f kW", power_kw);
    lv_label_set_text(w.chg_power_label, buf);

    // Charger Temp
    snprintf(buf, sizeof(buf), "%d C", state.charger.charger_temp);
    lv_label_set_text(w.chg_temp_label, buf);

    // Status
    if (state.charger.output_current < 0.1f) {
        lv_label_set_text(w.chg_status_label, "CHARGER CONNECTED - IDLE");
        lv_obj_set_style_text_color(w.chg_status_label, CLR_WHITE, 0);
    } else {
        lv_label_set_text(w.chg_status_label, "CHARGING ACTIVE");
        lv_obj_set_style_text_color(w.chg_status_label, CLR_STATUS_GREEN, 0);
    }
}

// ============================================================================
// dashboard_create()  -- build the full UI (call once under lvgl_port_lock)
// ============================================================================

void dashboard_create(void)
{
    // Create screens
    w.main_drive_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(w.main_drive_screen, CLR_BG, 0);

    w.charging_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(w.charging_screen, CLR_BG, 0);

    // Build UIs on respective screens
    build_drive_ui(w.main_drive_screen);
    build_charging_ui(w.charging_screen);

    // Initial load
    lv_scr_load(w.main_drive_screen);
}

// ============================================================================
// dashboard_refresh()  -- push DashboardState data into widgets
// ============================================================================

void dashboard_refresh(const DashboardState &state)
{
    // Determine charging state with watchdog
    uint32_t now = millis();
    bool evcc_alive = (now - state.evcc_last_rx_ms < CAN_TIMEOUT_MS);
    bool charger_alive = (now - state.charger_last_rx_ms < CAN_TIMEOUT_MS);
    
    // We are "charging" if the EVCC is enabled OR we see current flowing from the charger
    bool is_charging = (evcc_alive && state.evcc.en > 0) || (charger_alive && state.charger.output_current > 0.5f);
    
#ifdef DEBUG_CHARGING_SCREEN_ONLY
    is_charging = true;
#endif
#ifdef DEBUG_SPEEDOMETER_SCREEN_ONLY
    is_charging = false;
#endif

    lv_obj_t *active_scr = lv_scr_act();

    if (is_charging && active_scr != w.charging_screen) {
        lv_scr_load_anim(w.charging_screen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
    } else if (!is_charging && active_scr != w.main_drive_screen) {
        lv_scr_load_anim(w.main_drive_screen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
    }

    if (is_charging) {
        refresh_charging_ui(state);
    } else {
        refresh_drive_ui(state);
    }
}


// ============================================================================
// dashboardTask() -- FreeRTOS task that refreshes the UI
// ============================================================================

void dashboardTask(void *param)
{
    DashboardState *state = (DashboardState *)param;

    while (true) {
        if (lvgl_port_lock(-1)) {
            dashboard_refresh(*state);
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000 / REFRESH_RATE_HZ));
    }
}
