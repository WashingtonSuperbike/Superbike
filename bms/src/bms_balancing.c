
#include "bms_balancing.h"
#include <stddef.h>
#include <math.h>

bool BMS_SelectBalancing(
    const BMSMeasurements *measurements,
    const BMSBalancingConfig *config,
    uint32_t previous_mask,
    bool balancing_permitted,
    uint32_t *output_mask
)
{
    // 1. Verify output pointer
    if (output_mask == NULL) {
        return false;
    }

    // Default: no groups selected for balancing
    *output_mask = 0u;

    // 2. Validate incoming data and configuration
    if (measurements == NULL ||
        !BMS_ValidateBalancingConfig(config) ||
        !measurements->fresh) {
        return false;
    }

    float sum = 0.0f;

    // 3. Validate and sum all 24 cell-group voltages
    for (uint32_t i = 0; i < BMS_CELL_COUNT; i++) {

        float voltage = measurements->cell_voltage_v[i];

        if (!measurements->cell_voltage_valid[i] ||
            !isfinite(voltage) ||
            voltage <= 0.0f) {
            return false;
        }

        sum += voltage;
    }

    // 4. Check whether balancing is permitted
    if (!balancing_permitted) {
        return true;
    }

    // 5. Calculate average cell-group voltage
    float average = sum / (float)BMS_CELL_COUNT;

    // 6. Track which groups we select
    uint32_t selected_mask = 0u;

    // 7. Select up to the maximum allowed number
    for (uint32_t count = 0;
         count < config->max_balancing_groups;
         count++) {

        int best_index = -1;
        float highest_priority = 0.0f;

        // Examine all 24 groups
        for (uint32_t i = 0; i < BMS_CELL_COUNT; i++) {

            uint32_t group_bit = 1u << i; // ex: 00000001 = group cell 0, 00000010 = group cell 1, etc.

            // Skip groups already selected
            if ((selected_mask & group_bit) != 0u) {
                continue;
            }

            // Check whether this group was balancing
            bool was_balancing =
                (previous_mask & group_bit) != 0u;

            // Determine appropriate threshold
            float threshold;

            if (was_balancing) {
                threshold = config->stop_delta_v;
            } else {
                threshold = config->start_delta_v;
            }

            // Calculate voltage deviation from average
            float deviation =
                measurements->cell_voltage_v[i] - average;

            float priority = deviation;

            if (was_balancing) {
                priority += config->switch_margin_v;
            }

            // Check eligibility and priority
            if (deviation > threshold &&
                priority > highest_priority) {

                highest_priority = priority;
                best_index = (int)i;
            }
        }

        // No more eligible groups
        if (best_index < 0) {
            break;
        }

        // Select the highest-deviation group
        selected_mask |= (1u << (uint32_t)best_index);
    }

    // 8. Return the proposed balancing selection
    *output_mask = selected_mask;

    return true;
}
