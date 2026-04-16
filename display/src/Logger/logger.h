#pragma once

/**
 * FreeRTOS task: writes CSV telemetry rows at 20 Hz.
 * Creates a new datetime-named file on SD mount, writes a 34-column header,
 * logs data rows, flushes every 10s, and handles card removal gracefully.
 *
 * Pin to Core 0, priority 1. Pass DashboardState* as task parameter.
 * Uses sd_get_fs() and sd_get_spi_mutex() for direct SdFat access (D-02, D-04).
 */
void logger_task(void *param);
