/**
 * GPIO.h - GPIO control functions
 *
 * Header file for GPIO control functions including contactor control,
 * precharge control, and HV toggle checking.
 */

#pragma once

#include "Pins.h"
#include <Arduino.h>

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

bool check_HV_toggle();
void open_contactor();
void close_contactor();
void open_precharge();
void close_precharge();
void initGPIO();
