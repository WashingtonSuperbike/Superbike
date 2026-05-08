/**
 * main.cpp - Display board entry point
 *
 * Initialises the Waveshare ESP32-S3-Touch-LCD-5, starts LVGL, creates
 * the superbike dashboard UI, and optionally runs simulation tasks so the gauges
 * animate with realistic-looking data for visual validation.
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
// Driving simulation task: sweeps all gauges so you can see the layout on-screen
// ============================================================================
static void drivingSimulationTask(void *param)
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
        dashState.temps.motor_controller_temperature = (uint8_t)(40.0f + 5.0f * sin_t);
        dashState.temps.motor_temperature = (uint8_t)(50.0f + 5.0f * cos_t);

        // Update timestamps so UI icons don't show "disconnected" (gray)
        dashState.motor_last_rx_ms = millis();
        dashState.bms_last_rx_ms = millis();
        dashState.can_status.store(CanStatus::RECEIVING);

        // Simulated BMS thermistors for UI/testing
        for (int i = 0; i < THERMISTOR_COUNT; i++) {
            float wave = 3.0f * sinf(t * 0.7f + (float)i * 0.55f);
            dashState.thermistors.temps[i] = 32.0f + wave;
            dashState.thermistors.temps_valid[i] = true;
        }

        if (stage == 0) {
            // Stage 0: Normal Operation (Coasting / Regen test)
            dashState.motor.motor_current = 0.0f;
            dashState.temps.throttle = 0;
            // Speed stays high from general simulation logic above
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
            dashState.thermistors.temps[3] = BATT_TEMP_CRIT_CELSIUS + 1.5f;
        }
        else if (stage == 4) {
            // Stage 4: Both Critical (Both Red)
            dashState.bms.bms_status_flag = 0x02; // LOW CELL VOLT (CRIT)
            dashState.motor.error_message = 0x02; // OVER VOLTAGE (CRIT)
            dashState.thermistors.temps[6] = BATT_TEMP_CRIT_CELSIUS + 3.0f;
        }

        // Hold spinlock while writing cell voltages
        taskENTER_CRITICAL(&g_cell_voltages_mux);
        for (int i = 0; i < CELL_COUNT; i++) {
            dashState.battery.cell_voltages[i] = 3.7f + 0.4f * fabsf(sinf(t + i * 0.25f));
        }
        taskEXIT_CRITICAL(&g_cell_voltages_mux);

        t += 0.05f;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================================
// Charging simulation task: animates all charging-screen fields
// ============================================================================
static void chargingSimulationTask(void *param)
{
    (void)param;

    // Simulate a charge session from ~30% to 100%, then wrap.
    // 0% = 3.45 V/cell, 100% = 4.00 V/cell.
    const float CELL_EMPTY_V = CELL_LOWEST_V;
    const float CELL_FULL_V  = CELL_HIGHEST_V;
    const float CELL_RANGE_V = CELL_FULL_V - CELL_EMPTY_V;
    const float PACK_FULL_V  = PACK_VOLTAGE_FULL_V;

    float sim_soc = 0.30f; // 0.0..1.0
    float t = 0.0f;

    while (true) {
        // SOC ramp for charge progression
        sim_soc += 0.0012f;
        if (sim_soc >= 1.0f) {
            sim_soc = 0.30f;
        }

        // Generate per-cell voltages with small spread; enforce std dev <= 0.05 V.
        float base_cell_v = CELL_EMPTY_V + sim_soc * CELL_RANGE_V;
        float jitter[CELL_COUNT];
        float jitter_sum = 0.0f;
        for (int i = 0; i < CELL_COUNT; i++) {
            float r = ((float)(esp_random() % 2001) / 1000.0f) - 1.0f; // [-1, 1]
            float wave = 0.5f * sinf(t + (float)i * 0.31f);
            jitter[i] = 0.6f * r + 0.4f * wave;
            jitter_sum += jitter[i];
        }

        float jitter_mean = jitter_sum / (float)CELL_COUNT;
        float jitter_var = 0.0f;
        for (int i = 0; i < CELL_COUNT; i++) {
            jitter[i] -= jitter_mean; // zero-mean spread around base
            jitter_var += jitter[i] * jitter[i];
        }

        float jitter_std = sqrtf(jitter_var / (float)CELL_COUNT);
        const float target_std_v = 0.025f;
        float scale = (jitter_std > 1e-6f) ? (target_std_v / jitter_std) : 0.0f;

        float pack_voltage = 0.0f;
        taskENTER_CRITICAL(&g_cell_voltages_mux);
        for (int i = 0; i < CELL_COUNT; i++) {
            float cell_v = base_cell_v + jitter[i] * scale;
            if (cell_v < CELL_EMPTY_V) cell_v = CELL_EMPTY_V;
            if (cell_v > CELL_FULL_V) cell_v = CELL_FULL_V;
            dashState.battery.cell_voltages[i] = cell_v;
            pack_voltage += cell_v;
        }
        dashState.battery.cell_voltages_ready = true;
        taskEXIT_CRITICAL(&g_cell_voltages_mux);

        // Charger output voltage tracks pack voltage with a small constant offset
        dashState.charger.output_voltage = pack_voltage + 2.0f;

        // Ensure pack voltage equals sum of simulated cells.
        dashState.battery.hv_series_voltage = pack_voltage;

        // Charging current: full rate while bulk-charging, tapers near 100%
        float avg_cell_v = pack_voltage / (float)CELL_COUNT;
        float pct = (avg_cell_v - CELL_EMPTY_V) / CELL_RANGE_V; // 0.0..1.0
        if (pct < 0.0f) pct = 0.0f;
        if (pct > 1.0f) pct = 1.0f;
        float max_current = 20.0f; // A
        float current = max_current * (1.0f - 0.8f * pct); // tapers from 20 A → 4 A
        current += ((float)(esp_random() % 20) - 10.0f) * 0.1f; // ±1 A noise
        if (current < 0.5f) current = 0.5f;
        dashState.charger.output_current = current;

        // Charger temperature: warms up as session progresses, small noise
        dashState.charger.charger_temp = (int8_t)(30.0f + 25.0f * pct
                                          + ((float)(esp_random() % 10) - 5.0f) * 0.5f);

        // BMS thermistor simulation: warm trend with charge %, plus spatial variation
        for (int i = 0; i < THERMISTOR_COUNT; i++) {
            float wave = 2.0f * sinf(t * 0.8f + (float)i * 0.6f);
            float tilt = ((float)i - (float)(THERMISTOR_COUNT - 1) * 0.5f) * 0.25f;
            float noise = ((float)(esp_random() % 21) - 10.0f) * 0.08f; // ~±0.8C
            dashState.thermistors.temps[i] = 28.0f + 20.0f * pct + wave + tilt + noise;
            dashState.thermistors.temps_valid[i] = true;
        }

        // EVCC setpoints (charge controller telling the charger what to do)
        dashState.evcc.en             = 1;
        dashState.evcc.charge_voltage = PACK_FULL_V;
        dashState.evcc.charge_current = max_current;

        // Status flag: 0 = normal, simulate a brief fault partway through
        dashState.charger.status_flag = (pct > 0.60f && pct < 0.65f) ? 0x01 : 0x00;
        dashState.charger.charge_flag = 0x00;

        t += 0.07f;
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
#ifdef DEBUG_SPEEDOMETER_SCREEN_ONLY
    xTaskCreatePinnedToCore(
        drivingSimulationTask,
        "dash_sim",
        4096,
        NULL,
        1,
        NULL,
        1
    );
#endif

#ifdef DEBUG_CHARGING_SCREEN_ONLY
    xTaskCreatePinnedToCore(
        chargingSimulationTask,
        "charge_sim",
        4096,
        NULL,
        1,
        NULL,
        1
    );
#endif


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
