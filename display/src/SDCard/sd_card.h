#pragma once
#include <esp_display_panel.hpp>
#include <SdFat.h>
#include <freertos/semphr.h>

// Forward-declare the board namespace
using namespace esp_panel::board;

/**
 * Initialize SPI bus for SD card. Must be called AFTER board->begin().
 * Configures CH422G expander pin 4 (SD_CS) as output, disables hardware CS,
 * starts SPI on MOSI=11, CLK=12, MISO=13.
 *
 * @param board  Pointer to the already-initialized Board object (for expander access)
 */
void sd_init(Board *board);

/**
 * FreeRTOS task: polls SD mount state every 500ms, updates state->sd_started.
 * Pin to Core 0 (not Core 1 — avoids SPI contention with LVGL display).
 * Pass DashboardState* as task parameter.
 */
void sd_poll_task(void *param);

/**
 * Return a reference to the internal SdFs instance.
 * Logger task uses this to open/write/flush files directly via SdFat API.
 * Always acquire sd_get_spi_mutex() before calling any SdFat methods.
 */
SdFs& sd_get_fs();

/**
 * Return the SPI bus mutex. Both sd_poll_task and logger_task must hold
 * this mutex around any SdFat I/O to prevent concurrent SPI access (D-04).
 */
SemaphoreHandle_t sd_get_spi_mutex();
