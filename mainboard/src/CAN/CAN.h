/**
 * CAN.h - CAN bus task configuration and function declarations
 *
 * Contains definitions for CAN message IDs and relevant function declarations
 * for the bike's CAN bus task.
 */

#pragma once

#include "Config.h"
#include "Context.h"
#include "Constants.h"
#include "../GPIO/Pins.h"

// Forward declaration (struct defined in Context.h)
struct CANTaskData;

// Function declarations

/**
 * A FreeRTOS task that continuously checks the CAN bus for new messages
 * 
 * @param canData pointer to the CAN task data structure
 */

void canTask(void *canData);

/**
 * Ensure CAN bus is initialized and configured properly.
 * 
 * @returns 0 if initialization is successful, negative error code otherwise 
 */
int initCAN();

