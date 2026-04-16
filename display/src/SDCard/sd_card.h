#pragma once
#include <esp_display_panel.hpp>
#include <SdFat.h>
#include <freertos/semphr.h>

// Forward-declare the board namespace
using namespace esp_panel::board;

struct DashboardState;  // forward declare

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

/**
 * Open a new log file for writing. Creates or truncates the named file.
 * Call after state->sd_started transitions to true.
 * @param filename  Null-terminated filename (e.g. "2026-04-15_093045.csv")
 * @return true on success, false if open failed
 */
bool sd_open_log_file(const char *filename);

/**
 * Write bytes to the open log file. Caller's buffer must include the newline.
 * @param buf  Data to write (need not be null-terminated)
 * @param len  Number of bytes to write
 * @return true on success (bytes written == len), false on write error or no file open
 */
bool sd_write_log_line(const char *buf, size_t len);

/**
 * Flush the open log file to the SD card (sync write cache + update directory entry).
 * Safe to call when no file is open (returns false silently).
 * @return true on success, false on error or no file open
 */
bool sd_sync_log_file();

/**
 * Close the open log file. Flushes before closing.
 * Safe to call when no file is open (no-op).
 */
void sd_close_log_file();
