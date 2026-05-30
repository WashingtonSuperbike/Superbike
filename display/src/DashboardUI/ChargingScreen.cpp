/**
 * ChargingScreen.cpp — Charging screen builder and refresher.
 *
 * Extracted from DashboardUI.cpp. Contains all charging-screen-specific logic:
 * widget construction and data refresh.
 */

#include "ChargingScreen.h"
#include "Config.h"
#include <Arduino.h>
#include <cstdio>
#include <cmath>

// ============================================================================
// STATIC WIDGET STATE
// ============================================================================

static ChargingWidgets w;

// Include the large font for digital battery percentage readout
LV_FONT_DECLARE(lv_font_montserrat_144);

// ============================================================================
// BUILDER
// ============================================================================

void charging_screen_build(lv_obj_t *scr)
{
    // Big Battery Percentage — number only (144px font only has digits 0-9)
    w.chg_batt_pct_label = lv_label_create(scr);
    lv_obj_set_style_text_font(w.chg_batt_pct_label, &lv_font_montserrat_144, 0);
    lv_obj_set_style_text_color(w.chg_batt_pct_label, CLR_WHITE, 0);
    lv_obj_set_style_text_align(w.chg_batt_pct_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(w.chg_batt_pct_label, LV_ALIGN_TOP_MID, -30, 40);
    lv_obj_set_width(w.chg_batt_pct_label, 200);
    lv_label_set_text(w.chg_batt_pct_label, "0");

    // "%" symbol in a full font, positioned to the right of the number
    w.chg_batt_pct_symbol = lv_label_create(scr);
    lv_obj_set_style_text_font(w.chg_batt_pct_symbol, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(w.chg_batt_pct_symbol, CLR_WHITE, 0);
    lv_obj_align_to(w.chg_batt_pct_symbol, w.chg_batt_pct_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 20, 5);
    lv_label_set_text(w.chg_batt_pct_symbol, "%");

    // Charging Bar
    w.chg_bar = lv_bar_create(scr);
    lv_obj_set_size(w.chg_bar, 600, 20);
    lv_obj_align(w.chg_bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(w.chg_bar, lv_color_make(0x30, 0x30, 0x30), LV_PART_MAIN);
    lv_obj_set_style_bg_color(w.chg_bar, CLR_STATUS_GREEN, LV_PART_INDICATOR);

    // Stats Grid
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 700, 210);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_pad_row(cont, 10, 0);
    lv_obj_set_style_pad_column(cont, 10, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    auto create_stat = [&](lv_obj_t **lbl, const char *title) {
        lv_obj_t *stat_cont = lv_obj_create(cont);
        lv_obj_set_size(stat_cont, 160, 92);
        lv_obj_set_style_bg_color(stat_cont, lv_color_make(0x20, 0x20, 0x20), 0);
        lv_obj_set_style_border_color(stat_cont, lv_color_make(0x40, 0x40, 0x40), 0);

        lv_obj_t *t = lv_label_create(stat_cont);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_color(t, CLR_WHITE, 0);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 4);

        *lbl = lv_label_create(stat_cont);
        lv_label_set_text(*lbl, "--");
        lv_obj_set_style_text_color(*lbl, CLR_WHITE, 0);
        lv_obj_set_style_text_font(*lbl, &lv_font_montserrat_24, 0);
        lv_obj_align(*lbl, LV_ALIGN_CENTER, 0, 8);
    };

    create_stat(&w.chg_voltage_label, "PACK VOLTAGE");
    create_stat(&w.chg_current_label, "CHARGE CURRENT");
    create_stat(&w.chg_power_label, "INPUT POWER");
    create_stat(&w.chg_temp_label, "CHARGE TEMP");
    create_stat(&w.chg_avg_cell_v_label, "AVG CELL V");
    create_stat(&w.chg_cell_stddev_label, "CELL DEVIATION");
    create_stat(&w.chg_bms_max_temp_label, "MAX CELL TEMP");
    create_stat(&w.chg_eta_label, "TIME TO 100%");

    w.chg_status_label = lv_label_create(scr);
    // lv_obj_align(w.chg_status_label, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_align_to(w.chg_status_label, w.chg_batt_pct_label, LV_ALIGN_OUT_BOTTOM_MID, -50, 20);
    lv_obj_set_style_text_color(w.chg_status_label, CLR_STATUS_GREEN, 0);
    lv_obj_set_style_text_font(w.chg_status_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(w.chg_status_label, "CHARGING ACTIVE");
}

// ============================================================================
// REFRESHER
// ============================================================================

void charging_screen_refresh(const DashboardState &state)
{
    char buf[32];

    // Battery %
    float voltage = state.battery.hv_series_voltage;
    float pct_chg_f = pack_voltage_to_pct(voltage);
    int pct_chg = (int)pct_chg_f;

    snprintf(buf, sizeof(buf), "%d", pct_chg);
    lv_label_set_text(w.chg_batt_pct_label, buf);
    lv_bar_set_value(w.chg_bar, pct_chg, LV_ANIM_ON);
    lv_color_t bar_color = (pct_chg < 20) ? CLR_WARN_RED : (pct_chg < 80) ? CLR_WARN_YELLOW : CLR_STATUS_GREEN;
    lv_obj_set_style_bg_color(w.chg_bar, bar_color, LV_PART_INDICATOR);

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

    // Cell-voltage derived metrics
    if (state.battery.cell_voltages_ready) {
        float cells[CELL_COUNT];
        taskENTER_CRITICAL(&g_cell_voltages_mux);
        for (int i = 0; i < CELL_COUNT; i++) {
            cells[i] = state.battery.cell_voltages[i];
        }
        taskEXIT_CRITICAL(&g_cell_voltages_mux);

        float mean = 0.0f;
        for (int i = 0; i < CELL_COUNT; i++) mean += cells[i];
        mean /= (float)CELL_COUNT;

        float variance = 0.0f;
        for (int i = 0; i < CELL_COUNT; i++) {
            float d = cells[i] - mean;
            variance += d * d;
        }
        variance /= (float)CELL_COUNT;
        float stddev_v = sqrtf(variance);

        snprintf(buf, sizeof(buf), "%.3f V", mean);
        lv_label_set_text(w.chg_avg_cell_v_label, buf);

        snprintf(buf, sizeof(buf), "%.1f mV", stddev_v * 1000.0f);
        lv_label_set_text(w.chg_cell_stddev_label, buf);
    } else {
        lv_label_set_text(w.chg_avg_cell_v_label, "--");
        lv_label_set_text(w.chg_cell_stddev_label, "--");
    }

    // Max valid BMS thermistor temperature
    float max_therm = -1000.0f;
    bool have_therm = false;
    for (int i = 0; i < THERMISTOR_COUNT; i++) {
        if (state.thermistors.temps_valid[i]) {
            if (!have_therm || state.thermistors.temps[i] > max_therm) {
                max_therm = state.thermistors.temps[i];
                have_therm = true;
            }
        }
    }
    if (have_therm) {
        snprintf(buf, sizeof(buf), "%.1f C", max_therm);
        lv_label_set_text(w.chg_bms_max_temp_label, buf);
    } else {
        lv_label_set_text(w.chg_bms_max_temp_label, "--");
    }

    // ETA to 100% from charge current and pack capacity
    if (state.charger.output_current < 0.1f || pct_chg_f >= 99.9f) {
        lv_label_set_text(w.chg_eta_label, "--");
    } else {
        float ah_remaining    = PACK_CAPACITY_AH * (1.0f - pct_chg_f / 100.0f);
        float hours_remaining = ah_remaining / state.charger.output_current;
        if (hours_remaining > 99.0f) {
            lv_label_set_text(w.chg_eta_label, ">99h");
        } else if (hours_remaining < 1.0f) {
            int mins = (int)(hours_remaining * 60.0f + 0.5f);
            snprintf(buf, sizeof(buf), "%dm", mins);
            lv_label_set_text(w.chg_eta_label, buf);
        } else {
            int hours = (int)hours_remaining;
            int mins  = (int)((hours_remaining - (float)hours) * 60.0f + 0.5f);
            if (mins >= 60) { mins = 0; hours++; }
            snprintf(buf, sizeof(buf), "%dh %02dm", hours, mins);
            lv_label_set_text(w.chg_eta_label, buf);
        }
    }

    // Status
    if (state.charger.output_current < 0.1f) {
        lv_label_set_text(w.chg_status_label, "CHARGER IDLE");
        lv_obj_set_style_text_color(w.chg_status_label, CLR_WHITE, 0);
    } else {
        lv_label_set_text(w.chg_status_label, "CHARGING ACTIVE");
        lv_obj_set_style_text_color(w.chg_status_label, CLR_STATUS_GREEN, 0);
    }
}
