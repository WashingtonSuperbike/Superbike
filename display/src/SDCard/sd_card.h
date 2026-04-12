#pragma once
#include <esp_display_panel.hpp>

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
