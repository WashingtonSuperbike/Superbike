#include <FlexCAN_T4.h>

/// Motor controller message - CAN
#define MOTOR_STATS_MSG 0x0CF11E05
/// Motor controller message - CAN
#define MOTOR_TEMPS_MSG 0x0CF11F05

FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> CAN_2;


void setup(void) {
  Serial.begin(115200); delay(400);
  CAN_2.begin();
  CAN_2.setBaudRate(250000);
  CAN_2.setMaxMB(16);
  CAN_2.enableFIFO();
  CAN_2.enableFIFOInterrupt();
  //CAN_2.onReceive(canSniff);
  CAN_2.mailboxStatus();

  // Send faults here... or in loop 
  for (int i = 0; i < 2; i++) {
    sendStats();
    delay(1000);
    sendTemps();
    delay(1000);
  }
}

void sendStats() {
  CAN_message_t msg;
  msg.flags.extended = 1;

  msg.id = MOTOR_STATS_MSG;
  msg.len = 4;
  msg.buf[0] = 0;  
  msg.buf[1] = 0;  
  msg.buf[2] = 0; 
  msg.buf[3] = 0;  

  // Send message
  CAN_2.write(msg);

  delay(1000);
}

void sendTemps() {
  CAN_message_t msg;
  msg.flags.extended = 1;

  msg.id = MOTOR_TEMPS_MSG;
  msg.len = 4;
  msg.buf[0] = 0;  
  msg.buf[1] = 0;  
  msg.buf[2] = 0; 
  msg.buf[3] = 0;  

  // Send message
  CAN_2.write(msg);

  delay(1000);
}
