/**
 * main.cpp - Display board entry point
 *
 * Initialises the Waveshare ESP32-S3-Touch-LCD-5, starts LVGL, creates
 * the superbike dashboard UI, and runs a simulation task so the gauges
 * animate with realistic-looking data for visual validation.
 *
 * Replace the simulation task with real CAN/serial data once inter-board
 * communication is wired up.
 */

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_config/lvgl_v8_port.h"
#include "DashboardUI/DashboardUI.h"
#include "CAN/CAN_Receive.h"
#include "SDCard/sd_card.h"
#include "RTC/rtc.h"
#include "Logger/logger.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// Shared state between simulation task and dashboard refresh task
static DashboardState dashState = {};

// ============================================================================
// Simulation task: sweeps all gauges so you can see the layout on-screen
// ============================================================================

static void simulationTask(void *param)
{
    (void)param;
    float t = 0.0f;

    while (true) {
        float sin_t   = sinf(t);
        float cos_t   = cosf(t);
        float abs_sin  = fabsf(sin_t);

        // RPM: 0-5000 sine wave
        dashState.motor.RPM = 2500.0f + 2500.0f * sin_t;
        // Motor current: -100 to 100 A
        dashState.motor.motor_current = 100.0f * sin_t;
        // HV voltage from motor controller
        dashState.motor.motor_controller_battery_voltage = 60.0f + 8.0f * cos_t;
        // Error message: flash briefly every ~20 seconds
        dashState.motor.error_message = (((int)(t * 10) % 200) < 5) ? 0x0010 : 0;

        // Temperatures
        dashState.temps.motor_temperature = 40.0f + 50.0f * abs_sin;
        dashState.temps.motor_controller_temperature = 30.0f + 40.0f * abs_sin;
        dashState.temps.throttle = abs_sin;

        // Battery thermistors: spread between 20-55 C
        for (int i = 0; i < DASHBOARD_THERMISTOR_COUNT; i++) {
            dashState.thermistors.temps[i] = 25.0f + 20.0f * fabsf(sinf(t + i * 0.3f));
        }

        // Battery voltage: 55-70V range (20s pack)
        dashState.battery.hv_series_voltage = 62.5f + 7.5f * cos_t;
        dashState.battery.aux_battery_voltage = 12.4f + 0.6f * cos_t;

        // Gyro roll: gentle lean simulation
        dashState.gyro.roll_angle = 30.0f * sin_t;

        // BMS: flag an error briefly every ~30 seconds
        dashState.bms.bms_status_flag = (((int)(t * 10) % 300) < 10) ? 1.0f : 0.0f;

        // Cell voltages: 3.3-4.1V sweep with per-cell phase offset (D-03)
        for (int i = 0; i < 24; i++) {
            dashState.battery.hv_cell_voltages[i] = 3.7f + 0.4f * fabsf(sinf(t + i * 0.25f));
        }

        // SD card: owned by sd_poll_task — do not set here
        // dashState.sd_started = true;

        t += 0.02f;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================================
// Arduino setup / loop
// ============================================================================

void setup()
{
    Serial.begin(115200);
    Serial.println("Superbike Dashboard starting...");

    Board *board = new Board();
    board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif
    assert(board->begin());

    // Initialize SD card SPI bus (uses CH422G expander for CS)
    Serial.println("Initializing SD card");
    sd_init(board);

    // Initialize RTC (PCF85063A on shared I2C bus, addr 0x51)
    Serial.println("Initializing RTC");
    rtc_init();

    // Read RTC and print filename string for verification (RTC-02)
    char rtc_filename[30];
    rtc_get_filename(rtc_filename, sizeof(rtc_filename));
    Serial.printf("RTC filename: %s\n", rtc_filename);

    Serial.println("Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    // Build the dashboard UI (must hold LVGL lock)
    Serial.println("Creating dashboard UI");
    lvgl_port_lock(-1);
    dashboard_create();
    lvgl_port_unlock();

    // Initialize TWAI driver for CAN bus communication
    Serial.println("Initializing TWAI driver");
    if (!waveshare_twai_init()) {
        Serial.println("TWAI driver initialization failed");
    }

    // Dashboard refresh task: reads dashState at 10 Hz
    xTaskCreatePinnedToCore(
        dashboardTask,
        "dash_refresh",
        4096,
        &dashState,
        2,
        NULL,
        1
    );

    // Simulation task: animates gauge values including 24 cell voltages (D-03)
    xTaskCreatePinnedToCore(
        simulationTask,
        "dash_sim",
        4096,
        NULL,
        1,
        NULL,
        0
    );

    xTaskCreatePinnedToCore(
        [](void *param) {
            DashboardState *state = (DashboardState *)param;
            while (true) {
                waveshare_twai_receive(state);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        },
        "twai_recv",
        4096,
        &dashState,
        1,
        NULL,
        0
    );

    // SD card mount poll task: checks every 500ms, updates dashState.sd_started
    xTaskCreatePinnedToCore(
        sd_poll_task,
        "sd_poll",
        4096,
        &dashState,
        1,
        NULL,
        0   // Core 0 — avoids SPI contention with LVGL on Core 1
    );

    // CSV logging task: writes telemetry at 20 Hz, polls sd_started for card events
    xTaskCreatePinnedToCore(
        logger_task,
        "csv_log",
        4096,
        &dashState,
        1,
        NULL,
        0   // Core 0 — SdFat must not contend with LVGL on Core 1
    );

    Serial.println("Dashboard running");
}

void loop()
{
    // Nothing to do here - all work happens in FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
