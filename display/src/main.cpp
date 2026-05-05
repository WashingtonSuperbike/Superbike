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
    uint32_t stage_start_ms = millis();
    int stage = 0; // 0: Normal, 1: Warnings, 2: Critical

    while (true) {
        float sin_t   = sinf(t);
        float cos_t   = cosf(t);
        float abs_sin  = fabsf(sin_t);

        // -- DATA SIMULATION (v1.6: with high-frequency noise) --
        dashState.motor.RPM = 2500.0f + 2500.0f * sin_t + ((float)(esp_random() % 100) - 50.0f);
        dashState.motor.motor_current = 100.0f * sin_t + ((float)(esp_random() % 20) - 10.0f);
        dashState.motor.motor_controller_battery_voltage = 60.0f + 8.0f * cos_t;
        dashState.battery.hv_series_voltage = 90.0f + 15.0f * cos_t + ((float)(esp_random() % 20) - 10.0f) / 10.0f;
        dashState.gyro.roll_angle = 30.0f * sin_t;

        // -- ERROR SIMULATION CYCLE (v1.6: staggered for independent icon verification) --
        uint32_t elapsed = millis() - stage_start_ms;
        
        if (elapsed > 10000) { // Switch stage every 10 seconds
            stage = (stage + 1) % 5;
            stage_start_ms = millis();
            Serial.printf("Simulation switching to Stage %d\n", stage);
        }

        // Reset all error triggers
        dashState.bms.bms_status_flag = 0;
        dashState.bms.bms_c_fault = 0;
        dashState.motor.error_message = 0;
        dashState.temps.motor_controller_temperature = 40.0f;
        dashState.temps.motor_temperature = 50.0f;

        if (stage == 0) {
            // Stage 0: Normal Operation (All Green)
        } 
        else if (stage == 1) {
            // Stage 1: BMS Warning Only (BMS Yellow, MC Green)
            dashState.bms.bms_c_fault = 0x01; // CONFIG UNLOCKED
        }
        else if (stage == 2) {
            // Stage 2: MC Warning Only (BMS Green, MC Yellow)
            dashState.motor.error_message = 0x01; // ID ANGLE FAULT
        }
        else if (stage == 3) {
            // Stage 3: BMS Critical, MC Warning (BMS Red, MC Yellow)
            dashState.bms.bms_status_flag = 0x01; // HIGH CELL VOLT (CRIT)
            dashState.motor.error_message = 0x10; // STALL FAULT (WARN)
        }
        else if (stage == 4) {
            // Stage 4: Both Critical (Both Red)
            dashState.bms.bms_status_flag = 0x02; // LOW CELL VOLT (CRIT)
            dashState.motor.error_message = 0x02; // OVER VOLTAGE (CRIT)
        }

        // Hold spinlock while writing cell voltages
        taskENTER_CRITICAL(&g_cell_voltages_mux);
        for (int i = 0; i < CONFIG_HV_CELL_COUNT; i++) {
            dashState.battery.hv_cell_voltages[i] = 3.7f + 0.4f * fabsf(sinf(t + i * 0.25f));
        }
        taskEXIT_CRITICAL(&g_cell_voltages_mux);

        t += 0.05f;
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

    // Dashboard refresh task
    xTaskCreatePinnedToCore(
        dashboardTask,
        "dash_refresh",
        4096,
        &dashState,
        1,
        NULL,
        1
    );

    // Simulation task
    // xTaskCreatePinnedToCore(
    //     simulationTask,
    //     "dash_sim",
    //     4096,
    //     NULL,
    //     1,
    //     NULL,
    //     1
    // );

    // TWAI receive task
    xTaskCreatePinnedToCore(
        [](void *param) {
            DashboardState *state = (DashboardState *)param;
            while (true) {
                waveshare_twai_receive(state);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        },
        "twai_recv",
        8192,
        &dashState,
        1,
        NULL,
        1
    );

    // SD card mount poll task
    xTaskCreatePinnedToCore(
        sd_poll_task,
        "sd_poll",
        4096,
        &dashState,
        1,
        NULL,
        1
    );

    // CSV logging task
    xTaskCreatePinnedToCore(
        logger_task,
        "csv_log",
        8192,
        &dashState,
        1,
        NULL,
        1
    );

    Serial.println("Dashboard running");
}

void loop()
{
    // Nothing to do here - all work happens in FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}
