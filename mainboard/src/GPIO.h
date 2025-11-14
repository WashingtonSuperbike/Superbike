/// Teensy pin for contactor control (closing or opening). Need to update to new board
#define CONTACTOR_CONTROL 17 
/// Teensy pin for relay in series with precharge resistor. Need to update to new board 
#define PRECHARGE_CONTROL 16 
/// Original Teensy pin for starting precharge, exit precharging, exit done-precharginh. Need to update to new board
#define HIGH_VOLTAGE_TOGGLE 24 

#define CAN_RX 0 
#define CAN_TX 1 
bool check_HV_toggle(); 
void open_contactor(); 
void close_contactor(); 
void open_precharge(); 
void close_precharge(); 
void initGPIO(); 
