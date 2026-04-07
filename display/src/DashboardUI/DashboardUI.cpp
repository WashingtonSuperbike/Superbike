/**
 * DashboardUI.cpp - LVGL dashboard for 800x480 (Waveshare ESP32-S3-Touch-LCD-5)
 *
 * Layout modelled after NewUI.cpp (320x240 TFT mockup) but expanded for the
 * larger screen and using native LVGL widgets (arcs, bars, labels).
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │  SPEED (big)   │   RPM arc + value   │  temp arcs col   │
 * │  mph           │                     │  BATT / MOTOR/MC │
 * │                │                     │  GYRO arc        │
 * ├─────────────────────────────────────────────────────────┤
 * │  power bar (full width, positive + regen)               │
 * ├─────────────────────────────────────────────────────────┤
 * │  stopwatch  │ icons │ logo │ error │ batt V / %  │ batt │
 * └─────────────────────────────────────────────────────────┘
 */

#include "DashboardUI.h"
#include "../lvgl_config/lvgl_v8_port.h"
#include <cstdio>
#include <cmath>

// ============================================================================
// COLOUR PALETTE (matches NewUI.cpp hex values, converted to LVGL 32-bit)
// ============================================================================

#define CLR_BG          lv_color_black()
#define CLR_FG          lv_color_white()
#define CLR_SPEED       lv_color_white()
#define CLR_RPM_ARC     lv_color_make(0x24, 0xBE, 0x24)  // green-ish
#define CLR_POWER_POS   lv_color_make(0x24, 0xBE, 0x48)  // ~0x24BE
#define CLR_POWER_NEG   lv_color_make(0x4D, 0x6A, 0x4D)  // ~0x4D6A (regen)
#define CLR_BATT_TEMP   lv_color_make(0xF8, 0x71, 0x10)  // ~0xF8E2 warm
#define CLR_MOTOR_TEMP  lv_color_make(0x3E, 0xCA, 0x50)  // ~0x3F2A cool
#define CLR_MC_TEMP     lv_color_make(0xFE, 0x80, 0x00)  // ~0xFE40 orange
#define CLR_GYRO        lv_color_make(0xBD, 0xBD, 0xBD)  // ~0xBDF7 grey
#define CLR_WARN_RED    lv_color_make(0xF8, 0x20, 0x00)
#define CLR_WARN_YELLOW lv_color_make(0xFE, 0xE0, 0x00)
#define CLR_LOGO        lv_color_make(0x61, 0xD6, 0x61)

// ============================================================================
// STATIC WIDGET STATE
// ============================================================================

static DashboardWidgets w;

// Previous values for dirty-checking (only redraw when changed)
static int prev_speed = -1;
static int prev_rpm   = -1;

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

// ============================================================================
// HELPER: create a small "icon" label using LV_SYMBOL_* placeholder text
// ============================================================================

