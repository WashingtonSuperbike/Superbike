/**
 * Header file for the precharge FW + task
 */
#pragma once

#include "CAN.h"
#include "DataLogging.h"
#include "context.h"

#define PRECHARGE_TASK_STACK_SIZE configMINIMAL_STACK_SIZE + 2000

/// THIS NEEDS TO BE CHANGED TO OUR ACTUAL NUMBER OF LTCs
#define NUMBER_OF_LTCS 2
/// Motor controller temperature max. This might need to be changed
/// depending on how high the MCU hits during operation.
#define MOTORCONTROLLER_TEMP_MAX 65
/// Motor temperature max. This might need to be changed
/// depending on how high the motor hits during operation.
#define MOTOR_TEMP_MAX 80

/// An enum for all the states. OFF, Precharge, ON, Error
enum HV_STATE {HV_OFF , HV_PRECHARGING, HV_ON, HV_ERROR};

typedef struct {
  Context *context;
} PreChargeTaskData;

void preChargeTask(void *taskData);
bool isPrecharged(PreChargeTaskData preChargeData);
bool isHVSafe(PreChargeTaskData preChargeData);
const char* state_name(HV_STATE state);
