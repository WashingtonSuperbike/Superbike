/**
 * Display.h - TFT display functionality
 *
 * Header file for display task that manages the ILI9341 TFT display
 * and renders bike system data to the screen.
 */

#pragma once

#include <Adafruit_ILI9341.h>
#include <Adafruit_GFX.h>
#include <SPI.h>

#include "Config.h"
#include "Context.h"
#include "Pins.h"

// ============================================================================
// DISPLAY COLORS (Based on screen type from Config.h)
// ============================================================================

#ifdef USE_DEBUGGING_SCREEN
#  define BACKGROUND_COLOR ILI9341_WHITE
#  define PRINT_COLOR ILI9341_BLACK
#else
#  define BACKGROUND_COLOR ILI9341_BLACK
#  define PRINT_COLOR ILI9341_WHITE
#endif

// ============================================================================
// EXTERN DECLARATIONS FOR GLOBAL DISPLAY OBJECTS
// ============================================================================
// Note: Actual definitions are in Display.cpp

extern Adafruit_ILI9341 tft;
extern PrintedDataStruct printedVals[NUM_DATA];

extern PrintedDataStruct *batteryVoltage;
extern PrintedDataStruct *motorControllerVoltage;
extern PrintedDataStruct *auxBatteryVoltage;
extern PrintedDataStruct *rpm;
extern PrintedDataStruct *motorTemperature;
extern PrintedDataStruct *motorCurr;
extern PrintedDataStruct *errMessage;
extern PrintedDataStruct *chargerVolt;
extern PrintedDataStruct *chargerCurr;
extern PrintedDataStruct *bmsStatusFlag;
extern PrintedDataStruct *evccVolt;

extern PrintedDataThermStruct thermiData;
extern PrintedDataTimeStruct timeData;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Forward declaration (struct defined in Context.h)
struct DisplayTaskData;

void displayTask(void *displayTaskData);
void initDisplay(superbike::Context *context);
void displayUpdate(superbike::Context *context);
void thermiDataPrint(int numberOfLines);
void timePrint();
void setupMeasurementScreen();
bool eraseThenPrintIfDiff(int xPos, int yPos, String oldData, String newData);
void manualScreenDataUpdater();
float aux_voltage_read();
