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
// STATIC WIDGET STATE
// ============================================================================

static DashboardWidgets w;

// v1.6 Data Smoothing Filters
static EMAFilter f_rpm(0.5f);
static EMAFilter f_current(0.5f);
static EMAFilter f_voltage(1.5f);
static EMAFilter f_mc_voltage(1.5f);
static EMAFilter f_batt_temp(2.0f);
static EMAFilter f_motor_temp(2.0f);
static EMAFilter f_mc_temp(2.0f);
static EMAFilter f_gyro(0.25f);

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

static void add_error_to_list(DashboardErrorList &list, ErrorSource source, ErrorSeverity severity, const char *desc)
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
    for (int i = 0; i < DASHBOARD_THERMISTOR_COUNT; i++) {
        if (state->thermistors.temps[i] >= BATT_TEMP_MAX) { // Using gauge max as warning threshold
            char t_buf[32];
            snprintf(t_buf, sizeof(t_buf), "BMS: THERMISTOR %d HOT", i);
            add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::WARN, t_buf);
            if (max_bms < ErrorSeverity::WARN) max_bms = ErrorSeverity::WARN;
        }
    }

    // Only check LTC count if we are actually receiving CAN data
    if (state->can_status.load() == CanStatus::RECEIVING && state->bms.ltc_count == 0) {
        add_error_to_list(state->error_list, ErrorSource::BMS, ErrorSeverity::CRIT, "BMS: PACK INCOMPLETE");
        max_bms = ErrorSeverity::CRIT;
    }

    // --- Motor Controller Errors ---
    uint16_t mc_err = (uint16_t)state->motor.error_message;
    // Byte 7 (low byte of error_message)
    if (mc_err & 0x01) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: ID ANGLE FAULT");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x02) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "MC: OVER VOLTAGE");
        max_mc = ErrorSeverity::CRIT;
    }
    if (mc_err & 0x04) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: LOW VOLTAGE");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x08) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: ANGLE SENSOR FAULT");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x10) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: STALL FAULT");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x20) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: INTERNAL VOLTS");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x80) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: THROTTLE AT PWRUP");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }

    // Byte 8 (high byte of error_message)
    if (mc_err & 0x0200) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: INTERNAL RESET");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }
    if (mc_err & 0x0400) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "MC: THROTTLE CIRCUIT");
        max_mc = ErrorSeverity::CRIT;
    }
    if (mc_err & 0x8000) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: GALVANOMETER FAULT");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }

    // Temperature Escalations (using #defined thresholds)
    float mc_temp = state->temps.motor_controller_temperature;
    if (mc_temp >= MC_TEMP_CRIT_CELSIUS || (mc_err & 0x40)) { // 0x40 is OverTemp bit in Byte 7
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "MC: OVER-TEMP CRITICAL");
        max_mc = ErrorSeverity::CRIT;
    } else if (mc_temp >= MC_TEMP_WARN_CELSIUS) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MC: OVER-TEMP WARNING");
        if (max_mc < ErrorSeverity::WARN) max_mc = ErrorSeverity::WARN;
    }

    float motor_temp = state->temps.motor_temperature;
    if (motor_temp >= MOTOR_TEMP_CRIT_CELSIUS || (mc_err & 0x4000)) { // 0x4000 is Motor OverTemp bit in Byte 8
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::CRIT, "MOTOR: OVER-TEMP CRITICAL");
        max_mc = ErrorSeverity::CRIT;
    } else if (motor_temp >= MOTOR_TEMP_WARN_CELSIUS) {
        add_error_to_list(state->error_list, ErrorSource::MOTOR_CONTROLLER, ErrorSeverity::WARN, "MOTOR: OVER-TEMP WARNING");
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
// HELPER: pick arc indicator color based on temperature vs thresholds
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
    lv_arc_set_bg_angles(w.current_regen_arc, 115, 165);       // -100A (115°) to 0A (165°)
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
    lv_label_set_text(w.gyro_label, "0");
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

    // -- v1.6 Data Smoothing --
    // Snap to values on first run to avoid 0 -> value ramp transients
    if (prev_speed == -1) {
        f_rpm.reset(state.motor.RPM);
        f_current.reset(state.motor.motor_current);
        f_voltage.reset(state.battery.hv_series_voltage);
        f_mc_voltage.reset(state.motor.motor_controller_battery_voltage);
        f_gyro.reset(state.gyro.roll_angle);
        f_motor_temp.reset(state.temps.motor_temperature);
        f_mc_temp.reset(state.temps.motor_controller_temperature);
        
        float max_t = 0;
        for (int i = 0; i < DASHBOARD_THERMISTOR_COUNT; i++) {
            if (state.thermistors.temps[i] > max_t) max_t = state.thermistors.temps[i];
        }
        f_batt_temp.reset(max_t);
    }

    float s_rpm     = f_rpm.update(state.motor.RPM);
    float s_current = f_current.update(state.motor.motor_current);
    float s_voltage = f_voltage.update(state.battery.hv_series_voltage);
    float s_mc_v    = f_mc_voltage.update(state.motor.motor_controller_battery_voltage);
    float s_gyro    = f_gyro.update(state.gyro.roll_angle);
    float s_motor_t = f_motor_temp.update(state.temps.motor_temperature);
    float s_mc_t    = f_mc_temp.update(state.temps.motor_controller_temperature);

    float max_batt_temp = 0;
    for (int i = 0; i < DASHBOARD_THERMISTOR_COUNT; i++) {
        if (state.thermistors.temps[i] > max_batt_temp)
            max_batt_temp = state.thermistors.temps[i];
    }
    float s_batt_t = f_batt_temp.update(max_batt_temp);

    // -- Speed (needle + digital label) --
    int speed = rpm_to_mph(s_rpm);
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

    // -- Motor current arcs (outer ring) --
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

    // -- Battery and MC voltage readouts --
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

    // Battery % estimate: linear map across nominal 24s LiIO range
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

    // -- SD card icon --
    if (state.sd_started != prev_sd_started) {
        if (state.sd_started) {
            lv_obj_set_style_text_color(w.sd_icon, CLR_WHITE, 0);
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
            can_color = CLR_STATUS_GREEN;       // green — actively receiving frames
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

    // -- BMS/MC Status Icons (v1.5) --
    auto get_sev_color = [](ErrorSeverity sev) {
        if (sev == ErrorSeverity::CRIT) return CLR_WARN_RED;
        if (sev == ErrorSeverity::WARN) return CLR_WARN_YELLOW;
        if (sev == ErrorSeverity::INFO) return CLR_WHITE;
        return CLR_STATUS_GREEN; // Green for NONE
    };
    lv_obj_set_style_text_color(w.bms_status_label, get_sev_color(state.bms_severity.load()), 0);
    lv_obj_set_style_text_color(w.mc_status_icon,   get_sev_color(state.mc_severity.load()), 0);

    // -- Warning Carousel (v1.5) --
    static int carousel_idx = 0;
    static uint32_t last_carousel_ms = 0;
    const uint32_t carousel_interval_ms = 1500; // 1.5 seconds

    taskENTER_CRITICAL(&g_error_list_mux);
    
    // Count active warnings/infos (CRIT handled by modal in next phase)
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

        if (found_crit_idx != -1) {
            lv_label_set_text(w.modal_desc, state.error_list.errors[found_crit_idx].description);
        } else {
            lv_label_set_text(w.modal_desc, "CRITICAL FAULT");
        }

        taskEXIT_CRITICAL(&g_error_list_mux);
    } else {
        lv_obj_add_flag(w.error_modal, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================================
// dashboardTask()  -- FreeRTOS task that refreshes the UI at ~20 Hz
// ============================================================================

void dashboardTask(void *param)
{
    DashboardState *state = (DashboardState *)param;

    while (true) {
        if (lvgl_port_lock(-1)) {
            dashboard_refresh(*state);
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000 / UPDATE_RATE_HZ));
    }
}
