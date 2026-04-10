#include <Arduino.h>
#include "driver/twai.h"

// Hardware Configuration for ESP32-S3
#define CAN_TX_PIN GPIO_NUM_43
#define CAN_RX_PIN GPIO_NUM_44
#define LED_PIN GPIO_NUM_1

// BMS CAN Message IDs
#define DD_BMS_STATUS_IND           0x01dd0001  // BMS status indication
#define DD_BMSC_TH_STATUS_IND       0x01df0e00  // BMS thermistor readings
#define DD_BMS_CVCUR_REQ            0x01de0800  // REQUEST cell voltages (LTC1)
#define DD_BMS_CVCUR_C1_TO_C4_RSP   0x01df0900  // RESPONSE (cells 1-4)
#define DD_BMS_CVCUR_C5_TO_C8_RSP   0x01df0a00  // RESPONSE (cells 5-8)
#define DD_BMS_CVCUR_C9_TO_C12_RSP  0x01df0b00  // RESPONSE (cells 9-12)

// Fault Codes
#define BMS_FLAG_CELL_HVC        0x01 
#define BMS_FLAG_CELL_LVC        0x02 
#define BMS_FLAG_CELL_BVC        0x04 
#define BMS_FAULT_NOT_LOCKED     0x01 
#define BMS_FAULT_CENSUS         0x02 
#define BMS_FAULT_OVERTEMP       0x04 
#define BMS_FAULT_THERM_CENSUS   0x08 

// Function Prototypes
void handleIncomingMessages();
void sendThermistorTemps(int ltcID, bool overtemp);
void sendHVCFault();
void sendNormal();

void setup() {
    Serial.begin(115200);
    // Wait up to 3s for USB CDC connection, then continue anyway
    unsigned long start = millis();
    while (!Serial && millis() - start < 3000);
    delay(100);
    Serial.println("BMS Simulator Starting (ESP32-S3 TWAI)...");
    pinMode(LED_PIN, OUTPUT);

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS(); // Set to 250k
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
        Serial.println("TWAI Driver Started @ 250k");
    } else {
        Serial.println("TWAI Driver Failed!");
        return;
    }

    for (int i = 0; i < 2; i++) {
        sendNormal();
        delay(1000);
    }
}

static unsigned long loopCount = 0;

void loop() {
    // Replace FlexCAN events() with manual polling
    handleIncomingMessages();

    digitalWrite(LED_PIN, HIGH);

    // Send a high voltage fault every second
    sendHVCFault();
    delay(100);
    sendThermistorTemps(1, true);
    delay(100);
    sendThermistorTemps(2, true);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(500);

    Serial.printf("Loop %lu\n", loopCount++);
}

void handleIncomingMessages() {
    twai_message_t rec_msg;
    // Non-blocking check for messages
    while (twai_receive(&rec_msg, 0) == ESP_OK) {
        Serial.printf("RX ID: 0x%08X\n", rec_msg.identifier);

        // Prepare generic response template
        twai_message_t send_msg;
        send_msg.extd = 1; // All IDs in original code are Extended (29-bit)
        send_msg.data_length_code = 8;
        
        // Fill response buffer with 40000 (0x9C40) across 4 uint16s
        for (int i = 0; i < 4; i++) {
            send_msg.data[i*2]     = 0x40; 
            send_msg.data[i*2 + 1] = 0x9C;
        }

        // Cell voltages requested for LTC1 or LTC2
        if (rec_msg.identifier == DD_BMS_CVCUR_REQ || rec_msg.identifier == (DD_BMS_CVCUR_REQ | 0x01)) {
            uint32_t offset = (rec_msg.identifier & 0x01);
            
            send_msg.identifier = DD_BMS_CVCUR_C1_TO_C4_RSP | offset;
            twai_transmit(&send_msg, pdMS_TO_TICKS(10));
            
            send_msg.identifier = DD_BMS_CVCUR_C5_TO_C8_RSP | offset;
            twai_transmit(&send_msg, pdMS_TO_TICKS(10));
            
            send_msg.identifier = DD_BMS_CVCUR_C9_TO_C12_RSP | offset;
            twai_transmit(&send_msg, pdMS_TO_TICKS(10));
        }
    }
}

void sendNormal() {
    twai_message_t msg;
    msg.identifier = DD_BMS_STATUS_IND;
    msg.extd = 1;
    msg.data_length_code = 4;
    msg.data[0] = 0; msg.data[1] = 0; msg.data[2] = 0; msg.data[3] = 0;

    twai_transmit(&msg, pdMS_TO_TICKS(10));
    delay(100);

    for (int i = 0; i < 8; i++) {
        sendThermistorTemps(i, false);
    }
}

void sendHVCFault() {
    twai_message_t msg;
    msg.identifier = DD_BMS_STATUS_IND;
    msg.extd = 1;
    msg.data_length_code = 4;
    msg.data[0] = BMS_FLAG_CELL_HVC;
    msg.data[1] = 0; msg.data[2] = 0; msg.data[3] = 0;

    twai_transmit(&msg, pdMS_TO_TICKS(10));
}

void sendThermistorTemps(int ltcID, bool overtemp) {
    twai_message_t msg;
    msg.identifier = DD_BMSC_TH_STATUS_IND;
    msg.extd = 1;
    msg.data_length_code = 8;

    msg.data[0] = ltcID;
    msg.data[1] = 0b00011111; 
    msg.data[2] = 0b00011111;
    msg.data[3] = overtemp ? 60 : 30;
    for(int i=4; i<8; i++) msg.data[i] = 30;

    twai_transmit(&msg, pdMS_TO_TICKS(10));
}