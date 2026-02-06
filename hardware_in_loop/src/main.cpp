// Include header files
#include <Arduino.h>

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
