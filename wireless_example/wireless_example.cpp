#include <Arduino.h>

#define TX_BAUD   9600
#define RX_GPIO   18
#define TX_GPIO   17
#define SET_PIN   16

#define ONBOARD_LED 48

void setup() {
  neopixelWrite(ONBOARD_LED, 85, 0, 0);

  Serial.begin(115200);
  delay(50);
  Serial.println(F("Starting up"));

  // Initialize HC-12 Module (Wireless transceiver)
  pinMode(RX_GPIO, INPUT);
  Serial2.begin(9600, SERIAL_8N1, RX_GPIO, TX_GPIO);
  pinMode(SET_PIN, OUTPUT);
  digitalWrite(SET_PIN, LOW); // pull pin low to enter command mode
  delay(500);
  Serial2.print("AT+RX");  // send command to get status from HC-12
  while (!Serial2.available()) {
    delay(100)
  }
  Serial.write(Serial2.read());  // print out response
  digitalWrite(SET_PIN, HIGH);   // exit command mode

  Serial.println("Transmitter started");
  neopixelWrite(ONBOARD_LED, 0, 0, 0);
}

void loop() {
  if (Serial2.available()) {
    Serial.write(Serial2.read());
  }

  if(Serial.available()) {
    Serial2.write(Serial.read());
  }
}
