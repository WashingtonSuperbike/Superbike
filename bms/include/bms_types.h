
#ifndef BMS_TYPES_H
#define BMS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

// Number of series cell groups
#define BMS_CELL_COUNT 24u

// Latest battery measurements
typedef struct {

    // Cell group voltages
    float cell_voltage_v[BMS_CELL_COUNT];

    // Whether each voltage measurement is valid
    bool cell_voltage_valid[BMS_CELL_COUNT];

    // Temperatures from thermistor board
    float cell_temperature_c[BMS_CELL_COUNT];

    // Whether each temperature measurement is valid
    bool temperature_valid[BMS_CELL_COUNT];

    // Whether this measurement snapshot is fresh
    bool fresh;

} BMSMeasurements;

#endif
