/**
    Manages the CAN bus interface. Receives messages and updates relevant variables in bike_context.
    If any errors are detected in the CAN nodes, prints the relevant error to syslog.
*/
#include "CAN.h"
#include "CANDecoder.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Global message buffer for CAN reception
static twai_message_t message;

// Spinlock protecting BatteryVoltages.cell_voltages writes/reads.
portMUX_TYPE g_cell_voltages_mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Ensure CAN bus is initialized and configured properly
 */
 void initCAN()
{
    // Initialize configuration structures using macro initializers
    // TODO: Maybe adjust and utilize specific timing and filter configurations
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    //Set up TWAI driver (CAN for ESP32-S3)
    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        printf("Failed to install TWAI driver\n");
        return;
    }
    if (twai_start() != ESP_OK) {
        printf("Failed to start TWAI driver\n");
        return;
    }
}

// used to print the contents of a CAN msg
void printMessage(twai_message_t msg)
{
    for (int i = 0; i < msg.data_length_code; i++)
    {
        Serial.print(msg.data[i]);
        Serial.print(":");
    }
    Serial.println();
}

// unused currently but should be implemented into the current firmware
void printBMSStatus(BMSStatus bms_status)
{
    switch ((int)bms_status.bms_status_flag)
    {
    case 1:
        Serial.printf("at least one cell V is > High Voltage Cutoff\n");
        break;
    case 2:
        Serial.printf("at least one cell V is < Low Voltage Cutoff\n");
        break;
    case 4:
        Serial.printf("at least one cell V is > Balance Voltage Cutoff\n");
        break;
    }
    Serial.printf("The BMSC ID is %d\n", bms_status.bms_c_id);
    switch (bms_status.bms_c_fault)
    {
    case 1:
        Serial.printf("BMS Fault: configuration not locked\n");
        break;
    case 2:
        Serial.printf("BMS Fault: not all cells present\n");
        break;
    case 4:
        Serial.printf("BMS Fault: thermistor overtemp\n");
        break;
    case 8:
        Serial.printf("BMS Fault: not all thermistors present\n");
        break;
    }
    if (bms_status.ltc_fault == 1)
    {
        Serial.printf("LTC fault detected\n");
    }
}

void requestCellVoltages()
{
    static int next_can_id = BMSC1_LTC1_REQUEST_CELLS;
    twai_message_t cellVoltageRxMsg = {};
    cellVoltageRxMsg.identifier = next_can_id;
    cellVoltageRxMsg.extd = 1; // Extended 29-bit ID
    cellVoltageRxMsg.rtr = 1;  // Remote transmission request
    cellVoltageRxMsg.data_length_code = 0; // no data payload

    if (twai_transmit(&cellVoltageRxMsg, pdMS_TO_TICKS(1000)) != ESP_OK) {
        printf("Failed to request cell voltages\n");
    }

    // Toggle between requesting from LTC1 and LTC2
    next_can_id = next_can_id == BMSC1_LTC1_REQUEST_CELLS ?
        BMSC1_LTC2_REQUEST_CELLS :
        BMSC1_LTC1_REQUEST_CELLS;
}

/**
 * Checks the CAN bus for any buffered messages and decodes one into the bike context.
 * 
 * @param canData struct containing pointer to bike context
 *  */ 
static void checkCAN(superbike::CANTaskData canData)
{
    superbike::Context *context = canData.bike_context;

    if (twai_receive(&message, pdMS_TO_TICKS(0)) != ESP_OK) {
        // No message received
        return;
    }

    bool recognized = CANDecoder::dispatch(
        message,
        &context->motor_stats,
        &context->motor_temps,
        &context->bms_status,
        &context->charge_controller_stats,
        &context->charger_stats,
        &context->thermistor_temps,
        &context->battery_voltages,
        &g_cell_voltages_mux,
        &context->motor_last_rx_ms,
        &context->bms_last_rx_ms,
        &context->charger_last_rx_ms,
        &context->evcc_last_rx_ms
    );

    if (!recognized) {
        // Unrecognized message ID
        printf("Received unrecognized CAN message ID: 0x%lX\n", message.identifier);
    }
}

/**
 * A FreeRTOS task that continuously checks the CAN bus for new messages
 * 
 * @param canData pointer to the CAN task data structure
 */
void canTask(void *canData)
{
    TickType_t last_request = xTaskGetTickCount();
    while (1)
    {
        /* NOTE: CAN breaks if we try sending messages with 0 other nodes on the bus.
        / Therefore, change CAN_NODES in Main.h to make sure things dont break. */
        if (CAN_NODES != 0)
        {
            checkCAN(*(superbike::CANTaskData *)canData);
            /* Ask for other half of cell voltages from BMS every 2 seconds, 
            test timings later to improve boot performance */
            if (xTaskGetTickCount() > (last_request + 2000))
            {
                requestCellVoltages();
                last_request = xTaskGetTickCount();
            }
        }
        // delay 20ms
        vTaskDelay((20 * configTICK_RATE_HZ) / 1000);
    }
}
