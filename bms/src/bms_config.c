
#include "bms_config.h"

#include <math.h>
#include <stddef.h>

// TEST/SIM ONLY!!!

const BMSBalancingConfig BMS_TEST_BALANCING_CONFIG = {

    .start_delta_v = 0.050f,
    .stop_delta_v = 0.020f,
    .switch_margin_v = 0.015f, 

    .max_balancing_groups = 2,
    .reevaluate_interval_ms = 10000u

};

// Verify that balancing configuration is logically valid
bool BMS_ValidateBalancingConfig(
    const BMSBalancingConfig *config)
{
    if (config == NULL) {
        return false;
    }

    if (!isfinite(config->start_delta_v) ||
        !isfinite(config->stop_delta_v)) {
        return false;
    }

    if (config->start_delta_v <= 0.0f) {
        return false;
    }

    if (config->stop_delta_v < 0.0f ||
        config->stop_delta_v >= config->start_delta_v) {
        return false;
    }

    if (config->max_balancing_groups == 0 ||
        config->max_balancing_groups > BMS_CELL_COUNT) {
        return false;
    }

    if (config->reevaluate_interval_ms == 0u) {
        return false;
    }

    if (!isfinite(config->switch_margin_v) ||
    config->switch_margin_v < 0.0f) {
    return false;
}

    return true;
}
