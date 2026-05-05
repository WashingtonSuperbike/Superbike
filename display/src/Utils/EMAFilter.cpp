#include "EMAFilter.h"
#include <cmath>

EMAFilter::EMAFilter(float time_constant_sec)
    : last_output_(0.0f), initialized_(false)
{
    if (time_constant_sec <= 0.0f) {
        alpha_ = 1.0f; // No smoothing
    } else {
        // alpha = 1 - exp(-dt / tau)
        // where dt = 1 / REFRESH_RATE_HZ
        alpha_ = 1.0f - expf(-1.0f / (time_constant_sec * REFRESH_RATE_HZ));
    }
}

float EMAFilter::update(float raw_value)
{
    if (!initialized_) {
        last_output_ = raw_value;
        initialized_ = true;
        return last_output_;
    }

    // EMA Formula: output = alpha * raw + (1 - alpha) * last_output
    last_output_ = (alpha_ * raw_value) + ((1.0f - alpha_) * last_output_);
    return last_output_;
}

void EMAFilter::reset(float initial_value)
{
    last_output_ = initial_value;
    initialized_ = true;
}
