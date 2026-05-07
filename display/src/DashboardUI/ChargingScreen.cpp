/**
 * ChargingScreen.cpp — Charging screen builder and refresher.
 *
 * Extracted from DashboardUI.cpp. Contains all charging-screen-specific logic:
 * widget construction and data refresh.
 */

#include "ChargingScreen.h"
#include "Config.h"
#include <cstdio>

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
// REFRESHER
// ============================================================================

void charging_screen_refresh(const DashboardState &state)
{
    char buf[32];

    // Battery %
    float voltage = state.battery.hv_series_voltage;
    int pct_chg = (int)pack_voltage_to_pct(voltage);

    snprintf(buf, sizeof(buf), "%d%%", pct_chg);
    lv_label_set_text(w.chg_batt_pct_label, buf);
    lv_bar_set_value(w.chg_bar, pct_chg, LV_ANIM_ON);

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
