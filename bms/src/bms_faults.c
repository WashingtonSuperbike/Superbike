
#include "bms_faults.h"
#include "bms_config.h"
#include "bms_types.h"

#include <stddef.h>
#include <math.h>
#include <stdbool.h>

bool BMS_EvaluateVoltageFaults(
    const BMSMeasurements *measurements,
    const BMSProtectionConfig *config,
    BMSFaultReport *report
)
{
    // Check that the output pointer is valid
    if (report == NULL) {
        return false;
    }

    // Start with an empty fault report
    report->fault_flags = BMS_FAULT_NONE;
    report->overvoltage_mask = 0u;
    report->undervoltage_mask = 0u;
    report->invalid_voltage_mask = 0u;

    // More validation and fault detection will go here
    if (measurements == NULL) {
        report->fault_flags |= BMS_FAULT_INVALID_MEASUREMENT;
        return false;
    }

    // Check whether a protection configuration was provided
    if (config == NULL) {
        report->fault_flags |= BMS_FAULT_INVALID_CONFIG;
        return false;
    }


    // Validate voltage protection thresholds
    if (!isfinite(config->overvoltage_trip_v) ||
        !isfinite(config->undervoltage_trip_v) ||
        config->undervoltage_trip_v <= 0.0f ||
        config->overvoltage_trip_v <= config->undervoltage_trip_v) {

        report->fault_flags |= BMS_FAULT_INVALID_CONFIG;
        return false;
    }

    // Reject stale battery measurements
    if (!measurements->fresh) {
        report->fault_flags |= BMS_FAULT_INVALID_MEASUREMENT;
        return false;
    }


    // Examine all 24 battery cell groups
    for (uint32_t i = 0; i < BMS_CELL_COUNT; i++) {

        // Read the voltage of the current group
        float voltage = measurements->cell_voltage_v[i];

        // Voltage validity and fault comparisons
        // will go here.
        if (!measurements->cell_voltage_valid[i] ||
            !isfinite(voltage) ||
            voltage <= 0.0f) {

            report->fault_flags |= BMS_FAULT_INVALID_MEASUREMENT;

            report->invalid_voltage_mask |= (1u << i);

            continue;
        }

        if (voltage > config->overvoltage_trip_v) {

            // Record the fault type
            report->fault_flags |= BMS_FAULT_CELL_OVERVOLTAGE;

            // Record which group experienced the fault
            report->overvoltage_mask |= (1u << i);
        }


        // Check for cell-group undervoltage
        if (voltage < config->undervoltage_trip_v) {

            // Record the type of fault
            report->fault_flags |= BMS_FAULT_CELL_UNDERVOLTAGE;

            // Record which group experienced undervoltage
            report->undervoltage_mask |= (1u << i);
        }


        // Finished checking all 24 cell group

    }

    // Did any group have an invalid voltage reading?
        if (report->invalid_voltage_mask != 0u) {
            return false;
        }

        // All required voltage readings were valid
        return true;


}
