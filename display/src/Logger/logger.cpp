#include "logger.h"
#include <Arduino.h>
#include <cmath>
#include <cstring>
#include <SdFat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "DashboardUI/DashboardUI.h"
#include "SDCard/sd_card.h"
#include "RTC/rtc.h"

static const uint32_t FLUSH_INTERVAL_MS = 10000;  // 10 s flush cadence (LOG-05)
static const uint32_t LOG_INTERVAL_MS   = 50;     // 20 Hz write rate (LOG-04)

// ============================================================================
// Speed conversion constants (mirrored from DashboardUI.h)
// ============================================================================
#ifndef GEAR_RATIO
#define GEAR_RATIO   (48.0f / 16.0f)
#endif
#ifndef WHEEL_DIAM_M
#define WHEEL_DIAM_M 0.522f
#endif
#ifndef MPH_CONVERT
#define MPH_CONVERT  2.2369362920544f
#endif

// ============================================================================
// writeHeader — 34-column CSV header row (LOG-02, LOG-03)
// ============================================================================
static void writeHeader(FsFile &file)
{
    static const char HEADER[] =
        "elapsed_ms,speed_mph,motor_rpm,motor_current_A,motor_temp_C,mc_temp_C,"
        "bms_voltage_V,aux_voltage_V,"
        "cell_01_V,cell_02_V,cell_03_V,cell_04_V,cell_05_V,cell_06_V,"
        "cell_07_V,cell_08_V,cell_09_V,cell_10_V,cell_11_V,cell_12_V,"
        "cell_13_V,cell_14_V,cell_15_V,cell_16_V,cell_17_V,cell_18_V,"
        "cell_19_V,cell_20_V,cell_21_V,cell_22_V,cell_23_V,cell_24_V,"
        "thermistor_01_C,thermistor_02_C,thermistor_03_C,thermistor_04_C,"
        "thermistor_05_C,thermistor_06_C,thermistor_07_C,thermistor_08_C,"
        "thermistor_09_C,thermistor_10_C\n";
    file.write(HEADER, sizeof(HEADER) - 1);
}

// ============================================================================
// writeRow — format one 34-column CSV data row (LOG-03, LOG-04)
// ============================================================================
static void writeRow(FsFile &file, const DashboardState &state)
{
    // Speed: RPM -> mph using drivetrain constants
    float speed_mph = state.motor.RPM / GEAR_RATIO
                      * (float)M_PI * WHEEL_DIAM_M / 60.0f * MPH_CONVERT;

    // Snapshot cell voltages under spinlock (written by CAN path, read here)
    float cells[24];
    taskENTER_CRITICAL(&g_cell_voltages_mux);
    memcpy(cells, state.battery.hv_cell_voltages, sizeof(cells));
    taskEXIT_CRITICAL(&g_cell_voltages_mux);

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "%lu,%.2f,%.1f,%.2f,%.1f,%.1f,%.2f,%.2f,"
        "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
        "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
        "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
        "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
        (unsigned long)millis(),
        speed_mph,
        state.motor.RPM,
        state.motor.motor_current,
        state.temps.motor_temperature,
        state.temps.motor_controller_temperature,
        state.battery.hv_series_voltage,
        state.battery.aux_battery_voltage,
        cells[0],  cells[1],  cells[2],  cells[3],
        cells[4],  cells[5],  cells[6],  cells[7],
        cells[8],  cells[9],  cells[10], cells[11],
        cells[12], cells[13], cells[14], cells[15],
        cells[16], cells[17], cells[18], cells[19],
        cells[20], cells[21], cells[22], cells[23],
        state.thermistors.temps[0], state.thermistors.temps[1],
        state.thermistors.temps[2], state.thermistors.temps[3],
        state.thermistors.temps[4], state.thermistors.temps[5],
        state.thermistors.temps[6], state.thermistors.temps[7],
        state.thermistors.temps[8], state.thermistors.temps[9]
    );

    if (len > 0 && len < (int)sizeof(buf)) {
        file.write(buf, (size_t)len);
    }
}

// ============================================================================
// logger_task — FreeRTOS entry point (LOG-01 through LOG-06)
// ============================================================================
void logger_task(void *param)
{
    DashboardState *state = (DashboardState *)param;

    bool     file_open       = false;
    FsFile   logFile;
    bool     prev_sd_started = false;
    uint32_t last_flush_ms   = 0;

    while (true) {
        bool sd_now = state->sd_started;

        // --- Card insertion (false -> true): open new datetime-named file (LOG-01, D-05, D-07) ---
        if (!prev_sd_started && sd_now) {
            SemaphoreHandle_t mtx = sd_get_spi_mutex();
            if (xSemaphoreTake(mtx, pdMS_TO_TICKS(50)) == pdTRUE) {
                char buf[30];
                rtc_get_filename(buf, sizeof(buf));
                logFile = sd_get_fs().open(buf, O_WRONLY | O_CREAT | O_TRUNC);
                if (logFile) {
                    writeHeader(logFile);
                    file_open     = true;
                    last_flush_ms = millis();
                    Serial.printf("CSV log: opened %s\n", buf);
                } else {
                    Serial.println("CSV log: failed to open file");
                }
                xSemaphoreGive(mtx);
            }
        }

        // --- Card removal (true -> false): close file cleanly (LOG-06) ---
        if (prev_sd_started && !sd_now) {
            if (file_open) {
                SemaphoreHandle_t mtx = sd_get_spi_mutex();
                if (xSemaphoreTake(mtx, pdMS_TO_TICKS(50)) == pdTRUE) {
                    logFile.close();
                    xSemaphoreGive(mtx);
                }
                file_open = false;
                Serial.println("CSV log: card removed, file closed");
            }
        }

        // --- Write row at 20 Hz while mounted (LOG-04) ---
        if (file_open && sd_now) {
            SemaphoreHandle_t mtx = sd_get_spi_mutex();
            if (xSemaphoreTake(mtx, pdMS_TO_TICKS(10)) == pdTRUE) {
                writeRow(logFile, *state);

                // Flush every 10 s to persist data without thrashing (LOG-05)
                uint32_t now = millis();
                if ((now - last_flush_ms) >= FLUSH_INTERVAL_MS) {
                    logFile.flush();
                    last_flush_ms = now;
                }

                xSemaphoreGive(mtx);
            }
        }

        prev_sd_started = sd_now;
        vTaskDelay(pdMS_TO_TICKS(LOG_INTERVAL_MS));
    }
}
