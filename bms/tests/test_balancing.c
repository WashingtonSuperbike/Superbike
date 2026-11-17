
#include <assert.h>
#include <stdint.h>

#include "bms_balancing.h"

int main(void)
{
    // Create a measurement structure
    BMSMeasurements measurements = {0};

    // Simulate 24 valid cell groups
    for (uint32_t i = 0; i < BMS_CELL_COUNT; i++) {
        measurements.cell_voltage_v[i] = 3.80f;
        measurements.cell_voltage_valid[i] = true;
    }

    measurements.fresh = true;

    // Create our output variable
    uint32_t output_mask = 0xFFFFFFFFu;

    // Call our balancing function
    bool success = BMS_SelectBalancing(
        &measurements,
        &BMS_TEST_BALANCING_CONFIG,
        0u,
        false,
        &output_mask
    );

    // Verify the expected results
    assert(success == true);
    assert(output_mask == 0u);



    // TEST 2: One group above the balancing threshold

    // Simulate Group 3 having a higher voltage
    measurements.cell_voltage_v[2] = 3.90f;

    // Reset the output variable
    output_mask = 0xFFFFFFFFu;

    // Run the balancing algorithm
    success = BMS_SelectBalancing(
        &measurements,
        &BMS_TEST_BALANCING_CONFIG,
        0u,
        true,
        &output_mask
    );

    // Check expected results
    assert(success == true);
    assert(output_mask == (1u << 2));



    // TEST 3: Multiple eligible groups, maximum of 2

    // Reset all 24 groups to 3.80 V
    for (uint32_t i = 0; i < BMS_CELL_COUNT; i++) {
        measurements.cell_voltage_v[i] = 3.80f;
    }

    // Simulate four groups with elevated voltages
    measurements.cell_voltage_v[2]  = 3.95f;  // Group 3
    measurements.cell_voltage_v[7]  = 3.92f;  // Group 8
    measurements.cell_voltage_v[11] = 3.90f;  // Group 12
    measurements.cell_voltage_v[16] = 3.88f;  // Group 17

    
    // Reset the output mask before testing
    output_mask = 0xFFFFFFFFu;

    // Run the balancing algorithm
    success = BMS_SelectBalancing(
        &measurements,
        &BMS_TEST_BALANCING_CONFIG,
        0u,
        true,
        &output_mask
    );

    // Verify the function completed successfully
    assert(success == true);

    // Verify only Groups 3 and 8 were selected
    assert(output_mask == ((1u << 2) | (1u << 7)));


    // TEST 4: Only one balancing group permitted

    BMSBalancingConfig single_group_config =
        BMS_TEST_BALANCING_CONFIG;

    single_group_config.max_balancing_groups = 1;

    output_mask = 0xFFFFFFFFu;

    success = BMS_SelectBalancing(
        &measurements,
        &single_group_config,
        0u,
        true,
        &output_mask
    );

    assert(success == true);
    assert(output_mask == (1u << 2));



    // TEST 5A: Group is between start and stop thresholds

    // Reset all voltages
    for (uint32_t i = 0; i < BMS_CELL_COUNT; i++) {
        measurements.cell_voltage_v[i] = 3.80f;
    }

    // Group 3 is slightly above the average
    measurements.cell_voltage_v[2] = 3.83f;

    // Reset output mask
    output_mask = 0xFFFFFFFFu;

    // Group 3 was NOT previously balancing
    success = BMS_SelectBalancing(
        &measurements,
        &BMS_TEST_BALANCING_CONFIG,
        0u,
        true,
        &output_mask
    );

    assert(success == true);
    assert(output_mask == 0u);


    // TEST 5B: Previously active group remains eligible

    output_mask = 0xFFFFFFFFu;

    success = BMS_SelectBalancing(
        &measurements,
        &BMS_TEST_BALANCING_CONFIG,
        (1u << 2),
        true,
        &output_mask
    );

    assert(success == true);
    assert(output_mask == (1u << 2));


    // TEST 5C: Previously active group reaches stop threshold

    // Lower Group 3's voltage
    measurements.cell_voltage_v[2] = 3.82f;

    // Initialize output mask to all 1s
    output_mask = 0xFFFFFFFFu;

    // Group 3 was previously balancing
    success = BMS_SelectBalancing(
        &measurements,
        &BMS_TEST_BALANCING_CONFIG,
        (1u << 2),
        true,
        &output_mask
    );

    // Verify the calculation succeeded
    assert(success == true);

    // Verify Group 3 is no longer selected
    assert(output_mask == 0u);


    // TEST 6: Switching margin prevents unnecessary switching

    // Reset all 24 groups
    for (uint32_t i = 0; i < BMS_CELL_COUNT; i++) {
        measurements.cell_voltage_v[i] = 3.80f;
    }

    // Two groups above the starting threshold
    measurements.cell_voltage_v[2] = 3.875f;  // Group 3
    measurements.cell_voltage_v[7] = 3.885f;  // Group 8


    // Only one group can balance at a time
    BMSBalancingConfig margin_config =
        BMS_TEST_BALANCING_CONFIG;

    margin_config.max_balancing_groups = 1;

    // Initialize the output mask
    output_mask = 0xFFFFFFFFu;

    // Run the balancing algorithm
    success = BMS_SelectBalancing(
        &measurements,
        &margin_config,
        (1u << 2),
        true,
        &output_mask
    );

    // Verify the result
    assert(success == true);
    assert(output_mask == (1u << 2));



    // TEST 6B: Allow switching when a new group is sufficiently higher

    // Raise Group 8's voltage
    measurements.cell_voltage_v[7] = 3.895f;

    // Reset output
    output_mask = 0xFFFFFFFFu;

    // Group 3 was previously balancing
    success = BMS_SelectBalancing(
        &measurements,
        &margin_config,
        (1u << 2),
        true,
        &output_mask
    );

    assert(success == true);
    assert(output_mask == (1u << 7));


    // TEST 7: Reject stale measurements

    // Simulate measurements that are no longer fresh
    measurements.fresh = false;

    // Initialize output to a nonzero value
    output_mask = 0xFFFFFFFFu;

    success = BMS_SelectBalancing(
        &measurements,
        &margin_config,
        0u,
        true,
        &output_mask
    );

    assert(success == false);
    assert(output_mask == 0u);

    // Restore freshness for future tests
    measurements.fresh = true;


    return 0;
}
