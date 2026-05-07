/**
 * DashboardUI.cpp — Dashboard coordinator.
 *
 * Owns the two screen lv_obj_t pointers and routes between screens.
 * All widget construction and data refresh logic lives in DriveScreen.cpp
 * and ChargingScreen.cpp respectively.
 */

#include "DashboardUI.h"
#include "DriveScreen.h"
#include "ChargingScreen.h"
#include "../lvgl_config/lvgl_v8_port.h"
#include "Config.h"

// ============================================================================
// COORDINATOR STATE
// ============================================================================

// Root screen objects — created once in dashboard_create(), used for routing.
static lv_obj_t *main_drive_screen  = nullptr;
static lv_obj_t *charging_screen    = nullptr;

// ============================================================================
// dashboard_create()  -- build the full UI (call once under lvgl_port_lock)
// ============================================================================

void dashboard_create(void)
{
    // Create screens
    main_drive_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_drive_screen, CLR_BG, 0);

    charging_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(charging_screen, CLR_BG, 0);

    // Build UIs on respective screens
    drive_screen_build(main_drive_screen);
    charging_screen_build(charging_screen);

    // Initial load
    lv_scr_load(main_drive_screen);
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

    if (is_charging && active_scr != charging_screen) {
        lv_scr_load_anim(charging_screen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
    } else if (!is_charging && active_scr != main_drive_screen) {
        lv_scr_load_anim(main_drive_screen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
    }

    if (is_charging) {
        charging_screen_refresh(state);
    } else {
        drive_screen_refresh(state);
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
