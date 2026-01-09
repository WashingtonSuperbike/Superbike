/**
 * Precharge.h - High voltage precharge system
 *
 * Header file for the precharge firmware and task that manages the
 * high voltage system precharge sequence.
 */

#pragma once

#include "Config.h"
#include "Context.h"
#include "../GPIO/Pins.h"
#include "../GPIO/GPIO.h"
#include <Arduino.h>
// #include <Wire.h>

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void preChargeTask(void *taskData);
void preChargeCircuitFSMTransitions(superbike::PreChargeTaskData preChargeData);
bool isPrecharged(superbike::PreChargeTaskData preChargeData);
bool isHVSafe(superbike::PreChargeTaskData preChargeData);
const char* state_name(HV_STATE state);
