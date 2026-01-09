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
void canTask(void *canData);
void initCAN();
