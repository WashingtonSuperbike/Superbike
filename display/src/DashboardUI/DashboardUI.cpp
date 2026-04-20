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
#define CLR_DISABLED    lv_color_make(0x80, 0x80, 0x80)  // gray for boot/inactive
#define CLR_LOGO        lv_color_make(0x4B, 0x2C, 0x92)  // purple
#define CLR_MOTORING    lv_color_make(0x00, 0x80, 0xFF)  // blue (motoring / positive current)
#define CLR_REGEN       lv_color_make(0x00, 0xC8, 0x00)  // green (regen / negative current)
#define CLR_NEEDLE      lv_color_make(0xFF, 0x20, 0x00)  // red needle

// ============================================================================
// STATIC WIDGET STATE
// ============================================================================

static DashboardWidgets w;

// Previous values for dirty-checking (only redraw when changed)
static int prev_speed = -1;
static int32_t prev_current_value = 0;   // absolute amps, clamped to 100
static int     prev_current_dir   = 0;   // -1 regen, 0 idle, +1 motoring
static int     prev_batt_temp     = -999;
static int     prev_motor_temp    = -999;
static int     prev_mc_temp       = -999;
static int     prev_gyro_roll     = -999;
static int     prev_batt_mv       = -999; // (int)(voltage * 10)
static int     prev_batt_pct      = -999;
static int     prev_error_msg     = -1;
static int     prev_bms_flag      = -1;
static bool    prev_sd_started    = true;   // force color update on first refresh
static CanStatus prev_can_status  = CanStatus::RECEIVING;  // force color push on first refresh (sentinel != boot BOOT)

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
    lv_obj_align_to(title_lbl, *arc, LV_ALIGN_BOTTOM_MID, 0, 5);
}

// ============================================================================
// dashboard_create()  -- build the full UI (call once under lvgl_port_lock)
// ============================================================================

