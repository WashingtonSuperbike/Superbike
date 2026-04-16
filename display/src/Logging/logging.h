#pragma once

struct DashboardState;  // forward declare

/**
 * FreeRTOS task: writes a datetime-named CSV log file at 20 Hz.
 * Polls state->sd_started each iteration; closes the file on card removal
 * and opens a new dated file on re-insertion.
 * Pin to Core 0. Stack size: 8192 bytes. Pass DashboardState* as task parameter.
 */
void loggingTask(void *param);
