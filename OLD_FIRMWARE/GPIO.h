
/// Teensy pin for contactor control (closing or opening)
#define CONTACTOR_CONTROL 17
/// Teensy pin for relay in series with precharge resistor
#define PRECHARGE_CONTROL 16
/// Teensy pin for starting precharge, exit precharging, exit done-precharging
#define HIGH_VOLTAGE_TOGGLE 24

// IMD (ISOMETER IR155-3204) pins
#define IMD_OKHS_PIN   <pin_number>   // status output, high-side - digital in
#define IMD_MHS_PIN    <pin_number>   // measurement output, high-side PWM - must support attachInterrupt

#define CAN_RX 0
#define CAN_TX 1

bool check_HV_toggle();

void open_contactor();

void close_contactor();

void open_precharge();

void close_precharge();

void initGPIO();
