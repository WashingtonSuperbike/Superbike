#include <FlexCAN_T4.h>
 
/// Motor controller message - CAN
#define MOTOR_STATS_MSG 0x0CF11E05
/// Motor controller message - CAN
#define MOTOR_TEMPS_MSG 0x0CF11F05
 
// MOTOR_STATS errors
//Byte[6]:
#define IDENTIFICATION_FAULT 0x01
#define HIGH_VOLTAGE_FAULT   0x02
#define LOW_VOLTAGE_FAULT    0x04
#define STALL_FAULT          0x10
#define INTERNAL_VOLTS_FAULT 0x20
#define CONTROLLER_OVER_TEMPERATURE_FAULT 0x40
#define THROTTLE_SIGNAL_FAULT      0x80
//Byte[7]:
#define INTERNAL_RESET_FAULT   0x02
#define THROTTLE_CIRCUIT_FAULT 0x04
#define ANGLE_SENSOR_FAULT   0x08
#define MOTOR_OVER_TEMPERATURE_FAULT 0x40
#define GALVANOMETER_FAULT        0x80
 
// Instantiate the CAN object for CAN2
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> CAN_2;
 
// Prototypes (Optional in Arduino IDE, but good practice)
void sendIdentificationFault();
void sendHighVoltageFault();
void sendLowVoltageFault();
void sendStallFault();
void sendOverTemperatureFault();
 
void setup() {
  Serial.begin(115200);
  delay(400);
 
  CAN_2.begin();
  CAN_2.setBaudRate(250000);
 
  CAN_2.setMaxMB(16);
  CAN_2.enableFIFO();
  CAN_2.enableFIFOInterrupt();
 
  // Optional: Print status to Serial Monitor
  CAN_2.mailboxStatus();
 
  Serial.println("CAN bus initialized. Sending initial faults...");
 
  // Send initial faults once during setup
  sendIdentificationFault();
  delay(1000);
  sendHighVoltageFault();
  delay(1000);
  sendLowVoltageFault();
  delay(1000);
  sendStallFault();
  delay(1000);
  sendOverTemperatureFault();
  delay(1000);
}
 
// The loop function is required for the code to compile
void loop() {
  CAN_2.events(); // Keeps the CAN stack processing
 
 
 
  sendIdentificationFault();
  // delay(50);
  // sendStats();
  delay(1000);
  sendHighVoltageFault();
  // delay(50);
  // sendStats();
  delay(1000);
  sendLowVoltageFault();
  // delay(50);
  // sendStats();
  delay(1000);
  sendStallFault();
  // delay(50);
  // sendStats();
  delay(1000);
  sendOverTemperatureFault();
  // delay(50);
  // sendStats();
  delay(1000);
}
 
void sendStats() {
  uint16_t rpm = 1250;
 
  CAN_message_t msg;
  msg.flags.extended = 1; // Extended ID (29-bit)
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  msg.buf[0] = rpm & 0xFF;  
  msg.buf[1] = (rpm >> 8) & 0xFF;  
  msg.buf[2] = 0;
  msg.buf[3] = 0;
  msg.buf[4] = 0;  
  msg.buf[5] = 0;  
  msg.buf[6] = 0;
  msg.buf[7] = 0;
  CAN_2.write(msg);
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
  CAN_2.write(msg);
}
 
void sendIdentificationFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0; // Clear buffer
  msg.buf[6] = IDENTIFICATION_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Identification Fault");
}
 
void sendHighVoltageFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[6] = HIGH_VOLTAGE_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: High Voltage Fault");
}
 
void sendLowVoltageFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[6] = LOW_VOLTAGE_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Low Voltage Fault");
}
 
void sendStallFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[6] = STALL_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Stall Fault");
}
 
void sendInternalVoltsFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[6] = INTERNAL_VOLTS_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Internal Volts Fault");
}

void sendOverTemperatureFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[6] = CONTROLLER_OVER_TEMPERATURE_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Controller Over Temperature Fault");
}

void sendThrottleSignalFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[6] = THROTTLE_SIGNAL_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Throttle SignalFault");
}

void sendInternalResetFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[7] = INTERNAL_RESET_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Internal Reset Fault");
}

void sendThrottleCircuitFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[7] = THROTTLE_CIRCUIT_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Throttle Circuit Fault");
}

void sendAngleSensorFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[7] = ANGLE_SENSOR_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Angle Sensor Fault");
}

void motorOverTemperatureFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[7] = MOTOR_OVER_TEMPERATURE_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Motor Over Temperature Fault");
}

void sendGalvanometerFault() {
  CAN_message_t msg;
  msg.flags.extended = 1;
  msg.id = MOTOR_STATS_MSG;
  msg.len = 8;
  for(int i=0; i<8; i++) msg.buf[i] = 0;
  msg.buf[7] = GALVANOMETER_FAULT;
  CAN_2.write(msg);
  Serial.println("Sent: Galvanometer Fault");
}
