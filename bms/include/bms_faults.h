
#ifndef BMS_FAULTS_H
#define BMS_FAULTS_H


#include <stdint.h>
#include "bms_config.h"
#include <stdbool.h>


// Fault flags for the BMS system
// To create a fault flag, use a bitwise OR operation to combine the relevant flags. 
// Creating a bitmask.
typedef enum {
    BMS_FAULT_NONE                = 0u,
    BMS_FAULT_CELL_OVERVOLTAGE    = (1u << 0),
    BMS_FAULT_CELL_UNDERVOLTAGE   = (1u << 1),
    BMS_FAULT_OVERTEMPERATURE     = (1u << 2),
    BMS_FAULT_INVALID_MEASUREMENT = (1u << 3),
    BMS_FAULT_OVERCURRENT        = (1u << 4),
    BMS_FAULT_INVALID_CONFIG = (1u << 5)
} BMSFaultFlag;


typedef struct {
    uint32_t fault_flags; // Bitmask of BMSFaultFlag values

    uint32_t overvoltage_mask; // Bitmask indicating which cell groups are overvoltage
    uint32_t undervoltage_mask; // Bitmask indicating which cell groups are undervoltage
    uint32_t invalid_voltage_mask; // Bitmask indicating which cell groups have invalid voltage measurements
} BMSFaultReport;

// Evaluates the BMS measurements against the protection configuration and updates the fault report accordingly.
// Does two things: returns a boolean indicating if any faults were detected, and 
//updates the fault report with the specific fault flags and masks for overvoltage, undervoltage, and invalid measurements.
bool BMS_EvaluateVoltageFaults(
    const BMSMeasurements *measurements,
    const BMSProtectionConfig *config,
    BMSFaultReport *report
);

#endif