static lv_obj_t *create_icon_label(lv_obj_t *parent, const char *symbol,
                                   lv_color_t color, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

// ============================================================================
// HELPER: create a temperature arc (right column)
// ============================================================================

static void create_temp_arc(lv_obj_t *parent, lv_obj_t **arc, lv_obj_t **label,
                            const char *title, lv_color_t color,
                            int min_val, int max_val,
                            lv_coord_t x, lv_coord_t y)
{
    *arc = lv_arc_create(parent);
    lv_obj_set_size(*arc, 110, 110);
    lv_arc_set_range(*arc, min_val, max_val);
    lv_arc_set_value(*arc, min_val);
    lv_arc_set_bg_angles(*arc, 135, 45);          // 270 degree sweep
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
    lv_obj_set_style_text_color(*label, CLR_FG, 0);
    lv_obj_set_style_text_font(*label, &lv_font_montserrat_16, 0);
    lv_obj_center(*label);

    // Title label below the arc
    lv_obj_t *title_lbl = lv_label_create(parent);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, CLR_FG, 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align_to(title_lbl, *arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
}

// ============================================================================
// dashboard_create()  -- build the full UI (call once under lvgl_port_lock)
// ============================================================================

void dashboard_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);

    // -- SPEED (large, left) --
    w.speed_label = lv_label_create(scr);
    lv_label_set_text(w.speed_label, "0");
    lv_obj_set_style_text_color(w.speed_label, CLR_SPEED, 0);
    lv_obj_set_style_text_font(w.speed_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_transform_zoom(w.speed_label, 512, 0); // 2x zoom
    lv_obj_set_pos(w.speed_label, 40, 60);

    w.mph_label = lv_label_create(scr);
    lv_label_set_text(w.mph_label, "mph");
    lv_obj_set_style_text_color(w.mph_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.mph_label, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(w.mph_label, 220, 190);

    // -- RPM ARC (centre) --
    w.rpm_arc = lv_arc_create(scr);
    lv_obj_set_size(w.rpm_arc, 240, 240);
    lv_arc_set_range(w.rpm_arc, 0, 6000);
    lv_arc_set_value(w.rpm_arc, 0);
    lv_arc_set_bg_angles(w.rpm_arc, 135, 45);
    lv_obj_set_style_arc_color(w.rpm_arc, lv_color_make(0x25, 0x25, 0x25), LV_PART_MAIN);
    lv_obj_set_style_arc_color(w.rpm_arc, CLR_RPM_ARC, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(w.rpm_arc, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(w.rpm_arc, 16, LV_PART_INDICATOR);
    lv_obj_remove_style(w.rpm_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w.rpm_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(w.rpm_arc, 280, 30);

    w.rpm_value_label = lv_label_create(w.rpm_arc);
    lv_label_set_text(w.rpm_value_label, "0");
    lv_obj_set_style_text_color(w.rpm_value_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.rpm_value_label, &lv_font_montserrat_30, 0);
    lv_obj_center(w.rpm_value_label);

    lv_obj_t *rpm_title = lv_label_create(w.rpm_arc);
    lv_label_set_text(rpm_title, "RPM");
    lv_obj_set_style_text_color(rpm_title, CLR_FG, 0);
    lv_obj_set_style_text_font(rpm_title, &lv_font_montserrat_14, 0);
    lv_obj_align(rpm_title, LV_ALIGN_CENTER, 0, 30);

    // -- TEMPERATURE ARCS (right column) --
    const lv_coord_t arc_x = 570;
    create_temp_arc(scr, &w.batt_temp_arc, &w.batt_temp_label,
                    "BATT", CLR_BATT_TEMP,
                    BATT_TEMP_MIN, BATT_TEMP_MAX,
                    arc_x, 5);

    create_temp_arc(scr, &w.motor_temp_arc, &w.motor_temp_label,
                    "MOTOR", CLR_MOTOR_TEMP,
                    MOTOR_TEMP_MIN, MOTOR_TEMP_MAX,
                    arc_x + 120, 5);

    create_temp_arc(scr, &w.mc_temp_arc, &w.mc_temp_label,
                    "MTR CTRL", CLR_MC_TEMP,
                    MC_TEMP_MIN, MC_TEMP_MAX,
                    arc_x, 140);

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
    lv_obj_set_pos(w.gyro_arc, arc_x + 120, 140);

    w.gyro_label = lv_label_create(w.gyro_arc);
    lv_label_set_text(w.gyro_label, "0");
    lv_obj_set_style_text_color(w.gyro_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.gyro_label, &lv_font_montserrat_16, 0);
    lv_obj_center(w.gyro_label);

    lv_obj_t *gyro_title = lv_label_create(scr);
    lv_label_set_text(gyro_title, "GYRO");
    lv_obj_set_style_text_color(gyro_title, CLR_FG, 0);
    lv_obj_set_style_text_font(gyro_title, &lv_font_montserrat_12, 0);
    lv_obj_align_to(gyro_title, w.gyro_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    // -- POWER BAR (full-width, below gauges) --
    w.power_bar = lv_bar_create(scr);
    lv_obj_set_size(w.power_bar, 760, 18);
    lv_bar_set_range(w.power_bar, -100, 100);
    lv_bar_set_value(w.power_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(w.power_bar, lv_color_make(0x20, 0x20, 0x20), LV_PART_MAIN);
    lv_obj_set_style_bg_color(w.power_bar, CLR_POWER_POS, LV_PART_INDICATOR);
    lv_obj_set_style_radius(w.power_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(w.power_bar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(w.power_bar, CLR_FG, LV_PART_MAIN);
    lv_obj_set_style_border_width(w.power_bar, 1, LV_PART_MAIN);
    lv_obj_align(w.power_bar, LV_ALIGN_BOTTOM_MID, 0, -95);

    // -- BOTTOM STRIP --

    // Stopwatch (bottom-left)
    w.stopwatch_label = lv_label_create(scr);
    lv_label_set_text(w.stopwatch_label, "00:00.00");
    lv_obj_set_style_text_color(w.stopwatch_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.stopwatch_label, &lv_font_montserrat_30, 0);
    lv_obj_set_pos(w.stopwatch_label, 15, 410);

    // Status icons row
    lv_coord_t icon_y = 445;
    w.sd_icon           = create_icon_label(scr, LV_SYMBOL_SD_CARD,  CLR_FG,          15,  icon_y);
    w.wifi_icon         = create_icon_label(scr, LV_SYMBOL_WIFI,     CLR_FG,          50,  icon_y);
    w.temp_warning_icon = create_icon_label(scr, LV_SYMBOL_WARNING,  CLR_WARN_RED,    90,  icon_y);
    w.warning_icon      = create_icon_label(scr, LV_SYMBOL_WARNING,  CLR_WARN_YELLOW, 125, icon_y);
    w.info_icon         = create_icon_label(scr, LV_SYMBOL_LIST,     CLR_WARN_YELLOW, 160, icon_y);

    // Logo placeholder (centre-bottom)
    w.logo_icon = lv_label_create(scr);
    lv_label_set_text(w.logo_icon, "SB");
    lv_obj_set_style_text_color(w.logo_icon, CLR_LOGO, 0);
    lv_obj_set_style_text_font(w.logo_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(w.logo_icon, 380, 440);

    // Error message (centre-bottom, above icons)
    w.error_label = lv_label_create(scr);
    lv_label_set_text(w.error_label, "");
    lv_obj_set_style_text_color(w.error_label, CLR_WARN_RED, 0);
    lv_obj_set_style_text_font(w.error_label, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(w.error_label, 430, 410);

    // BMS status
    w.bms_status_label = lv_label_create(scr);
    lv_label_set_text(w.bms_status_label, "BMS: OK");
    lv_obj_set_style_text_color(w.bms_status_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.bms_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(w.bms_status_label, 430, 435);

    // Battery voltage + percentage (bottom-right)
    w.batt_voltage_label = lv_label_create(scr);
    lv_label_set_text(w.batt_voltage_label, "0.0 V");
    lv_obj_set_style_text_color(w.batt_voltage_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.batt_voltage_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(w.batt_voltage_label, 650, 415);

    w.batt_percent_label = lv_label_create(scr);
    lv_label_set_text(w.batt_percent_label, "-- %");
    lv_obj_set_style_text_color(w.batt_percent_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.batt_percent_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(w.batt_percent_label, 650, 445);

    // Battery icon placeholder
    w.batt_icon = create_icon_label(scr, LV_SYMBOL_BATTERY_FULL,
                                    CLR_WARN_RED, 740, 425);
}

// ============================================================================
// dashboard_refresh()  -- push DashboardState data into widgets
// ============================================================================

static int rpm_to_mph(float rpm)
{
    return (int)(rpm / GEAR_RATIO * (float)M_PI * WHEEL_DIAM_M / 60.0f * MPH_CONVERT);
}

void dashboard_refresh(const DashboardState &state)
{
    char buf[32];

    // -- Speed --
    int speed = rpm_to_mph(state.motor.RPM);
    if (speed != prev_speed) {
        snprintf(buf, sizeof(buf), "%d", speed);
        lv_label_set_text(w.speed_label, buf);
        prev_speed = speed;
    }

    // -- RPM --
    int rpm = (int)state.motor.RPM;
    if (rpm != prev_rpm) {
        lv_arc_set_value(w.rpm_arc, rpm);
        snprintf(buf, sizeof(buf), "%d", rpm);
        lv_label_set_text(w.rpm_value_label, buf);
        prev_rpm = rpm;
    }

    // -- Temperature arcs --
    // Battery pack: max thermistor reading
    float max_batt_temp = 0;
    for (int i = 0; i < DASHBOARD_THERMISTOR_COUNT; i++) {
        if (state.thermistors.temps[i] > max_batt_temp)
            max_batt_temp = state.thermistors.temps[i];
    }
    lv_arc_set_value(w.batt_temp_arc, (int)max_batt_temp);
    snprintf(buf, sizeof(buf), "%d C", (int)max_batt_temp);
    lv_label_set_text(w.batt_temp_label, buf);

    // Motor temperature
    lv_arc_set_value(w.motor_temp_arc, (int)state.temps.motor_temperature);
    snprintf(buf, sizeof(buf), "%d C", (int)state.temps.motor_temperature);
    lv_label_set_text(w.motor_temp_label, buf);

    // Motor controller temperature
    lv_arc_set_value(w.mc_temp_arc, (int)state.temps.motor_controller_temperature);
    snprintf(buf, sizeof(buf), "%d C", (int)state.temps.motor_controller_temperature);
    lv_label_set_text(w.mc_temp_label, buf);

    // -- Gyro (roll angle) --
    int roll = (int)state.gyro.roll_angle;
    lv_arc_set_value(w.gyro_arc, roll);
    snprintf(buf, sizeof(buf), "%d deg", roll);
    lv_label_set_text(w.gyro_label, buf);

    // -- Power bar (motor current as proxy) --
    // Scale motor_current into -100..100 range; 300A full scale
    int power_pct = (int)(state.motor.motor_current / 3.0f);
    if (power_pct > 100)  power_pct = 100;
    if (power_pct < -100) power_pct = -100;
    lv_bar_set_value(w.power_bar, power_pct, LV_ANIM_OFF);
    if (power_pct >= 0) {
        lv_obj_set_style_bg_color(w.power_bar, CLR_POWER_POS, LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_bg_color(w.power_bar, CLR_POWER_NEG, LV_PART_INDICATOR);
    }

    // -- Battery voltage (sum of HV cells) --
    snprintf(buf, sizeof(buf), "%.1f V", state.battery.hv_series_voltage);
    lv_label_set_text(w.batt_voltage_label, buf);

    // Battery % estimate: linear map across nominal 20s LiFePO4 range
    // 20 cells x 2.5V empty = 50V, 20 cells x 3.65V full = 73V
    float pct = (state.battery.hv_series_voltage - 50.0f)
              / (73.0f - 50.0f) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    snprintf(buf, sizeof(buf), "%.0f %%", pct);
    lv_label_set_text(w.batt_percent_label, buf);

    if (pct < 20.0f)
        lv_obj_set_style_text_color(w.batt_icon, CLR_WARN_RED, 0);
    else if (pct < 50.0f)
        lv_obj_set_style_text_color(w.batt_icon, CLR_WARN_YELLOW, 0);
    else
        lv_obj_set_style_text_color(w.batt_icon, CLR_FG, 0);

    // -- Motor error message --
    if (state.motor.error_message != 0) {
        snprintf(buf, sizeof(buf), "ERR: 0x%04X", state.motor.error_message);
        lv_label_set_text(w.error_label, buf);
        lv_obj_clear_flag(w.error_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(w.error_label, LV_OBJ_FLAG_HIDDEN);
    }

    // -- BMS status flag --
    int bms_flag = (int)state.bms.bms_status_flag;
    if (bms_flag == 0) {
        lv_label_set_text(w.bms_status_label, "BMS: OK");
        lv_obj_set_style_text_color(w.bms_status_label, CLR_FG, 0);
    } else {
        snprintf(buf, sizeof(buf), "BMS: 0x%02X", bms_flag);
        lv_label_set_text(w.bms_status_label, buf);
        lv_obj_set_style_text_color(w.bms_status_label, CLR_WARN_RED, 0);
    }

    // -- SD card icon --
    if (state.sd_started) {
        lv_obj_set_style_text_color(w.sd_icon, CLR_FG, 0);
    } else {
        lv_obj_set_style_text_color(w.sd_icon, CLR_WARN_YELLOW, 0);
    }
}

// ============================================================================
// dashboardTask()  -- FreeRTOS task that refreshes the UI at ~10 Hz
// ============================================================================

void dashboardTask(void *param)
{
    DashboardState *state = (DashboardState *)param;

    while (true) {
        if (lvgl_port_lock(-1)) {
            dashboard_refresh(*state);
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
