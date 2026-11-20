/**
   @file config.h
     @author    Washington Superbike
     @date      1-March-2023
     @brief
        If you are looking to change configurations for the overall
        firmware this is the place to do it. Changing the number of CAN devices connected,
        the Screen type used, etc. All can be done from here.
     
     \todo
        CHANGE THE NUM_THERMI based on the number of thermistors that Powertain settles on.
        \n \n
*/

#ifndef _CONFIG_H
#define _CONFIG_H

/// This primarily exists to debug the changes made in the FlexCAN library.
/// If there are no devices connected on the CAN bus, the firmware crashes
/// This line can be set to 0 to ensure that the CAN bus does not bother
/// to check the CAN bus if there are 0 nodes connected.
#define CAN_NODES 1

/// Maximum time (ms) allowed between BMS status messages before isHVSafe() treats
/// the BMS as offline and forces HV_ERROR. Only enforced after the first message.
#define CAN_BMS_TIMEOUT_MS 2000

/// Maximum time (ms) allowed with NO CAN traffic of any kind before isHVSafe()
/// treats the bus as failed (bus-off, transceiver/wiring fault) and forces
/// HV_ERROR. Only enforced after the first frame has ever been received.
/// canTask also attempts a rate-limited controller reinstall while silent.
#define CAN_BUS_TIMEOUT_MS 1000

/// This exists to be changed based on the final number of thermistors
/// we settle on having in the code later.
#define THERMISTOR_COUNT 10

/* We only have 20 cells but need to check all 24 LTC cell locations */
#define CELL_COUNT 24

#define CONFIG_LOG_COUNT 7

//#define CONFIG_TEST_SCREEN_DATA 1

#endif // _CONFIG_H
