#pragma once
#include "Config.h"

/**
 * EMAFilter - Exponential Moving Average Filter
 *
 * A lightweight, O(1) filter for smoothing noisy sensor data.
 * Requires only 4 bytes (one float) of state per signal.
 */
class EMAFilter {
public:
    /**
     * @param time_constant_sec Time constant (tau) in seconds.
     */
    EMAFilter(float time_constant_sec);

    /**
     * Feed a raw value into the filter and get the smoothed output.
     */
    float update(float raw_value);

    /**
     * Instantly set the filter state to a specific value.
     * Useful for boot/initialization to avoid ramp-up from zero.
     */
    void reset(float initial_value);

private:
    float alpha_;
    float last_output_;
    bool  initialized_;
};
