#include "waveshare_twai_port.h"

unsigned long previousMillis = 0;  // Will store the last time a message was sent

// Send message
static void send_message() {
  // Configure message to transmit
  twai_message_t message;
  message.identifier = 0x0F6; // Set message identifier
  message.data_length_code = 8; // Set data length
  for (int i = 0; i < message.data_length_code; i++) {
    message.data[i] = i; // Populate message data
  }
  // Queue message for transmission 
  if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    printf("Message queued for transmission\n"); // Print success message
  } else {
    printf("Failed to queue message for transmission\n"); // Print failure message
  }
  memset(message.data, 0, sizeof(message.data)); // Clear the entire array
}

bool waveshare_twai_init()
{ 
    // Initialize configuration structures using macro initializers
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();   // Set 250Kbps
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); // Accept all messages

    // Install TWAI driver
    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        printf("Failed to install driver\n"); // Print error message
        return false; // Return false if driver installation fails
    }
    printf("Driver installed\n"); // Print success message

    // Start TWAI driver
    if (twai_start() != ESP_OK) {
        printf("Failed to start driver\n"); // Print error message
        return false; // Return false if starting the driver fails
    }
    printf("Driver started\n"); // Print success message

    // Reconfigure alerts to detect TX alerts and Bus-Off errors
    uint32_t alerts_to_enable = TWAI_ALERT_TX_IDLE | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED;
    if (twai_reconfigure_alerts(alerts_to_enable, NULL) != ESP_OK) {
        printf("Failed to reconfigure alerts\n"); // Print error message
        return false; // Return false if alert reconfiguration fails
    }
    printf("CAN Alerts reconfigured\n"); // Print success message

    // TWAI driver is now successfully installed and started
    return true; // Return true on success
}

void waveshare_twai_transmit()
{
    // Check if alert happened
    uint32_t alerts_triggered;
    twai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(POLLING_RATE_MS)); // Read triggered alerts
    twai_status_info_t twaistatus; // Create status info structure
    twai_get_status_info(&twaistatus); // Get status information

    // Handle alerts
    if (alerts_triggered & TWAI_ALERT_BUS_OFF) {
      printf("Alert: Bus-Off state detected! Initiating recovery...\n");
      twai_initiate_recovery(); // Start bus-off recovery — transitions to TWAI_STATE_RECOVERING
      printf("Recovery initiated. Waiting for bus to recover...\n");
      return; // Skip this cycle to allow recovery
    }
    if (alerts_triggered & TWAI_ALERT_BUS_RECOVERED) {
      printf("Alert: Bus recovery complete. Restarting driver...\n");
      if (twai_start() == ESP_OK) {
        printf("Driver restarted after bus recovery.\n");
      } else {
        printf("Failed to restart driver after bus recovery.\n");
      }
      return;
    }
    if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
      printf("Alert: TWAI controller has become error passive.\n"); // Print passive error alert
    }
    if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
      // printf("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.\n"); // Print bus error alert
      // printf("Bus error count: %d\n", twaistatus.bus_error_count); // Print bus error count
    }
    if (alerts_triggered & TWAI_ALERT_TX_FAILED) {
      printf("Alert: The Transmission failed.\n"); // Print transmission failure alert
      printf("TX buffered: %d\t", twaistatus.msgs_to_tx); // Print buffered TX messages
      printf("TX error: %d\t", twaistatus.tx_error_counter); // Print TX error count
      printf("TX failed: %d\n", twaistatus.tx_failed_count); // Print failed TX count
    }
    if (alerts_triggered & TWAI_ALERT_TX_SUCCESS) {
      printf("Alert: The Transmission was successful.\n"); // Print successful transmission alert
      printf("TX buffered: %d\t", twaistatus.msgs_to_tx); // Print buffered TX messages
    }
    
    // Check controller state and print status
    if (twaistatus.state == TWAI_STATE_BUS_OFF) {
      printf("WARNING: Controller is in Bus-Off state!\n");
    } else if (twaistatus.state == TWAI_STATE_RECOVERING) {
      printf("INFO: Controller is recovering...\n");
    }

    // Send message
    unsigned long currentMillis = millis(); // Get current time in milliseconds
    if (currentMillis - previousMillis >= TRANSMIT_RATE_MS) { // Check if it's time to send a message
      previousMillis = currentMillis; // Update last send time
      send_message(); // Call function to send message
    }
}
