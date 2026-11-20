/**
   @file Config.h
     @author    Washington Superbike
     @date      1-May-2026
      @brief     Centralized configuration file for Superbike firmware. 
*/

#pragma once

#define CONFIG_LOG_COUNT 7

#define CAN_BMS_TIMEOUT_MS_MS 2000

#define REFRESH_RATE_HZ 30.0f

// DEBUG FLAGS: Uncomment one of these to force the dashboard to only show a specific screen.
// #define DEBUG_SPEEDOMETER_SCREEN_ONLY
// #define DEBUG_CHARGING_SCREEN_ONLY