void dashboard_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);

    // -- SPEEDOMETER DIAL (static pre-rendered image) --
    // Tick marks and labels are baked into a 430x430 RGB565 image to avoid
    // redrawing 141 ticks every frame (the main FPS bottleneck with lv_meter).
    w.dial_img = lv_img_create(scr);
    lv_img_set_src(w.dial_img, &dial_face);
    lv_obj_set_pos(w.dial_img, 35, 15);
    lv_obj_clear_flag(w.dial_img, LV_OBJ_FLAG_CLICKABLE);

    // --- Current arcs (standalone, overlaid on dial image) ---
    // Original meter current scale: range -100..+100, 210° sweep, rotation=60°
    // In LVGL lv_arc angles: 0°=3 o'clock (CW). Meter 0°=12 o'clock (CW).
    // Meter rotation 60° => arc start = 60+90 = 150°. Arc end = 150+210 = 360 = 0°.
    // The arc widget size is 450x450 to match the r_mod=+10 offset from the 430px dial.
    // Centered on dial center (250,230) => top-left = (250-225, 230-225) = (25, 5).

    // Motoring arc (blue): shows 0A to +current, clockwise from 0 mph origin
    // 0A is at the midpoint of the -100..+100 range => 150 + 105 = 255° in arc coords
    w.current_motoring_arc = lv_arc_create(scr);
    lv_obj_set_size(w.current_motoring_arc, 450, 450);
    lv_obj_set_pos(w.current_motoring_arc, 25, 5);
    lv_arc_set_mode(w.current_motoring_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(w.current_motoring_arc, 0, 100);
    lv_arc_set_value(w.current_motoring_arc, 0);
    lv_arc_set_bg_angles(w.current_motoring_arc, 165, 375);   // 0A (165°) to +100A (375°)
    lv_arc_set_rotation(w.current_motoring_arc, 0);
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
    lv_arc_set_bg_angles(w.current_regen_arc, 15, 165);       // -100A (15°) to 0A (165°)
    lv_arc_set_rotation(w.current_regen_arc, 0);
    lv_obj_set_style_arc_color(w.current_regen_arc, CLR_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_color(w.current_regen_arc, CLR_REGEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(w.current_regen_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(w.current_regen_arc, 10, LV_PART_INDICATOR);
    lv_obj_remove_style(w.current_regen_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w.current_regen_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(w.current_regen_arc, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(w.current_regen_arc, LV_OBJ_FLAG_HIDDEN);

    // Speed needle: standalone lv_line so only its narrow strip is redrawn each frame.
    w.needle_line = lv_line_create(scr);
    lv_obj_set_style_line_width(w.needle_line, 5, 0);
    lv_obj_set_style_line_color(w.needle_line, CLR_NEEDLE, 0);
    lv_obj_set_style_line_rounded(w.needle_line, true, 0);
    // Initialise at 0 mph (angle=165°, pointing lower-left from dial centre)
    // Dial centre on screen: (35+215, 15+215) = (250, 230); needle length = 195px
    // LVGL angle convention: 0°=12-o'clock, clockwise. x=sin(θ), y=-cos(θ)
    w.needle_pts[0] = {250, 230};
    w.needle_pts[1] = {(lv_coord_t)(250 + 195.0f * sinf(255.0f * M_PI / 180.0f)),
                       (lv_coord_t)(230 - 195.0f * cosf(255.0f * M_PI / 180.0f))};
    lv_line_set_points(w.needle_line, w.needle_pts, 2);

    // --- Digital speed readout (positioned over dial image) ---
    w.meter_speed_label = lv_label_create(scr);
    lv_label_set_text(w.meter_speed_label, "0");
    lv_obj_set_style_text_color(w.meter_speed_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.meter_speed_label, &lv_font_montserrat_144, 0);
    lv_obj_set_style_text_align(w.meter_speed_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(w.meter_speed_label, 300);
    lv_obj_set_pos(w.meter_speed_label, 100, 300);

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
    lv_obj_align_to(gyro_title, w.gyro_arc, LV_ALIGN_BOTTOM_MID, 0, 5);

    // -- BOTTOM STRIP --

    // Status icons row
    lv_coord_t icon_y = 445;
    w.sd_icon           = create_icon_label(scr, LV_SYMBOL_SD_CARD,  CLR_FG,          15,  icon_y);
    w.can_icon          = create_icon_label(scr, LV_SYMBOL_CAN_LINK, CLR_DISABLED,    50,  icon_y, &can_link);
    w.temp_warning_icon = create_icon_label(scr, LV_SYMBOL_WARNING,  CLR_WARN_RED,    90,  icon_y);
    w.warning_icon      = create_icon_label(scr, LV_SYMBOL_WARNING,  CLR_WARN_YELLOW, 125, icon_y);
    w.info_icon         = create_icon_label(scr, LV_SYMBOL_LIST,     CLR_WARN_YELLOW, 160, icon_y);

    // Logo placeholder (centre-bottom)
    w.logo_icon = lv_img_create(scr);
    lv_img_set_src(w.logo_icon, &logo55);
    lv_obj_set_style_img_recolor(w.logo_icon, CLR_LOGO, 0);
    lv_obj_set_style_img_recolor_opa(w.logo_icon, LV_OPA_COVER, 0);
    lv_obj_align(w.logo_icon, LV_ALIGN_BOTTOM_MID, 0, -15);

    // Error message (centre-bottom, above icons)
    w.error_label = lv_label_create(scr);
    lv_label_set_text(w.error_label, "");
    lv_obj_set_style_text_color(w.error_label, CLR_WARN_RED, 0);
    lv_obj_set_style_text_font(w.error_label, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(w.error_label, 500, 410);

    // BMS status
    w.bms_status_label = lv_label_create(scr);
    lv_label_set_text(w.bms_status_label, "BMS: OK");
    lv_obj_set_style_text_color(w.bms_status_label, CLR_FG, 0);
    lv_obj_set_style_text_font(w.bms_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(w.bms_status_label, 500, 435);

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
    w.batt_icon = create_icon_label(scr, LV_SYMBOL_BATTERY_FULL, CLR_WARN_RED, 740, 425);
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

    // -- Speed (needle + digital label) --
    int speed = rpm_to_mph(state.motor.RPM);
    if (speed < 0)   speed = 0;
    if (speed > 140) speed = 140;   // clamp to dial face max
    if (speed != prev_speed) {
        // Update the standalone needle line — only its narrow strip is redrawn
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
    // Battery pack: max thermistor reading
    float max_batt_temp = 0;
    for (int i = 0; i < DASHBOARD_THERMISTOR_COUNT; i++) {
        if (state.thermistors.temps[i] > max_batt_temp)
            max_batt_temp = state.thermistors.temps[i];
    }
    int batt_temp_int = (int)max_batt_temp;
    if (batt_temp_int != prev_batt_temp) {
        lv_arc_set_value(w.batt_temp_arc, batt_temp_int);
        snprintf(buf, sizeof(buf), "%d C", batt_temp_int);
        lv_label_set_text(w.batt_temp_label, buf);
        prev_batt_temp = batt_temp_int;
    }

    // Motor temperature
    int motor_temp_int = (int)state.temps.motor_temperature;
    if (motor_temp_int != prev_motor_temp) {
        lv_arc_set_value(w.motor_temp_arc, motor_temp_int);
        snprintf(buf, sizeof(buf), "%d C", motor_temp_int);
        lv_label_set_text(w.motor_temp_label, buf);
        prev_motor_temp = motor_temp_int;
    }

    // Motor controller temperature
    int mc_temp_int = (int)state.temps.motor_controller_temperature;
    if (mc_temp_int != prev_mc_temp) {
        lv_arc_set_value(w.mc_temp_arc, mc_temp_int);
        snprintf(buf, sizeof(buf), "%d C", mc_temp_int);
        lv_label_set_text(w.mc_temp_label, buf);
        prev_mc_temp = mc_temp_int;
    }

    // -- Gyro (roll angle) --
    int roll = (int)state.gyro.roll_angle;
    if (roll != prev_gyro_roll) {
        lv_arc_set_value(w.gyro_arc, roll);
        snprintf(buf, sizeof(buf), "%d deg", roll);
        lv_label_set_text(w.gyro_label, buf);
        prev_gyro_roll = roll;
    }

    // -- Motor current arcs (outer ring) --
    float current = state.motor.motor_current;
    float abs_current = fabsf(current);
    if (abs_current > 100.0f) abs_current = 100.0f;

    int cur_dir = (current > 0.0f) ? 1 : (current < 0.0f) ? -1 : 0;
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

    // -- Battery voltage (sum of HV cells) --
    int batt_mv = (int)(state.battery.hv_series_voltage * 10.0f);
    if (batt_mv != prev_batt_mv) {
        snprintf(buf, sizeof(buf), "%.1f V", state.battery.hv_series_voltage);
        lv_label_set_text(w.batt_voltage_label, buf);
        prev_batt_mv = batt_mv;
    }

    // Battery % estimate: linear map across nominal 24s LiIO range
    // 24 cells x 2.5V empty = 60V, 24 cells x 4.2V full = 100.8V
    float pct = (state.battery.hv_series_voltage - 60.0f)
              / (100.8f - 60.0f) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    int pct_int = (int)pct;
    if (pct_int != prev_batt_pct) {
        snprintf(buf, sizeof(buf), "%d %%", pct_int);
        lv_label_set_text(w.batt_percent_label, buf);

        if (pct < 20.0f)
            lv_obj_set_style_text_color(w.batt_icon, CLR_WARN_RED, 0);
        else if (pct < 50.0f)
            lv_obj_set_style_text_color(w.batt_icon, CLR_WARN_YELLOW, 0);
        else
            lv_obj_set_style_text_color(w.batt_icon, CLR_FG, 0);
        prev_batt_pct = pct_int;
    }

    // -- Motor error message --
    int err_msg = state.motor.error_message;
    if (err_msg != prev_error_msg) {
        if (err_msg != 0) {
            snprintf(buf, sizeof(buf), "ERR: 0x%04X", err_msg);
            lv_label_set_text(w.error_label, buf);
            lv_obj_clear_flag(w.error_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(w.error_label, LV_OBJ_FLAG_HIDDEN);
        }
        prev_error_msg = err_msg;
    }

    // -- BMS status flag --
    int bms_flag = (int)state.bms.bms_status_flag;
    if (bms_flag != prev_bms_flag) {
        if (bms_flag == 0) {
            lv_label_set_text(w.bms_status_label, "BMS: OK");
            lv_obj_set_style_text_color(w.bms_status_label, CLR_FG, 0);
        } else {
            snprintf(buf, sizeof(buf), "BMS: 0x%02X", bms_flag);
            lv_label_set_text(w.bms_status_label, buf);
            lv_obj_set_style_text_color(w.bms_status_label, CLR_WARN_RED, 0);
        }
        prev_bms_flag = bms_flag;
    }

    // -- SD card icon --
    if (state.sd_started != prev_sd_started) {
        if (state.sd_started) {
            lv_obj_set_style_text_color(w.sd_icon, CLR_FG, 0);
        } else {
            lv_obj_set_style_text_color(w.sd_icon, CLR_WARN_RED, 0);
        }
        prev_sd_started = state.sd_started;
    }

    // -- CAN status icon (CANU-02, D-11) --
    CanStatus can_st = state.can_status.load();
    if (can_st != prev_can_status) {
        lv_color_t can_color;
        if (can_st == CanStatus::RECEIVING) {
            can_color = CLR_RPM_ARC;       // green — actively receiving frames
        } else if (can_st == CanStatus::TIMEOUT) {
            can_color = CLR_WARN_YELLOW;   // yellow — no frame in last 1 s (after contact)
        } else if (can_st == CanStatus::BOOT) {
            can_color = CLR_DISABLED;      // gray — waiting for first frame
        } else {
            can_color = CLR_WARN_RED;      // red — ERR_PASSIVE or BUS_OFF
        }
        lv_obj_set_style_text_color(w.can_icon, can_color, 0);
        prev_can_status = can_st;
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
