
#ifndef BMS_CONFIG_H
#define BMS_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "bms_types.h"

// Passive balancing configuration - Parameters for balancing decisions
typedef struct {

    float start_delta_v;
    float stop_delta_v;

    float switch_margin_v;

    uint8_t max_balancing_groups;
    uint32_t reevaluate_interval_ms;

} BMSBalancingConfig;

// Battery protection configuration - Parameters for battery protection decisions (over voltage/under voltage/over temperature)
typedef struct {

    float overvoltage_trip_v;
    float undervoltage_trip_v;
    float overtemperature_trip_c;

} BMSProtectionConfig;

// Complete BMS configuration
typedef struct {

    BMSBalancingConfig balancing;
    BMSProtectionConfig protection;

} BMSConfig;

// Test configuration defined in bms_config.c
extern const BMSBalancingConfig
    BMS_TEST_BALANCING_CONFIG;

// Configuration validation function
bool BMS_ValidateBalancingConfig(
    const BMSBalancingConfig *config
);

#endif
