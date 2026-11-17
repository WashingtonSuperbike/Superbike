
#ifndef BMS_BALANCING_H
#define BMS_BALANCING_H

#include <stdbool.h>
#include <stdint.h>

#include "bms_config.h"
#include "bms_types.h"

// Determine which cell groups should be balanced
bool BMS_SelectBalancing(
    const BMSMeasurements *measurements,
    const BMSBalancingConfig *config,
    uint32_t previous_mask, //Storing the previously selected cell groups for balancing
    bool balancing_permitted,
    uint32_t *output_mask //Storing the newly selected cell groups for balancing
);

#endif
