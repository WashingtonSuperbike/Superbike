/**
 * Pins.h - Mainboard-specific GPIO pin definitions and hardware constants
 *
 * This file contains all pin definitions and hardware-specific constants
 * for the superbike mainboard. Other boards (display board, hardware-in-loop)
 * will have their own pin definition files.
 */

#pragma once

#include "driver/gpio.h"
#include <stdint.h>

// ============================================================================
// CAN BUS PINS
// ============================================================================

/// CAN TX pin for ESP32
#define CAN_TX_PIN GPIO_NUM_17

/// CAN RX pin for ESP32
#define CAN_RX_PIN GPIO_NUM_18

/// Alternative CAN pin definitions for GPIO compatibility
/// These are legacy Teensy definitions - need verification for ESP32
#define CAN_TX 1
#define CAN_RX 0

// ============================================================================
// HIGH VOLTAGE CONTROL PINS
// ============================================================================

/// Teensy pin for contactor control (closing or opening)
/// TODO: Verify these pin numbers for ESP32-S3 board
#define CONTACTOR_CONTROL 17

/// Teensy pin for relay in series with precharge resistor
/// TODO: Verify these pin numbers for ESP32-S3 board
#define PRECHARGE_CONTROL 16

/// Original Teensy pin for starting precharge, exit precharging, exit done-precharging
/// TODO: Verify these pin numbers for ESP32-S3 board
#define HIGH_VOLTAGE_TOGGLE 24

// ============================================================================
// DISPLAY PINS (TFT + Touchscreen)
// ============================================================================

/// The Teensy pin used for the display's chip select pin
/// TODO: Verify these pin numbers for ESP32-S3 board
#define TFT_CS 10

/// The Teensy pin used for the display data/command pin
/// TODO: Verify these pin numbers for ESP32-S3 board
#define TFT_DC 9

/// The Teensy pin used for the display reset pin
/// TODO: Verify these pin numbers for ESP32-S3 board
#define TFT_RST 8

/// The Teensy pin used for the display touch-screen chip select
/// This is unused because we don't use the touch screen
#define TS_CS 10

/// The TIRQ interrupt signal (unused)
#define TIRQ_PIN -1

// Note: MOSI=11, MISO=12, SCK=13 are SPI hardware pins
// These may need to change for ESP32

// ============================================================================
// SD CARD PINS (SDIO Interface)
// ============================================================================

/// SD card clock pin
#define SD_clk 4

/// SD card command pin
#define SD_cmd 5

/// SD card data pin 0
#define SD_d0 6

/// SD card data pin 1
#define SD_d1 7

/// SD card data pin 2
#define SD_d2 8

/// SD card data pin 3
#define SD_d3 9

// ============================================================================
// FREERTOS TASK STACK SIZES
// ============================================================================

/// CAN task stack size
#define CAN_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE + 1000)

/// Data logging task stack size
#define DATALOGGING_TASK_STACK_SIZE 10000

/// Precharge task stack size
#define PRECHARGE_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE + 2000)

/// Display task stack size
#define DISPLAY_TASK_STACK_SIZE 10000

// ============================================================================
// SYSTEM CONSTANTS
// ============================================================================

/// Number of LTC (Linear Technology battery monitors) connected
/// TODO: Verify this matches actual hardware
#define NUMBER_OF_LTCS 2

/// Motor controller maximum safe operating temperature (Celsius)
/// TODO: Verify this value
#define MOTORCONTROLLER_TEMP_MAX 65

/// Motor maximum safe operating temperature (Celsius)
/// TODO: Verify this value
#define MOTOR_TEMP_MAX 80

// ============================================================================
// DISPLAY CONSTANTS
// ============================================================================

/// Max time in ms it takes for display to update
#define DISPLAY_UPDATE_TIME_MAX 50

/// Number of PrintedData values (length of the array that contains all the printedData)
#define NUM_DATA 11

/// The default starter value used for floats in the PrintedDataStruct
#define DEFAULT_FLOAT -1

/// The default x position used for data in the PrintedDataStruct
#define DEFAULT_X_POS 235

/// A scaler for moving vertical data down
/// Each row of data is a factor of 16 pixels away from each other, vertically
#define VERTICAL_SCALER 16

// Touch screen calibration parameters (unused but kept for reference)
#define TS_MINX 327
#define TS_MAXX 3903
#define TS_MINY 243
#define TS_MAXY 3842
