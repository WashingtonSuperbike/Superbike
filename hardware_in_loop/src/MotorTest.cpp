#include <FlexCAN_T4.h>

/// Motor controller message - CAN
#define MOTOR_STATS_MSG 0x0CF11E05
/// Motor controller message - CAN
#define MOTOR_TEMPS_MSG 0x0CF11F05

FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> CAN_2;


